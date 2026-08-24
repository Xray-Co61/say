#include <glad/gl.h>

#include "Game.hpp"
#include "Shaders.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>
#include <utility>

namespace fallen {
namespace {

constexpr Vec3 kUp{0.0F, 1.0F, 0.0F};
constexpr float kProjectileSpeed = 138.0F;
constexpr float kMaxProjectileLifetime = 8.0F;
constexpr std::size_t kMaximumAngels = 9;

void appendTriangle(std::vector<Vertex>& vertices,
                    const Vec3& first, const Vec3& second, const Vec3& third,
                    const Vec3& color) {
    const Vec3 normal = normalise(cross(second - first, third - first));
    vertices.push_back({first, normal, color});
    vertices.push_back({second, normal, color});
    vertices.push_back({third, normal, color});
}

void appendQuad(std::vector<Vertex>& vertices,
                const Vec3& bottomLeft, const Vec3& bottomRight,
                const Vec3& topRight, const Vec3& topLeft,
                const Vec3& color) {
    appendTriangle(vertices, bottomLeft, bottomRight, topRight, color);
    appendTriangle(vertices, bottomLeft, topRight, topLeft, color);
}

void appendLowPolySphere(std::vector<Vertex>& vertices,
                         const Vec3& center, float radius,
                         int latitudeCount, int longitudeCount,
                         const Vec3& color) {
    for (int latitude = 0; latitude < latitudeCount; ++latitude) {
        const float lowTheta = -kPi * 0.5F + kPi * static_cast<float>(latitude) / latitudeCount;
        const float highTheta = -kPi * 0.5F + kPi * static_cast<float>(latitude + 1) / latitudeCount;

        for (int longitude = 0; longitude < longitudeCount; ++longitude) {
            const float lowPhi = 2.0F * kPi * static_cast<float>(longitude) / longitudeCount;
            const float highPhi = 2.0F * kPi * static_cast<float>(longitude + 1) / longitudeCount;

            const auto point = [&](float theta, float phi) {
                return center + Vec3{
                    radius * std::cos(theta) * std::cos(phi),
                    radius * std::sin(theta),
                    radius * std::cos(theta) * std::sin(phi),
                };
            };

            const Vec3 lowerLeft = point(lowTheta, lowPhi);
            const Vec3 lowerRight = point(lowTheta, highPhi);
            const Vec3 upperRight = point(highTheta, highPhi);
            const Vec3 upperLeft = point(highTheta, lowPhi);
            appendQuad(vertices, lowerLeft, lowerRight, upperRight, upperLeft, color);
        }
    }
}

float wrapDegrees(float degrees) {
    while (degrees > 180.0F) {
        degrees -= 360.0F;
    }
    while (degrees < -180.0F) {
        degrees += 360.0F;
    }
    return degrees;
}

} // namespace

Game::Game(GLFWwindow* window)
    : window_(window) {}

Game::~Game() {
    if (hudVertexBuffer_ != 0) {
        glDeleteBuffers(1, &hudVertexBuffer_);
    }
    if (hudVertexArray_ != 0) {
        glDeleteVertexArrays(1, &hudVertexArray_);
    }
}

void Game::initialise() {
    worldShader_ = Shader(shaders::worldVertex, shaders::worldFragment, "world");
    skyShader_ = Shader(shaders::skyVertex, shaders::skyFragment, "sky");
    hudShader_ = Shader(shaders::hudVertex, shaders::hudFragment, "hud");

    terrain_ = makeTerrain();
    angelBody_ = makeAngelBody();
    leftWing_ = makeWing(-1.0F);
    rightWing_ = makeWing(1.0F);
    halo_ = makeHalo();
    skyQuad_ = makeSkyQuad();
    trailLines_ = Mesh(std::vector<Vertex>{}, std::vector<std::uint32_t>{}, GL_LINES, GL_DYNAMIC_DRAW);
    projectileLines_ = Mesh(std::vector<Vertex>{}, std::vector<std::uint32_t>{}, GL_LINES, GL_DYNAMIC_DRAW);
    createHudBuffer();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.005F, 0.015F, 0.035F, 1.0F);

    glfwSetWindowUserPointer(window_, this);
    glfwSetCursorPosCallback(window_, &Game::cursorPositionCallback);
    glfwSetMouseButtonCallback(window_, &Game::mouseButtonCallback);

    resetEncounter();
    setCursorCaptured(true);
    updateWindowTitle();
}

