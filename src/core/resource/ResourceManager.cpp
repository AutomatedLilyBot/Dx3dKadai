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
            getTexture(p);
        }
        // else: skip unknown extension
    }
}

void ResourceManager::cleanup() {
    models_.clear();
    textures_.clear();
}
