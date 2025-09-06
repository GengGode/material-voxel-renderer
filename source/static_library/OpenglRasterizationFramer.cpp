#include "OpenglRasterizationFramer.hpp"

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
static voxel<uint16_t> vol = make_voxel<uint16_t>({ 3, 3, 3 });

static uint16_t view_width = 800;
static uint16_t view_height = 600;
static uint32_t user_framebuffer_id = 0;
static uint32_t user_framebuffer_texture_id = 0;
static uint32_t user_shader_program = 0;
static uint32_t user_vertex_array_object = 0;

using texture_t = uint32_t;
using shader_t = uint32_t;
using program_t = uint32_t;

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

void init()
{
    float vertices[] = { -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f };
    unsigned int vertices_buffer_object;
    glGenBuffers(1, &vertices_buffer_object);
    glBindBuffer(GL_ARRAY_BUFFER, vertices_buffer_object);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    const char* vertex_shader_source = R"(
        #version 330 core
        layout (location = 0) in vec3 position;

        void main()
        {
            gl_Position = vec4(position, 1.0);
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

        void main()
        {
            float brightness = time - floor(time);
            FragColor = vec4(0.6, 0.5, brightness, 1.0);
        }
    )";

    shader_t fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, nullptr);
    glCompileShader(fragment_shader);
    if (!check_compile_errors(fragment_shader))
        return;

    user_shader_program = glCreateProgram();
    glAttachShader(user_shader_program, vertex_shader);
    glAttachShader(user_shader_program, fragment_shader);
    glLinkProgram(user_shader_program);
    if (!check_link_errors(user_shader_program))
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
void update()
{
    static float time = 0.0f;
    time += 0.01f;
    glUniform1f(glGetUniformLocation(user_shader_program, "time"), time);
    glUseProgram(user_shader_program);
    glBindVertexArray(user_vertex_array_object);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
void uninit()
{
    if (user_vertex_array_object != 0)
        glDeleteVertexArrays(1, &user_vertex_array_object);
    if (user_shader_program != 0)
        glDeleteProgram(user_shader_program);
}

int OpenglRasterizationFramer::initialize()
{
    glGenFramebuffers(1, &user_framebuffer_id);
    glBindFramebuffer(GL_FRAMEBUFFER, user_framebuffer_id);

    glGenTextures(1, &user_framebuffer_texture_id);
    glBindTexture(GL_TEXTURE_2D, user_framebuffer_texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, view_width, view_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, user_framebuffer_texture_id, 0);
    if (GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
        return code_err("Framebuffer is not complete! (status: {})", status);
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
    if (user_framebuffer_texture_id != 0)
        ImGui::Image((ImTextureID)(intptr_t)user_framebuffer_texture_id, ImVec2(view_width, view_height), ImVec2(0, 1), ImVec2(1, 0));
    ImGui::End();
}

void OpenglRasterizationFramer::destroy()
{
    uninit();
    if (user_framebuffer_texture_id != 0)
        glDeleteTextures(1, &user_framebuffer_texture_id);
    if (user_framebuffer_id != 0)
        glDeleteFramebuffers(1, &user_framebuffer_id);
}
