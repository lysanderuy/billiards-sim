#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/glfw_context.h"
#include "core/input.h"
#include "core/window.h"

namespace {

constexpr int kCircleSegments = 40;
constexpr float kPi = 3.14159265358979323846f;

constexpr float kInch = 0.0254f;
constexpr float kGravity = 9.81f;

constexpr float kTableHalfLength = 46.0f * kInch;
constexpr float kTableHalfWidth = 23.0f * kInch;
constexpr float kCushionWidth = 0.05f;
constexpr float kRailWidth = 0.13f;
constexpr float kFrameTrim = 0.015f;
constexpr float kDiamondSize = 0.008f;
constexpr float kPocketRadius = 0.07f;
constexpr float kPocketRimWidth = 0.008f;

constexpr float kViewHalfWidth = kTableHalfLength + kRailWidth;
constexpr float kViewHalfHeight = kTableHalfWidth + kRailWidth;
constexpr float kPixelsPerMeter = 460.0f;
constexpr int kWindowWidth = static_cast<int>(2.0f * kViewHalfWidth * kPixelsPerMeter + 0.5f);
constexpr int kWindowHeight = static_cast<int>(2.0f * kViewHalfHeight * kPixelsPerMeter + 0.5f);

constexpr float kBallScale = 1.15f;
constexpr float kBallRadius = 2.25f * kInch * 0.5f * kBallScale;
constexpr float kRackGap = 0.0005f;
constexpr float kCushionRestitution = 0.8f;
constexpr float kBallRestitution = 0.95f;
constexpr float kRollingResistance = 0.01f;
constexpr float kStopSpeed = 0.01f;

constexpr float kStepDt = 1.0f / 480.0f;
constexpr float kMaxFrameTime = 0.05f;

constexpr float kMaxPull = 0.35f;
constexpr float kMinPull = 0.01f;
constexpr float kMaxShotSpeed = 6.0f;
constexpr float kStickGap = 0.01f;
constexpr float kStickLength = 1.45f;
constexpr float kStickTipHalfWidth = 0.0065f;
constexpr float kStickButtHalfWidth = 0.015f;
constexpr float kAimLineLength = 0.5f;

constexpr float kHeadSpotX = -kTableHalfLength * 0.5f;
constexpr float kFootSpotX = kTableHalfLength * 0.5f;

constexpr int kRackRows = 5;

struct Ball {
    float x, y, vx, vy;
    float r, g, b;
    bool active = true;
};

struct Segment {
    float x0, y0, x1, y1;
    float nx, ny;
};

struct Pocket {
    float x, y;
};

struct Cue {
    float angle = 0.0f;
    float pull = 0.0f;
    float pressX = 0.0f;
    float pressY = 0.0f;
    bool pulling = false;
};

constexpr std::array<std::array<float, 3>, 15> kRackColors = {{
    {1.0f, 0.85f, 0.1f}, {0.1f, 0.3f, 0.9f}, {0.9f, 0.15f, 0.15f}, {0.5f, 0.15f, 0.6f},
    {1.0f, 0.5f, 0.1f}, {0.1f, 0.6f, 0.25f}, {0.55f, 0.1f, 0.15f}, {0.05f, 0.05f, 0.05f},
    {1.0f, 0.9f, 0.4f}, {0.4f, 0.6f, 1.0f}, {1.0f, 0.5f, 0.5f}, {0.75f, 0.5f, 0.85f},
    {1.0f, 0.7f, 0.4f}, {0.4f, 0.8f, 0.5f}, {0.75f, 0.35f, 0.4f},
}};

constexpr const char* kVertexShaderSource = R"(#version 330 core
layout (location = 0) in vec2 aPos;
uniform vec2 uViewScale;
void main() { gl_Position = vec4(aPos * uViewScale, 0.0, 1.0); }
)";

constexpr const char* kFragmentShaderSource = R"(#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main() { FragColor = uColor; }
)";

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        glDeleteShader(shader);
        throw std::runtime_error(std::string("Shader compilation failed: ") + log);
    }
    return shader;
}

GLuint createProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        glDeleteProgram(program);
        throw std::runtime_error(std::string("Program linking failed: ") + log);
    }
    return program;
}

