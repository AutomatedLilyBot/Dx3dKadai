#include "UINumberDisplay.hpp"
#include <algorithm>
#include <cmath>

void UINumberDisplay::setValue(float value) {
    isNaN_ = false;
    // 限制范围：0-99.99
    value_ = max(0.0f, min(99.99f, value));
}

void UINumberDisplay::setNaN() {
    isNaN_ = true;
}

void UINumberDisplay::render(Renderer* renderer) {
    if (!visible_ || !texture_) return;

    // 1. NaN状态：只渲染单个"-"
    if (isNaN_) {
        renderDigit(renderer, 11, transform_.x, transform_.y);
        return;
    }

    // 2. 数值格式化：分离整数和小数部分
    int intPart = static_cast<int>(value_);
    int fracPart = static_cast<int>((value_ - intPart) * 100.0f + 0.5f); // 四舍五入到2位小数

    float currentX = transform_.x;

    // 3. 渲染整数部分
    if (intPart == 0 && value_ < 1.0f) {
        // <1的情况：渲染"0"
        renderDigit(renderer, 0, currentX, transform_.y);
        currentX += digitWidth_ + digitSpacing_;
    } else {
        // 提取十位和个位
        if (intPart >= 10) {
            int tens = intPart / 10;
            renderDigit(renderer, tens, currentX, transform_.y);
            currentX += digitWidth_ + digitSpacing_;
        }
        int ones = intPart % 10;
        renderDigit(renderer, ones, currentX, transform_.y);
        currentX += digitWidth_ + digitSpacing_;
    }

    // 4. 渲染小数部分（如果有）
    if (fracPart > 0) {
        // 渲染小数点
        renderDigit(renderer, 10, currentX, transform_.y);
        currentX += digitWidth_ * 0.5f + digitSpacing_; // 小数点占半宽

        // 渲染小数位
        int frac1 = fracPart / 10;
        int frac2 = fracPart % 10;
        renderDigit(renderer, frac1, currentX, transform_.y);
        currentX += digitWidth_ + digitSpacing_;
        renderDigit(renderer, frac2, currentX, transform_.y);
    }
}

void UINumberDisplay::renderDigit(Renderer* renderer, int spriteIndex, float x, float y) {
    // 计算UV偏移和缩放（5×3布局）
    // 每个sprite占用的UV空间：宽度=0.2 (1/5), 高度=0.333 (1/3)
    float uvOffsetX = (spriteIndex % 5) * 0.2f;
    float uvOffsetY = (spriteIndex / 5) * 0.333333f;
    float uvScaleX = 0.2f;      // 1/5 宽度
    float uvScaleY = 0.333333f; // 1/3 高度

    // 应用缩放
    float finalWidth = digitWidth_ * transform_.scaleX;
    float finalHeight = transform_.height * transform_.scaleY;
    float offsetY = (transform_.height - finalHeight) * 0.5f;

    renderer->drawUiQuad(x, y + offsetY, finalWidth, finalHeight,
                         texture_->srv(), tint_, emissive_,
                         uvOffsetX, uvOffsetY, uvScaleX, uvScaleY);
}
