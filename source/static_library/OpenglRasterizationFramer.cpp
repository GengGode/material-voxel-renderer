#include "OpenglRasterizationFramer.hpp"

#include <global-register-error.hpp>
#include <global-variables-pool.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <spdlog/spdlog.h>

#include <glad/glad.h>

#include <imgui.h>
#include <implot.h>

#include "interface/pixel.hpp"
#include "interface/voxel.hpp"
#include "texture_from.hpp"

#include <set>

using texture_t = uint32_t;
using shader_t = uint32_t;
using program_t = uint32_t;

using texture_pool = std::set<texture_t>;

static texture_t color_table_tex = 0;
static texture_t vol_le_tex = 0;
static texture_t vol_he_tex = 0;
static texture_t vol_tex = 0;

static pixel<uint32_t> color_table = make_pixel<uint32_t>({ 256, 256 });
// Equivalent thickness and equivalent atomic number
// attenuation coefficient
static voxel<uint16_t> vol_le = make_voxel<uint16_t>({ 64, 64, 64 });
static voxel<uint16_t> vol_he = make_voxel<uint16_t>({ 64, 64, 64 });
static voxel<uint16_t> vol = make_voxel<uint16_t>({ 64, 64, 64 });

static uint16_t view_width = 800;
static uint16_t view_height = 600;
static texture_t render_texture = 0;
static program_t user_program = 0;
static uint32_t user_framebuffer_id = 0;
static uint32_t user_vertex_array_object = 0;

static inline bool check_compile_errors(shader_t shader, std::source_location loc = std::source_location::current())
{
    int success;
    char info_log[1024];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(shader, 1024, nullptr, info_log);
        SPDLOG_ERROR("shader compilation failed: {} (at {}:{})", info_log, loc.file_name(), loc.line());
        return false;
    }
    return true;
}
static inline bool check_link_errors(program_t program, std::source_location loc = std::source_location::current())
{
    int success;
    char info_log[1024];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(program, 1024, nullptr, info_log);
        SPDLOG_ERROR("program linking failed: {} (at {}:{})", info_log, loc.file_name(), loc.line());
        return false;
    }
    return true;
}

