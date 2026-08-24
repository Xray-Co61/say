#include <glad/gl.h>

#include "Game.hpp"
#include "Shaders.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace fallen {
namespace {

constexpr Vec3 kUp{0.0F, 1.0F, 0.0F};
constexpr Vec3 kBallisticGravity{0.0F, -8.2F, 0.0F};
constexpr float kProjectileSpeed = 230.0F;
constexpr float kMaxProjectileLifetime = 9.0F;
constexpr std::size_t kMaximumAngels = 6;

float solveInterceptTime(const Vec3& relativePosition, const Vec3& targetVelocity) {
    const float squaredSpeed = kProjectileSpeed * kProjectileSpeed;
    const float quadratic = dot(targetVelocity, targetVelocity) - squaredSpeed;
    const float linear = 2.0F * dot(relativePosition, targetVelocity);
    const float constant = dot(relativePosition, relativePosition);

    if (std::abs(quadratic) < 0.0001F) {
        if (std::abs(linear) < 0.0001F) {
            return std::sqrt(constant) / kProjectileSpeed;
        }
        const float time = -constant / linear;
        return time > 0.0F ? time : std::sqrt(constant) / kProjectileSpeed;
    }

    const float discriminant = linear * linear - 4.0F * quadratic * constant;
    if (discriminant < 0.0F) {
        return std::sqrt(constant) / kProjectileSpeed;
    }

    const float root = std::sqrt(discriminant);
    const float first = (-linear - root) / (2.0F * quadratic);
    const float second = (-linear + root) / (2.0F * quadratic);
    float time = std::numeric_limits<float>::max();
    if (first > 0.0F) {
        time = first;
    }
    if (second > 0.0F) {
        time = std::min(time, second);
    }
    return time == std::numeric_limits<float>::max() ? std::sqrt(constant) / kProjectileSpeed : time;
}

} // namespace

Game::Game(GLFWwindow* window)
    : window_(window) {}

Game::~Game() {
    destroySceneTarget();
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
    angelShader_ = Shader(shaders::angelVertex, shaders::angelFragment, "angel sprite");
    postShader_ = Shader(shaders::postVertex, shaders::postFragment, "cinematic post");
    hudShader_ = Shader(shaders::hudVertex, shaders::hudFragment, "optic hud");

    terrain_ = makeTerrain();
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
    glClearColor(0.001F, 0.007F, 0.008F, 1.0F);

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
    recoil_ = std::max(0.0F, recoil_ - deltaSeconds * 4.8F);
    zoomPulse_ = std::max(0.0F, zoomPulse_ - deltaSeconds * 3.5F);

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

    ensureSceneTarget(framebufferWidth, framebufferHeight);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer_);
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
    const float fieldOfView = radians(20.0F / zoom_);
    const Mat4 projection = Mat4::perspective(fieldOfView, aspectRatio, 0.20F, 2200.0F);
    const Vec3 forward = cameraForward();
    const Vec3 renderCamera = cameraPosition_ - forward * (recoil_ * 0.10F);
    const Mat4 view = Mat4::lookAt(renderCamera, renderCamera + forward, kUp);
    renderWorld(view, projection);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    postShader_.use();
    postShader_.setInt("uScene", 0);
    postShader_.setFloat("uTime", elapsedSeconds_);
    postShader_.setFloat("uResolutionX", static_cast<float>(framebufferWidth));
    postShader_.setFloat("uResolutionY", static_cast<float>(framebufferHeight));
    postShader_.setFloat("uZoom", zoom_);
    postShader_.setFloat("uKick", recoil_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneColorTexture_);
    skyQuad_.draw();
    glBindTexture(GL_TEXTURE_2D, 0);
    glDepthMask(GL_TRUE);

    renderHud();
}

float Game::terrainHeight(float x, float z) {
    const float broadRidge = 8.5F * std::sin(x * 0.009F) * std::cos(z * 0.011F);
    const float middleRidge = 3.1F * std::sin((x + z) * 0.029F);
    const float fineRidge = 0.85F * std::cos(x * 0.15F - z * 0.11F);
    const float basin = -0.000017F * (x * x + z * z);
    return broadRidge + middleRidge + fineRidge + basin - 9.5F;
}

