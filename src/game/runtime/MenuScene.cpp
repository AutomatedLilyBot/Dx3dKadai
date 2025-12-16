#include "MenuScene.hpp"
#include "SceneManager.hpp"
#include "TransitionScene.hpp"
#include "BattleScene.hpp"

#include <SFML/Window/Keyboard.hpp>
#include <windows.h>

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

    tryLoad(background_, L"menu_background.png", DirectX::XMFLOAT4{0.1f, 0.1f, 0.15f, 1.0f});
    tryLoad(title_, L"menu_title.png", DirectX::XMFLOAT4{0.8f, 0.8f, 0.8f, 1.0f});
    tryLoad(startButton_, L"menu_start.png", DirectX::XMFLOAT4{0.2f, 0.5f, 0.2f, 1.0f});
    tryLoad(exitButton_, L"menu_exit.png", DirectX::XMFLOAT4{0.5f, 0.2f, 0.2f, 1.0f});
}

void MenuScene::tick(float dt) {
    (void) dt;
    if (startRequested_ && manager_) {
        startRequested_ = false;
        manager_->transitionTo(std::make_unique<BattleScene>());
    }
}

void MenuScene::handleInput(float dt, const void *window) {
    (void) dt;
    (void) window;
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

void MenuScene::render() {
    if (!renderer_) return;

    renderer_->drawUiQuad(0.0f, 0.0f, 1.0f, 1.0f, background_.srv(), DirectX::XMFLOAT4{1, 1, 1, 1});
    renderer_->drawUiQuad(0.25f, 0.1f, 0.5f, 0.2f, title_.srv(), DirectX::XMFLOAT4{1, 1, 1, 1});
    renderer_->drawUiQuad(0.35f, 0.45f, 0.3f, 0.15f, startButton_.srv(), DirectX::XMFLOAT4{1, 1, 1, 1});
    renderer_->drawUiQuad(0.35f, 0.65f, 0.3f, 0.15f, exitButton_.srv(), DirectX::XMFLOAT4{1, 1, 1, 1});
}
