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
    explicit ResourceManager(ID3D11Device *device = nullptr) : device_(device) {}

    void setDevice(ID3D11Device *device) { device_ = device; }

    Model *getModel(const std::wstring &path);
    ID3D11ShaderResourceView *getTexture(const std::wstring &path);

    void preloadResources(const std::vector<std::wstring> &paths);
    void cleanup();

private:
    ID3D11Device *device_ = nullptr;
    std::unordered_map<std::wstring, std::unique_ptr<Model>> models_;
    std::unordered_map<std::wstring, std::unique_ptr<Texture>> textures_;
};
