#pragma once
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <wrl/client.h>
#include "../gfx/Model.hpp"
#include "../gfx/ModelLoader.hpp"
#include "../gfx/Texture.hpp"

// ResourceManager: simple cache for models and textures
class ResourceManager {
public:
    explicit ResourceManager(ID3D11Device *device = nullptr) : device_(device) {
    }

    void setDevice(ID3D11Device *device) { device_ = device; }

    Model *getModel(const std::wstring &path);

    ID3D11ShaderResourceView *getTextureSrv(const std::wstring &path);

    Texture *getTexture(const std::wstring &path);

    // 获取或创建四边形模型（用于 Billboard）
    Model *getQuadModel();

    // 创建带纹理的四边形模型（每次创建新实例）
    std::unique_ptr<Model> createQuadModelWithTexture(const std::wstring &texturePath);
    // 创建地面 quad（用于地面指示器）
    std::unique_ptr<Model> createGroundQuadModelWithTexture(const std::wstring& texturePath);

    // 获取或创建 Trail 默认渐变纹理（程序生成）
    ID3D11ShaderResourceView *getTrailGradientTexture();

    void preloadResources(const std::vector<std::wstring> &paths);

    void cleanup();

private:
    ID3D11Device *device_ = nullptr;
    std::unordered_map<std::wstring, std::unique_ptr<Model> > models_;
    std::unordered_map<std::wstring, std::unique_ptr<Texture> > textures_;

    // 缓存的四边形模型（延迟初始化）
    std::unique_ptr<Model> quadModel_;

    // 缓存的 Trail 渐变纹理（延迟初始化）
    std::unique_ptr<Texture> trailGradientTexture_;
};