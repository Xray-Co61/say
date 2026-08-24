#pragma once

#include "Math.hpp"
#include "Mesh.hpp"
#include "Shader.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdint>
#include <random>
#include <vector>

namespace fallen {

class Game final {
public:
    explicit Game(GLFWwindow* window);
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    void initialise();
    void update(float deltaSeconds);
    void render(int framebufferWidth, int framebufferHeight);

private:
    struct Angel {
        Vec3 position{};
        Vec3 velocity{};
        Vec3 heading{};
        Vec3 side{};
        std::vector<Vec3> trail;
        float scale = 1.0F;
        float baseAltitude = 0.0F;
        float waveAmplitude = 0.0F;
        float waveFrequency = 0.0F;
        float phase = 0.0F;
        float age = 0.0F;
        float disruption = 0.0F;
        float trailClock = 0.0F;
    };

    struct Projectile {
        Vec3 position{};
        Vec3 previousPosition{};
        Vec3 velocity{};
        std::vector<Vec3> trail;
        float age = 0.0F;
    };

    struct HudVertex {
        float x = 0.0F;
        float y = 0.0F;
        float r = 1.0F;
        float g = 1.0F;
        float b = 1.0F;
        float a = 1.0F;
    };

    struct HudColor {
        float r = 1.0F;
        float g = 1.0F;
        float b = 1.0F;
        float a = 1.0F;
    };

    GLFWwindow* window_ = nullptr;

    Shader worldShader_{};
    Shader skyShader_{};
    Shader angelShader_{};
    Shader projectileShader_{};
    Shader postShader_{};
    Shader hudShader_{};

    Mesh terrain_{};
    Mesh skyQuad_{};
    Mesh trailLines_{};
    Mesh projectileLines_{};

    GLuint sceneFramebuffer_ = 0;
    GLuint sceneColorTexture_ = 0;
    GLuint sceneDepthBuffer_ = 0;
    int sceneWidth_ = 0;
    int sceneHeight_ = 0;
    GLuint hudVertexArray_ = 0;
    GLuint hudVertexBuffer_ = 0;

    std::vector<Angel> angels_;
    std::vector<Projectile> projectiles_;
    std::mt19937 random_{0xF411E9U};

    Vec3 cameraPosition_{};
    float yawDegrees_ = -90.0F;
    float pitchDegrees_ = 11.0F;
    float zoom_ = 1.0F;
    float elapsedSeconds_ = 0.0F;
    float spawnClock_ = 0.0F;
    float fireCooldown_ = 0.0F;
    float recoil_ = 0.0F;
    float titleClock_ = 0.0F;
    float zoomPulse_ = 0.0F;
    int lockedAngel_ = -1;
    float lockedDistance_ = 0.0F;
    float fireControlFlightSeconds_ = 0.0F;
    Vec3 fireControlPoint_{};
    int signalsResolved_ = 0;
    bool previousSpaceDown_ = false;
    bool previousEscapeDown_ = false;
    bool previousResetDown_ = false;
    bool cursorCaptured_ = false;
    bool firstCursorSample_ = true;
    double lastCursorX_ = 0.0;
    double lastCursorY_ = 0.0;
    float mouseDeltaX_ = 0.0F;
    float mouseDeltaY_ = 0.0F;

    static void cursorPositionCallback(GLFWwindow* window, double x, double y);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int modifiers);

    static float terrainHeight(float x, float z);
    static Mesh makeTerrain();
    static Mesh makeSkyQuad();

    void createHudBuffer();
    void ensureSceneTarget(int width, int height);
    void destroySceneTarget() noexcept;
    void setCursorCaptured(bool captured);
    void updateAim(float deltaSeconds);
    void updateAngels(float deltaSeconds);
    void updateProjectiles(float deltaSeconds);
    void spawnAngel(bool immediatelyVisible);
    void resetAngel(Angel& angel, bool immediatelyVisible);
    void fire();
    void acquireLock();
    void resetEncounter();
    void updateWindowTitle();

    [[nodiscard]] Vec3 cameraForward() const;
    [[nodiscard]] Vec3 cameraRight() const;
    [[nodiscard]] float randomRange(float minimum, float maximum);
    [[nodiscard]] bool projectileHitsAngel(const Projectile& projectile, const Angel& angel) const;

    void renderWorld(const Mat4& view, const Mat4& projection);
    void renderAngel(const Angel& angel, const Mat4& view, const Mat4& projection);
    void renderTrails(const Mat4& view, const Mat4& projection);
    void renderProjectiles(const Mat4& view, const Mat4& projection);
    void renderHud();

    void addHudLine(std::vector<HudVertex>& vertices,
                    float fromX, float fromY, float toX, float toY,
                    HudColor color) const;
    void addHudChevron(std::vector<HudVertex>& vertices,
                       float x, float y, float size, HudColor color) const;
    void addHudDigit(std::vector<HudVertex>& vertices,
                     int digit, float x, float y, float size, HudColor color) const;
    void addHudNumber(std::vector<HudVertex>& vertices,
                      int number, int minimumDigits,
                      float x, float y, float size, HudColor color) const;
};

} // namespace fallen
