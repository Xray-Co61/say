#pragma once

#include "Math.hpp"

#include <glad/gl.h>

#include <cstdint>
#include <vector>

namespace fallen {

struct Vertex {
    Vec3 position{};
    Vec3 normal{};
    Vec3 color{};
};

class Mesh final {
public:
    Mesh() = default;
    Mesh(const std::vector<Vertex>& vertices,
         const std::vector<std::uint32_t>& indices = {},
         GLenum primitive = GL_TRIANGLES,
         GLenum usage = GL_STATIC_DRAW);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void updateVertices(const std::vector<Vertex>& vertices);
    void draw() const;

private:
    GLuint vao_ = 0;
    GLuint vertexBuffer_ = 0;
    GLuint indexBuffer_ = 0;
    GLsizei vertexCount_ = 0;
    GLsizei indexCount_ = 0;
    GLenum primitive_ = GL_TRIANGLES;
    bool indexed_ = false;

    void release() noexcept;
};

} // namespace fallen
