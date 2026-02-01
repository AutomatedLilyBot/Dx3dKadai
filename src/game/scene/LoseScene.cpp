#include "LoseScene.hpp"

#include <string>

#include "../runtime/SceneManager.hpp"
#include "TransitionScene.hpp"
#include "MenuScene.hpp"

#include <SFML/Window/Mouse.hpp>
#include <windows.h>
#include <SFML/Window/Window.hpp>

#include "core/gfx/Renderer.hpp"
#include "../ui/UIImage.hpp"
#include "../ui/UIButton.hpp"

static std::wstring ExeDirLose() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf);
    auto p = s.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? L"." : s.substr(0, p);
}

void LoseScene::init(Renderer *renderer) {
    renderer_ = renderer;
    if (!renderer_ || !renderer_->device()) return;

    auto tryLoad = [&](Texture &tex, const std::wstring &name, const DirectX::XMFLOAT4 &color) {
        std::wstring path = ExeDirLose() + L"\\asset\\" + name;
        if (!tex.loadFromFile(renderer_->device(), path)) {
            tex.createSolidColor(renderer_->device(),
                                 static_cast<unsigned char>(color.x * 255.0f),
                                 static_cast<unsigned char>(color.y * 255.0f),
                                 static_cast<unsigned char>(color.z * 255.0f),
                                 255);
        }
    };

    tryLoad(background_, L"lose_background.png", DirectX::XMFLOAT4{1.0f, 1.0f, 1.0f, 1.0f});
    tryLoad(returnButton_, L"menu_return.png", DirectX::XMFLOAT4{0.7f, 0.7f, 0.7f, 1.0f});

    // 背景图片
    auto bgImage = std::make_unique<UIImage>();
    bgImage->transform().x = 0.0f;
    bgImage->transform().y = 0.0f;
    bgImage->transform().width = 1.0f;
    bgImage->transform().height = 1.0f;
    bgImage->setTexture(&background_);
    bgImage->setLayer(0);
    addUIElement(std::move(bgImage));

    // 返回主菜单按钮
    auto returnBtn = std::make_unique<UIButton>();
    returnBtn->transform().x = 0.35f;
    returnBtn->transform().y = 0.7f;
    returnBtn->transform().width = 0.3f;
    returnBtn->transform().height = 0.15f;
    returnBtn->setTexture(&returnButton_);
    returnBtn->setLayer(1);
    returnBtn->setInputManager(&inputManager_);
    returnBtn->setScreenSize(1280, 720);  // TODO: 获取实际屏幕尺寸
    returnBtn->setOnClick([this]() {
        returnRequested_ = true;
    });
    returnButtonUI_ = returnBtn.get();
    addUIElement(std::move(returnBtn));
}

void LoseScene::tick(float dt) {
    // 更新InputManager（处理鼠标按键状态）
    inputManager_.updateMouseButtons(dt);

    // 调用父类tick（会更新UI元素）
    Scene::tick(dt);

    if (returnRequested_ && manager_) {
        returnRequested_ = false;
        manager_->transitionTo(std::make_unique<MenuScene>());
    }
}

void LoseScene::handleInput(float dt, const void *window) {
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
}
