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
static voxel<uint16_t> vol_le = make_voxel<uint16_t>({ 3, 3, 3 });
static voxel<uint16_t> vol_he = make_voxel<uint16_t>({ 3, 3, 3 });

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

struct camera_parameters
{
    glm::vec3 position; // 摄像机位置
    glm::vec3 forward;  // 摄像机前方向
    glm::vec3 up;       // 摄像机上方向
    glm::vec3 right;    // 摄像机右方向
    float fov;          // 摄像机视野
    float aspect;       // 视口宽高比
    float near_plane;   // 近裁剪面
    float far_plane;    // 远裁剪面
    bool orthographic;  // 是否为正交投影
};
// constexpr size_t camera_parameters_size = sizeof(camera_parameters); // 68 bytes
struct shader_camera
{
    glm::mat4 view;   // 视图矩阵
    glm::vec4 eye;    // 摄像机位置
    float view_plane; // 视平面距离
};
// constexpr size_t shader_camera_size = sizeof(shader_camera); // 80 bytes

void compute_shader_init()
{
    color_table_tex = texture_from(color_table);
    vol_le_tex = texture_from(vol_le);
    vol_he_tex = texture_from(vol_he);
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

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    if (pixel.x >= imageSize(output_texture).x || pixel.y >= imageSize(output_texture).y)
        return;

    // 示例：从 3D 体数据采样
    vec3 vol_coord = vec3(float(pixel.x) / float(imageSize(output_texture).x),
                          float(pixel.y) / float(imageSize(output_texture).y),
                          0.5); // 中间切片

    float le_val = texture(vol_le_tex, vol_coord).r;
    float he_val = texture(vol_he_tex, vol_coord).r;

    vec4 color = vec4(le_val, he_val, 0.5, 1.0);
    imageStore(output_texture, pixel, color);
    imageStore(output_texture, pixel, vec4(float(pixel.x) / float(imageSize(output_texture).x), float(pixel.y) / float(imageSize(output_texture).y), 0.5, 1.0));
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
