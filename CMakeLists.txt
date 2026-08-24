cmake_minimum_required(VERSION 3.24)

project(FallenSignal
    VERSION 0.1.0
    DESCRIPTION "A no-engine OpenGL vertical slice"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FALLEN_SIGNAL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

include(FetchContent)

# GLFW only owns the OS window, input events, and OpenGL context. It is not a game engine.
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
# The game targets Windows first; keeping Wayland disabled avoids an unnecessary
# wayland-scanner dependency when contributors configure it on a minimal Linux VM.
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

# GLAD is a generated OpenGL 3.3 function loader, committed in its small generated
# form so a first build needs neither Python nor a GLAD installation.
add_library(glad STATIC third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC third_party/glad/include)
target_link_libraries(glad PUBLIC ${CMAKE_DL_LIBS})

add_executable(fallen_signal
    src/main.cpp
    src/Game.cpp
    src/Mesh.cpp
    src/Shader.cpp
)

target_include_directories(fallen_signal PRIVATE src)
target_link_libraries(fallen_signal PRIVATE glfw glad)

target_compile_definitions(fallen_signal PRIVATE
    $<$<CONFIG:Debug>:FALLEN_SIGNAL_DEBUG=1>
)

if(MSVC)
    target_compile_options(fallen_signal PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(fallen_signal PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(FALLEN_SIGNAL_WARNINGS_AS_ERRORS)
    if(MSVC)
        target_compile_options(fallen_signal PRIVATE /WX)
    else()
        target_compile_options(fallen_signal PRIVATE -Werror)
    endif()
endif()

# GLFW links the appropriate system libraries on each desktop platform. Explicitly request
# the native OpenGL import library for MSVC, where the loader calls into opengl32.dll.
if(WIN32)
    target_link_libraries(fallen_signal PRIVATE opengl32)
endif()

set_target_properties(fallen_signal PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:fallen_signal>"
)
