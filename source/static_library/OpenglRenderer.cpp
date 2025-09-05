#include "OpenglRenderer.hpp"

#include <global-register-error.hpp>

#include <glad/glad.h>

#include <GLFW/glfw3.h>

void error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int OpenglRenderer::initialize()
{
    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
        return code_err("{}: Failed to initialize GLFW", __func__);

    // Request an OpenGL 4.1, core, context from GLFW.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create a window on the operating system, then tie the OpenGL context to it.
    window = std::shared_ptr<GLFWwindow>(glfwCreateWindow((int)(1280), (int)(800), "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr), glfwDestroyWindow);
    if (window == nullptr)
        return code_err("{}: Failed to create GLFW window", __func__);
    glfwMakeContextCurrent(window.get());

    // Start Glad, so we can call OpenGL functions.
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        return code_err("{}: Failed to initialize OpenGL context", __func__);

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
void OpenglRenderer::render_loop(std::stop_token& token)
{
    auto glfw_window = window.get();
    while (!token.stop_requested() && !glfwWindowShouldClose(glfw_window))
    {
        glfwPollEvents();
        if (glfwGetWindowAttrib(glfw_window, GLFW_ICONIFIED) != 0)
        {
            glfwWaitEventsTimeout(0.1);
            continue;
        }

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glUseProgram(user_shader_program);
        glBindVertexArray(user_vertex_array_object);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        // Put the stuff we've been drawing onto the visible area.
        glfwSwapBuffers(glfw_window);
    }
}
void OpenglRenderer::destroy()
{
    // Close OpenGL window, context, and any other GLFW resources.
    window.reset();
    glfwTerminate();
}

void OpenglRenderer::set_vertex_shader(const std::string& source)
{
    vertex_shader_source = source;
}
void OpenglRenderer::set_fragment_shader(const std::string& source)
{
    fragment_shader_source = source;
}
