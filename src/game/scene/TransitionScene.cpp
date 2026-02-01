#include "TransitionScene.hpp"
#include "../runtime/SceneManager.hpp"

#include <windows.h>

#include "game/ui/UIImage.hpp"

static std::wstring ExeDirTransition() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf);
    auto p = s.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? L"." : s.substr(0, p);
}

TransitionScene::TransitionScene(std::unique_ptr<Scene> next)
    : nextScene_(std::move(next)) {}

void TransitionScene::init(Renderer *renderer) {
    renderer_ = renderer;
    if (renderer_ && renderer_->device()) {
        std::wstring loadingpath = ExeDirTransition() + L"\\asset\\loading.png";
        std::wstring bgpath = ExeDirTransition() + L"\\asset\\menu_background.png";
        if (!loadingTexture_.loadFromFile(renderer_->device(), loadingpath)) {
            loadingTexture_.createSolidColor(renderer_->device(), 255, 255, 255, 255);
        }
        if (!bgTexture_.loadFromFile(renderer_->device(), bgpath)) {
            bgTexture_.createSolidColor(renderer_->device(), 255, 255, 255, 255);
        }
    }
    auto bgImage = std::make_unique<UIImage>();
    bgImage->transform().x = 0.0f;
    bgImage->transform().y = 0.0f;
    bgImage->transform().width = 1.0f;
    bgImage->transform().height = 1.0f;
    bgImage->setTexture(&bgTexture_);
    bgImage->setLayer(0);
    addUIElement(std::move(bgImage));

    auto loadingImage = std::make_unique<UIImage>();
    loadingImage->transform().x = 0.25f;
    loadingImage->transform().y = 0.4f;
    loadingImage->transform().width = 0.5f;
    loadingImage->transform().height = 0.2f;
    loadingImage->setTexture(&loadingTexture_);
    loadingImage->setLayer(1);
    addUIElement(std::move(loadingImage));


}

void TransitionScene::tick(float dt) {
    timer_ += dt;
    if (timer_ >= duration_ && manager_ && nextScene_) {
        manager_->finishTransition(std::move(nextScene_));
    }
}

void TransitionScene::render() {
    Scene::render();
}
