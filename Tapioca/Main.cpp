#include "pch.h"

constexpr double gravity = 1.5;
constexpr double floorHeight = 80;
constexpr int numBlocksX = 8;

enum class Scene {
    Title,
    Playing,
    GameOver
};

class Animation {
public:
    Animation(std::vector<FilePath> textures, double intervals, bool looped = true, bool immediatelyStarted = true) :
        looped(looped),
        timer(intervals) {
        for (const auto& path : textures) {
            texAssets.emplace_back(path);
        }
        if (immediatelyStarted) {
            start();
        }
    }

    void start() {
        timer.restart();
        started = true;
    }

    void stop() {
        timer.pause();
        started = false;
    }

    void update() {
        if (timer.reachedZero()) {
            if (looped) {
                idx = (idx + 1) % texAssets.size();
                timer.restart();
            } else if (idx < texAssets.size() - 1) {
                ++idx;
                timer.restart();
            } else {
                started = false;
            }
        }
    }

    const TextureAsset& get() const {
        return texAssets.at(idx);
    }

    bool isFinished() const {
        return !looped && idx == texAssets.size() - 1 && timer.reachedZero();
    }

    bool isStarted() const {
        return started;
    }

private:
    bool looped;
    std::vector<TextureAsset> texAssets;
    Timer timer;
    size_t idx = 0;
    bool started = false;
};

class Stage {
public:
    Stage() :
        floorRect(0, Window::Height() - floorHeight, Window::Width(), floorHeight),
        sunRect(0, 0, 170, 170),
        sunAnim({ U"sun1", U"sun2" }, 0.5) {}

    void update() {
        sunAnim.update();
    }

    void draw() const {
        floorRect.draw(Color(123, 58, 21));
        floorRect.top().draw(5.0, Palette::Black);
        sunRect(sunAnim.get()).draw();
    }

private:
    RectF floorRect, sunRect;
    Animation sunAnim;
};

