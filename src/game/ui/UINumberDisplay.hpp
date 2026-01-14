#pragma once
#include "UIImage.hpp"

// UINumberDisplay: 使用数字图集和UV滚动显示多位数字
// 图集格式：5×3布局，索引0-9为数字，10为小数点，11为减号
// 支持显示范围：0-99.99（最多2位整数+2位小数）
class UINumberDisplay : public UIImage {
public:
    UINumberDisplay() = default;
    virtual ~UINumberDisplay() = default;

    void render(Renderer* renderer) override;

    // 设置数值（范围：0-99.99）
    void setValue(float value);

    // 设置NaN状态（显示单个"-"）
    void setNaN();

    // 访问器
    float value() const { return value_; }
    bool isNaN() const { return isNaN_; }

    // 配置单个数字显示尺寸
    void setDigitWidth(float width) { digitWidth_ = width; }
    void setDigitSpacing(float spacing) { digitSpacing_ = spacing; }

private:
    // 渲染单个数字（使用UV偏移）
    void renderDigit(Renderer* renderer, int spriteIndex, float x, float y);

    float value_ = 0.0f;           // 实际数值
    bool isNaN_ = false;           // NaN状态标志
    float digitWidth_ = 0.02f;     // 单个数字宽度（归一化坐标）
    float digitSpacing_ = 0.005f;  // 数字间距
};