struct camera_info
{
    // 摄像机位置
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f);
    // 摄像机四元数
    glm::quat orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    // 摄像机视点距离
    float target_distance = 3.0f;
    // 摄像机视角立体角
    float fov = glm::radians(45.0f);
    // 摄像机宽高比
    float aspect_ratio = 4.0f / 3.0f;
    // 摄像机前方向
    glm::vec3 front() const { return orientation * glm::vec3(0.0f, 0.0f, -1.0f); }
    // 摄像机上方向
    glm::vec3 up() const { return orientation * glm::vec3(0.0f, 1.0f, 0.0f); }
    // 摄像机右方向
    glm::vec3 right() const { return orientation * glm::vec3(1.0f, 0.0f, 0.0f); }
    // 摄像机lookAt矩阵
    glm::mat4 view() const { return glm::lookAt(position, position + front(), up()); }
    // 摄像机投影矩阵
    glm::mat4 projection() const { return glm::perspective(fov, aspect_ratio, 0.1f, 100.0f); }
    // 通过鼠标移动摄像机位置
    void rotate_from_mouse(int mouse_dx, int mouse_dy, float sensitivity = 0.005f)
    {
        float yaw = -static_cast<float>(mouse_dx) * sensitivity;
        float pitch = -static_cast<float>(mouse_dy) * sensitivity;

        glm::quat yaw_quat = glm::angleAxis(yaw, up());
        glm::quat pitch_quat = glm::angleAxis(pitch, right());

        orientation = glm::normalize(yaw_quat * pitch_quat * orientation);
    }
    void orbit_around_target(int mouse_dx, int mouse_dy, float sensitivity = 0.005f)
    {
        // 先计算目标点
        glm::vec3 target_point = position + front() * target_distance;
        rotate_from_mouse(mouse_dx, mouse_dy, sensitivity);
        position = target_point - front() * target_distance;
    }
    // 通过鼠标滚轮缩放摄像机位置
    void zoom_from_scroll(int scroll_offset, float sensitivity = 0.05f)
    {
        target_distance -= static_cast<float>(scroll_offset) * sensitivity;
        if (target_distance < 0.1f)
            target_distance = 0.1f;
        position = position + front() * (static_cast<float>(scroll_offset) * sensitivity);
    }
    // 通过鼠标滚轮缩放调整fov
    void adjust_fov(int scroll_offset, float sensitivity = 0.05f)
    {
        fov -= static_cast<float>(scroll_offset) * sensitivity;
        fov = glm::clamp(fov, glm::radians(1.0f), glm::radians(90.0f));
    }
    // 设置目标点
    void set_target(const glm::vec3& target, bool maintain_distance = true)
    {
        if (glm::length(target - position) < 0.001f)
            return;
        if (maintain_distance)
            target_distance = glm::length(target - position);
        look_at(target);
    }
    // 看向特定点
    void look_at(const glm::vec3& target)
    {
        glm::vec3 direction = glm::normalize(target - position);
        orientation = glm::quatLookAt(direction, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    struct status
    {
        double last_mouse_x = 0.0;
        double last_mouse_y = 0.0;
        bool is_rotating = false;
        bool is_orbiting = false;
        bool is_zooming = false;
        float move_sensitivity = 0.005f;
        float scroll_sensitivity = 0.05f;
        float zoom_sensitivity = 0.05f;
        void reset_flags()
        {
            last_mouse_x = 0.0;
            last_mouse_y = 0.0;
            is_rotating = false;
            is_orbiting = false;
            is_zooming = false;
        }
        void mouse_controls(ImGuiIO& io, camera_info& cam)
        {
            if (io.WantCaptureMouse)
                return reset_flags();

            double current_mouse_x = io.MousePos.x;
            double current_mouse_y = io.MousePos.y;
            double wheel = io.MouseWheel;

            bool left_pressed = io.MouseDown[0];
            bool right_pressed = io.MouseDown[1];
            bool middle_pressed = io.MouseDown[2];

            if (right_pressed)
            {
                if (is_rotating)
                {
                    int dx = static_cast<int>(current_mouse_x - last_mouse_x);
                    int dy = static_cast<int>(current_mouse_y - last_mouse_y);
                    cam.rotate_from_mouse(dx, dy, move_sensitivity);
                }
                is_rotating = true;
            }
            else
            {
                is_rotating = false;
                // SPDLOG_INFO("Stop rotating");
            }

            if (left_pressed)
            {
                if (is_orbiting)
                {
                    int dx = static_cast<int>(current_mouse_x - last_mouse_x);
                    int dy = static_cast<int>(current_mouse_y - last_mouse_y);
                    cam.orbit_around_target(dx, dy, move_sensitivity);
                }
                is_orbiting = true;
            }
            else
            {
                is_orbiting = false;
                // SPDLOG_INFO("Stop orbiting");
            }

            if (middle_pressed)
            {
                if (is_zooming)
                {
                    int dy = static_cast<int>(current_mouse_y - last_mouse_y);
                    cam.zoom_from_scroll(-dy, scroll_sensitivity);
                }
                is_zooming = true;
            }
            else
            {
                is_zooming = false;
                // SPDLOG_INFO("Stop zooming");
            }

            last_mouse_x = current_mouse_x;
            last_mouse_y = current_mouse_y;

            if (wheel != 0.0f)
            {
                cam.adjust_fov(static_cast<int>(-wheel), zoom_sensitivity);
            }
        }
        void keyboard_controls(ImGuiIO& io, camera_info& cam)
        {
#if 0
            if (io.WantCaptureKeyboard)
                return reset_flags();

            float delta_time = io.DeltaTime;
            float velocity = move_sensitivity * delta_time;

            float move_speed = 0.1f;
            if (io.KeyShift)
                move_speed *= 2.0f;
            if (io.KeyCtrl)
                move_speed *= 0.5f;

            if (io.KeysData[ImGuiKey_W].Down)
                cam.position += cam.front() * move_speed;
            if (io.KeysData[ImGuiKey_S].Down)
                cam.position -= cam.front() * move_speed;
            if (io.KeysData[ImGuiKey_A].Down)
                cam.position -= cam.right() * move_speed;
            if (io.KeysData[ImGuiKey_D].Down)
                cam.position += cam.right() * move_speed;
            if (io.KeysData[ImGuiKey_Q].Down)
                cam.position -= cam.up() * move_speed;
            if (io.KeysData[ImGuiKey_E].Down)
                cam.position += cam.up() * move_speed;
#else
            (void)io;
            (void)cam;
#endif
        }
    } status;
};

void init()
{
    global::onlyone::create<texture_pool>();

    auto rand_memory = [](auto& mem) {
        for (auto& v : mem)
            v = static_cast<std::remove_reference_t<decltype(v)>>(rand() % 256);
    };
    rand_memory(color_table.memory);
    rand_memory(vol_le.memory);
    rand_memory(vol_he.memory);
    rand_memory(vol.memory);

    color_table_tex = texture_from(color_table);
    vol_le_tex = texture_from(vol_le);
    vol_he_tex = texture_from(vol_he);
    vol_tex = texture_from(vol);

    global::onlyone::call<texture_pool>([&](texture_pool& pool) {
        pool.insert(vol_le_tex);
        pool.insert(vol_he_tex);
        return true;
    });

    float vertices[] = { -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f };
    unsigned int vertices_buffer_object;
    glGenBuffers(1, &vertices_buffer_object);
    glBindBuffer(GL_ARRAY_BUFFER, vertices_buffer_object);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    const char* vertex_shader_source = R"(
        #version 330 core
        layout (location = 0) in vec3 position;
        layout (location = 1) in vec3 normal;
        layout (location = 2) in vec2 texcoord;
        uniform mat4 view;
        uniform mat4 projection;
        out vec2 ver_TexCoord;

        void main()
        {
            gl_Position = projection * view * vec4(position, 1.0);
            ver_TexCoord = texcoord;
        }
    )";
    shader_t vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, nullptr);
    glCompileShader(vertex_shader);
    if (!check_compile_errors(vertex_shader))
        return;

    // texture_t color_table_texture = texture_from(color_table);
    const char* fragment_shader_source = R"(
        #version 410 core
        uniform float time;
        uniform sampler2D color_table_texture;
        out vec4 FragColor;

        in vec2 ver_TexCoord;

        void main()
        {
            float brightness = time - floor(time);
            FragColor = vec4(vec3(ver_TexCoord, 0.2) * brightness, 1.0);
        }
    )";

    shader_t fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, nullptr);
    glCompileShader(fragment_shader);
    if (!check_compile_errors(fragment_shader))
        return;

    user_program = glCreateProgram();
    glAttachShader(user_program, vertex_shader);
    glAttachShader(user_program, fragment_shader);
    glLinkProgram(user_program);
    if (!check_link_errors(user_program))
        return;
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glGenVertexArrays(1, &user_vertex_array_object);
    glBindVertexArray(user_vertex_array_object);
    glBindBuffer(GL_ARRAY_BUFFER, vertices_buffer_object);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}
