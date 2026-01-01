#pragma once
#include "UIImage.hpp"

// UINumberDisplay: 使用数字图集和UV滚动显示数字
// 图集格式：0-9横向排列，每个数字占1/10宽度
class UINumberDisplay : public UIImage {
public:
    UINumberDisplay() = default;
    virtual ~UINumberDisplay() = default;

    void render(Renderer* renderer) override;

    // 设置显示的数字（0-9）
    void setNumber(int number) {
        number_ = number < 0 ? 0 : (number > 9 ? 9 : number);
    }

    int number() const { return number_; }

private:
    int number_ = 0;
};
