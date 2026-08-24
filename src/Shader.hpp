#pragma once

#include "Math.hpp"

#include <glad/gl.h>

#include <string_view>

namespace fallen {

class Shader final {
public:
    Shader() = default;
    Shader(const char* vertexSource, const char* fragmentSource, std::string_view debugName);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    void use() const;
    void setMat4(const char* name, const Mat4& value) const;
    void setVec3(const char* name, const Vec3& value) const;
    void setFloat(const char* name, float value) const;
    void setInt(const char* name, int value) const;

private:
    GLuint program_ = 0;

    [[nodiscard]] GLint location(const char* name) const;
    void release() noexcept;
};

} // namespace fallen
