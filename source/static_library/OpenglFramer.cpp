#include "OpenglFramer.hpp"

#include <spdlog/spdlog.h>

#include <global-register-error.hpp>

#include <glad/glad.h>

#include <glm/glm.hpp>

#include <imgui.h>
#include <implot.h>

#include "interface/voxel.hpp"
#include "texture_from.hpp"

int init_frame_buffer_object(glm::ivec2 target_size = { 800, 600 })
{
    // Create the framebuffer object
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create a texture to render to
    GLuint fbo_texture;
    glGenTextures(1, &fbo_texture);
    glBindTexture(GL_TEXTURE_2D, fbo_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, target_size.x, target_size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Attach the texture to the framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_texture, 0);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);

    // // Setup a texture and load data later..
    // glGenTextures(1, &vol_tex3D);

    // // Create a renderbuffer object for depth and stencil attachment (optional)
    // GLuint rbo;
    // glGenRenderbuffers(1, &rbo);
    // glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    // glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 800, 600);
    // glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    // Check if the framebuffer is complete
    if (GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
        return code_err("{}: Framebuffer is not complete! (status: {})", __func__, status);

    // // Unbind the framebuffer
    // glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return 0;
}

voxel<uint16_t> vol = make_voxel<uint16_t>({ 3, 3, 3 });

uint32_t user_shader_program = 0;
uint32_t user_vertex_array_object = 0;

int OpenglFramer::initialize()
{
    // auto ret = init_frame_buffer_object();
    vol(0, 0, 0) = 0;
    vol(1, 0, 0) = 1;
    vol(2, 0, 0) = 2;
    vol(0, 1, 0) = 3;
    vol(1, 1, 0) = 4;
    vol(2, 1, 0) = 5;
    vol(0, 2, 0) = 6;
    vol(1, 2, 0) = 7;
    vol(2, 2, 0) = 8;
    vol(0, 0, 1) = 9;
    vol(1, 0, 1) = 10;
    vol(2, 0, 1) = 11;
    vol(0, 1, 1) = 12;
    vol(1, 1, 1) = 13;
    vol(2, 1, 1) = 14;
    vol(0, 2, 1) = 15;
    vol(1, 2, 1) = 16;
    vol(2, 2, 1) = 17;
    vol(0, 0, 2) = 18;
    vol(1, 0, 2) = 19;
    vol(2, 0, 2) = 20;
    vol(0, 1, 2) = 21;
    vol(1, 1, 2) = 22;
    vol(2, 1, 2) = 23;
    vol(0, 2, 2) = 24;
    vol(1, 2, 2) = 25;
    vol(2, 2, 2) = 26;
    GLuint tex3d = texture_from(vol);

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
    uint32_t vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, nullptr);
    glCompileShader(vertex_shader);

    int success;
    char info_log[512];
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertex_shader, 512, nullptr, info_log);
        spdlog::error("{}: Vertex shader compilation failed: {}", __func__, info_log);
        return -1;
    }

    const char* fragment_shader_source = R"(
        #version 330 core
        out vec4 FragColor;

        void main()
        {
            FragColor = vec4(1.0, 0.5, 0.2, 1.0);
        }
    )";

    uint32_t fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, nullptr);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragment_shader, 512, nullptr, info_log);
        spdlog::error("{}: Fragment shader compilation failed: {}", __func__, info_log);
        return -1;
    }

    user_shader_program = glCreateProgram();
    glAttachShader(user_shader_program, vertex_shader);
    glAttachShader(user_shader_program, fragment_shader);
    glLinkProgram(user_shader_program);
    glGetProgramiv(user_shader_program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(user_shader_program, 512, nullptr, info_log);
        spdlog::error("{}: Shader program linking failed: {}", __func__, info_log);
        return -1;
    }
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glGenVertexArrays(1, &user_vertex_array_object);
    glBindVertexArray(user_vertex_array_object);
    glBindBuffer(GL_ARRAY_BUFFER, vertices_buffer_object);
    // glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    return 0;
}
void OpenglFramer::next_frame()
{
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glUseProgram(user_shader_program);
    glBindVertexArray(user_vertex_array_object);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void OpenglFramer::destroy() {}
