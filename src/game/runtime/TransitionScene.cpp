#include "TransitionScene.hpp"
#include <windows.h>

static std::wstring ExeDirTransition() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf);
    auto p = s.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? L"." : s.substr(0, p);
}

TransitionScene::TransitionScene(std::unique_ptr<Scene> target, float waitSeconds)
    : targetScene_(std::move(target)), waitTime_(waitSeconds) {}

void TransitionScene::init(Renderer *renderer) {
    renderer_ = renderer;
    resourceManager_.setDevice(renderer ? renderer->device() : nullptr);
    std::wstring loadingPath = ExeDirTransition() + L"\\asset\\loading.png";
    loadingTexture_ = resourceManager_.getTexture(loadingPath);
}

void TransitionScene::onUpdate(WorldContext &, float dt) {
    elapsed_ += dt;
    if (elapsed_ >= waitTime_) {
        readyForNext_ = true;
    }
}

void TransitionScene::render() {
    if (!renderer_) return;
    const Texture *tex = loadingTexture_;
    if (!tex) {
        tex = &renderer_->defaultTexture();
    }
    renderer_->drawFullscreenQuad(*tex);
}

std::unique_ptr<Scene> TransitionScene::takeNextScene() {
    readyForNext_ = false;
    return std::move(targetScene_);
}
