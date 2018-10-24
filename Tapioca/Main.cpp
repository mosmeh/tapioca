#include "pch.h"

constexpr double gravity = 1.5;
constexpr double floorHeight = 10;
constexpr int numBlocksX = 8;

enum class Scene {
    Title,
    Playing,
    GameOver
};

struct Data {
    Font font = Font(30);
    int score = 0;
    int highScore = 0;
};

using App = SceneManager<Scene, Data>;

class Block {
public:
    static const double fallingSpeed;
    static const double size;

    Block(double x) : rect(x, -size, size, size) {}

    void update(const std::vector<Block>& blocks) {
        speed = fallingSpeed;
        if (willCollide(blocks)) {
            if (rect.y <= 0.0) {
                touchingTop = true;
            }

            moving = false;
            speed = 0.0;
        } else {
            moving = true;
            rect.y += speed;
        }
    }

    void draw() const {
        rect.draw(Palette::Blue);
    }

    bool intersects(RectF other) const {
        return other.intersects(rect);
    }

    void destroy() {
        destroyed = true;
    }

    bool isDestroyed() const {
        return destroyed;
    }

    bool isMoving() const {
        return moving;
    }

    bool isTouchingTop() const {
        return touchingTop;
    }

private:
    RectF rect;
    bool destroyed = false;
    bool moving = true;
    bool touchingTop = false;
    double speed = 3.0;

    bool willCollide(const std::vector<Block>& blocks) const {
        if (rect.y + rect.h + speed > Window::Height() - floorHeight) {
            return true;
        }

        auto nextRect = rect;
        nextRect.y += speed;
        for (const auto& block : blocks) {
            if (this != &block && nextRect.intersects(block.rect)) {
                return true;
            }
        }
        return false;
    }
};

const double Block::fallingSpeed = 3.0;
const double Block::size = 50.0;

class Bullet {
public:
    Bullet(Vec2 pos, bool right) :
        rect(pos - Vec2(size / 2, size), size, size),
        velocity(right ? speed : -speed, -speed), active(true) {}

    void update(std::vector<Block>& blocks) {
        if (!active) {
            return;
        }

        if (rect.x + rect.w <= 0.0 || rect.x > Window::Width()) {
            active = false;
            return;
        }

        for (auto& block : blocks) {
            if (block.intersects(rect)) {
                block.destroy();
                active = false;
                return;
            }
        }

        velocity.y += gravity;
        rect.pos += velocity;
    }

    void draw() const {
        if (active) {
            rect.draw(Palette::Green);
        }
    }

    bool active;

private:
    static const double speed;
    static const double size;
    RectF rect;
    Vec2 velocity;
};

const double Bullet::speed = 20.0;
const double Bullet::size = 50.0;

class Player {
public:
    Player() :
        rect(50, 50, 50, 100),
        bulletFireTimer(1.0, false) {}

    void update(std::vector<Block>& blocks) {
        if (KeyZ.down() &&
            (!bullet || (!bullet->active && bulletFireTimer.reachedZero()))) {
            bullet = Bullet(rect.topCenter(), facingRight);
            bulletFireTimer.restart();
        }
        if (bullet) {
            bullet->update(blocks);
        }

        const bool left = KeyLeft.pressed() && rect.x > 0.0;
        const bool right = KeyRight.pressed() && rect.x + rect.w < Window::Width();
        double vx = 0.0;
        if (left ^ right) {
            vx = left ? -speed : speed;
            facingRight = right;
        }
        {
            auto nextRect = rect;
            nextRect.x += vx;
            for (const auto& block : blocks) {
                if (block.intersects(nextRect)) {
                    vx = 0.0;
                    break;
                }
            }
        }
        rect.x += vx;

        if (grounded && KeyUp.down()) {
            vy = -20.0;
            grounded = false;
        }

        vy += gravity;
        bool touching = false;
        {
            auto nextRect = rect;
            nextRect.y += vy;
            for (const auto& block : blocks) {
                if (block.intersects(nextRect)) {
                    if (block.isMoving()) {
                        if (vy > 0.0) {
                            grounded = touching = true;
                        }
                        vy = Block::fallingSpeed;
                    } else {
                        grounded = touching = true;
                        vy = 0.0;
                    }
                    break;
                }
            }
        }

        if (!touching && rect.y + rect.h + vy > Window::Height() - floorHeight) {
            grounded = true;
            vy = 0.0;
        }

        rect.y += vy;

        for (const auto& block : blocks) {
            if (block.intersects(rect)) {
                dead = true;
                break;
            }
        }
    }

