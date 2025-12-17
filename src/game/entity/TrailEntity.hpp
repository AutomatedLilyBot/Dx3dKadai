#pragma once
#include "StaticEntity.hpp"
#include "../../core/gfx/Renderer.hpp"
#include "../../core/resource/ResourceManager.hpp"
#include <deque>
#include <vector>
#include <DirectXMath.h>

// Trail 历史点结构
struct TrailPoint {
    DirectX::XMFLOAT3 position;
    float timestamp; // 创建时间（用于计算淡出）
};

// Trail 实体：用于渲染子弹拖尾效果（Ribbon 方式）
// 特点：
// - 不需要物理碰撞（继承 StaticEntity）
// - 动态生成 mesh（每帧从历史点构建 ribbon）
// - 支持纹理（彩色 trail 或图案）
// - 自动淡出（老点逐渐消失）
class TrailEntity : public StaticEntity {
public:
    TrailEntity() = default;

    // 初始化（可选加载纹理）
    void initialize(ResourceManager *resourceMgr, const std::wstring &texturePath = L"");

    // 添加新的拖尾点（通常在每帧由 BulletEntity 调用）
    void addPoint(const DirectX::XMFLOAT3 &position, float currentTime);

    // 更新逻辑：移除过期的点
    void update(WorldContext &ctx, float dt) override;

    // 自定义渲染（不通过 IDrawable，直接调用 Renderer::drawRibbon）
    void render(Renderer *renderer, float currentTime);

    // 配置参数
    float trailWidth = 0.15f; // 拖尾宽度（世界空间单位）
    float lifetimePerPoint = 0.8f; // 每个点的生存时间（秒）
    DirectX::XMFLOAT4 baseColor{1.0f, 0.6f, 0.2f, 0.8f}; // 基础颜色（橙色半透明）
    bool enableTexture = false; // 是否使用纹理
    int minPointsToRender = 2; // 至少需要多少个点才渲染

    // 判断拖尾是否应该被销毁（所有点都过期）
    bool shouldDestroy() const { return points_.empty(); }

private:
    std::deque<TrailPoint> points_; // 历史位置队列
    ID3D11ShaderResourceView *texture_ = nullptr; // 可选纹理

    // Ribbon 生成算法：从历史点生成四边形带
    void generateRibbon(std::vector<Renderer::RibbonVertex> &vertices,
                        std::vector<uint16_t> &indices,
                        float currentTime);
};