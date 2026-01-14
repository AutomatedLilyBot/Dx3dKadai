#pragma once
#include "Mesh.hpp"
#include <d3d11.h>

// 创建一个单位四边形 Mesh（用于 Billboard）
// 中心在原点，XY平面，法线朝向+Z，尺寸为 1x1
inline bool CreateQuadMesh(ID3D11Device *device, Mesh &outMesh, float size = 1.0f) {
    float halfSize = size * 0.5f;

    // 四个顶点：左下、左上、右下、右上
    Vertex vertices[] = {
        // pos                          normal              color                       uv
        {{-halfSize, -halfSize, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}, // 左下
        {{-halfSize, halfSize, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 左上
        {{halfSize, -halfSize, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}, // 右下
        {{halfSize, halfSize, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}} // 右上
    };

    // 两个三角形：逆时针绕序
    uint32_t indices[] = {
        0, 1, 2, // 第一个三角形
        2, 1, 3 // 第二个三角形
    };

    return outMesh.create(device, vertices, 4, indices, 6);
}