#include "TrailEntity.hpp"
#include "game/runtime/WorldContext.hpp"
#include <algorithm>
#include <cmath>

using namespace DirectX;

void TrailEntity::initialize(ResourceManager *resourceMgr, const std::wstring &texturePath) {
    if (!resourceMgr) return;

    // 加载纹理
    if (!texturePath.empty()) {
        // 用户指定了纹理路径，尝试加载
        texture_ = resourceMgr->getTexture(texturePath);
        enableTexture = (texture_ != nullptr);
    } else {
        // 未指定纹理，使用默认渐变纹理
        texture_ = resourceMgr->getTrailGradientTexture();
        enableTexture = (texture_ != nullptr);
    }

    // Trail 不需要碰撞体（纯视觉效果）
    // 不需要设置 model（使用自定义渲染）
}

void TrailEntity::addPoint(const DirectX::XMFLOAT3 &position, float currentTime) {
    TrailPoint point;
    point.position = position;
    point.timestamp = currentTime;
    points_.push_back(point);

    // 限制队列长度（避免无限增长）
    const size_t maxPoints = 50;
    if (points_.size() > maxPoints) {
        points_.pop_front();
    }
}

void TrailEntity::update(WorldContext &ctx, float dt) {
    // 移除过期的点（基于生存时间）
    float currentTime = ctx.time;

    while (!points_.empty()) {
        const TrailPoint &oldest = points_.front();
        float age = currentTime - oldest.timestamp;

        if (age > lifetimePerPoint) {
            points_.pop_front();
        } else {
            break; // 队列按时间排序，后续点更新
        }
    }

    // 如果所有点都过期，自动销毁实体
    if (points_.empty()) {
        ctx.commands->destroyEntity(id());
    }
}

void TrailEntity::render(Renderer *renderer, float currentTime) {
    // 检查是否有足够的点来渲染
    if (points_.size() < static_cast<size_t>(minPointsToRender) || !renderer) {
        return;
    }

    // 生成 Ribbon mesh
    std::vector<Renderer::RibbonVertex> vertices;
    std::vector<uint16_t> indices;
    generateRibbon(vertices, indices, currentTime);

    if (vertices.empty() || indices.empty()) {
        return;
    }

    // 调用 Renderer 绘制 Ribbon
    renderer->drawRibbon(vertices, indices, enableTexture ? texture_ : nullptr, baseColor);
}

void TrailEntity::generateRibbon(std::vector<Renderer::RibbonVertex> &vertices,
                                 std::vector<uint16_t> &indices,
                                 float currentTime) {
    const size_t numPoints = points_.size();
    if (numPoints < 2) return;

    // 为每个点生成两个顶点（ribbon 的左右边）
    vertices.reserve(numPoints * 2);
    indices.reserve((numPoints - 1) * 6); // 每个段需要 2 个三角形 = 6 个索引

    // 计算每个点的垂直向量（用于生成 ribbon 宽度）
    // 策略：使用相邻点的方向计算垂直向量
    for (size_t i = 0; i < numPoints; ++i) {
        const DirectX::XMFLOAT3 &pos = points_[i].position;
        float age = currentTime - points_[i].timestamp;
        float t = age / lifetimePerPoint; // 归一化年龄 [0, 1]

        // 计算前向方向（用于确定 ribbon 方向）
        XMVECTOR forward;
        if (i == 0) {
            // 第一个点：使用到下一个点的方向
            forward = XMVectorSubtract(
                XMLoadFloat3(&points_[i + 1].position),
                XMLoadFloat3(&pos)
            );
        } else if (i == numPoints - 1) {
            // 最后一个点：使用从上一个点的方向
            forward = XMVectorSubtract(
                XMLoadFloat3(&pos),
                XMLoadFloat3(&points_[i - 1].position)
            );
        } else {
            // 中间点：使用平均方向
            XMVECTOR toNext = XMVectorSubtract(
                XMLoadFloat3(&points_[i + 1].position),
                XMLoadFloat3(&pos)
            );
            XMVECTOR fromPrev = XMVectorSubtract(
                XMLoadFloat3(&pos),
                XMLoadFloat3(&points_[i - 1].position)
            );
            forward = XMVectorAdd(toNext, fromPrev);
        }

        forward = XMVector3Normalize(forward);

        // 计算垂直向量（假设 Y 轴为 up）
        XMVECTOR up = XMVectorSet(0, 1, 0, 0);
        XMVECTOR right = XMVector3Normalize(XMVector3Cross(forward, up));

        // 如果 forward 和 up 平行，使用备用方案
        float dotProduct = XMVectorGetX(XMVector3Dot(forward, up));
        if (std::fabs(dotProduct) > 0.99f) {
            right = XMVectorSet(1, 0, 0, 0); // 使用 X 轴作为右方向
        }

        // 计算 ribbon 宽度的偏移
        XMVECTOR offset = XMVectorScale(right, trailWidth * 0.5f);

        // 计算 alpha（基于年龄淡出）
        float alpha = 1.0f - t;
        alpha = max(0.0f, min(1.0f, alpha));

        // 计算 UV 坐标
        float u = static_cast<float>(i) / static_cast<float>(numPoints - 1);

        // 生成左右两个顶点
        XMFLOAT3 leftPos, rightPos;
        XMStoreFloat3(&leftPos, XMVectorSubtract(XMLoadFloat3(&pos), offset));
        XMStoreFloat3(&rightPos, XMVectorAdd(XMLoadFloat3(&pos), offset));

        // 法线（简化：统一向上）
        XMFLOAT3 normal{0, 1, 0};

        // 顶点颜色（包含 alpha 淡出）
        XMFLOAT4 vertexColor = baseColor;
        vertexColor.w *= alpha;

        // 添加左侧顶点
        Renderer::RibbonVertex leftVert;
        leftVert.position = leftPos;
        leftVert.normal = normal;
        leftVert.color = vertexColor;
        leftVert.texCoord = XMFLOAT2{u, 0.0f};
        vertices.push_back(leftVert);

        // 添加右侧顶点
        Renderer::RibbonVertex rightVert;
        rightVert.position = rightPos;
        rightVert.normal = normal;
        rightVert.color = vertexColor;
        rightVert.texCoord = XMFLOAT2{u, 1.0f};
        vertices.push_back(rightVert);
    }

    // 生成索引（三角形带转换为三角形列表）
    for (size_t i = 0; i < numPoints - 1; ++i) {
        uint16_t baseIdx = static_cast<uint16_t>(i * 2);

        // 第一个三角形（左下→右下→左上）
        indices.push_back(baseIdx); // 左下
        indices.push_back(baseIdx + 1); // 右下
        indices.push_back(baseIdx + 2); // 左上

        // 第二个三角形（左上→右下→右上）
        indices.push_back(baseIdx + 2); // 左上
        indices.push_back(baseIdx + 1); // 右下
        indices.push_back(baseIdx + 3); // 右上
    }
}