void Game::update(float deltaSeconds) {
    deltaSeconds = std::min(deltaSeconds, 0.1F);
    elapsedSeconds_ += deltaSeconds;
    fireCooldown_ = std::max(0.0F, fireCooldown_ - deltaSeconds);
    recoil_ = std::max(0.0F, recoil_ - deltaSeconds * 4.2F);
    zoomPulse_ = std::max(0.0F, zoomPulse_ - deltaSeconds * 3.0F);

    updateAim(deltaSeconds);

    const bool spaceDown = glfwGetKey(window_, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (spaceDown && !previousSpaceDown_) {
        fire();
    }
    previousSpaceDown_ = spaceDown;

    const bool escapeDown = glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (escapeDown && !previousEscapeDown_) {
        if (cursorCaptured_) {
            setCursorCaptured(false);
        } else {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
    }
    previousEscapeDown_ = escapeDown;

    const bool resetDown = glfwGetKey(window_, GLFW_KEY_R) == GLFW_PRESS;
    if (resetDown && !previousResetDown_) {
        resetEncounter();
    }
    previousResetDown_ = resetDown;

    updateAngels(deltaSeconds);
    updateProjectiles(deltaSeconds);
    acquireLock();

    titleClock_ += deltaSeconds;
    if (titleClock_ > 0.15F) {
        titleClock_ = 0.0F;
        updateWindowTitle();
    }
}

void Game::render(int framebufferWidth, int framebufferHeight) {
    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        return;
    }

    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    skyShader_.use();
    skyShader_.setFloat("uTime", elapsedSeconds_);
    skyQuad_.draw();
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    const float aspectRatio = static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);
    const float fieldOfView = radians(62.0F / zoom_);
    const Mat4 projection = Mat4::perspective(fieldOfView, aspectRatio, 0.15F, 2400.0F);
    const Vec3 forward = cameraForward();
    const Vec3 renderCamera = cameraPosition_ - forward * (recoil_ * 0.16F);
    const Mat4 view = Mat4::lookAt(renderCamera, renderCamera + forward, kUp);

    renderWorld(view, projection);
    renderHud();
}

float Game::terrainHeight(float x, float z) {
    const float broadRidge = 11.5F * std::sin(x * 0.010F) * std::cos(z * 0.012F);
    const float middleRidge = 4.4F * std::sin((x + z) * 0.033F);
    const float fineRidge = 1.8F * std::cos(x * 0.11F - z * 0.075F);
    const float basin = -0.000020F * (x * x + z * z);
    return broadRidge + middleRidge + fineRidge + basin - 9.5F;
}

Mesh Game::makeTerrain() {
    constexpr int cellsPerAxis = 176;
    constexpr float spacing = 6.0F;
    constexpr float halfExtent = cellsPerAxis * spacing * 0.5F;
    constexpr float sampleStep = 1.0F;

    std::vector<Vertex> vertices;
    vertices.reserve(static_cast<std::size_t>((cellsPerAxis + 1) * (cellsPerAxis + 1)));
    std::vector<std::uint32_t> indices;
    indices.reserve(static_cast<std::size_t>(cellsPerAxis * cellsPerAxis * 6));

    for (int zIndex = 0; zIndex <= cellsPerAxis; ++zIndex) {
        for (int xIndex = 0; xIndex <= cellsPerAxis; ++xIndex) {
            const float x = -halfExtent + static_cast<float>(xIndex) * spacing;
            const float z = -halfExtent + static_cast<float>(zIndex) * spacing;
            const float height = terrainHeight(x, z);
            const Vec3 normal = normalise({
                terrainHeight(x - sampleStep, z) - terrainHeight(x + sampleStep, z),
                2.0F * sampleStep,
                terrainHeight(x, z - sampleStep) - terrainHeight(x, z + sampleStep),
            });

            const float elevation = clamp((height + 22.0F) / 37.0F, 0.0F, 1.0F);
            const Vec3 lowColor{0.006F, 0.075F, 0.088F};
            const Vec3 highColor{0.055F, 0.255F, 0.245F};
            Vec3 color = lerp(lowColor, highColor, elevation);
            const float coldNoise = 0.5F + 0.5F * std::sin(x * 0.23F + z * 0.17F);
            color += Vec3{0.006F, 0.026F, 0.030F} * coldNoise;
            vertices.push_back({{x, height, z}, normal, color});
        }
    }

    for (int zIndex = 0; zIndex < cellsPerAxis; ++zIndex) {
        for (int xIndex = 0; xIndex < cellsPerAxis; ++xIndex) {
            const std::uint32_t topLeft = static_cast<std::uint32_t>(zIndex * (cellsPerAxis + 1) + xIndex);
            const std::uint32_t topRight = topLeft + 1;
            const std::uint32_t bottomLeft = topLeft + static_cast<std::uint32_t>(cellsPerAxis + 1);
            const std::uint32_t bottomRight = bottomLeft + 1;
            indices.insert(indices.end(), {topLeft, bottomLeft, topRight, topRight, bottomLeft, bottomRight});
        }
    }

    return Mesh(vertices, indices);
}

Mesh Game::makeAngelBody() {
    std::vector<Vertex> vertices;
    constexpr int segmentCount = 9;
    const Vec3 dressColor{0.45F, 0.88F, 1.0F};

    for (int segment = 0; segment < segmentCount; ++segment) {
        const float firstAngle = 2.0F * kPi * static_cast<float>(segment) / segmentCount;
        const float secondAngle = 2.0F * kPi * static_cast<float>(segment + 1) / segmentCount;

        const Vec3 topFirst{0.34F * std::cos(firstAngle), 0.53F, 0.34F * std::sin(firstAngle)};
        const Vec3 topSecond{0.34F * std::cos(secondAngle), 0.53F, 0.34F * std::sin(secondAngle)};
        const Vec3 bottomFirst{0.83F * std::cos(firstAngle), -1.43F, 0.83F * std::sin(firstAngle)};
        const Vec3 bottomSecond{0.83F * std::cos(secondAngle), -1.43F, 0.83F * std::sin(secondAngle)};
        appendQuad(vertices, bottomFirst, bottomSecond, topSecond, topFirst, dressColor);
    }

    appendLowPolySphere(vertices, {0.0F, 1.05F, 0.0F}, 0.31F, 5, 8, {0.72F, 0.98F, 1.0F});
    appendTriangle(vertices, {-0.18F, 0.52F, 0.0F}, {-1.04F, -0.10F, 0.07F}, {-0.67F, 0.72F, -0.03F}, dressColor);
    appendTriangle(vertices, {0.18F, 0.52F, 0.0F}, {1.04F, -0.10F, 0.07F}, {0.67F, 0.72F, -0.03F}, dressColor);

    return Mesh(vertices);
}