    void draw() const {
        if (bullet) {
            bullet->draw();
        }
        rect.draw(Palette::Red);
    }

    bool isDead() const {
        return dead;
    }

private:
    static const double speed;
    double vy = 0.0;
    RectF rect;
    bool grounded = false;
    bool facingRight = true;
    bool dead = false;
    Optional<Bullet> bullet;
    Timer bulletFireTimer;
};

const double Player::speed = 10;

class Title : public App::Scene {
public:
    Title(const InitData& init) : IScene(init) {}
};

class Playing : public App::Scene {
public:
    Playing(const InitData& init) : IScene(init) {
        blockFallSW.start();
    }

    void update() override {
        constexpr int blockFallIntervalms = 1000;
        if (blockFallSW.ms() > blockFallIntervalms) {
            blocks.emplace_back(
                Random(0, numBlocksX - 1) * Window::Width() /
                static_cast<double>(numBlocksX));
            blockFallSW.restart();
        }
        for (auto& block : blocks) {
            block.update(blocks);
            if (block.isTouchingTop()) {
                changeScene(Scene::GameOver, 0, false);
            }
        }
        const auto numBlocks = blocks.size();
        blocks.erase(
            std::remove_if(blocks.begin(), blocks.end(),
                [](const auto& block) { return block.isDestroyed(); }),
            blocks.end());
        getData().score += 10 * std::max(0, static_cast<int>(numBlocks - blocks.size()));
        getData().highScore = std::max(getData().score, getData().highScore);

        player.update(blocks);
        if (player.isDead()) {
            changeScene(Scene::GameOver, 0, false);
        }
    }

    void draw() const override {
        for (auto& block : blocks) {
            block.draw();
        }
        player.draw();
        getData().font(U"SCORE: ", getData().score, U" HIGHSCORE: ", getData().highScore)
            .draw(Vec2(0, 0), Palette::Black);
    }

private:
    Player player;
    std::vector<Block> blocks;
    Stopwatch blockFallSW;
};

class GameOver : public App::Scene {
public:
    GameOver(const InitData& init) : IScene(init) {}
};

void Main() {
    Window::SetTitle(U"Tapioca");
    Window::Resize({static_cast<int>(Block::size * numBlocksX), 600});
    Graphics::SetTargetFrameRateHz(60);
    Graphics::SetBackground(Color(212, 255, 252));

    TextureAsset::Register(U"block", U"block.png");
    TextureAsset::Register(U"boom1", U"boom1.png");
    TextureAsset::Register(U"boom2", U"boom2.png");
    TextureAsset::Register(U"stop1", U"stop1.png");
    TextureAsset::Register(U"stop2", U"stop2.png");
    TextureAsset::Register(U"sun1", U"sun1.png");
    TextureAsset::Register(U"sun2", U"sun2.png");
    TextureAsset::Register(U"tamago", U"tamago.png");
    TextureAsset::Register(U"throw1", U"throw1.png");
    TextureAsset::Register(U"throw2", U"throw2.png");
    TextureAsset::Register(U"throw3", U"throw3.png");

    App mgr;
    mgr.add<Title>(Scene::Title)
        .add<Playing>(Scene::Playing)
        .add<GameOver>(Scene::GameOver);
    mgr.init(Scene::Title);
    mgr.changeScene(Scene::Playing, 0, false);

    while (System::Update()) {
        if (!mgr.update()) {
            break;
        }
    }
}
