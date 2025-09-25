#include "ImRenderer.hpp"
#include "interface/ImFramerInterface.hpp"

#include <global-register-error.hpp>

#include <glad/glad.h>

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

int ImRenderer::initialize()
{
    if (!glfwInit())
        return code_err("{}: Failed to initialize GLFW", __func__);

#if 1
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#else
    const char* glsl_version = "#version 300 es";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#endif
    // float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
    GLFWwindow* glfw_window = glfwCreateWindow((int)(1280), (int)(800), "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
    if (glfw_window == nullptr)
        return code_err("{}: Failed to create GLFW window", __func__);

    window = std::shared_ptr<GLFWwindow>(glfw_window, glfwDestroyWindow);
    glfwMakeContextCurrent(glfw_window);
    // Load OpenGL functions via glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        return code_err("{}: Failed to initialize OpenGL context", __func__);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsLight();

    // Setup scaling
    // ImGuiStyle& style = ImGui::GetStyle();
    // style.ScaleAllSizes(main_scale); // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    // style.FontScaleDpi = main_scale; // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(glfw_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\msyh.ttc", 20.0f, nullptr, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());

    if (framer)
        if (int ret = framer->initialize(); ret != 0)
            return code_err("Failed to initialize framer, ret={}", ret);
    return 0;
}

void ImRenderer::render_loop(std::stop_token& token)
{
    auto glfw_window = window.get();

    bool show_demo_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Rendering loop code for ImRenderer
    while (!token.stop_requested() && !glfwWindowShouldClose(glfw_window))
    {
        glfwPollEvents();
        if (glfwGetWindowAttrib(glfw_window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 点击左上角100，100范围内3次，显示调试窗口
        if (ImGui::IsMouseClicked(0) && ImGui::GetMousePos().x < 100 && ImGui::GetMousePos().y < 100)
        {
            static int click_count = 0;
            if (++click_count >= 3)
                show_demo_window = !show_demo_window, click_count = 0;
        }

        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        if (framer)
            framer->next_frame();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(glfw_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(glfw_window);
    }
}

void ImRenderer::destroy()
{
    if (framer)
        framer->destroy();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window.get());
    glfwTerminate();
}

void ImRenderer::set_framer(std::shared_ptr<ImFramerInterface> framer)
{
    if (window == nullptr)
        this->framer = framer;
}