GLuint createDynamicVao(GLuint& vbo) {
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return vao;
}

struct Renderer {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLint colorLoc = -1;

    void draw(GLenum mode, const std::vector<float>& vertices, float r, float g, float b) const {
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(),
                     GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glUniform4f(colorLoc, r, g, b, 1.0f);
        glBindVertexArray(vao);
        glDrawArrays(mode, 0, static_cast<GLsizei>(vertices.size() / 2));
    }

    void drawRect(float x0, float y0, float x1, float y1, float r, float g, float b) const {
        draw(GL_TRIANGLE_FAN, {x0, y0, x1, y0, x1, y1, x0, y1}, r, g, b);
    }

    void drawCircle(float cx, float cy, float radius, float r, float g, float b) const {
        std::vector<float> vertices = {cx, cy};
        for (int i = 0; i <= kCircleSegments; ++i) {
            float angle = static_cast<float>(i) / kCircleSegments * 2.0f * kPi;
            vertices.push_back(cx + radius * cosf(angle));
            vertices.push_back(cy + radius * sinf(angle));
        }
        draw(GL_TRIANGLE_FAN, vertices, r, g, b);
    }
};

std::vector<Ball> buildBalls() {
    std::vector<Ball> balls;
    balls.push_back({kHeadSpotX, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f});

    float spacing = 2.0f * kBallRadius + kRackGap;
    float rowStep = sqrtf(3.0f) * 0.5f * spacing;
    int index = 0;
    for (int row = 0; row < kRackRows; ++row) {
        for (int i = 0; i <= row; ++i) {
            const auto& color = kRackColors[index++];
            balls.push_back({kFootSpotX + row * rowStep, (i - row * 0.5f) * spacing, 0.0f, 0.0f,
                             color[0], color[1], color[2]});
        }
    }
    return balls;
}

std::vector<Segment> buildCushions() {
    float hx = kTableHalfLength;
    float hy = kTableHalfWidth;
    float r = kPocketRadius;
    return {
        {-hx + r, hy, -r, hy, 0.0f, -1.0f},
        {r, hy, hx - r, hy, 0.0f, -1.0f},
        {-hx + r, -hy, -r, -hy, 0.0f, 1.0f},
        {r, -hy, hx - r, -hy, 0.0f, 1.0f},
        {-hx, -hy + r, -hx, hy - r, 1.0f, 0.0f},
        {hx, -hy + r, hx, hy - r, -1.0f, 0.0f},
    };
}

std::vector<Pocket> buildPockets() {
    float hx = kTableHalfLength;
    float hy = kTableHalfWidth;
    return {{-hx, -hy}, {0.0f, -hy}, {hx, -hy}, {-hx, hy}, {0.0f, hy}, {hx, hy}};
}

bool allAtRest(const std::vector<Ball>& balls) {
    for (const Ball& ball : balls) {
        if (ball.active && (ball.vx != 0.0f || ball.vy != 0.0f)) return false;
    }
    return true;
}

bool overlapsActiveBall(const std::vector<Ball>& balls, float x, float y) {
    for (size_t i = 1; i < balls.size(); ++i) {
        float dx = balls[i].x - x;
        float dy = balls[i].y - y;
        if (balls[i].active && dx * dx + dy * dy < 4.0f * kBallRadius * kBallRadius) return true;
    }
    return false;
}

void respawnCueBall(std::vector<Ball>& balls) {
    Ball& cueBall = balls[0];
    cueBall.vx = 0.0f;
    cueBall.vy = 0.0f;
    cueBall.active = true;
    cueBall.x = kHeadSpotX;
    cueBall.y = 0.0f;

    float minX = -kTableHalfLength + kBallRadius;
    float maxY = kTableHalfWidth - kBallRadius;
    for (int row = 0; row <= 4; ++row) {
        float y = ((row + 1) / 2) * 2.0f * kBallRadius * (row % 2 == 0 ? 1.0f : -1.0f);
        if (std::fabs(y) > maxY) continue;
        for (float x = kHeadSpotX; x >= minX; x -= kBallRadius) {
            if (!overlapsActiveBall(balls, x, y)) {
                cueBall.x = x;
                cueBall.y = y;
                return;
            }
        }
    }
}

