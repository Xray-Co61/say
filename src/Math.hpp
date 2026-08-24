#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace fallen {

constexpr float kPi = 3.14159265358979323846F;

inline constexpr float radians(float degrees) {
    return degrees * kPi / 180.0F;
}

inline constexpr float clamp(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}

struct Vec2 {
    float x = 0.0F;
    float y = 0.0F;
};

struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;

    constexpr Vec3() = default;
    constexpr Vec3(float xValue, float yValue, float zValue)
        : x(xValue), y(yValue), z(zValue) {}

    constexpr Vec3& operator+=(const Vec3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    constexpr Vec3& operator-=(const Vec3& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    constexpr Vec3& operator*=(float scalar) {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }
};

inline constexpr Vec3 operator+(Vec3 left, const Vec3& right) {
    return left += right;
}

inline constexpr Vec3 operator-(Vec3 left, const Vec3& right) {
    return left -= right;
}

inline constexpr Vec3 operator-(const Vec3& value) {
    return {-value.x, -value.y, -value.z};
}

inline constexpr Vec3 operator*(Vec3 value, float scalar) {
    return value *= scalar;
}

inline constexpr Vec3 operator*(float scalar, Vec3 value) {
    return value *= scalar;
}

inline constexpr Vec3 operator/(Vec3 value, float scalar) {
    return {value.x / scalar, value.y / scalar, value.z / scalar};
}

inline constexpr float dot(const Vec3& left, const Vec3& right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

inline constexpr Vec3 cross(const Vec3& left, const Vec3& right) {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

inline float lengthSquared(const Vec3& value) {
    return dot(value, value);
}

inline float length(const Vec3& value) {
    return std::sqrt(lengthSquared(value));
}

inline Vec3 normalise(const Vec3& value) {
    const float valueLength = length(value);
    return valueLength > 0.00001F ? value / valueLength : Vec3{};
}

inline constexpr Vec3 lerp(const Vec3& from, const Vec3& to, float t) {
    return from * (1.0F - t) + to * t;
}

// Column-major matrix layout, matching OpenGL's default matrix convention.
struct Mat4 {
    std::array<float, 16> values{};

    [[nodiscard]] const float* data() const {
        return values.data();
    }

    static Mat4 identity() {
        Mat4 result{};
        result.values[0] = 1.0F;
        result.values[5] = 1.0F;
        result.values[10] = 1.0F;
        result.values[15] = 1.0F;
        return result;
    }

    static Mat4 translation(const Vec3& offset) {
        Mat4 result = identity();
        result.values[12] = offset.x;
        result.values[13] = offset.y;
        result.values[14] = offset.z;
        return result;
    }

    static Mat4 scale(float factor) {
        Mat4 result{};
        result.values[0] = factor;
        result.values[5] = factor;
        result.values[10] = factor;
        result.values[15] = 1.0F;
        return result;
    }

    static Mat4 scale(const Vec3& factors) {
        Mat4 result{};
        result.values[0] = factors.x;
        result.values[5] = factors.y;
        result.values[10] = factors.z;
        result.values[15] = 1.0F;
        return result;
    }

    static Mat4 rotationX(float angle) {
        Mat4 result = identity();
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        result.values[5] = cosine;
        result.values[6] = sine;
        result.values[9] = -sine;
        result.values[10] = cosine;
        return result;
    }

    static Mat4 rotationY(float angle) {
        Mat4 result = identity();
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        result.values[0] = cosine;
        result.values[2] = -sine;
        result.values[8] = sine;
        result.values[10] = cosine;
        return result;
    }

    static Mat4 rotationZ(float angle) {
        Mat4 result = identity();
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        result.values[0] = cosine;
        result.values[1] = sine;
        result.values[4] = -sine;
        result.values[5] = cosine;
        return result;
    }

    static Mat4 perspective(float verticalFovRadians, float aspectRatio, float nearPlane, float farPlane) {
        const float tangent = std::tan(verticalFovRadians / 2.0F);
        Mat4 result{};
        result.values[0] = 1.0F / (aspectRatio * tangent);
        result.values[5] = 1.0F / tangent;
        result.values[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
        result.values[11] = -1.0F;
        result.values[14] = -(2.0F * farPlane * nearPlane) / (farPlane - nearPlane);
        return result;
    }

    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
        const Vec3 forward = normalise(target - eye);
        const Vec3 right = normalise(cross(forward, up));
        const Vec3 correctedUp = cross(right, forward);

        Mat4 result = identity();
        result.values[0] = right.x;
        result.values[1] = correctedUp.x;
        result.values[2] = -forward.x;
        result.values[4] = right.y;
        result.values[5] = correctedUp.y;
        result.values[6] = -forward.y;
        result.values[8] = right.z;
        result.values[9] = correctedUp.z;
        result.values[10] = -forward.z;
        result.values[12] = -dot(right, eye);
        result.values[13] = -dot(correctedUp, eye);
        result.values[14] = dot(forward, eye);
        return result;
    }
};

inline Mat4 operator*(const Mat4& left, const Mat4& right) {
    Mat4 result{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float value = 0.0F;
            for (int part = 0; part < 4; ++part) {
                value += left.values[part * 4 + row] * right.values[column * 4 + part];
            }
            result.values[column * 4 + row] = value;
        }
    }
    return result;
}

} // namespace fallen
