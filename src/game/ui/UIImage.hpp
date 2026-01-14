#pragma once
#include "UIElement.hpp"

// UIImage: 静态图片显示元素
class UIImage : public UIElement {
public:
    UIImage() = default;
    virtual ~UIImage() = default;

    // 无需额外逻辑，直接使用基类的render()
};