Mesh Game::makeWing(float direction) {
    std::vector<Vertex> vertices;
    const Vec3 wingColor{0.38F, 0.91F, 1.0F};
    const Vec3 brightFeather{0.68F, 0.98F, 1.0F};

    const Vec3 root{0.0F, 0.55F, 0.02F};
    const Vec3 shoulder{direction * 0.96F, 0.82F, 0.02F};
    const Vec3 highTip{direction * 2.15F, 1.78F, 0.10F};
    const Vec3 farTip{direction * 3.50F, 0.18F, 0.14F};
    const Vec3 lowTip{direction * 2.28F, -0.42F, 0.10F};

    appendTriangle(vertices, root, shoulder, highTip, wingColor);
    appendTriangle(vertices, root, highTip, farTip, wingColor);
    appendTriangle(vertices, root, farTip, lowTip, wingColor);

    for (int feather = 0; feather < 5; ++feather) {
        const float fraction = static_cast<float>(feather) / 5.0F;
        const Vec3 inner{direction * (0.75F + fraction * 1.9F), 0.48F - fraction * 0.32F, 0.13F};
        const Vec3 outer{direction * (1.36F + fraction * 2.48F), 0.20F - fraction * 0.48F, 0.09F};
        const Vec3 tip{direction * (1.82F + fraction * 2.12F), -0.20F - fraction * 0.55F, 0.12F};
        appendTriangle(vertices, inner, outer, tip, brightFeather);
    }

    return Mesh(vertices);
}

Mesh Game::makeHalo() {
    std::vector<Vertex> vertices;
    constexpr int segments = 42;
    for (int segment = 0; segment <= segments; ++segment) {
        const float angle = 2.0F * kPi * static_cast<float>(segment) / segments;
        vertices.push_back({
            {0.58F * std::cos(angle), 1.60F, 0.58F * std::sin(angle)},
            {0.0F, 1.0F, 0.0F},
            {0.7F, 0.98F, 1.0F},
        });
    }
    return Mesh(vertices, {}, GL_LINE_STRIP);
}

Mesh Game::makeSkyQuad() {
    const std::vector<Vertex> vertices{
        {{-1.0F, -1.0F, 0.0F}, {}, {}},
        {{1.0F, -1.0F, 0.0F}, {}, {}},
        {{1.0F, 1.0F, 0.0F}, {}, {}},
        {{-1.0F, 1.0F, 0.0F}, {}, {}},
    };
    const std::vector<std::uint32_t> indices{0, 1, 2, 0, 2, 3};
    return Mesh(vertices, indices);
}

void Game::createHudBuffer() {
    glGenVertexArrays(1, &hudVertexArray_);
    glGenBuffers(1, &hudVertexBuffer_);

    glBindVertexArray(hudVertexArray_);
    glBindBuffer(GL_ARRAY_BUFFER, hudVertexBuffer_);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(HudVertex),
                          reinterpret_cast<void*>(offsetof(HudVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(HudVertex),
                          reinterpret_cast<void*>(offsetof(HudVertex, r)));
    glBindVertexArray(0);
}

void Game::setCursorCaptured(bool captured) {
    cursorCaptured_ = captured;
    firstCursorSample_ = true;
    mouseDeltaX_ = 0.0F;
    mouseDeltaY_ = 0.0F;
    glfwSetInputMode(window_, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported() == GLFW_TRUE) {
        glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, captured ? GLFW_TRUE : GLFW_FALSE);
    }
}

