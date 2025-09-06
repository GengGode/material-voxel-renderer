#include "OpenglImRenderer.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glad/glad.h>

#include <fmt/core.h>

int OpenglImRenderer::initialize()
{
    if (int err = OpenglRenderer::initialize(); err != 0)
        return err;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // 支持键盘

    ImGui::StyleColorsDark(); // 主题风格

    // 初始化 ImGui 的 GLFW+OpenGL3 后端
    ImGui_ImplGlfw_InitForOpenGL(window.get(), true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    std::shared_ptr<std::function<void()>> begin_recursive_task = std::make_shared<std::function<void()>>();
    *begin_recursive_task = [this, begin_recursive_task]() {
        // === ImGui 新帧 ===
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        static bool show_metrics_window = false;
        if (show_metrics_window)
            ImGui::ShowMetricsWindow(&show_metrics_window);

        // 半透明控制层
        ImGui::SetNextWindowBgAlpha(0.35f);
        ImGui::Begin("Control Panel", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Shader Debug UI");
        ImGui::Checkbox("Show ImGui Metrics Window", &show_metrics_window);
        static float clear_color[3] = { 0.2f, 0.3f, 0.3f };
        if (ImGui::ColorEdit3("Clear color", clear_color))
        {
            glClearColor(clear_color[0], clear_color[1], clear_color[2], 1.0f);
        }
        if (ImGui::Button("Reload Shaders"))
        {
            this->set_vertex_shader(R"(
                #version 330 core
                layout (location = 0) in vec3 position;

                void main()
                {
                    gl_Position = vec4(position, 1.0);
                }
            )");
            this->set_fragment_shader(fmt::format(R"(
                #version 330 core
                out vec4 FragColor;

                void main()
                {{
                    FragColor = vec4({},{},{}, 1.0);
                }}
            )",
                                                  clear_color[0], clear_color[1], clear_color[2]));
        }

        ImGui::End();

        // === 渲染 ImGui ===
        ImGui::Render();

        // 下一帧继续执行
        command_queue_on_begin->enqueue(*begin_recursive_task);
    };

    std::shared_ptr<std::function<void()>> end_recursive_task = std::make_shared<std::function<void()>>();
    *end_recursive_task = [this, end_recursive_task]() {
        // === ImGui 结束帧 ===
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // 下一帧继续执行
        command_queue_on_swap_before->enqueue(*end_recursive_task);
    };

    command_queue_on_begin->enqueue(*begin_recursive_task);
    command_queue_on_swap_before->enqueue(*end_recursive_task);

    return 0;
}

void OpenglImRenderer::destroy()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    OpenglRenderer::destroy();
}