void applyFriction(Ball& ball, float dt) {
    float speed = sqrtf(ball.vx * ball.vx + ball.vy * ball.vy);
    if (speed == 0.0f) return;
    float newSpeed = speed - kRollingResistance * kGravity * dt;
    if (newSpeed < kStopSpeed) {
        ball.vx = 0.0f;
        ball.vy = 0.0f;
        return;
    }
    float scale = newSpeed / speed;
    ball.vx *= scale;
    ball.vy *= scale;
}

void bounceOffNormal(Ball& ball, float nx, float ny) {
    float normalSpeed = ball.vx * nx + ball.vy * ny;
    if (normalSpeed < 0.0f) {
        float change = (1.0f + kCushionRestitution) * normalSpeed;
        ball.vx -= change * nx;
        ball.vy -= change * ny;
    }
}

void resolveSegment(Ball& ball, const Segment& seg) {
    float dirX = seg.x1 - seg.x0;
    float dirY = seg.y1 - seg.y0;
    float length = sqrtf(dirX * dirX + dirY * dirY);
    float relX = ball.x - seg.x0;
    float relY = ball.y - seg.y0;
    float along = (relX * dirX + relY * dirY) / length;
    float depth = relX * seg.nx + relY * seg.ny;
    if (along >= 0.0f && along <= length && depth < kBallRadius) {
        ball.x += (kBallRadius - depth) * seg.nx;
        ball.y += (kBallRadius - depth) * seg.ny;
        bounceOffNormal(ball, seg.nx, seg.ny);
    }
}

void resolveJawTip(Ball& ball, float tipX, float tipY) {
    float dx = ball.x - tipX;
    float dy = ball.y - tipY;
    float dist2 = dx * dx + dy * dy;
    if (dist2 >= kBallRadius * kBallRadius || dist2 == 0.0f) return;
    float dist = sqrtf(dist2);
    float nx = dx / dist;
    float ny = dy / dist;
    ball.x = tipX + nx * kBallRadius;
    ball.y = tipY + ny * kBallRadius;
    bounceOffNormal(ball, nx, ny);
}

void resolveCushions(Ball& ball, const std::vector<Segment>& cushions,
                     const std::vector<Pocket>& pockets) {
    for (const Segment& seg : cushions) {
        resolveSegment(ball, seg);
        resolveJawTip(ball, seg.x0, seg.y0);
        resolveJawTip(ball, seg.x1, seg.y1);
    }
    for (const Pocket& pocket : pockets) {
        float dx = ball.x - pocket.x;
        float dy = ball.y - pocket.y;
        if (dx * dx + dy * dy < kPocketRadius * kPocketRadius) {
            ball.active = false;
            ball.vx = 0.0f;
            ball.vy = 0.0f;
            return;
        }
    }
}

void resolveBallCollision(Ball& a, Ball& b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float minDist = 2.0f * kBallRadius;
    float dist2 = dx * dx + dy * dy;
    if (dist2 >= minDist * minDist || dist2 == 0.0f) return;

    float dist = sqrtf(dist2);
    float nx = dx / dist;
    float ny = dy / dist;

    float relativeNormalSpeed = (b.vx - a.vx) * nx + (b.vy - a.vy) * ny;
    if (relativeNormalSpeed < 0.0f) {
        float impulse = 0.5f * (1.0f + kBallRestitution) * relativeNormalSpeed;
        a.vx += impulse * nx;
        a.vy += impulse * ny;
        b.vx -= impulse * nx;
        b.vy -= impulse * ny;
    }

    float push = 0.5f * (minDist - dist);
    a.x -= push * nx;
    a.y -= push * ny;
    b.x += push * nx;
    b.y += push * ny;
}

void stepBalls(std::vector<Ball>& balls, const std::vector<Segment>& cushions,
               const std::vector<Pocket>& pockets, float dt) {
    for (Ball& ball : balls) {
        if (!ball.active) continue;
        ball.x += ball.vx * dt;
        ball.y += ball.vy * dt;
        applyFriction(ball, dt);
        resolveCushions(ball, cushions, pockets);
    }
    for (size_t i = 0; i < balls.size(); ++i) {
        if (!balls[i].active) continue;
        for (size_t j = i + 1; j < balls.size(); ++j) {
            if (balls[j].active) resolveBallCollision(balls[i], balls[j]);
        }
    }
}

