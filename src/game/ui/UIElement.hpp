#pragma once
#include <DirectXMath.h>
#include <functional>
#include "../../core/gfx/Texture.hpp"
#include "../../core/gfx/Renderer.hpp"

// 2D屏幕空间变换（归一化坐标：0-1）
struct Transform2D {
    float x = 0.0f;        // 屏幕空间X（0=左，1=右）
    float y = 0.0f;        // 屏幕空间Y（0=上，1=下）
    float width = 0.1f;    // 归一化宽度
    float height = 0.1f;   // 归一化高度
    float scaleX = 1.0f;   // 缩放（用于hover效果）
    float scaleY = 1.0f;
};

// UI混合模式
enum class UIBlendMode {
    Alpha,      // 标准alpha混合
    Additive,   // 叠加（发光效果）
    Multiply    // 正片叠底（变暗效果）
};

// UI元素基类（不继承IEntity，UI有独立的生命周期）
class UIElement {
public:
    virtual ~UIElement() = default;

    // 生命周期
    virtual void init() {}
    virtual void update(float dt) {}
    virtual void render(Renderer* renderer);

    // 交互检测（屏幕空间AABB）
    bool containsPoint(float screenX, float screenY) const;

    // 访问器
    Transform2D& transform() { return transform_; }
    const Transform2D& transform() const { return transform_; }

    void setLayer(int layer) { layer_ = layer; }
    int layer() const { return layer_; }

    void setTexture(Texture* tex) { texture_ = tex; }
    Texture* texture() const { return texture_; }

    void setTint(const DirectX::XMFLOAT4& tint) { tint_ = tint; }
    const DirectX::XMFLOAT4& tint() const { return tint_; }

    void setBlendMode(UIBlendMode mode) { blendMode_ = mode; }
    UIBlendMode blendMode() const { return blendMode_; }

    void setEmissive(float emissive) { emissive_ = emissive; }
    float emissive() const { return emissive_; }

    void setVisible(bool visible) { visible_ = visible; }
    bool visible() const { return visible_; }

protected:
    Transform2D transform_;
    int layer_ = 0;                                      // 渲染层级（越大越靠前）
    Texture* texture_ = nullptr;
    DirectX::XMFLOAT4 tint_{1.0f, 1.0f, 1.0f, 1.0f};   // 颜色调制
    UIBlendMode blendMode_ = UIBlendMode::Alpha;
    float emissive_ = 1.0f;                              // 发光强度（用于shader）
    bool visible_ = true;
};
