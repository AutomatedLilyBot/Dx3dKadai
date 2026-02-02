#include "HelpScene.hpp"

#include <string>

#include "../runtime/SceneManager.hpp"
#include "MenuScene.hpp"

#include <SFML/Window/Keyboard.hpp>
#include <windows.h>

#include "core/gfx/Renderer.hpp"
#include "../ui/UIImage.hpp"

static std::wstring ExeDirHelp() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf);
    auto p = s.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? L"." : s.substr(0, p);
}

void HelpScene::init(Renderer *renderer) {
    renderer_ = renderer;
    if (!renderer_ || !renderer_->device()) return;

    auto tryLoad = [&](Texture &tex, const std::wstring &name, const DirectX::XMFLOAT4 &color) {
        std::wstring path = ExeDirHelp() + L"\\asset\\" + name;
        if (!tex.loadFromFile(renderer_->device(), path)) {
            tex.createSolidColor(renderer_->device(),
                                 static_cast<unsigned char>(color.x * 255.0f),
                                 static_cast<unsigned char>(color.y * 255.0f),
                                 static_cast<unsigned char>(color.z * 255.0f),
                                 255);
        }
    };

    tryLoad(helpImage_, L"help_content.png", DirectX::XMFLOAT4{1.0f, 1.0f, 1.0f, 1.0f});

    // 帮助内容图片（全屏）
    auto helpImg = std::make_unique<UIImage>();
    helpImg->transform().x = 0.0f;
    helpImg->transform().y = 0.0f;
    helpImg->transform().width = 1.0f;
    helpImg->transform().height = 1.0f;
    helpImg->setTexture(&helpImage_);
    helpImg->setLayer(0);
    addUIElement(std::move(helpImg));
}

void HelpScene::tick(float dt) {
    // 调用父类tick（会更新UI元素）
    Scene::tick(dt);

    if (returnRequested_ && manager_) {
        returnRequested_ = false;
        manager_->transitionTo(std::make_unique<MenuScene>());
    }
}

void HelpScene::handleInput(float dt, const void *window) {
    (void) dt;
    (void) window;

    // ESC键返回主菜单
    bool escPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape);
    if (escPressed && !escWasPressed_) {
        returnRequested_ = true;
    }
    escWasPressed_ = escPressed;
}
