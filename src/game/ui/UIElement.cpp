#include "UIElement.hpp"

void UIElement::render(Renderer* renderer) {
    if (!visible_ || !texture_) return;

    // 计算应用缩放后的实际尺寸和位置
    float finalWidth = transform_.width * transform_.scaleX;
    float finalHeight = transform_.height * transform_.scaleY;

    // 缩放时保持中心不变
    float offsetX = (transform_.width - finalWidth) * 0.5f;
    float offsetY = (transform_.height - finalHeight) * 0.5f;
    float finalX = transform_.x + offsetX;
    float finalY = transform_.y + offsetY;

    // 调用Renderer的UI绘制接口（传递emissive、UV偏移和UV缩放）
    // 默认uvScale=1.0表示显示整张纹理
    renderer->drawUiQuad(finalX, finalY, finalWidth, finalHeight,
                         texture_->srv(), tint_, emissive_, 0.0f, 0.0f, 1.0f, 1.0f);
}

bool UIElement::containsPoint(float screenX, float screenY) const {
    // screenX, screenY 是归一化坐标（0-1范围）
    float finalWidth = transform_.width * transform_.scaleX;
    float finalHeight = transform_.height * transform_.scaleY;
    float offsetX = (transform_.width - finalWidth) * 0.5f;
    float offsetY = (transform_.height - finalHeight) * 0.5f;
    float finalX = transform_.x + offsetX;
    float finalY = transform_.y + offsetY;

    return screenX >= finalX && screenX <= (finalX + finalWidth) &&
           screenY >= finalY && screenY <= (finalY + finalHeight);
}
