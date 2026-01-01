#include "UINumberDisplay.hpp"

void UINumberDisplay::render(Renderer* renderer) {
    if (!visible_ || !texture_) return;

    // 计算应用缩放后的实际尺寸和位置
    float finalWidth = transform_.width * transform_.scaleX;
    float finalHeight = transform_.height * transform_.scaleY;

    // 缩放时保持中心不变
    float offsetX = (transform_.width - finalWidth) * 0.5f;
    float offsetY = (transform_.height - finalHeight) * 0.5f;
    float finalX = transform_.x + offsetX;
    float finalY = transform_.y + offsetY;

    // 计算UV偏移（假设数字图集是0-9横向排列）
    // 每个数字占1/10的宽度
    float uvOffsetX = static_cast<float>(number_) * 0.1f;

    // 调用Renderer的UI绘制接口（使用UV偏移实现数字滚动）
    renderer->drawUiQuad(finalX, finalY, finalWidth, finalHeight,
                         texture_->srv(), tint_, emissive_, uvOffsetX, 0.0f);
}
