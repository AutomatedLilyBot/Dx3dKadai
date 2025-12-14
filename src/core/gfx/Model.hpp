#pragma once
#include <vector>
#include <DirectXMath.h>
#include "../../../src/core/gfx/Mesh.hpp"
#include "Texture.hpp"

// Aggregated model consisting of multiple meshes, materials, and per-node draw items
struct Model {
    struct Material {
        Texture diffuse; // diffuse texture only for now
    };

    struct MeshGPU {
        Mesh mesh; // geometry buffers
        int materialIndex = -1; // index into materials
    };

    struct DrawItem {
        uint32_t meshIndex = 0; // index into meshes
        DirectX::XMFLOAT4X4 nodeGlobal{}; // accumulated node transform (relative to model origin)
    };

    std::vector<Material> materials;
    std::vector<MeshGPU> meshes;
    std::vector<DrawItem> drawItems;

    bool empty() const { return meshes.empty() || drawItems.empty(); }
};
