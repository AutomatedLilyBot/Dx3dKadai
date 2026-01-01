#include "MenuScene.hpp"

#include <string>

#include "SceneManager.hpp"
#include "TransitionScene.hpp"
#include "BattleScene.hpp"

#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <windows.h>

#include "core/gfx/Renderer.hpp"
#include "../ui/UIImage.hpp"
#include "../ui/UIButton.hpp"

static std::wstring ExeDirMenu() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf);
    auto p = s.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? L"." : s.substr(0, p);
}

void MenuScene::init(Renderer *renderer) {
    renderer_ = renderer;
    if (!renderer_ || !renderer_->device()) return;

    auto tryLoad = [&](Texture &tex, const std::wstring &name, const DirectX::XMFLOAT4 &color) {
        std::wstring path = ExeDirMenu() + L"\\asset\\" + name;
        if (!tex.loadFromFile(renderer_->device(), path)) {
            tex.createSolidColor(renderer_->device(),
                                 static_cast<unsigned char>(color.x * 255.0f),
                                 static_cast<unsigned char>(color.y * 255.0f),
                                 static_cast<unsigned char>(color.z * 255.0f),
                                 255);
        }
    };

    tryLoad(background_, L"menu_background.png", DirectX::XMFLOAT4{1, 1, 1, 1.0f});
    tryLoad(title_, L"menu_title.png", DirectX::XMFLOAT4{1.0f, 1.0f, 1.0f, 1.0f});
    tryLoad(startButton_, L"menu_start.png", DirectX::XMFLOAT4{1.0f, 1.0f, 1.0f, 1.0f});
    tryLoad(exitButton_, L"menu_exit.png", DirectX::XMFLOAT4{0.5f, 0.2f, 0.2f, 1.0f});

    // 创建UI元素（使用新的UI系统）
    // 背景图片
    auto bgImage = std::make_unique<UIImage>();
    bgImage->transform().x = 0.0f;
    bgImage->transform().y = 0.0f;
    bgImage->transform().width = 1.0f;
    bgImage->transform().height = 1.0f;
    bgImage->setTexture(&background_);
    bgImage->setLayer(0);
    addUIElement(std::move(bgImage));

    // 标题图片
    auto titleImage = std::make_unique<UIImage>();
    titleImage->transform().x = 0.25f;
    titleImage->transform().y = 0.1f;
    titleImage->transform().width = 0.5f;
    titleImage->transform().height = 0.2f;
    titleImage->setTexture(&title_);
    titleImage->setLayer(1);
    addUIElement(std::move(titleImage));

    // 开始按钮
    auto startBtn = std::make_unique<UIButton>();
    startBtn->transform().x = 0.35f;
    startBtn->transform().y = 0.45f;
    startBtn->transform().width = 0.3f;
    startBtn->transform().height = 0.15f;
    startBtn->setTexture(&startButton_);
    startBtn->setLayer(2);
    startBtn->setInputManager(&inputManager_);
    startBtn->setScreenSize(1280, 720);  // TODO: 获取实际屏幕尺寸
    startBtn->setOnClick([this]() {
        startRequested_ = true;
    });
    startButtonUI_ = startBtn.get();
    addUIElement(std::move(startBtn));

    // 退出按钮
    auto exitBtn = std::make_unique<UIButton>();
    exitBtn->transform().x = 0.35f;
    exitBtn->transform().y = 0.65f;
    exitBtn->transform().width = 0.3f;
    exitBtn->transform().height = 0.15f;
    exitBtn->setTexture(&exitButton_);
    exitBtn->setLayer(2);
    exitBtn->setInputManager(&inputManager_);
    exitBtn->setScreenSize(1280, 720);  // TODO: 获取实际屏幕尺寸
    exitBtn->setOnClick([this]() {
        exitRequested_ = true;
    });
    exitButtonUI_ = exitBtn.get();
    addUIElement(std::move(exitBtn));
}

void MenuScene::tick(float dt) {
    // 更新InputManager（处理鼠标按键状态）
    inputManager_.updateMouseButtons(dt);

    // 调用父类tick（会更新UI元素）
    Scene::tick(dt);

    if (startRequested_ && manager_) {
        startRequested_ = false;
        manager_->transitionTo(std::make_unique<BattleScene>());
    }
}

void MenuScene::handleInput(float dt, const void *window) {
    (void) dt;

    // 获取鼠标位置
    const sf::Window* sfmlWindow = static_cast<const sf::Window*>(window);
    if (sfmlWindow) {
        sf::Vector2i mousePos = sf::Mouse::getPosition(*sfmlWindow);
        inputManager_.setMousePosition(mousePos.x, mousePos.y);
    }

    // 获取鼠标按钮状态
    bool leftButtonDown = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    inputManager_.setLeftButtonDown(leftButtonDown);

    // 保留键盘输入（用于快捷键）
    bool enterPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter);
    bool escPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape);

    if (enterPressed && !enterWasPressed_) {
        startRequested_ = true;
    }

    if (escPressed && !escWasPressed_) {
        exitRequested_ = true;
    }

    enterWasPressed_ = enterPressed;
    escWasPressed_ = escPressed;
}
