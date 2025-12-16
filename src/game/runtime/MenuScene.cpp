#include "MenuScene.hpp"
#include <windows.h>

using namespace DirectX;

static std::wstring ExeDirMenu() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf);
    auto p = s.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? L"." : s.substr(0, p);
}

void MenuScene::init(Renderer *renderer) {
    renderer_ = renderer;
    resourceManager_.setDevice(renderer ? renderer->device() : nullptr);

    std::wstring bgPath = ExeDirMenu() + L"\\asset\\menu_background.png";
    backgroundTex_ = resourceManager_.getTexture(bgPath);
}

void MenuScene::render() {
    if (!renderer_) return;
    const Texture *tex = backgroundTex_;
    if (!tex) {
        tex = &renderer_->defaultTexture();
    }
    renderer_->drawFullscreenQuad(*tex);
}
