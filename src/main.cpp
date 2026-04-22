#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#ifdef _WIN32
#include <GLFW/glfw3native.h>
#endif
#include <cstdio>
#include <filesystem>

#include "gui/application.h"
#include "gui/theme.h"
#include "core/paths.h"

int main() {
#ifdef _WIN32
    // Ensure working directory is always the folder containing the executable.
    // Without this, relative paths like "assets/..." fail when launched from
    // a Desktop or Start Menu shortcut.
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::filesystem::current_path(std::filesystem::path(exePath).parent_path());
#endif

    if (!glfwInit()) {
#ifdef _WIN32
        MessageBoxA(nullptr, "Failed to initialize GLFW.", "CoreDeck", MB_OK | MB_ICONERROR);
#else
        std::fprintf(stderr, "Failed to initialize GLFW\n");
#endif
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow *window = glfwCreateWindow(1200, 800, "CoreDeck", nullptr, nullptr);
    if (!window) {
#ifdef _WIN32
        MessageBoxA(nullptr, "Failed to create window.\nYour system may not support OpenGL 3.3.", "CoreDeck",
                    MB_OK | MB_ICONERROR);
#else
        std::fprintf(stderr, "Failed to create GLFW window\n");
#endif
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(window);
    HICON icon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(1));
    SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    static std::string imguiIniPath = CoreDeck::Paths::GetAppConfigPath("imgui.ini");
    io.IniFilename = imguiIniPath.c_str();

    if (std::filesystem::exists("assets/fonts/JetBrainsMono-Regular.ttf")) {
        io.Fonts->AddFontFromFileTTF("assets/fonts/JetBrainsMono-Regular.ttf", 16.0f);
    }

    if (std::filesystem::exists("assets/fonts/FontAwesome7Free-Solid-900.otf")) {
        ImFontConfig iconConfig;
        iconConfig.MergeMode = true;
        iconConfig.PixelSnapH = true;
        iconConfig.GlyphMinAdvanceX = 16.0f;

        static constexpr ImWchar iconRanges[] = {0xf000, 0xf8ff, 0};
        io.Fonts->AddFontFromFileTTF(
            "assets/fonts/FontAwesome7Free-Solid-900.otf",
            12.0f,
            &iconConfig,
            iconRanges
        );
    }

    ImGui::StyleColorsDark();
    CoreDeck::ApplyCustomImGuiTheme();
    CoreDeck::Application app;
    app.SetMainWindow(window);

    const auto glsl_version = "#version 330";
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    glfwSetWindowUserPointer(window, &app);

    glfwSetScrollCallback(window, [](GLFWwindow *w, const double x, const double y) {
        ImGuiIO &imGuiIO = ImGui::GetIO();
        imGuiIO.AddMouseWheelEvent(static_cast<float>(x) * 0.3f, static_cast<float>(y) * 0.3f);
    });

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow *w, const int width, const int height) {
        if (width == 0 || height == 0) return;

        auto *a = static_cast<CoreDeck::Application *>(glfwGetWindowUserPointer(w));

        glViewport(0, 0, width, height);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        a->Build();

        ImGui::Render();
        glClearColor(0.06f, 0.06f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(w);
    });

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        app.Build();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
