#include "ResourceManager.hpp"
#include "../gfx/Vertex.hpp"
#include <d3d11.h>

Model *ResourceManager::getModel(const std::wstring &path) {
    auto it = models_.find(path);
    if (it != models_.end()) {
        return it->second.get();
    }
    if (!device_) return nullptr;
    auto model = std::make_unique<Model>();
    if (!ModelLoader::LoadFBX(device_, path, *model)) {
        return nullptr;
    }
    Model *ptr = model.get();
    models_[path] = std::move(model);
    return ptr;
}

ID3D11ShaderResourceView *ResourceManager::getTextureSrv(const std::wstring &path) {
    auto it = textures_.find(path);
    if (it != textures_.end()) {
        return it->second->srv();
    }
    if (!device_) return nullptr;
    auto tex = std::make_unique<Texture>();
    if (!tex->loadFromFile(device_, path)) {
        return nullptr;
    }
    ID3D11ShaderResourceView *srv = tex->srv();
    textures_[path] = std::move(tex);
    return srv;
}


Texture *ResourceManager::getTexture(const std::wstring &path) {
    auto it = textures_.find(path);
    if (it != textures_.end()) {
        return it->second.get();
    }
    if (!device_) return nullptr;
    auto tex = std::make_unique<Texture>();
    if (!tex->loadFromFile(device_, path)) {
        return nullptr;
    }
    textures_[path] = std::move(tex);
    return textures_[path].get();  // 从 map 中获取
}


// Helper function to get lowercase file extension from a wstring path
static std::wstring getLowercaseExtension(const std::wstring& path) {
    size_t pos = path.find_last_of(L'.');
    if (pos == std::wstring::npos) return L"";
    std::wstring ext = path.substr(pos);
    // Convert to lowercase
    for (auto& c : ext) c = towlower(c);
    return ext;
}

void ResourceManager::preloadResources(const std::vector<std::wstring> &paths) {
    for (const auto &p : paths) {
        std::wstring ext = getLowercaseExtension(p);
        // Model extensions: .fbx, .obj
        if (ext == L".fbx" || ext == L".obj") {
            getModel(p);
        }
        // Texture extensions: .png, .jpg, .jpeg, .dds, .bmp, .tga
        else if (ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".dds" || ext == L".bmp" || ext == L".tga") {
            getTextureSrv(p);
        }
        // else: skip unknown extension
    }
}

void ResourceManager::cleanup() {
    models_.clear();
    textures_.clear();
    quadModel_.reset();
    trailGradientTexture_.reset();
}

// 获取或创建 Trail 默认渐变纹理（程序生成）
ID3D11ShaderResourceView *ResourceManager::getTrailGradientTexture() {
    if (trailGradientTexture_) {
        return trailGradientTexture_->srv();
    }

    if (!device_) return nullptr;

    // 创建一个简单的水平渐变纹理（中心亮，边缘暗，用于 Trail）
    const int width = 64;
    const int height = 8;
    std::vector<uint8_t> pixels(width * height * 4);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 4;

            // 计算到中心的距离（归一化）
            float centerY = (height - 1) * 0.5f;
            float distY = std::abs(static_cast<float>(y) - centerY) / centerY;

            // Alpha 渐变：中心（distY=0）→ 1.0，边缘（distY=1）→ 0.0
            float alpha = 1.0f - distY;
            float mini = min(1.0f, alpha);
            alpha = max(0.0f, mini);

            // 白色 RGB，alpha 变化
            pixels[idx + 0] = 255; // R
            pixels[idx + 1] = 255; // G
            pixels[idx + 2] = 255; // B
            pixels[idx + 3] = static_cast<uint8_t>(alpha * 255); // A
        }
    }

    trailGradientTexture_ = std::make_unique<Texture>();
    if (!trailGradientTexture_->createFromRGBA(device_, pixels.data(), width, height)) {
        trailGradientTexture_.reset();
        return nullptr;
    }

    return trailGradientTexture_->srv();
}

