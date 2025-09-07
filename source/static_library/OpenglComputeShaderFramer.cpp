#include "OpenglComputeShaderFramer.hpp"

#include <spdlog/spdlog.h>

#include <global-register-error.hpp>

#include <glad/glad.h>

#include <glm/glm.hpp>

#include <imgui.h>
#include <implot.h>

#include "interface/pixel.hpp"
#include "interface/voxel.hpp"
#include "texture_from.hpp"

static pixel<uint32_t> color_table = make_pixel<uint32_t>({ 256, 256 });
// Equivalent thickness and equivalent atomic number
// attenuation coefficient
static voxel<uint16_t> vol_le = make_voxel<uint16_t>({ 64, 64, 64 });
static voxel<uint16_t> vol_he = make_voxel<uint16_t>({ 64, 64, 64 });
static voxel<uint8_t> vol = make_voxel<uint8_t>({ 64, 64, 64 });

using texture_t = uint32_t;
using shader_t = uint32_t;
using program_t = uint32_t;

static uint16_t view_width = 800;
static uint16_t view_height = 600;
static texture_t render_texture = 0;
static program_t user_program = 0;

static texture_t color_table_tex = 0;
static texture_t vol_le_tex = 0;
static texture_t vol_he_tex = 0;
static texture_t vol_tex = 0;

static float compute_time_ms = 0.0f;

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

void compute_shader_init()
{
    for (auto y = 0; y < 256; y++)
        for (auto x = 0; x < 256; x++)
        {
            if (x > 255 - y)
                color_table(x, y) = (255 << 24) | (y << 16) | (y << 8) | (255);
            else
                color_table(x, y) = (255 << 24) | (0 << 16) | (0 << 8) | (0);
        }
    for (auto& v : vol_le.memory)
        v = rand() % 65535;
    for (auto& v : vol_he.memory)
        v = rand() % 65535;
    for (auto& v : vol.memory)
        v = rand() % 255;
    color_table_tex = texture_from(color_table);
    vol_le_tex = texture_from(vol_le);
    vol_he_tex = texture_from(vol_he);
    vol_tex = texture_from(vol);
    const char* compute_shader_source = R"(
#version 430
layout (local_size_x = 16, local_size_y = 16) in;

// 输出二维纹理
layout (rgba8, binding = 0) writeonly uniform image2D output_texture;

// 输入纹理
uniform sampler2D color_table_tex;
uniform sampler3D vol_le_tex;
uniform sampler3D vol_he_tex;
uniform float time;

// 相机结构体
uniform struct {
    vec3 position;
    vec3 direction;
    vec3 up;
    float fov;
} camera;

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    if (pixel.x >= imageSize(output_texture).x || pixel.y >= imageSize(output_texture).y)
        return;

    // 示例：从 3D 体数据采样
    vec3 vol_coord = vec3(0.5,
                          float(pixel.y) / float(imageSize(output_texture).y),
                          0.5); // 中间切片

    float le_val = texture(vol_le_tex, vol_coord).r ;
    float he_val = texture(vol_he_tex, vol_coord).r ;

    vec4 color = vec4(le_val, he_val, 0.0, 1.0);
    //vec4 color = texture(color_table_tex, pixel / vec2(imageSize(output_texture)));
    imageStore(output_texture, pixel, color);
    //imageStore(output_texture, pixel, vec4(float(pixel.x) / float(imageSize(output_texture).x), float(pixel.y) / float(imageSize(output_texture).y), 0.5, 1.0));
}
    )";

    shader_t compute_shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(compute_shader, 1, &compute_shader_source, nullptr);
    glCompileShader(compute_shader);
    if (!check_compile_errors(compute_shader))
        return;

    user_program = glCreateProgram();
    glAttachShader(user_program, compute_shader);
    glLinkProgram(user_program);
    if (!check_link_errors(user_program))
        return;
    glDeleteShader(compute_shader);
}
void compute_shader_update()
{
    static float time = 0.0f;
    time += 0.01f;

    GLuint query;
    glGenQueries(1, &query);
    glBeginQuery(GL_TIME_ELAPSED, query);

    glUseProgram(user_program);

    // 绑定输出纹理
    glBindImageTexture(0, render_texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

    // 绑定二维颜色表纹理
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, color_table_tex);
    glUniform1i(glGetUniformLocation(user_program, "color_table_tex"), 0);

    // 绑定灰度 3D 纹理
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, vol_le_tex);
    glUniform1i(glGetUniformLocation(user_program, "vol_le_tex"), 1);

    // 绑定 Zeff 3D 纹理
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_3D, vol_he_tex);
    glUniform1i(glGetUniformLocation(user_program, "vol_he_tex"), 2);

    glUniform1f(glGetUniformLocation(user_program, "time"), time);

    glDispatchCompute((view_width + 15) / 16, (view_height + 15) / 16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // 结束计时
    glEndQuery(GL_TIME_ELAPSED);
    // 获取结果（纳秒）
    GLuint64 elapsed_time = 0;
    glGetQueryObjectui64v(query, GL_QUERY_RESULT, &elapsed_time);
    compute_time_ms = elapsed_time / 1'000'000.0f; // 转换为毫秒
}

void compute_shader_uninit()
{
    if (user_program != 0)
        glDeleteProgram(user_program);
}
GLuint render_3d_texture_preview(GLuint tex3d, int axis, float slice, int width, int height)
{
    static GLuint program = 0;
    static GLuint output_tex = 0;

    if (program == 0)
    {
        const char* cs_src = R"(
        #version 430
        layout (local_size_x = 16, local_size_y = 16) in;

        layout (rgba8, binding = 0) writeonly uniform image2D output_tex;
        uniform sampler3D tex3d;
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

            vec4 color = texture(tex3d, coord);
            imageStore(output_tex, pixel, color);
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
int OpenglComputeShaderFramer::initialize()
{
    compute_shader_init();

    glGenTextures(1, &render_texture);
    glBindTexture(GL_TEXTURE_2D, render_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, view_width, view_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    return 0;
}

void OpenglComputeShaderFramer::next_frame()
{
    compute_shader_update();

    // ImGui::SetNextWindowSize(ImVec2(820, 620), ImGuiCond_Once);
    ImGui::Begin("Preview", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (render_texture != 0)
        ImGui::Image((ImTextureID)(intptr_t)render_texture, ImVec2(view_width, view_height), ImVec2(0, 1), ImVec2(1, 0));
    ImGui::End();
    ImGui::Begin("Preview color table", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (color_table_tex != 0)
        ImGui::Image((ImTextureID)(intptr_t)color_table_tex, ImVec2(256, 256), ImVec2(0, 1), ImVec2(1, 0));
    ImGui::End();

    ImGui::Begin("3D Texture Preview");
    static int axis = 2; // 默认 z 方向
    static float slice = 0.5f;
    ImGui::SliderInt("Axis", &axis, 0, 2);
    ImGui::SliderFloat("Slice", &slice, 0.0f, 1.0f);

    GLuint preview_tex = render_3d_texture_preview(vol_tex, axis, slice, 256, 256);
    ImGui::Image((ImTextureID)(intptr_t)preview_tex, ImVec2(256, 256), ImVec2(0, 1), ImVec2(1, 0));
    ImGui::End();

    ImGui::Begin("Compute Shader Performance", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Compute Shader Time: %.3f ms", compute_time_ms);
    ImGui::End();
}

void OpenglComputeShaderFramer::destroy()
{
    if (render_texture != 0)
        glDeleteTextures(1, &render_texture);
    compute_shader_uninit();
}
