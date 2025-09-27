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

#include "camera_info.hpp"

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

    auto buffer = load_raw_file("foot.raw");
    std::vector<uint16_t> buffer16(buffer.size());
    for (size_t i = 0; i < buffer.size(); ++i)
        buffer16[i] = static_cast<uint16_t>(buffer[i]) << 8;
    vol = make_voxel<uint16_t>({ 256, 256, 256 }, buffer16);

    color_table_tex = texture_from(color_table);
    vol_le_tex = texture_from(vol_le);
    vol_he_tex = texture_from(vol_he);
    vol_tex = texture_from(vol);

    global::onlyone::call<texture_pool>([&](texture_pool& pool) {
        pool.insert(vol_le_tex);
        pool.insert(vol_he_tex);
        pool.insert(vol_tex);
        return true;
    });
}
void update()
{
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
void uninit() {}

int OpenglRasterizationFramer::initialize()
{
    // 立方体顶点（位置 + 纹理坐标）
    float cube_vertices[] = {
        -0.5f, -0.5f, 0.5f,  0.0f, 0.0f, 0.5f, -0.5f, 0.5f,  1.0f, 0.0f, 0.5f, 0.5f, 0.5f,  1.0f, 1.0f, -0.5f, 0.5f, 0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.5f, -0.5f, 0.0f, 1.0f, -0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
    };
    unsigned int cube_indices[] = { 0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 5, 4, 7, 7, 6, 5, 4, 0, 3, 3, 7, 4, 3, 2, 6, 6, 7, 3, 4, 5, 1, 1, 0, 4 };
    unsigned int vertex_buffer_object = 0;
    unsigned int element_buffer_object = 0;
    unsigned int vertex_array_object = 0;
    glGenVertexArrays(1, &vertex_array_object);
    glGenBuffers(1, &vertex_buffer_object);
    glGenBuffers(1, &element_buffer_object);
    glBindVertexArray(vertex_array_object);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_object);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer_object);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW);
    // 位置属性
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // 纹理坐标属性
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    user_vertex_array_object = vertex_array_object;

    const char* vertex_shader_source = R"(
        #version 330 core
        layout (location = 0) in vec3 position;
        layout (location = 2) in vec2 texcoord;
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;

        out vec2 ver_TexCoord;
        out vec3 ver_FragPos; // 片段位置（世界空间）
        
        void main()
        {
            gl_Position = projection * view * model * vec4(position, 1.0);
            ver_TexCoord = texcoord;
            ver_FragPos = vec3(model * vec4(position, 1.0));
        }
    )";
    const char* fragment_shader_source = R"(
        #version 410 core
        uniform usampler3D volume1_tex;
        uniform vec3 camera_position;

        in vec2 ver_TexCoord;
        in vec3 ver_FragPos;
        out vec4 FragColor;

        struct ray {
            vec3 direction;
            vec3 position;
            float value;
            int count;
        };

        void main()
        {
            if (gl_FrontFacing == false)
                discard;

            ray r;
            r.direction = normalize(ver_FragPos - camera_position);
            r.position = ver_FragPos;
            r.value = 0.0;
            r.count = 0;
            for (int i = 0; i < 10000; i++)
            {
                vec3 coord = r.position + r.direction * float(i) * 0.005;
                if (any(lessThan(coord, vec3(-0.5))) || any(greaterThan(coord, vec3(0.5))))
                    break;
                vec3 tex_coord = coord + vec3(0.5);
                uint intensity = texture(volume1_tex, tex_coord).r;
                float alpha = float(intensity) / 256.0;
                if (alpha < 0.01)
                    continue;

                r.count++;
                r.value = r.value + (alpha - r.value) / float(r.count);
            }
            
            FragColor = vec4(vec3(r.value), 1.0);
        }
    )";

    shader_t vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, nullptr);
    glCompileShader(vertex_shader);
    if (!check_compile_errors(vertex_shader))
        return code_err("Vertex shader compilation failed");

    shader_t fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, nullptr);
    glCompileShader(fragment_shader);
    if (!check_compile_errors(fragment_shader))
        return code_err("Fragment shader compilation failed");

    user_program = glCreateProgram();
    glAttachShader(user_program, vertex_shader);
    glAttachShader(user_program, fragment_shader);
    glLinkProgram(user_program);
    if (!check_link_errors(user_program))
        return code_err("Shader program linking failed");
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

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

    glUseProgram(user_program);
    glUniformMatrix4fv(glGetUniformLocation(user_program, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(user_program, "view"), 1, GL_FALSE, glm::value_ptr(cam.view()));
    glUniformMatrix4fv(glGetUniformLocation(user_program, "projection"), 1, GL_FALSE, glm::value_ptr(cam.projection()));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, vol_le_tex);
    glUniform1i(glGetUniformLocation(user_program, "volume1_tex"), 0);
    glUniform3fv(glGetUniformLocation(user_program, "camera_position"), 1, glm::value_ptr(cam.position));

    glBindVertexArray(user_vertex_array_object);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    update();
}

void OpenglRasterizationFramer::destroy()
{
    if (user_vertex_array_object != 0)
        glDeleteVertexArrays(1, &user_vertex_array_object);
    if (user_program != 0)
        glDeleteProgram(user_program);
    uninit();
    if (render_texture != 0)
        glDeleteTextures(1, &render_texture);
    if (user_framebuffer_id != 0)
        glDeleteFramebuffers(1, &user_framebuffer_id);
}