Mesh Game::makeTerrain() {
    constexpr int cellsPerAxis = 240;
    constexpr float spacing = 5.0F;
    constexpr float halfExtent = cellsPerAxis * spacing * 0.5F;
    constexpr float sampleStep = 0.7F;

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

            const float elevation = clamp((height + 17.0F) / 23.0F, 0.0F, 1.0F);
            const Vec3 lowColor{0.0025F, 0.020F, 0.018F};
            const Vec3 highColor{0.018F, 0.078F, 0.066F};
            const float weathering = 0.5F + 0.5F * std::sin(x * 0.11F + z * 0.07F);
            const Vec3 color = lerp(lowColor, highColor, elevation) + Vec3{0.002F, 0.006F, 0.005F} * weathering;
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

void Game::ensureSceneTarget(int width, int height) {
    if (sceneFramebuffer_ != 0 && sceneWidth_ == width && sceneHeight_ == height) {
        return;
    }

    destroySceneTarget();
    sceneWidth_ = width;
    sceneHeight_ = height;

    glGenFramebuffers(1, &sceneFramebuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer_);

    glGenTextures(1, &sceneColorTexture_);
    glBindTexture(GL_TEXTURE_2D, sceneColorTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTexture_, 0);

    glGenRenderbuffers(1, &sceneDepthBuffer_);
    glBindRenderbuffer(GL_RENDERBUFFER, sceneDepthBuffer_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, sceneDepthBuffer_);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        destroySceneTarget();
        throw std::runtime_error("Could not create the cinematic render target.");
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Game::destroySceneTarget() noexcept {
    if (sceneDepthBuffer_ != 0) {
        glDeleteRenderbuffers(1, &sceneDepthBuffer_);
        sceneDepthBuffer_ = 0;
    }
    if (sceneColorTexture_ != 0) {
        glDeleteTextures(1, &sceneColorTexture_);
        sceneColorTexture_ = 0;
    }
    if (sceneFramebuffer_ != 0) {
        glDeleteFramebuffers(1, &sceneFramebuffer_);
        sceneFramebuffer_ = 0;
    }
    sceneWidth_ = 0;
    sceneHeight_ = 0;
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
    constexpr float fastAimSpeed = 28.0F;
    constexpr float fineAimSpeed = 5.0F;

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

    // A constrained sight aperture: the player moves an optic, never a free camera.
    yawDegrees_ = clamp(yawDegrees_ + horizontalSpeed * deltaSeconds + mouseDeltaX_ * 0.014F, -102.0F, -78.0F);
    pitchDegrees_ = clamp(pitchDegrees_ + verticalSpeed * deltaSeconds - mouseDeltaY_ * 0.014F, -8.0F, 12.0F);
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
        zoom_ = clamp(zoom_ + zoomDirection * deltaSeconds * 0.55F, 1.0F, 2.6F);
        if (zoom_ != previousZoom) {
            zoomPulse_ = 1.0F;
        }
    }
}

void Game::updateAngels(float deltaSeconds) {
    spawnClock_ -= deltaSeconds;
    if (spawnClock_ <= 0.0F && angels_.size() < kMaximumAngels) {
        spawnAngel(false);
        spawnClock_ = randomRange(2.2F, 4.4F);
    }

    for (Angel& angel : angels_) {
        if (angel.disruption > 0.0F) {
            angel.disruption -= deltaSeconds;
            angel.position += angel.velocity * (deltaSeconds * 0.18F);
            angel.position.y -= deltaSeconds * 3.5F;
        } else {
            angel.age += deltaSeconds;
            angel.position += angel.velocity * deltaSeconds;
            angel.position.y = angel.baseAltitude + std::sin(angel.age * angel.waveFrequency + angel.phase) * angel.waveAmplitude;
        }

        angel.trailClock += deltaSeconds;
        if (angel.trailClock >= 0.050F) {
            angel.trailClock = 0.0F;
            angel.trail.push_back(angel.position);
            if (angel.trail.size() > 26) {
                angel.trail.erase(angel.trail.begin());
            }
        }

        const Vec3 relativePosition = angel.position - cameraPosition_;
        if (angel.disruption <= 0.0F && (dot(relativePosition, angel.heading) > 940.0F || length(relativePosition) > 1350.0F)) {
            resetAngel(angel, false);
        }
        if (angel.disruption <= 0.0F && angel.position.y < -80.0F) {
            resetAngel(angel, false);
        }
        if (angel.disruption <= 0.0F && angel.age > 25.0F) {
            resetAngel(angel, false);
        }
    }
}

void Game::updateProjectiles(float deltaSeconds) {
    for (Projectile& projectile : projectiles_) {
        projectile.previousPosition = projectile.position;
        projectile.position += projectile.velocity * deltaSeconds + kBallisticGravity * (0.5F * deltaSeconds * deltaSeconds);
        projectile.velocity += kBallisticGravity * deltaSeconds;
        projectile.age += deltaSeconds;
        projectile.trail.push_back(projectile.position);
        if (projectile.trail.size() > 15) {
            projectile.trail.erase(projectile.trail.begin());
        }

        for (Angel& angel : angels_) {
            if (angel.disruption > 0.0F) {
                continue;
            }
            if (projectileHitsAngel(projectile, angel)) {
                angel.disruption = 1.15F;
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
    const Vec3 forward = cameraForward();
    const Vec3 flatForward = normalise({forward.x, 0.0F, forward.z});
    const Vec3 right = normalise(cross(flatForward, kUp));
    const float speed = randomRange(38.0F, 66.0F);

    angel.heading = flatForward;
    angel.side = right;
    angel.velocity = flatForward * speed + right * randomRange(-3.0F, 3.0F);
    angel.scale = randomRange(1.35F, 2.95F);
    angel.waveAmplitude = randomRange(0.35F, 2.6F);
    angel.waveFrequency = randomRange(0.24F, 0.68F);
    angel.phase = randomRange(0.0F, 2.0F * kPi);
    angel.baseAltitude = cameraPosition_.y + randomRange(10.0F, 44.0F);

    if (immediatelyVisible) {
        const float distance = randomRange(420.0F, 980.0F);
        angel.position = cameraPosition_ + flatForward * distance + right * randomRange(-distance * 0.075F, distance * 0.075F);
    } else {
        angel.position = cameraPosition_ - flatForward * randomRange(260.0F, 760.0F) + right * randomRange(-78.0F, 78.0F);
    }
    angel.position.y = angel.baseAltitude + std::sin(angel.phase) * angel.waveAmplitude;

    constexpr int trailSamples = 22;
    angel.trail.reserve(trailSamples + 8);
    for (int sample = 0; sample < trailSamples; ++sample) {
        const float secondsBehind = static_cast<float>(trailSamples - sample) * 0.050F;
        angel.trail.push_back(angel.position - angel.velocity * secondsBehind);
    }
}

void Game::fire() {
    if (fireCooldown_ > 0.0F || projectiles_.size() >= 12) {
        return;
    }

    const Vec3 forward = cameraForward();
    Projectile projectile{};
    projectile.position = cameraPosition_ + forward * 1.4F + Vec3{0.0F, -0.18F, 0.0F};
    projectile.previousPosition = projectile.position;

    // A restrained fire-control assist leads a locked moving signal and compensates
    // for shell drop. Outside a lock, the player still fires exactly along the optic.
    Vec3 shotDirection = forward;
    if (lockedAngel_ >= 0 && lockedAngel_ < static_cast<int>(angels_.size()) && fireControlFlightSeconds_ > 0.0F) {
        const Angel& target = angels_[static_cast<std::size_t>(lockedAngel_)];
        Vec3 compensatedPoint = fireControlPoint_;
        float refinedTime = length(compensatedPoint - projectile.position) / kProjectileSpeed;
        compensatedPoint = target.position + target.velocity * refinedTime
            - kBallisticGravity * (0.5F * refinedTime * refinedTime);
        shotDirection = normalise(compensatedPoint - projectile.position);
    }

    projectile.velocity = shotDirection * kProjectileSpeed;
    projectile.trail.push_back(projectile.position - projectile.velocity * 0.055F);
    projectile.trail.push_back(projectile.position);
    projectiles_.push_back(std::move(projectile));

    fireCooldown_ = 0.62F;
    recoil_ = 1.0F;
}

void Game::acquireLock() {
    lockedAngel_ = -1;
    lockedDistance_ = 0.0F;
    fireControlFlightSeconds_ = 0.0F;
    fireControlPoint_ = {};
    const Vec3 forward = cameraForward();
    const float coneRadians = radians(0.58F) / zoom_;
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

    if (lockedAngel_ >= 0) {
        const Angel& target = angels_[static_cast<std::size_t>(lockedAngel_)];
        fireControlFlightSeconds_ = solveInterceptTime(target.position - cameraPosition_, target.velocity);
        fireControlFlightSeconds_ = clamp(fireControlFlightSeconds_, 0.05F, kMaxProjectileLifetime);
        fireControlPoint_ = target.position + target.velocity * fireControlFlightSeconds_
            - kBallisticGravity * (0.5F * fireControlFlightSeconds_ * fireControlFlightSeconds_);
    }
}

void Game::resetEncounter() {
    cameraPosition_ = {0.0F, terrainHeight(0.0F, 0.0F) + 3.0F, 0.0F};
    yawDegrees_ = -90.0F;
    pitchDegrees_ = -2.0F;
    zoom_ = 1.0F;
    projectiles_.clear();
    angels_.clear();
    signalsResolved_ = 0;
    fireCooldown_ = 0.0F;
    recoil_ = 0.0F;
    spawnClock_ = 1.6F;

    for (int index = 0; index < 3; ++index) {
        spawnAngel(true);
    }

    // The first signal always begins inside the narrow optic's usable field.
    if (!angels_.empty()) {
        Angel& guide = angels_.front();
        const Vec3 forward = normalise({cameraForward().x, 0.0F, cameraForward().z});
        guide.position = cameraPosition_ + forward * 620.0F;
        guide.baseAltitude = cameraPosition_.y + 8.0F;
        guide.position.y = guide.baseAltitude;
        guide.waveAmplitude = 0.65F;
        guide.trail.clear();
        for (int sample = 0; sample < 22; ++sample) {
            guide.trail.push_back(guide.position - guide.velocity * (static_cast<float>(22 - sample) * 0.05F));
        }
    }
    acquireLock();
}

void Game::updateWindowTitle() {
    const int rangeMetres = lockedAngel_ >= 0 ? static_cast<int>(lockedDistance_) : 0;
    const float flightSeconds = lockedAngel_ >= 0 ? fireControlFlightSeconds_ : 0.0F;
    char title[256]{};
    std::snprintf(title, sizeof(title),
                  "FALLEN SIGNAL | %s | RANGE %04dm | OPTIC x%.1f | FLIGHT %.2fs | RESOLVED %02d",
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
    const float hitRadius = angel.scale * 3.8F;
    return lengthSquared(angel.position - nearestPoint) <= hitRadius * hitRadius;
}

void Game::renderWorld(const Mat4& view, const Mat4& projection) {
    worldShader_.use();
    worldShader_.setMat4("uView", view);
    worldShader_.setMat4("uProjection", projection);
    worldShader_.setVec3("uCameraPosition", cameraPosition_);
    worldShader_.setVec3("uFogColor", {0.008F, 0.092F, 0.082F});
    worldShader_.setVec3("uLightDirection", {-0.28F, 0.68F, 0.31F});
    worldShader_.setFloat("uFogDensity", 0.0060F);
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
    const Vec3 forward = cameraForward();
    const Vec3 right = cameraRight();
    const Vec3 up = normalise(cross(right, forward));
    const float distance = length(angel.position - cameraPosition_);

    angelShader_.use();
    angelShader_.setMat4("uView", view);
    angelShader_.setMat4("uProjection", projection);
    angelShader_.setVec3("uCenter", angel.position);
    angelShader_.setVec3("uCameraRight", right);
    angelShader_.setVec3("uCameraUp", up);
    angelShader_.setVec3("uFogColor", {0.008F, 0.092F, 0.082F});
    angelShader_.setFloat("uScale", angel.scale * 1.55F * (1.0F + std::max(0.0F, angel.disruption) * 0.24F));
    angelShader_.setFloat("uDistance", distance);
    angelShader_.setFloat("uDisruption", std::max(0.0F, angel.disruption));
    angelShader_.setFloat("uTime", elapsedSeconds_);
    angelShader_.setFloat("uSeed", angel.phase);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    skyQuad_.draw();
    glDepthMask(GL_TRUE);
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
            const Vec3 oldColor = lerp({0.0005F, 0.006F, 0.006F}, {0.022F, 0.18F, 0.16F}, intensity * 0.60F);
            const Vec3 newColor = lerp({0.0005F, 0.006F, 0.006F}, {0.22F, 0.70F, 0.61F}, intensity);
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
    worldShader_.setVec3("uFogColor", {0.008F, 0.092F, 0.082F});
    worldShader_.setVec3("uLightDirection", {-0.28F, 0.68F, 0.31F});
    worldShader_.setVec3("uEmission", {0.035F, 0.15F, 0.12F});
    worldShader_.setFloat("uFogDensity", 0.0034F);
    worldShader_.setFloat("uOpacity", 0.56F);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glLineWidth(1.0F);
    trailLines_.draw();
    glDepthMask(GL_TRUE);
}

void Game::renderProjectiles(const Mat4& view, const Mat4& projection) {
    std::vector<Vertex> vertices;
    for (const Projectile& projectile : projectiles_) {
        for (std::size_t index = 1; index < projectile.trail.size(); ++index) {
            const float intensity = static_cast<float>(index) / static_cast<float>(projectile.trail.size() - 1);
            const Vec3 color = lerp({0.015F, 0.055F, 0.048F}, {0.55F, 0.86F, 0.72F}, intensity);
            vertices.push_back({projectile.trail[index - 1], {}, color * 0.48F});
            vertices.push_back({projectile.trail[index], {}, color});
        }

        const float markerSize = 0.16F;
        vertices.push_back({projectile.position - kUp * markerSize, {}, {0.72F, 0.92F, 0.80F}});
        vertices.push_back({projectile.position + kUp * markerSize, {}, {0.72F, 0.92F, 0.80F}});
    }
    projectileLines_.updateVertices(vertices);

    worldShader_.use();
    worldShader_.setMat4("uView", view);
    worldShader_.setMat4("uProjection", projection);
    worldShader_.setMat4("uModel", Mat4::identity());
    worldShader_.setVec3("uCameraPosition", cameraPosition_);
    worldShader_.setVec3("uFogColor", {0.008F, 0.092F, 0.082F});
    worldShader_.setVec3("uLightDirection", {-0.28F, 0.68F, 0.31F});
    worldShader_.setVec3("uEmission", {0.18F, 0.40F, 0.32F});
    worldShader_.setFloat("uFogDensity", 0.0028F);
    worldShader_.setFloat("uOpacity", 0.84F);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    projectileLines_.draw();
    glDepthMask(GL_TRUE);
}

void Game::renderHud() {
    std::vector<HudVertex> vertices;
    vertices.reserve(420);

    const HudColor graphite{0.002F, 0.012F, 0.010F, 0.84F};
    const HudColor fadedInk{0.014F, 0.060F, 0.052F, 0.43F};
    const HudColor red{0.82F, 0.085F, 0.022F, 0.92F};
    const HudColor cueColor = lockedAngel_ >= 0 ? red : fadedInk;

    // Optical ruler: intentionally incomplete, thin, and matte rather than a neon game HUD.
    addHudLine(vertices, -0.95F, 0.39F, -0.29F, 0.39F, graphite);
    addHudLine(vertices, 0.29F, 0.39F, 0.95F, 0.39F, graphite);
    for (int tick = 0; tick <= 18; ++tick) {
        const float fraction = static_cast<float>(tick) / 18.0F;
        const float tickLength = tick % 5 == 0 ? 0.060F : (tick % 2 == 0 ? 0.036F : 0.020F);
        const float leftX = -0.95F + fraction * 0.66F;
        const float rightX = 0.29F + fraction * 0.66F;
        addHudLine(vertices, leftX, 0.39F, leftX, 0.39F - tickLength, graphite);
        addHudLine(vertices, rightX, 0.39F, rightX, 0.39F - tickLength, graphite);
    }

    addHudLine(vertices, 0.0F, 0.34F, 0.0F, -0.74F, graphite);
    addHudChevron(vertices, 0.0F, 0.285F, 0.042F, cueColor);
    for (int marker = 0; marker < 4; ++marker) {
        addHudChevron(vertices, 0.0F, -0.03F - static_cast<float>(marker) * 0.115F, 0.035F, graphite);
    }

    // A hand-drawn ballistic curve in the lower-left part of the optic.
    constexpr int curveSegments = 24;
    for (int segment = 0; segment < curveSegments; ++segment) {
        const float firstT = static_cast<float>(segment) / curveSegments;
        const float secondT = static_cast<float>(segment + 1) / curveSegments;
        const auto curveY = [](float t) { return -0.60F + t * 0.17F + t * t * 0.43F; };
        addHudLine(vertices, -0.94F + firstT * 0.62F, curveY(firstT),
                   -0.94F + secondT * 0.62F, curveY(secondT), fadedInk);
    }

    const float aperture = clamp(0.050F / zoom_, 0.013F, 0.050F);
    addHudLine(vertices, -aperture, 0.0F, aperture, 0.0F, graphite);
    addHudLine(vertices, 0.0F, -aperture, 0.0F, aperture, graphite);

    const int range = lockedAngel_ >= 0 ? static_cast<int>(lockedDistance_) : 0;
    const int flightCentiseconds = lockedAngel_ >= 0 ? static_cast<int>(fireControlFlightSeconds_ * 100.0F) : 0;
    addHudNumber(vertices, range, 3, -0.88F, 0.27F, 0.022F, fadedInk);
    addHudNumber(vertices, flightCentiseconds, 2, 0.76F, 0.27F, 0.022F, fadedInk);
    addHudNumber(vertices, signalsResolved_, 2, -0.90F, -0.86F, 0.021F, fadedInk);

    if (fireCooldown_ > 0.0F || zoomPulse_ > 0.0F) {
        const float pulse = fireCooldown_ > 0.0F ? clamp(fireCooldown_ / 0.62F, 0.0F, 1.0F) : zoomPulse_;
        const float radius = aperture * (3.0F + (1.0F - pulse) * 2.4F);
        for (int section = 0; section < 12; ++section) {
            const float first = 2.0F * kPi * static_cast<float>(section) / 12.0F;
            const float second = 2.0F * kPi * static_cast<float>(section + 1) / 12.0F;
            const HudColor pulseColor{0.22F, 0.36F, 0.31F, pulse * 0.25F};
            addHudLine(vertices, std::cos(first) * radius, std::sin(first) * radius,
                       std::cos(second) * radius, std::sin(second) * radius, pulseColor);
        }
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    hudShader_.use();
    glBindVertexArray(hudVertexArray_);
    glBindBuffer(GL_ARRAY_BUFFER, hudVertexBuffer_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(HudVertex)),
                 vertices.empty() ? nullptr : vertices.data(),
                 GL_DYNAMIC_DRAW);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);
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