static inline GLuint render_3d_texture_preview(GLuint tex3d, int axis, float slice, int width, int height, bool force_update = false)
{

    static GLuint program = 0;
    static GLuint output_tex = 0;
    static int last_preview_axis = -1;
    static float last_preview_slice = -1.0f;

    bool is_changed = (axis != last_preview_axis) || (std::abs(slice - last_preview_slice) > 0.001f);
    if (!is_changed && !force_update)
        return output_tex;

    last_preview_axis = axis;
    last_preview_slice = slice;

    if (program == 0)
    {
        const char* cs_src = R"(
        #version 430
        layout (local_size_x = 16, local_size_y = 16) in;

        layout (rgba8, binding = 0) writeonly uniform image2D output_tex;
        uniform usampler3D tex3d;
        uniform int axis;   // 0=x, 1=y, 2=z
        uniform float slice; // 0~1

        void main() {
            ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
            ivec2 size2d = imageSize(output_tex);
            if (pixel.x >= size2d.x || pixel.y >= size2d.y)
                return;

            vec2 uv = vec2(pixel) / vec2(size2d);

            vec3 coord;
            if (axis == 0)       coord = vec3(slice, uv);
            else if (axis == 1)  coord = vec3(uv.x, slice, uv.y);
            else                 coord = vec3(uv, slice);

            uint color = texture(tex3d, coord).r;
            float fcolor = color / 255.0;
            imageStore(output_tex, pixel, vec4(fcolor, fcolor, fcolor, 1.0));
        }
        )";

        GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(cs, 1, &cs_src, nullptr);
        glCompileShader(cs);
        GLint success;
        glGetShaderiv(cs, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char log[1024];
            glGetShaderInfoLog(cs, 1024, nullptr, log);
            SPDLOG_ERROR("Compute Shader Error: {}", log);
        }

        program = glCreateProgram();
        glAttachShader(program, cs);
        glLinkProgram(program);
        glDeleteShader(cs);
    }

    if (output_tex == 0)
    {
        glGenTextures(1, &output_tex);
        glBindTexture(GL_TEXTURE_2D, output_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glUseProgram(program);

    glBindImageTexture(0, output_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, tex3d);
    glUniform1i(glGetUniformLocation(program, "tex3d"), 0);
    glUniform1i(glGetUniformLocation(program, "axis"), axis);
    glUniform1f(glGetUniformLocation(program, "slice"), slice);

    glDispatchCompute((width + 15) / 16, (height + 15) / 16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    return output_tex;
}

void update()
{
    static float time = 0.0f;
    time += 0.01f;

    static camera_info cam;
    cam.aspect_ratio = static_cast<float>(view_width) / static_cast<float>(view_height);
    // cam.look_at(glm::vec3(0.0f, 0.0f, 0.0f));
    cam.status.mouse_controls(ImGui::GetIO(), cam);
    cam.status.keyboard_controls(ImGui::GetIO(), cam);
    ImGui::Begin("Camera Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("位置: (%.2f, %.2f, %.2f)", cam.position.x, cam.position.y, cam.position.z);
    ImGui::Text("前向: (%.2f, %.2f, %.2f)", cam.front().x, cam.front().y, cam.front().z);
    ImGui::Text("上方: (%.2f, %.2f, %.2f)", cam.up().x, cam.up().y, cam.up().z);
    ImGui::Text("右侧: (%.2f, %.2f, %.2f)", cam.right().x, cam.right().y, cam.right().z);
    ImGui::Text("视点目标距离: %.2f", cam.target_distance);
    ImGui::Text("FOV: %.2f °", glm::degrees(cam.fov));
    ImGui::Text("宽高比: %.2f", cam.aspect_ratio);
    ImGui::Separator();
    ImGui::Checkbox("旋转摄像机(右键拖拽)", &cam.status.is_rotating);
    ImGui::Checkbox("环绕目标点旋转摄像机(左键拖拽)", &cam.status.is_orbiting);
    ImGui::Checkbox("视线方向推拉摄像机(中键拖拽)", &cam.status.is_zooming);
    ImGui::SliderFloat("移动灵敏度", &cam.status.move_sensitivity, 0.001f, 0.02f);
    ImGui::SliderFloat("视线方向推拉灵敏度", &cam.status.scroll_sensitivity, 0.01f, 0.2f);
    ImGui::SliderFloat("滚轮调整FOV灵敏度", &cam.status.zoom_sensitivity, 0.01f, 0.2f);

    if (ImGui::Button("Reset Camera"))
    {
        cam.position = glm::vec3(0.0f, 0.0f, 3.0f);
        cam.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        cam.target_distance = 3.0f;
    }

    ImGui::Text("使用说明:");
    ImGui::Text("右键拖拽: 旋转摄像机");
    ImGui::Text("左键拖拽: 环绕目标点旋转摄像机");
    ImGui::Text("中键拖拽: 视线方向推拉摄像机");
    ImGui::Text("滚轮: 调整视角大小(FOV)");

    ImGui::End();

    glm::mat4 model = glm::mat4(1.0f);

    glUniform1f(glGetUniformLocation(user_program, "time"), time);
    glUniformMatrix4fv(glGetUniformLocation(user_program, "view"), 1, GL_FALSE, glm::value_ptr(cam.view()));
    glUniformMatrix4fv(glGetUniformLocation(user_program, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(user_program, "projection"), 1, GL_FALSE, glm::value_ptr(cam.projection()));

    glUseProgram(user_program);
    glBindVertexArray(user_vertex_array_object);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
void uninit()
{
    if (user_vertex_array_object != 0)
        glDeleteVertexArrays(1, &user_vertex_array_object);
    if (user_program != 0)
        glDeleteProgram(user_program);
}

int OpenglRasterizationFramer::initialize()
{
    glGenFramebuffers(1, &user_framebuffer_id);
    glBindFramebuffer(GL_FRAMEBUFFER, user_framebuffer_id);

    glGenTextures(1, &render_texture);
    glBindTexture(GL_TEXTURE_2D, render_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, view_width, view_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render_texture, 0);
    if (GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
        return code_err("Framebuffer is not complete! (status: {})", (int)status);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    init();

    return 0;
}

void OpenglRasterizationFramer::next_frame()
{
    glBindFramebuffer(GL_FRAMEBUFFER, user_framebuffer_id);
    glViewport(0, 0, view_width, view_height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 这里画你的 OpenGL 三角形、场景
    update();

    // 回到默认 framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ImGui::SetNextWindowSize(ImVec2(820, 620), ImGuiCond_Once);
    ImGui::Begin("Preview", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (render_texture != 0)
        ImGui::Image((ImTextureID)(intptr_t)render_texture, ImVec2(view_width, view_height), ImVec2(0, 1), ImVec2(1, 0));
    ImGui::End();
    static texture_t selected_preview_tex = 0;
    bool force_update = false;
    {
        ImGui::Begin("3D Texture List", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        global::onlyone::call<texture_pool>([&](texture_pool& pool) {
            for (const auto& tex : pool)
            {
                if (ImGui::Button(fmt::format("Texture {}", tex).c_str()))
                {
                    selected_preview_tex = tex;
                    force_update = true;
                }
            }
            return true;
        });
        ImGui::End();
    }

    ImGui::Begin("3D Texture Preview");
    static int preview_axis = 2; // 默认 z 方向
    static float preview_slice = 0.5f;
    ImGui::SliderInt("Axis", &preview_axis, 0, 2);
    ImGui::SliderFloat("Slice", &preview_slice, 0.0f, 1.0f);

    if (selected_preview_tex != 0)
    {
        GLuint preview_tex = render_3d_texture_preview(selected_preview_tex, preview_axis, preview_slice, 640, 640, force_update);
        ImGui::Image((ImTextureID)(intptr_t)preview_tex, ImVec2(640, 640), ImVec2(0, 1), ImVec2(1, 0));
    }
    ImGui::End();
}

void OpenglRasterizationFramer::destroy()
{
    uninit();
    if (render_texture != 0)
        glDeleteTextures(1, &render_texture);
    if (user_framebuffer_id != 0)
        glDeleteFramebuffers(1, &user_framebuffer_id);
}