class Block {
public:
    using HitCallback = std::function<void(double height)>;

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
        rect(TextureAsset(U"block")).draw();
    }

    bool intersects(RectF other) const {
        return other.intersects(rect);
    }

    void destroy() {
        destroyed = true;
        hitCallback(1.0 - rect.y / Window::Height());
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

    void setHitCallback(HitCallback callback) {
        hitCallback = callback;
    }

private:
    RectF rect;
    bool destroyed = false;
    bool moving = true;
    bool touchingTop = false;
    double speed = 3.0;
    HitCallback hitCallback;

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

class Egg {
public:
    Egg(Vec2 pos, bool right) :
        rect(pos - Vec2(size / 2.0, size / 4.0), size, size),
        velocity(right ? speed : -speed, -speed),
        explosionAnim({ U"boom1", U"boom2" }, 0.1, false, false) {}

    void update(std::vector<Block>& blocks) {
        explosionAnim.update();

        if (explosionAnim.isFinished()) {
            destroyed = true;
            return;
        }
        if (explosionAnim.isStarted()) {
            return;
        }

        if (rect.x + rect.w <= 0.0 || rect.x > Window::Width()) {
            explosionAnim.start();
            return;
        }

        for (auto& block : blocks) {
            if (block.intersects(rect)) {
                block.destroy();
                explosionAnim.start();
                return;
            }
        }

        velocity.y += gravity;
        rect.pos += velocity;
    }

    void draw() const {
        const auto& tex = explosionAnim.isStarted() ? explosionAnim.get() : TextureAsset(U"tamago");
        rect(tex).draw();
    }

    bool isDestroyed() const {
        return destroyed;
    }

private:
    static const double speed;
    static const double size;
    RectF rect;
    Vec2 velocity;
    bool destroyed = false;
    Animation explosionAnim;
};

const double Egg::speed = 20.0;
const double Egg::size = 50.0;

class Player {
public:
    Player() :
        rect(100, Window::Height() - floorHeight - size.y, size),
        eggLaunchTimer(0.5, false),
        restingAnim({ U"stop1", U"stop2" }, 0.3),
        throwingAnim({ U"throw1" }, 0.2, false, false) {
        eggLaunchTimer.set(0s);
    }

    void update(std::vector<Block>& blocks) {
        if (KeyZ.down() && eggLaunchTimer.reachedZero()) {
            throwingAnim.start();
            egg = Egg(rect.topCenter(), facingRight);
            eggLaunchTimer.restart();
        }
        if (egg) {
            egg->update(blocks);
            if (egg->isDestroyed()) {
                egg = none;
            }
        }

        if (KeyLeft.pressed() ^ KeyRight.pressed()) {
            facingRight = KeyRight.pressed();

            const bool left = KeyLeft.pressed() && rect.x > 0.0;
            const bool right = KeyRight.pressed() && rect.x + rect.w < Window::Width();
            if (left ^ right) {
                double vx = left ? -speed : speed;
                auto nextRect = rect;
                nextRect.x += vx;
                for (const auto& block : blocks) {
                    if (block.intersects(nextRect)) {
                        vx = 0.0;
                        break;
                    }
                }
                rect.x += vx;
            }
        }

        if (grounded && KeyUp.down()) {
            constexpr double jumpSpeed = 20.0;
            vy = -jumpSpeed;
            grounded = false;
        }

        vy += gravity;
        bool touching = false;
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

        if (!touching && rect.y + rect.h + vy > Window::Height() - floorHeight) {
            grounded = true;
            vy = 0.0;
        }

        rect.y += vy;

        if (grounded) {
            for (const auto& block : blocks) {
                if (block.isMoving() && block.intersects(rect)) {
                    dead = true;
                    break;
                }
            }
        }

        restingAnim.update();
        throwingAnim.update();
    }

    void draw() const {
        if (egg) {
            egg->draw();
        }
        if (dead) {
            constexpr double armHeightInTexels = 30.0;
            const auto tex = TextureAsset(U"death");
            const auto tr = tex.scaled(rect.h / tex.height());
            tr.drawAt(rect.bottomCenter() - Vec2(0.0, tr.size.y / 2.0 - armHeightInTexels * rect.h / tex.height()));
        } else {
            constexpr double heightInTexels = 315.0;
            const auto& tex = (!throwingAnim.isStarted() || throwingAnim.isFinished()) ? restingAnim.get() : throwingAnim.get();
            const auto tr = tex.mirrored(facingRight).scaled(rect.h / heightInTexels);
            tr.drawAt(rect.bottomCenter() - Vec2(0.0, tr.size.y / 2.0));
        }
    }

    bool isDead() const {
        return dead;
    }

private:
    static const Size size;
    static const double speed;
    double vy = 0.0;
    RectF rect;
    bool grounded = false;
    bool facingRight = true;
    bool dead = false;
    Optional<Egg> egg;
    Timer eggLaunchTimer;
    Animation restingAnim, throwingAnim;
};

const Size Player::size = Size(40, 70);

const double Player::speed = 8;

struct Data {
    Font font = Font(28, U"PixelMplus10-Regular.ttf");
    int score = 0;
    int highScore = 0;
    Stage stage;
    Player player;
    std::vector<Block> blocks;
};

using App = SceneManager<Scene, Data>;

void drawScore(const Data& data) {
    data.font(U"SCORE ", Pad(data.score, { 5, U'0' })).draw(Vec2::Zero(), Palette::Black);
    data.font(U"HIGHSCORE ", Pad(data.highScore, { 5, U'0' })).draw(Arg::topRight = Vec2(Window::Width(), 0), Palette::Black);
}

class Title : public App::Scene {
public:
    Title(const InitData& init) : IScene(init) {
        const auto tex = TextureAsset(U"title");
        titleTex = tex.scaled(static_cast<double>(Window::Width()) / tex.width());
    }

    void update() override {
        if (KeyZ.down()) {
            changeScene(Scene::Playing, 0, false);
        }
    }

    void draw() const override {
        getData().stage.draw();
        getData().player.draw();
        drawScore(getData());
        titleTex.drawAt(Window::Center() - Vec2(0.0, Window::Height() / 8.0));

        int i = 0;
        for (const auto& line : { U"← → うごく", U"↑ ジャンプ", U"Z たまごをなげる", U"", U"Zをおして はじめる" }) {
            getData().font(line).drawAt(Window::Center() + Vec2(0.0, i++ * getData().font.height()), Palette::Black);
        }
    }

private:
    TextureRegion titleTex;
};

class Playing : public App::Scene {
public:
    Playing(const InitData& init) : IScene(init) {
        getData().score = 0;
        getData().player = Player();
        getData().blocks = {};
        blockFallSW.start();
    }

    void update() override {
        getData().stage.update();

        auto& blocks = getData().blocks;
        constexpr int blockFallIntervalms = 500;
        if (blockFallSW.ms() > blockFallIntervalms) {
            blocks.emplace_back(
                Random(0, numBlocksX - 1) * Window::Width() /
                static_cast<double>(numBlocksX));
            blocks.back().setHitCallback([this](double height) {
                getData().score += static_cast<int>(100.0 * height);
                getData().highScore = std::max(getData().score, getData().highScore);
            });
            blockFallSW.restart();
        }
        for (auto& block : blocks) {
            block.update(blocks);
            if (block.isTouchingTop()) {
                changeScene(Scene::GameOver, 0, false);
                return;
            }
        }

        getData().player.update(getData().blocks);
        if (getData().player.isDead()) {
            changeScene(Scene::GameOver, 0, false);
            return;
        }

        blocks.erase(
            std::remove_if(blocks.begin(), blocks.end(),
                [](const auto& block) { return block.isDestroyed(); }),
            blocks.end());
    }

    void draw() const override {
        getData().stage.draw();
        for (auto& block : getData().blocks) {
            block.draw();
        }
        getData().player.draw();
        drawScore(getData());
    }

private:
    Stopwatch blockFallSW;
};

class GameOver : public App::Scene {
public:
    GameOver(const InitData& init) : IScene(init) {
        const auto tex = TextureAsset(U"gameover");
        gameOverTex = tex.scaled(static_cast<double>(Window::Width()) / tex.width());
    }

    void update() override {
        if (KeyR.down()) {
            changeScene(Scene::Playing, 0, false);
        }
    }

    void draw() const override {
        getData().stage.draw();
        for (auto& block : getData().blocks) {
            block.draw();
        }
        getData().player.draw();
        drawScore(getData());
        gameOverTex.drawAt(Window::Center() - Vec2(0.0, Window::Height() / 8.0));
        getData().font(U"Rをおして もういちどはじめる").drawAt(Window::Center() + Vec2(0.0, Window::Height() / 8.0), Palette::Black);
    }

private:
    TextureRegion gameOverTex;
};

void Main() {
    Window::SetTitle(U"Tapioca");
    Window::Resize({ static_cast<int>(Block::size * numBlocksX), 600 });
    Graphics::SetTargetFrameRateHz(60);
    Graphics::SetBackground(Color(212, 255, 252));

    TextureAsset::Register(U"block", U"imgs/block.png");
    TextureAsset::Register(U"boom1", U"imgs/boom1.png");
    TextureAsset::Register(U"boom2", U"imgs/boom2.png");
    TextureAsset::Register(U"death", U"imgs/death.png");
    TextureAsset::Register(U"gameover", U"imgs/gameover.png");
    TextureAsset::Register(U"stop1", U"imgs/stop1.png");
    TextureAsset::Register(U"stop2", U"imgs/stop2.png");
    TextureAsset::Register(U"sun1", U"imgs/sun1.png");
    TextureAsset::Register(U"sun2", U"imgs/sun2.png");
    TextureAsset::Register(U"tamago", U"imgs/tamago.png");
    TextureAsset::Register(U"throw1", U"imgs/throw1.png");
    TextureAsset::Register(U"title", U"imgs/title.png");

    AudioAsset::Register(U"bgm", U"tapiocamild.mp3");
    AudioAsset(U"bgm").setLoop(true);
    AudioAsset(U"bgm").play();

    const auto data = std::make_shared<Data>();

    const auto scoreFile = U"score";
    if (FileSystem::Exists(scoreFile)) {
        BinaryReader reader(scoreFile);
        if (!reader.read(data->highScore)) {
            data->highScore = 0;
        }
    }

    App mgr(data);
    mgr.add<Title>(Scene::Title)
        .add<Playing>(Scene::Playing)
        .add<GameOver>(Scene::GameOver);
    mgr.changeScene(Scene::Title, 0, false);

    while (System::Update()) {
        if (!mgr.update()) {
            break;
        }
    }

    BinaryWriter writer(scoreFile);
    writer.write(&data->highScore, sizeof(data->highScore));
}
