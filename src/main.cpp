#include <glad/gl.h>

#include "Game.hpp"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <exception>

namespace {

void glfwErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description == nullptr ? "unknown error" : description);
}

} // namespace

int main() {
    glfwSetErrorCallback(glfwErrorCallback);
    if (glfwInit() != GLFW_TRUE) {
        std::fprintf(stderr, "Could not initialise GLFW.\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);

    // A sight mounted inside a vehicle should occupy the whole display, not appear
    // as a desktop utility with a title bar and taskbar around the optic.
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* videoMode = monitor == nullptr ? nullptr : glfwGetVideoMode(monitor);
    GLFWwindow* window = glfwCreateWindow(
        videoMode == nullptr ? 1600 : videoMode->width,
        videoMode == nullptr ? 900 : videoMode->height,
        "FALLEN SIGNAL", monitor, nullptr);
    if (window == nullptr) {
        std::fprintf(stderr, "Could not create an OpenGL 3.3 window. Update the GPU driver and try again.\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    const int loadedVersion = gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress));
    if (loadedVersion == 0) {
        std::fprintf(stderr, "Could not load required OpenGL functions.\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::fprintf(stdout, "OpenGL %s\n", glGetString(GL_VERSION));

    int result = 0;
    try {
        {
            fallen::Game game(window);
            game.initialise();

            double previousTime = glfwGetTime();
            while (glfwWindowShouldClose(window) == GLFW_FALSE) {
                const double currentTime = glfwGetTime();
                const float deltaSeconds = static_cast<float>(currentTime - previousTime);
                previousTime = currentTime;

                glfwPollEvents();
                game.update(deltaSeconds);

                int width = 0;
                int height = 0;
                glfwGetFramebufferSize(window, &width, &height);
                game.render(width, height);
                glfwSwapBuffers(window);
            }
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Fatal error: %s\n", error.what());
        result = 1;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
