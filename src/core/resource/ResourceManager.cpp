#include "ResourceManager.hpp"

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

ID3D11ShaderResourceView *ResourceManager::getTexture(const std::wstring &path) {
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

void ResourceManager::preloadResources(const std::vector<std::wstring> &paths) {
    for (const auto &p : paths) {
        getModel(p);
        getTexture(p);
    }
}

void ResourceManager::cleanup() {
    models_.clear();
    textures_.clear();
}