void updateCue(Cue& cue, Ball& cueBall, float mouseX, float mouseY, bool down) {
    if (!cue.pulling) {
        cue.angle = atan2f(mouseY - cueBall.y, mouseX - cueBall.x);
    }

    float dirX = cosf(cue.angle);
    float dirY = sinf(cue.angle);

    if (down && !cue.pulling) {
        cue.pulling = true;
        cue.pressX = mouseX;
        cue.pressY = mouseY;
    }

    if (cue.pulling && down) {
        float behind = -((mouseX - cue.pressX) * dirX + (mouseY - cue.pressY) * dirY);
        cue.pull = std::clamp(behind, 0.0f, kMaxPull);
    } else if (cue.pulling) {
        if (cue.pull > kMinPull) {
            float speed = cue.pull / kMaxPull * kMaxShotSpeed;
            cueBall.vx = dirX * speed;
            cueBall.vy = dirY * speed;
        }
        cue.pulling = false;
        cue.pull = 0.0f;
    }
}

void drawDiamond(const Renderer& renderer, float cx, float cy) {
    renderer.draw(GL_TRIANGLE_FAN,
                  {cx, cy + kDiamondSize, cx + kDiamondSize, cy, cx, cy - kDiamondSize,
                   cx - kDiamondSize, cy},
                  0.9f, 0.85f, 0.7f);
}

void drawTable(const Renderer& renderer, const std::vector<Pocket>& pockets) {
    float hx = kTableHalfLength;
    float hy = kTableHalfWidth;
    float outerX = hx + kRailWidth;
    float outerY = hy + kRailWidth;
    renderer.drawRect(-outerX, -outerY, outerX, outerY, 0.24f, 0.13f, 0.06f);
    renderer.drawRect(-outerX + kFrameTrim, -outerY + kFrameTrim, outerX - kFrameTrim,
                      outerY - kFrameTrim, 0.4f, 0.24f, 0.11f);
    renderer.drawRect(-hx, -hy, hx, hy, 0.05f, 0.35f, 0.15f);

    float w = kCushionWidth;
    renderer.drawRect(-hx, hy, hx, hy + w, 0.04f, 0.27f, 0.11f);
    renderer.drawRect(-hx, -hy - w, hx, -hy, 0.04f, 0.27f, 0.11f);
    renderer.drawRect(-hx - w, -hy, -hx, hy, 0.04f, 0.27f, 0.11f);
    renderer.drawRect(hx, -hy, hx + w, hy, 0.04f, 0.27f, 0.11f);

    for (const Pocket& pocket : pockets) {
        renderer.drawCircle(pocket.x, pocket.y, kPocketRadius + kPocketRimWidth, 0.15f, 0.08f,
                            0.04f);
        renderer.drawCircle(pocket.x, pocket.y, kPocketRadius, 0.02f, 0.02f, 0.02f);
    }

    float diamondY = hy + kCushionWidth + (kRailWidth - kCushionWidth) * 0.5f;
    float diamondX = hx + kCushionWidth + (kRailWidth - kCushionWidth) * 0.5f;
    for (int k = 1; k <= 7; ++k) {
        if (k == 4) continue;
        float x = -hx + k * hx / 4.0f;
        drawDiamond(renderer, x, diamondY);
        drawDiamond(renderer, x, -diamondY);
    }
    for (int k = 1; k <= 3; ++k) {
        float y = -hy + k * hy / 2.0f;
        drawDiamond(renderer, diamondX, y);
        drawDiamond(renderer, -diamondX, y);
    }
}

void drawBall(const Renderer& renderer, const Ball& ball) {
    renderer.drawCircle(ball.x, ball.y, kBallRadius, ball.r, ball.g, ball.b);
    renderer.drawCircle(ball.x - 0.3f * kBallRadius, ball.y + 0.3f * kBallRadius,
                        0.25f * kBallRadius, ball.r + (1.0f - ball.r) * 0.55f,
                        ball.g + (1.0f - ball.g) * 0.55f, ball.b + (1.0f - ball.b) * 0.55f);
}

