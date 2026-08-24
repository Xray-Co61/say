#include "Shader.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <utility>

namespace fallen {
namespace {

GLuint compileStage(GLenum type, const char* source, std::string_view debugName) {
    const GLuint stage = glCreateShader(type);
    glShaderSource(stage, 1, &source, nullptr);
    glCompileShader(stage);

    GLint compiled = GL_FALSE;
    glGetShaderiv(stage, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return stage;
    }

    GLint logLength = 0;
    glGetShaderiv(stage, GL_INFO_LOG_LENGTH, &logLength);
    std::string log(static_cast<std::size_t>(std::max(logLength, 1)), '\0');
    glGetShaderInfoLog(stage, logLength, nullptr, log.data());
    glDeleteShader(stage);

    const char* stageName = type == GL_VERTEX_SHADER ? "vertex" : "fragment";
    throw std::runtime_error(std::string(debugName) + " " + stageName + " shader failed:\n" + log);
}

} // namespace

Shader::Shader(const char* vertexSource, const char* fragmentSource, std::string_view debugName) {
    const GLuint vertex = compileStage(GL_VERTEX_SHADER, vertexSource, debugName);
    const GLuint fragment = compileStage(GL_FRAGMENT_SHADER, fragmentSource, debugName);

    program_ = glCreateProgram();
    glAttachShader(program_, vertex);
    glAttachShader(program_, fragment);
    glLinkProgram(program_);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) {
        return;
    }

    GLint logLength = 0;
    glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &logLength);
    std::string log(static_cast<std::size_t>(std::max(logLength, 1)), '\0');
    glGetProgramInfoLog(program_, logLength, nullptr, log.data());
    release();
    throw std::runtime_error(std::string(debugName) + " program failed to link:\n" + log);
}

Shader::~Shader() {
    release();
}

Shader::Shader(Shader&& other) noexcept
    : program_(std::exchange(other.program_, 0)) {}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        release();
        program_ = std::exchange(other.program_, 0);
    }
    return *this;
}

void Shader::use() const {
    glUseProgram(program_);
}

void Shader::setMat4(const char* name, const Mat4& value) const {
    glUniformMatrix4fv(location(name), 1, GL_FALSE, value.data());
}

void Shader::setVec3(const char* name, const Vec3& value) const {
    glUniform3f(location(name), value.x, value.y, value.z);
}

void Shader::setFloat(const char* name, float value) const {
    glUniform1f(location(name), value);
}

void Shader::setInt(const char* name, int value) const {
    glUniform1i(location(name), value);
}

GLint Shader::location(const char* name) const {
    return glGetUniformLocation(program_, name);
}

void Shader::release() noexcept {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
}

} // namespace fallen
