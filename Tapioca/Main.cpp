#include "pch.h"

const double GRAVITY = 1.5;
const double FLOOR_HEIGHT = 10;
const int NUM_X_BLOCKS = 8;

class Block;
std::vector<std::unique_ptr<Block>> blocks;

class Block {
public:
    Block(double x) : rect(x, -50, 50, 50) {}

    void update() {
        speed = FALLING_SPEED;
        if (willCollide()) {
            speed = 0.0;
        } else {
            rect.y += speed;
        }
    }

    void draw() const { rect.draw(Palette::Blue); }

    bool intersects(RectF other) const { return other.intersects(rect); }

    RectF rect;
    bool destroyed = false;

    double speed = 3.0;

private:
    const double FALLING_SPEED = 3.0;

    bool willCollide() const {
        if (rect.y + rect.h + speed > Window::Height() - FLOOR_HEIGHT) {
            return true;
        }

        auto nextRect = rect;
        nextRect.y += speed;
        for (const auto &block : blocks) {
            if (this != block.get() && nextRect.intersects(block->rect)) {
                return true;
            }
        }
        return false;
    }
};

class Bullet {
public:
    Bullet(Vec2 pos, bool right)
        : rect(pos - Vec2(25.0, 50.0), 50.0, 50.0),
        velocity(right ? SPEED : -SPEED, -SPEED), active(true) {}

    void update() {
        if (!active) {
            return;
        }

        if (rect.x + rect.w <= 0.0 || rect.x > Window::Width()) {
            active = false;
            return;
        }

        for (auto &block : blocks) {
            if (block->intersects(rect)) {
                block->destroyed = true;
                active = false;
                return;
            }
        }

        velocity.y += GRAVITY;
        rect.pos += velocity;
    }

    void draw() const {
        if (active) {
            rect.draw(Palette::Green);
        }
    }

    bool active;

private:
    const double SPEED = 20.0;
    RectF rect;
    Vec2 velocity;
};

class Player {
public:
    Player() : rect(50, 50, 50, 100), bulletFireTimer(1.0, false) {}

    void update() {
        if (KeyZ.down() &&
            (!bullet || (!bullet->active && bulletFireTimer.reachedZero()))) {
            bullet = std::make_unique<Bullet>(rect.topCenter(), facingRight);
            bulletFireTimer.restart();
        }
        if (bullet) {
            bullet->update();
        }

        const bool left = KeyLeft.pressed() && rect.x > 0.0;
        const bool right = KeyRight.pressed() && rect.x + rect.w < Window::Width();
        double vx = 0.0;
        if (left ^ right) {
            if (left) {
                vx = -SPEED;
                facingRight = false;
            }
            if (right) {
                vx = SPEED;
                facingRight = true;
            }
        }
        {
            auto nextRect = rect;
            nextRect.x += vx;
            for (const auto &block : blocks) {
                if (!block->intersects(rect) && block->intersects(nextRect)) {
                    vx = 0.0;
                    break;
                }
            }
        }
        rect.x += vx;

        if (grounded && KeyUp.down()) {
            vy = -20;
            grounded = false;
        }

        vy += GRAVITY;
        bool touching = false;
        {
            auto nextRect = rect;
            nextRect.y += vy;
            for (const auto &block : blocks) {
                if (block->intersects(nextRect)) {
                    if (block->speed > 0.0) {
                        if (vy > 0.0) {
                            grounded = touching = true;
                        }
                        vy = block->speed;
                    } else {
                        grounded = touching = true;
                        vy = 0.0;
                    }
                    break;
                }
            }
        }

        if (!touching && rect.y + rect.h + vy > Window::Height() - FLOOR_HEIGHT) {
            grounded = true;
            vy = 0.0;
        }

        rect.y += vy;

        for (const auto &block : blocks) {
            if (block->intersects(rect)) {
                System::Exit();
            }
        }
    }

    void draw() {
        if (bullet) {
            bullet->draw();
        }
        rect.draw(Palette::Red);
    }

private:
    const double SPEED = 10;
    double vy = 0.0;
    RectF rect;
    bool grounded = false;
    bool facingRight = true;
    std::unique_ptr<Bullet> bullet;
    Timer bulletFireTimer;
};

void Main() {
    Window::Resize({ 50 * NUM_X_BLOCKS, 600 });
    Graphics::SetTargetFrameRateHz(60);
    Graphics::SetBackground(ColorF(0.8, 0.9, 1.0));

    Font font(30);
    int score = 0;
    int highScore = 0;

    Player player;
    Stopwatch sw;
    sw.start();
    while (System::Update()) {
        if (sw.ms() > 1000) {
            blocks.emplace_back(std::make_unique<Block>(
                Random(0, NUM_X_BLOCKS - 1) * Window::Width() /
                static_cast<double>(NUM_X_BLOCKS)));
            sw.restart();
        }
        for (auto &block : blocks) {
            block->update();
            if (block->speed == 0.0 && block->rect.y <= 0.0) {
                System::Exit();
            }
        }
        const int nBefore = blocks.size();
        blocks.erase(
            std::remove_if(blocks.begin(), blocks.end(),
                [](const auto &block) { return block->destroyed; }),
            blocks.end());
        player.update();

        score += 10 * std::max(0, static_cast<int>(nBefore - blocks.size()));
        highScore = std::max(score, highScore);

        for (auto &block : blocks) {
            block->draw();
        }
        player.draw();
        font(U"SCORE: ", score, U" HIGHSCORE: ", highScore)
            .draw(Vec2(0, 0), Palette::Black);
    }
}