void drawCue(const Renderer& renderer, const Cue& cue, const Ball& cueBall) {
    float dirX = cosf(cue.angle);
    float dirY = sinf(cue.angle);
    float perpX = -dirY;
    float perpY = dirX;

    float startOffset = kBallRadius + kStickGap;
    renderer.draw(GL_LINES,
                  {cueBall.x + dirX * startOffset, cueBall.y + dirY * startOffset,
                   cueBall.x + dirX * (startOffset + kAimLineLength),
                   cueBall.y + dirY * (startOffset + kAimLineLength)},
                  0.75f, 0.9f, 0.75f);

    float tipX = cueBall.x - dirX * (startOffset + cue.pull);
    float tipY = cueBall.y - dirY * (startOffset + cue.pull);
    float buttX = tipX - dirX * kStickLength;
    float buttY = tipY - dirY * kStickLength;
    renderer.draw(GL_TRIANGLE_STRIP,
                  {tipX + perpX * kStickTipHalfWidth, tipY + perpY * kStickTipHalfWidth,
                   tipX - perpX * kStickTipHalfWidth, tipY - perpY * kStickTipHalfWidth,
                   buttX + perpX * kStickButtHalfWidth, buttY + perpY * kStickButtHalfWidth,
                   buttX - perpX * kStickButtHalfWidth, buttY - perpY * kStickButtHalfWidth},
                  0.85f, 0.7f, 0.45f);
}

} // namespace

int main() {
    try {
        gfx::GlfwContext glfw;
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        gfx::Window window(
            {.width = kWindowWidth, .height = kWindowHeight, .title = "Billiards Sim"});

        GLuint program = createProgram(kVertexShaderSource, kFragmentShaderSource);
        glUseProgram(program);
        glUniform2f(glGetUniformLocation(program, "uViewScale"), 1.0f / kViewHalfWidth,
                    1.0f / kViewHalfHeight);

        Renderer renderer;
        renderer.colorLoc = glGetUniformLocation(program, "uColor");
        renderer.vao = createDynamicVao(renderer.vbo);

        std::vector<Ball> balls = buildBalls();
        std::vector<Segment> cushions = buildCushions();
        std::vector<Pocket> pockets = buildPockets();
        Cue cue;

        glClearColor(0.08f, 0.06f, 0.05f, 1.0f);

        double previousTime = glfwGetTime();
        float accumulator = 0.0f;

        while (!window.shouldClose()) {
            gfx::processInput(window);

            double now = glfwGetTime();
            float frameTime = std::min(static_cast<float>(now - previousTime), kMaxFrameTime);
            previousTime = now;

            double cursorX, cursorY;
            window.getCursorPosition(cursorX, cursorY);
            float mouseX =
                (static_cast<float>(cursorX / kWindowWidth) * 2.0f - 1.0f) * kViewHalfWidth;
            float mouseY =
                (1.0f - static_cast<float>(cursorY / kWindowHeight) * 2.0f) * kViewHalfHeight;
            bool down = window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);

            if (allAtRest(balls) && !balls[0].active) respawnCueBall(balls);

            if (allAtRest(balls)) {
                updateCue(cue, balls[0], mouseX, mouseY, down);
            } else {
                cue.pulling = false;
                cue.pull = 0.0f;
            }

            accumulator += frameTime;
            while (accumulator >= kStepDt) {
                stepBalls(balls, cushions, pockets, kStepDt);
                accumulator -= kStepDt;
            }

            glClear(GL_COLOR_BUFFER_BIT);

            drawTable(renderer, pockets);

            for (const Ball& ball : balls) {
                if (ball.active) {
                    drawBall(renderer, ball);
                }
            }

            if (allAtRest(balls)) {
                drawCue(renderer, cue, balls[0]);
            }

            window.swapBuffers();
            window.pollEvents();
        }

        glDeleteVertexArrays(1, &renderer.vao);
        glDeleteBuffers(1, &renderer.vbo);
        glDeleteProgram(program);
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