void Game::updateAim(float deltaSeconds) {
    float horizontalSpeed = 0.0F;
    float verticalSpeed = 0.0F;
    constexpr float fastAimSpeed = 105.0F;
    constexpr float fineAimSpeed = 24.0F;

    if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS) {
        horizontalSpeed -= fastAimSpeed;
    }
    if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS) {
        horizontalSpeed += fastAimSpeed;
    }
    if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS) {
        verticalSpeed += fastAimSpeed;
    }
    if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS) {
        verticalSpeed -= fastAimSpeed;
    }

    if (glfwGetKey(window_, GLFW_KEY_LEFT) == GLFW_PRESS) {
        horizontalSpeed -= fineAimSpeed;
    }
    if (glfwGetKey(window_, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        horizontalSpeed += fineAimSpeed;
    }
    if (glfwGetKey(window_, GLFW_KEY_UP) == GLFW_PRESS) {
        verticalSpeed += fineAimSpeed;
    }
    if (glfwGetKey(window_, GLFW_KEY_DOWN) == GLFW_PRESS) {
        verticalSpeed -= fineAimSpeed;
    }

    yawDegrees_ = wrapDegrees(yawDegrees_ + horizontalSpeed * deltaSeconds + mouseDeltaX_ * 0.075F);
    pitchDegrees_ = clamp(pitchDegrees_ + verticalSpeed * deltaSeconds - mouseDeltaY_ * 0.075F, -62.0F, 58.0F);
    mouseDeltaX_ = 0.0F;
    mouseDeltaY_ = 0.0F;

    float zoomDirection = 0.0F;
    if (glfwGetKey(window_, GLFW_KEY_EQUAL) == GLFW_PRESS || glfwGetKey(window_, GLFW_KEY_KP_ADD) == GLFW_PRESS) {
        zoomDirection += 1.0F;
    }
    if (glfwGetKey(window_, GLFW_KEY_MINUS) == GLFW_PRESS || glfwGetKey(window_, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) {
        zoomDirection -= 1.0F;
    }
    if (zoomDirection != 0.0F) {
        const float previousZoom = zoom_;
        zoom_ = clamp(zoom_ + zoomDirection * deltaSeconds * 1.8F, 1.0F, 6.0F);
        if (zoom_ != previousZoom) {
            zoomPulse_ = 1.0F;
        }
    }
}

void Game::updateAngels(float deltaSeconds) {
    spawnClock_ -= deltaSeconds;
    if (spawnClock_ <= 0.0F && angels_.size() < kMaximumAngels) {
        spawnAngel(false);
        spawnClock_ = randomRange(1.2F, 2.8F);
    }

    for (Angel& angel : angels_) {
        if (angel.disruption > 0.0F) {
            angel.disruption -= deltaSeconds;
            angel.position += angel.velocity * (deltaSeconds * 0.22F);
            angel.position.y -= deltaSeconds * 7.0F;
            angel.trailClock += deltaSeconds;
            if (angel.trailClock >= 0.035F) {
                angel.trailClock = 0.0F;
                angel.trail.push_back(angel.position);
                if (angel.trail.size() > 32) {
                    angel.trail.erase(angel.trail.begin());
                }
            }
            if (angel.disruption <= 0.0F) {
                resetAngel(angel, false);
            }
            continue;
        }

        angel.age += deltaSeconds;
        angel.position += angel.velocity * deltaSeconds;
        angel.position.y = angel.baseAltitude + std::sin(angel.age * angel.waveFrequency + angel.phase) * angel.waveAmplitude;
        angel.trailClock += deltaSeconds;
        if (angel.trailClock >= 0.035F) {
            angel.trailClock = 0.0F;
            angel.trail.push_back(angel.position);
            if (angel.trail.size() > 32) {
                angel.trail.erase(angel.trail.begin());
            }
        }

        const Vec3 relativePosition = angel.position - cameraPosition_;
        if (dot(relativePosition, angel.heading) > 960.0F || length(relativePosition) > 1450.0F || angel.position.y < -80.0F) {
            resetAngel(angel, false);
        }
    }
}

void Game::updateProjectiles(float deltaSeconds) {
    for (Projectile& projectile : projectiles_) {
        projectile.previousPosition = projectile.position;
        projectile.position += projectile.velocity * deltaSeconds;
        projectile.age += deltaSeconds;
        projectile.trail.push_back(projectile.position);
        if (projectile.trail.size() > 18) {
            projectile.trail.erase(projectile.trail.begin());
        }

        for (Angel& angel : angels_) {
            if (angel.disruption > 0.0F) {
                continue;
            }
            if (projectileHitsAngel(projectile, angel)) {
                angel.disruption = 1.25F;
                angel.trail.clear();
                angel.trail.push_back(angel.position);
                ++signalsResolved_;
                projectile.age = kMaxProjectileLifetime;
                break;
            }
        }
    }

    std::erase_if(projectiles_, [](const Projectile& projectile) {
        return projectile.age >= kMaxProjectileLifetime || length(projectile.position) > 2200.0F;
    });
}

void Game::spawnAngel(bool immediatelyVisible) {
    Angel angel{};
    resetAngel(angel, immediatelyVisible);
    angels_.push_back(std::move(angel));
}

void Game::resetAngel(Angel& angel, bool immediatelyVisible) {
    angel = Angel{};
    const Vec3 flatForward = normalise({cameraForward().x, 0.0F, cameraForward().z});
    const Vec3 right = normalise(cross(flatForward, kUp));
    const float altitude = randomRange(12.0F, 88.0F);
    const float lateralOffset = randomRange(-150.0F, 150.0F);
    const float speed = randomRange(46.0F, 82.0F);

    angel.heading = flatForward;
    angel.side = right;
    angel.velocity = flatForward * speed + right * randomRange(-7.0F, 7.0F);
    angel.scale = randomRange(1.15F, 2.85F);
    angel.waveAmplitude = randomRange(1.0F, 7.0F);
    angel.waveFrequency = randomRange(0.55F, 1.55F);
    angel.phase = randomRange(0.0F, 2.0F * kPi);
    angel.baseAltitude = altitude;

    if (immediatelyVisible) {
        angel.position = cameraPosition_ + flatForward * randomRange(75.0F, 420.0F) + right * lateralOffset;
    } else {
        angel.position = cameraPosition_ - flatForward * randomRange(90.0F, 300.0F) + right * lateralOffset;
    }
    angel.position.y = altitude + std::sin(angel.phase) * angel.waveAmplitude;

    constexpr int trailSamples = 26;
    angel.trail.reserve(trailSamples + 8);
    for (int sample = 0; sample < trailSamples; ++sample) {
        const float secondsBehind = static_cast<float>(trailSamples - sample) * 0.035F;
        angel.trail.push_back(angel.position - angel.velocity * secondsBehind);
    }
}

void Game::fire() {
    if (fireCooldown_ > 0.0F || projectiles_.size() >= 28) {
        return;
    }

    const Vec3 forward = cameraForward();
    Projectile projectile{};
    projectile.position = cameraPosition_ + forward * 2.0F + Vec3{0.0F, -0.24F, 0.0F};
    projectile.previousPosition = projectile.position;
    projectile.velocity = forward * kProjectileSpeed;
    projectile.trail.push_back(projectile.position - projectile.velocity * 0.08F);
    projectile.trail.push_back(projectile.position);
    projectiles_.push_back(std::move(projectile));

    fireCooldown_ = 0.14F;
    recoil_ = 1.0F;
}

void Game::acquireLock() {
    lockedAngel_ = -1;
    lockedDistance_ = 0.0F;
    const Vec3 forward = cameraForward();
    const float coneRadians = radians(2.2F) / zoom_;
    const float minimumDot = std::cos(coneRadians);
    float bestDot = minimumDot;

    for (std::size_t index = 0; index < angels_.size(); ++index) {
        const Angel& angel = angels_[index];
        if (angel.disruption > 0.0F) {
            continue;
        }
        const Vec3 offset = angel.position - cameraPosition_;
        const float distance = length(offset);
        if (distance < 1.0F) {
            continue;
        }
        const float alignment = dot(offset / distance, forward);
        if (alignment > bestDot) {
            bestDot = alignment;
            lockedAngel_ = static_cast<int>(index);
            lockedDistance_ = distance;
        }
    }
}

void Game::resetEncounter() {
    cameraPosition_ = {0.0F, terrainHeight(0.0F, 0.0F) + 4.0F, 0.0F};
    yawDegrees_ = -90.0F;
    pitchDegrees_ = -4.0F;
    zoom_ = 1.0F;
    projectiles_.clear();
    angels_.clear();
    signalsResolved_ = 0;
    fireCooldown_ = 0.0F;
    recoil_ = 0.0F;
    spawnClock_ = 1.0F;

    for (int index = 0; index < 4; ++index) {
        spawnAngel(true);
    }
    acquireLock();
}

void Game::updateWindowTitle() {
    const int rangeMetres = lockedAngel_ >= 0 ? static_cast<int>(lockedDistance_) : 0;
    const float flightSeconds = lockedAngel_ >= 0 ? lockedDistance_ / kProjectileSpeed : 0.0F;
    char title[256]{};
    std::snprintf(title, sizeof(title),
                  "FALLEN SIGNAL | %s | R:%04dm | ZOOM x%.1f | FLIGHT %.2fs | RESOLVED %02d | WASD fast / arrows fine / +/- zoom / click or space fire / R reset",
                  lockedAngel_ >= 0 ? "LOCK" : "SEARCH", rangeMetres, zoom_, flightSeconds, signalsResolved_);
    glfwSetWindowTitle(window_, title);
}

Vec3 Game::cameraForward() const {
    const float yaw = radians(yawDegrees_);
    const float pitch = radians(pitchDegrees_);
    return normalise({
        std::cos(yaw) * std::cos(pitch),
        std::sin(pitch),
        std::sin(yaw) * std::cos(pitch),
    });
}

Vec3 Game::cameraRight() const {
    return normalise(cross(cameraForward(), kUp));
}

float Game::randomRange(float minimum, float maximum) {
    std::uniform_real_distribution<float> distribution(minimum, maximum);
    return distribution(random_);
}

bool Game::projectileHitsAngel(const Projectile& projectile, const Angel& angel) const {
    const Vec3 segment = projectile.position - projectile.previousPosition;
    const float segmentLengthSquared = lengthSquared(segment);
    if (segmentLengthSquared < 0.00001F) {
        return false;
    }

    const float interpolation = clamp(dot(angel.position - projectile.previousPosition, segment) / segmentLengthSquared, 0.0F, 1.0F);
    const Vec3 nearestPoint = projectile.previousPosition + segment * interpolation;
    const float hitRadius = angel.scale * 2.65F;
    return lengthSquared(angel.position - nearestPoint) <= hitRadius * hitRadius;
}

void Game::renderWorld(const Mat4& view, const Mat4& projection) {
    worldShader_.use();
    worldShader_.setMat4("uView", view);
    worldShader_.setMat4("uProjection", projection);
    worldShader_.setVec3("uCameraPosition", cameraPosition_);
    worldShader_.setVec3("uFogColor", {0.014F, 0.11F, 0.14F});
    worldShader_.setVec3("uLightDirection", {-0.35F, 0.82F, 0.29F});
    worldShader_.setFloat("uFogDensity", 0.0025F);
    worldShader_.setVec3("uEmission", {0.0F, 0.0F, 0.0F});
    worldShader_.setFloat("uOpacity", 1.0F);
    worldShader_.setMat4("uModel", Mat4::identity());
    terrain_.draw();

    renderTrails(view, projection);
    for (const Angel& angel : angels_) {
        renderAngel(angel, view, projection);
    }
    renderProjectiles(view, projection);
}

void Game::renderAngel(const Angel& angel, const Mat4& view, const Mat4& projection) {
    const Vec3 cameraOffset = cameraPosition_ - angel.position;
    const float facingAngle = std::atan2(cameraOffset.x, cameraOffset.z);
    const float flap = std::sin(elapsedSeconds_ * 8.5F + angel.phase) * 0.42F;
    const float disruptionScale = 1.0F + std::max(0.0F, angel.disruption) * 0.30F;
    const Mat4 baseTransform = Mat4::translation(angel.position)
        * Mat4::rotationY(facingAngle)
        * Mat4::scale(angel.scale * disruptionScale);

    worldShader_.use();
    worldShader_.setMat4("uView", view);
    worldShader_.setMat4("uProjection", projection);
    worldShader_.setVec3("uCameraPosition", cameraPosition_);
    worldShader_.setVec3("uFogColor", {0.014F, 0.11F, 0.14F});
    worldShader_.setVec3("uLightDirection", {-0.35F, 0.82F, 0.29F});
    worldShader_.setFloat("uFogDensity", 0.00165F);
    worldShader_.setFloat("uOpacity", 1.0F);

    const Vec3 emission = angel.disruption > 0.0F
        ? Vec3{1.4F, 0.24F, 0.08F}
        : Vec3{0.38F, 0.83F, 1.0F};
    worldShader_.setVec3("uEmission", emission);

    glDisable(GL_CULL_FACE);
    worldShader_.setMat4("uModel", baseTransform * Mat4::rotationZ(-flap));
    leftWing_.draw();
    worldShader_.setMat4("uModel", baseTransform * Mat4::rotationZ(flap));
    rightWing_.draw();
    worldShader_.setMat4("uModel", baseTransform);
    angelBody_.draw();

    glLineWidth(2.0F);
    worldShader_.setMat4("uModel", baseTransform * Mat4::scale(1.0F + std::max(0.0F, angel.disruption) * 1.8F));
    halo_.draw();
    glLineWidth(1.0F);
    glEnable(GL_CULL_FACE);
}

void Game::renderTrails(const Mat4& view, const Mat4& projection) {
    std::vector<Vertex> vertices;
    for (const Angel& angel : angels_) {
        if (angel.trail.size() < 2) {
            continue;
        }
        for (std::size_t index = 1; index < angel.trail.size(); ++index) {
            const float intensity = static_cast<float>(index) / static_cast<float>(angel.trail.size() - 1);
            const Vec3 oldColor = lerp({0.01F, 0.05F, 0.09F}, {0.28F, 0.82F, 1.0F}, intensity * 0.72F);
            const Vec3 newColor = lerp({0.01F, 0.05F, 0.09F}, {0.72F, 0.98F, 1.0F}, intensity);
            vertices.push_back({angel.trail[index - 1], {}, oldColor});
            vertices.push_back({angel.trail[index], {}, newColor});
        }
    }
    trailLines_.updateVertices(vertices);

    worldShader_.use();
    worldShader_.setMat4("uView", view);
    worldShader_.setMat4("uProjection", projection);
    worldShader_.setMat4("uModel", Mat4::identity());
    worldShader_.setVec3("uCameraPosition", cameraPosition_);
    worldShader_.setVec3("uFogColor", {0.014F, 0.11F, 0.14F});
    worldShader_.setVec3("uLightDirection", {-0.35F, 0.82F, 0.29F});
    worldShader_.setVec3("uEmission", {0.40F, 0.88F, 1.0F});
    worldShader_.setFloat("uFogDensity", 0.00095F);
    worldShader_.setFloat("uOpacity", 0.82F);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glLineWidth(2.0F);
    trailLines_.draw();
    glLineWidth(1.0F);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Game::renderProjectiles(const Mat4& view, const Mat4& projection) {
    std::vector<Vertex> vertices;
    for (const Projectile& projectile : projectiles_) {
        for (std::size_t index = 1; index < projectile.trail.size(); ++index) {
            const float intensity = static_cast<float>(index) / static_cast<float>(projectile.trail.size() - 1);
            const Vec3 color = lerp({0.08F, 0.16F, 0.22F}, {0.82F, 0.98F, 1.0F}, intensity);
            vertices.push_back({projectile.trail[index - 1], {}, color * 0.55F});
            vertices.push_back({projectile.trail[index], {}, color});
        }

        const float markerSize = 0.38F;
        vertices.push_back({projectile.position - kUp * markerSize, {}, {1.0F, 1.0F, 1.0F}});
        vertices.push_back({projectile.position + kUp * markerSize, {}, {1.0F, 1.0F, 1.0F}});
        const Vec3 right = cameraRight();
        vertices.push_back({projectile.position - right * markerSize, {}, {1.0F, 1.0F, 1.0F}});
        vertices.push_back({projectile.position + right * markerSize, {}, {1.0F, 1.0F, 1.0F}});
    }
    projectileLines_.updateVertices(vertices);

    worldShader_.use();
    worldShader_.setMat4("uView", view);
    worldShader_.setMat4("uProjection", projection);
    worldShader_.setMat4("uModel", Mat4::identity());
    worldShader_.setVec3("uCameraPosition", cameraPosition_);
    worldShader_.setVec3("uFogColor", {0.014F, 0.11F, 0.14F});
    worldShader_.setVec3("uLightDirection", {-0.35F, 0.82F, 0.29F});
    worldShader_.setVec3("uEmission", {0.74F, 0.95F, 1.0F});
    worldShader_.setFloat("uFogDensity", 0.00070F);
    worldShader_.setFloat("uOpacity", 0.95F);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glLineWidth(2.5F);
    projectileLines_.draw();
    glLineWidth(1.0F);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Game::renderHud() {
    std::vector<HudVertex> vertices;
    vertices.reserve(700);

    const HudColor cyan{0.34F, 0.93F, 1.0F, 0.84F};
    const HudColor dimCyan{0.12F, 0.48F, 0.56F, 0.52F};
    const HudColor faint{0.05F, 0.25F, 0.31F, 0.42F};
    const HudColor red{1.0F, 0.12F, 0.055F, 0.94F};
    const HudColor lockColor = lockedAngel_ >= 0 ? red : dimCyan;

    // Scope-like outer frame.
    addHudLine(vertices, -0.98F, 0.94F, -0.78F, 0.94F, dimCyan);
    addHudLine(vertices, -0.98F, 0.94F, -0.98F, 0.72F, dimCyan);
    addHudLine(vertices, 0.98F, 0.94F, 0.78F, 0.94F, dimCyan);
    addHudLine(vertices, 0.98F, 0.94F, 0.98F, 0.72F, dimCyan);
    addHudLine(vertices, -0.98F, -0.94F, -0.78F, -0.94F, faint);
    addHudLine(vertices, -0.98F, -0.94F, -0.98F, -0.72F, faint);
    addHudLine(vertices, 0.98F, -0.94F, 0.78F, -0.94F, faint);
    addHudLine(vertices, 0.98F, -0.94F, 0.98F, -0.72F, faint);

    // The long ranging bars reproduce the optical-sight language from the reference.
    addHudLine(vertices, -0.92F, 0.42F, -0.17F, 0.42F, dimCyan);
    addHudLine(vertices, 0.17F, 0.42F, 0.92F, 0.42F, dimCyan);
    for (int tick = 0; tick <= 16; ++tick) {
        const float offset = static_cast<float>(tick) * (0.75F / 16.0F);
        const float tickLength = tick % 4 == 0 ? 0.058F : (tick % 2 == 0 ? 0.035F : 0.020F);
        addHudLine(vertices, -0.92F + offset, 0.42F, -0.92F + offset, 0.42F - tickLength, faint);
        addHudLine(vertices, 0.17F + offset, 0.42F, 0.17F + offset, 0.42F - tickLength, faint);
    }

    // Vertical rail and the four descending chevrons.
    addHudLine(vertices, 0.0F, 0.58F, 0.0F, 0.13F, faint);
    addHudLine(vertices, 0.0F, -0.13F, 0.0F, -0.69F, faint);
    for (int marker = 0; marker < 4; ++marker) {
        addHudChevron(vertices, 0.0F, -0.17F - static_cast<float>(marker) * 0.125F, 0.035F, dimCyan);
    }

    const float sightSize = clamp(0.13F / zoom_, 0.026F, 0.13F);
    addHudLine(vertices, -sightSize * 2.0F, sightSize, -sightSize, sightSize, lockColor);
    addHudLine(vertices, -sightSize, sightSize, -sightSize, sightSize * 2.0F, lockColor);
    addHudLine(vertices, sightSize * 2.0F, sightSize, sightSize, sightSize, lockColor);
    addHudLine(vertices, sightSize, sightSize, sightSize, sightSize * 2.0F, lockColor);
    addHudLine(vertices, -sightSize * 2.0F, -sightSize, -sightSize, -sightSize, lockColor);
    addHudLine(vertices, -sightSize, -sightSize, -sightSize, -sightSize * 2.0F, lockColor);
    addHudLine(vertices, sightSize * 2.0F, -sightSize, sightSize, -sightSize, lockColor);
    addHudLine(vertices, sightSize, -sightSize, sightSize, -sightSize * 2.0F, lockColor);

    for (int section = 0; section < 14; ++section) {
        const float first = 2.0F * kPi * static_cast<float>(section) / 14.0F;
        const float second = 2.0F * kPi * static_cast<float>(section + 1) / 14.0F;
        const float radius = sightSize * 0.54F;
        addHudLine(vertices, std::cos(first) * radius, std::sin(first) * radius,
                   std::cos(second) * radius, std::sin(second) * radius, cyan);
    }

    // Red target cue: always visible, but only bright when a moving angel is aligned.
    addHudChevron(vertices, 0.0F, sightSize * 2.75F, sightSize * 0.72F, lockColor);
    if (lockedAngel_ >= 0) {
        addHudChevron(vertices, 0.0F, sightSize * 3.85F, sightSize * 0.46F, red);
    }

    // Numeric telemetry: range on the left, zoom/firing-time/score on the right.
    const int range = lockedAngel_ >= 0 ? static_cast<int>(lockedDistance_) : 0;
    const int flightCentiseconds = lockedAngel_ >= 0 ? static_cast<int>((lockedDistance_ / kProjectileSpeed) * 100.0F) : 0;
    addHudNumber(vertices, range, 4, -0.94F, 0.52F, 0.031F, cyan);
    addHudNumber(vertices, static_cast<int>(zoom_ * 10.0F), 2, 0.72F, 0.52F, 0.031F, cyan);
    addHudNumber(vertices, flightCentiseconds, 3, 0.70F, -0.84F, 0.026F, dimCyan);
    addHudNumber(vertices, signalsResolved_, 2, -0.94F, -0.84F, 0.026F, dimCyan);

    if (fireCooldown_ > 0.0F || zoomPulse_ > 0.0F) {
        const float pulse = fireCooldown_ > 0.0F ? fireCooldown_ / 0.14F : zoomPulse_;
        const float radius = sightSize * (2.2F + (1.0F - pulse) * 2.2F);
        const HudColor pulseColor{0.55F, 0.95F, 1.0F, pulse * 0.65F};
        for (int section = 0; section < 18; ++section) {
            const float first = 2.0F * kPi * static_cast<float>(section) / 18.0F;
            const float second = 2.0F * kPi * static_cast<float>(section + 1) / 18.0F;
            addHudLine(vertices, std::cos(first) * radius, std::sin(first) * radius,
                       std::cos(second) * radius, std::sin(second) * radius, pulseColor);
        }
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    hudShader_.use();
    glBindVertexArray(hudVertexArray_);
    glBindBuffer(GL_ARRAY_BUFFER, hudVertexBuffer_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(HudVertex)),
                 vertices.empty() ? nullptr : vertices.data(),
                 GL_DYNAMIC_DRAW);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
}

void Game::addHudLine(std::vector<HudVertex>& vertices,
                      float fromX, float fromY, float toX, float toY,
                      HudColor color) const {
    vertices.push_back({fromX, fromY, color.r, color.g, color.b, color.a});
    vertices.push_back({toX, toY, color.r, color.g, color.b, color.a});
}

void Game::addHudChevron(std::vector<HudVertex>& vertices,
                         float x, float y, float size, HudColor color) const {
    addHudLine(vertices, x - size, y + size * 0.50F, x, y - size * 0.62F, color);
    addHudLine(vertices, x, y - size * 0.62F, x + size, y + size * 0.50F, color);
}

void Game::addHudDigit(std::vector<HudVertex>& vertices,
                       int digit, float x, float y, float size, HudColor color) const {
    static constexpr std::array<std::array<bool, 7>, 10> segments{{
        {{true, true, true, true, true, true, false}},
        {{false, true, true, false, false, false, false}},
        {{true, true, false, true, true, false, true}},
        {{true, true, true, true, false, false, true}},
        {{false, true, true, false, false, true, true}},
        {{true, false, true, true, false, true, true}},
        {{true, false, true, true, true, true, true}},
        {{true, true, true, false, false, false, false}},
        {{true, true, true, true, true, true, true}},
        {{true, true, true, true, false, true, true}},
    }};

    if (digit < 0 || digit > 9) {
        return;
    }

    const float left = x;
    const float right = x + size * 0.70F;
    const float top = y + size;
    const float middle = y + size * 0.50F;
    const float bottom = y;
    const std::array<std::array<float, 4>, 7> lines{{
        {{left, top, right, top}},
        {{right, top, right, middle}},
        {{right, middle, right, bottom}},
        {{left, bottom, right, bottom}},
        {{left, middle, left, bottom}},
        {{left, top, left, middle}},
        {{left, middle, right, middle}},
    }};

    for (std::size_t segment = 0; segment < segments[digit].size(); ++segment) {
        if (segments[digit][segment]) {
            const auto& line = lines[segment];
            addHudLine(vertices, line[0], line[1], line[2], line[3], color);
        }
    }
}

void Game::addHudNumber(std::vector<HudVertex>& vertices,
                        int number, int minimumDigits,
                        float x, float y, float size, HudColor color) const {
    std::string digits = std::to_string(std::max(0, number));
    if (static_cast<int>(digits.size()) < minimumDigits) {
        digits.insert(digits.begin(), static_cast<std::size_t>(minimumDigits - static_cast<int>(digits.size())), '0');
    }

    for (std::size_t index = 0; index < digits.size(); ++index) {
        addHudDigit(vertices, digits[index] - '0', x + static_cast<float>(index) * size * 0.95F, y, size, color);
    }
}

void Game::cursorPositionCallback(GLFWwindow* window, double x, double y) {
    auto* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
    if (game == nullptr || !game->cursorCaptured_) {
        return;
    }

    if (game->firstCursorSample_) {
        game->lastCursorX_ = x;
        game->lastCursorY_ = y;
        game->firstCursorSample_ = false;
        return;
    }

    game->mouseDeltaX_ += static_cast<float>(x - game->lastCursorX_);
    game->mouseDeltaY_ += static_cast<float>(y - game->lastCursorY_);
    game->lastCursorX_ = x;
    game->lastCursorY_ = y;
}

void Game::mouseButtonCallback(GLFWwindow* window, int button, int action, int /*modifiers*/) {
    auto* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
    if (game == nullptr || button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) {
        return;
    }

    if (!game->cursorCaptured_) {
        game->setCursorCaptured(true);
    } else {
        game->fire();
    }
}

} // namespace fallen
