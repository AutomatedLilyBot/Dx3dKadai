#include "TransitionScene.hpp"
#include "SceneManager.hpp"

#include <windows.h>

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
        std::wstring path = ExeDirTransition() + L"\\asset\\loading.png";
        if (!loadingTexture_.loadFromFile(renderer_->device(), path)) {
            loadingTexture_.createSolidColor(renderer_->device(), 30, 30, 30, 255);
        }
    }
}

void TransitionScene::tick(float dt) {
    timer_ += dt;
    if (timer_ >= duration_ && manager_ && nextScene_) {
        manager_->finishTransition(std::move(nextScene_));
    }
}

void TransitionScene::render() {
    if (!renderer_) return;
    renderer_->drawUiQuad(0.25f, 0.4f, 0.5f, 0.2f,
                          loadingTexture_.srv(), DirectX::XMFLOAT4{1, 1, 1, 1});
}