// 获取或创建四边形模型（用于 Billboard）
Model *ResourceManager::getQuadModel() {
    if (quadModel_) {
        return quadModel_.get();
    }

    if (!device_) return nullptr;

    // 创建一个单位四边形（中心在原点，XY平面，法线朝向+Z）
    quadModel_ = std::make_unique<Model>();

    float halfSize = 0.5f;

    // 使用 VertexPNCT 格式的向量
    std::vector<VertexPNCT> vertices = {
        // pos                          normal              color                       uv
        {{-halfSize, -halfSize, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}, // 左下
        {{-halfSize, halfSize, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 左上
        {{halfSize, -halfSize, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}, // 右下
        {{halfSize, halfSize, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}} // 右上
    };

    std::vector<uint16_t> indices = {
        0, 1, 2, // 第一个三角形
        2, 1, 3 // 第二个三角形
    };

    // 创建 Mesh
    Mesh quadMesh;
    if (!quadMesh.createFromPNCT(device_, vertices, indices)) {
        quadModel_.reset();
        return nullptr;
    }

    // 创建 MeshGPU
    Model::MeshGPU meshGPU;
    meshGPU.mesh = std::move(quadMesh);
    meshGPU.materialIndex = -1; // 无材质

    quadModel_->meshes.push_back(std::move(meshGPU));

    // 创建 DrawItem
    Model::DrawItem drawItem;
    drawItem.meshIndex = 0;
    drawItem.nodeGlobal = DirectX::XMFLOAT4X4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    ); // 单位矩阵

    quadModel_->drawItems.push_back(drawItem);

    return quadModel_.get();
}

// 创建带纹理的四边形模型（每次创建新实例，用于 Billboard）
std::unique_ptr<Model> ResourceManager::createQuadModelWithTexture(const std::wstring &texturePath) {
    if (!device_) return nullptr;

    auto model = std::make_unique<Model>();

    float halfSize = 0.5f;

    // 使用 VertexPNCT 格式的向量
    std::vector<VertexPNCT> vertices = {
        {{-halfSize, -halfSize, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
        {{-halfSize, halfSize, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
        {{halfSize, -halfSize, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{halfSize, halfSize, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}}
    };

    std::vector<uint16_t> indices = {
        0, 1, 2,
        2, 1, 3
    };

    // 创建 Mesh
    Mesh quadMesh;
    if (!quadMesh.createFromPNCT(device_, vertices, indices)) {
        return nullptr;
    }

    // 创建 MeshGPU
    Model::MeshGPU meshGPU;
    meshGPU.mesh = std::move(quadMesh);
    meshGPU.materialIndex = 0; // 使用第一个材质

    model->meshes.push_back(std::move(meshGPU));

    // 加载纹理并创建材质
    Model::Material mat;
    auto tex = textures_.find(texturePath);
    if (tex != textures_.end() && tex->second) {
        // 纹理已缓存，直接使用
        mat.diffuse = *(tex->second);
    } else {
        // 尝试加载新纹理
        Texture newTex;
        if (newTex.loadFromFile(device_, texturePath)) {
            mat.diffuse = std::move(newTex);
            // 可选：缓存这个纹理
            textures_[texturePath] = std::make_unique<Texture>(std::move(mat.diffuse));
        }
        // 如果加载失败，diffuse 会是空的，Renderer 会使用默认纹理
    }

    model->materials.push_back(std::move(mat));

    // 创建 DrawItem
    Model::DrawItem drawItem;
    drawItem.meshIndex = 0;
    drawItem.nodeGlobal = DirectX::XMFLOAT4X4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    );

    model->drawItems.push_back(drawItem);

    return model;
}

std::unique_ptr<Model> ResourceManager::createGroundQuadModelWithTexture(const std::wstring &texturePath) {
    if (!device_) return nullptr;

    auto model = std::make_unique<Model>();

    float halfSize = 0.5f;

    // 使用 VertexPNCT 格式的向量
    std::vector<VertexPNCT> vertices = {
        {{-halfSize, 0.0f, -halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
        {{-halfSize, 0.0f, halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
        {{halfSize, 0.0f, -halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{halfSize, 0.0f, halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}}
    };
    std::vector<uint16_t> indices = {
        0, 1, 2,
        2, 1, 3
    };

    // 创建 Mesh
    Mesh quadMesh;
    if (!quadMesh.createFromPNCT(device_, vertices, indices)) {
        return nullptr;
    }

    // 创建 MeshGPU
    Model::MeshGPU meshGPU;
    meshGPU.mesh = std::move(quadMesh);
    meshGPU.materialIndex = 0; // 使用第一个材质

    model->meshes.push_back(std::move(meshGPU));

    // 加载纹理并创建材质
    Model::Material mat;
    auto tex = textures_.find(texturePath);
    if (tex != textures_.end() && tex->second) {
        // 纹理已缓存，直接使用
        mat.diffuse = *(tex->second);
    } else {
        // 尝试加载新纹理
        Texture newTex;
        if (newTex.loadFromFile(device_, texturePath)) {
            mat.diffuse = std::move(newTex);
            // 可选：缓存这个纹理
            textures_[texturePath] = std::make_unique<Texture>(std::move(mat.diffuse));
        }
        // 如果加载失败，diffuse 会是空的，Renderer 会使用默认纹理
    }

    model->materials.push_back(std::move(mat));

    // 创建 DrawItem
    Model::DrawItem drawItem;
    drawItem.meshIndex = 0;
    drawItem.nodeGlobal = DirectX::XMFLOAT4X4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    );

    model->drawItems.push_back(drawItem);

    return model;
}
