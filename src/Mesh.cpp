#include "Mesh.hpp"

#include <cstddef>
#include <utility>

namespace fallen {

Mesh::Mesh(const std::vector<Vertex>& vertices,
           const std::vector<std::uint32_t>& indices,
           GLenum primitive,
           GLenum usage)
    : primitive_(primitive), indexed_(!indices.empty()) {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vertexBuffer_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.empty() ? nullptr : vertices.data(),
                 usage);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, color)));

    vertexCount_ = static_cast<GLsizei>(vertices.size());
    if (indexed_) {
        glGenBuffers(1, &indexBuffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
                     indices.data(),
                     usage);
        indexCount_ = static_cast<GLsizei>(indices.size());
    }

    glBindVertexArray(0);
}

Mesh::~Mesh() {
    release();
}

Mesh::Mesh(Mesh&& other) noexcept
    : vao_(std::exchange(other.vao_, 0)),
      vertexBuffer_(std::exchange(other.vertexBuffer_, 0)),
      indexBuffer_(std::exchange(other.indexBuffer_, 0)),
      vertexCount_(std::exchange(other.vertexCount_, 0)),
      indexCount_(std::exchange(other.indexCount_, 0)),
      primitive_(other.primitive_),
      indexed_(other.indexed_) {}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        release();
        vao_ = std::exchange(other.vao_, 0);
        vertexBuffer_ = std::exchange(other.vertexBuffer_, 0);
        indexBuffer_ = std::exchange(other.indexBuffer_, 0);
        vertexCount_ = std::exchange(other.vertexCount_, 0);
        indexCount_ = std::exchange(other.indexCount_, 0);
        primitive_ = other.primitive_;
        indexed_ = other.indexed_;
    }
    return *this;
}

void Mesh::updateVertices(const std::vector<Vertex>& vertices) {
    if (vao_ == 0 || indexed_) {
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.empty() ? nullptr : vertices.data(),
                 GL_DYNAMIC_DRAW);
    vertexCount_ = static_cast<GLsizei>(vertices.size());
}

void Mesh::draw() const {
    if (vao_ == 0) {
        return;
    }

    glBindVertexArray(vao_);
    if (indexed_) {
        glDrawElements(primitive_, indexCount_, GL_UNSIGNED_INT, nullptr);
    } else {
        glDrawArrays(primitive_, 0, vertexCount_);
    }
    glBindVertexArray(0);
}

void Mesh::release() noexcept {
    if (indexBuffer_ != 0) {
        glDeleteBuffers(1, &indexBuffer_);
        indexBuffer_ = 0;
    }
    if (vertexBuffer_ != 0) {
        glDeleteBuffers(1, &vertexBuffer_);
        vertexBuffer_ = 0;
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    vertexCount_ = 0;
    indexCount_ = 0;
}

} // namespace fallen
