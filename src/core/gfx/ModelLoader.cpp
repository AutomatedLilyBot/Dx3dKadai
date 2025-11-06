#include "core/gfx/ModelLoader.hpp"

#include <vector>
#include <string>
#include <cassert>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef min
#undef max
#include <algorithm>

// Assimp (headers are placed directly under external/assimp)
#include <Importer.hpp>
#include <scene.h>
#include <postprocess.h>

using namespace DirectX;

static std::string WStringToUTF8(const std::wstring &ws) {
    if (ws.empty()) return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int) ws.size(), nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int) ws.size(), &strTo[0], size_needed, nullptr, nullptr);
    return strTo;
}

static std::wstring UTF8ToWString(const std::string &s) {
    if (s.empty()) return {};
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int) s.size(), nullptr, 0);
    std::wstring ws(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int) s.size(), &ws[0], size_needed);
    return ws;
}

static std::wstring GetDirectoryOf(const std::wstring &path) {
    auto pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return L".";
    return path.substr(0, pos);
}

static std::wstring CombinePath(const std::wstring &dir, const std::wstring &rel) {
    if (rel.empty()) return dir;
    if (!rel.empty() && (rel[0] == L'/' || rel[0] == L'\\')) return rel; // already absolute (drive not handled)
    if (!dir.empty() && (dir.back() == L'/' || dir.back() == L'\\')) return dir + rel;
    return dir + L"\\" + rel;
}

bool ModelLoader::LoadFBX(ID3D11Device *device,
                          const std::wstring &filepath,
                          Mesh &outMesh,
                          Texture &outTexture) {
    if (!device) return false;

    Assimp::Importer importer;
    unsigned int flags = 0
                         | aiProcess_Triangulate
                         | aiProcess_JoinIdenticalVertices
                         | aiProcess_GenSmoothNormals // Changed from GenNormals for consistent smooth normals
                         | aiProcess_ImproveCacheLocality
                         | aiProcess_SortByPType
                         | aiProcess_ConvertToLeftHanded
                         | aiProcess_FlipUVs
                         | aiProcess_FixInfacingNormals; // Fix inverted normals

    const std::string path8 = WStringToUTF8(filepath);
    const aiScene *scene = importer.ReadFile(path8.c_str(), flags);
    if (!scene || !scene->HasMeshes()) {
        return false;
    }

    // For simplicity, only load the first mesh
    const aiMesh *mesh = scene->mMeshes[0];

    std::vector<VertexPNCT> vertices;
    vertices.reserve(mesh->mNumVertices);

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        VertexPNCT v{};
        aiVector3D p = mesh->mVertices[i];
        v.pos = XMFLOAT3(p.x, p.y, p.z);

        aiVector3D n = mesh->HasNormals() ? mesh->mNormals[i] : aiVector3D(0, 1, 0);
        v.normal = XMFLOAT3(n.x, n.y, n.z);

        if (mesh->HasTextureCoords(0)) {
            aiVector3D t = mesh->mTextureCoords[0][i];
            v.uv = XMFLOAT2(t.x, t.y);
        } else {
            v.uv = XMFLOAT2(0, 0);
        }
        // default white color
        v.col = XMFLOAT4(1, 1, 1, 1);

        vertices.push_back(v);
    }

    // Indices (16-bit). Ensure count fits.
    std::vector<uint16_t> indices;
    indices.reserve(mesh->mNumFaces * 3);
    for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
        const aiFace &face = mesh->mFaces[f];
        if (face.mNumIndices != 3) continue; // skip non-triangles (should be triangulated)
        for (unsigned int k = 0; k < 3; ++k) {
            unsigned int idx = face.mIndices[k];
            if (idx > 0xFFFF) return false; // too large for uint16
            indices.push_back(static_cast<uint16_t>(idx));
        }
    }

    if (indices.empty() || vertices.empty()) return false;

    if (!outMesh.createFromPNCT(device, vertices, indices)) {
        return false;
    }

    // Try to load diffuse texture from material
    bool textureLoaded = false;
    if (scene->HasMaterials() && mesh->mMaterialIndex < scene->mNumMaterials) {
        const aiMaterial *mat = scene->mMaterials[mesh->mMaterialIndex];
        aiString texPath;
        if (AI_SUCCESS == mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath)) {
            // Handle embedded or external
            if (texPath.length > 0) {
                const aiTexture *embedded = nullptr;
                if (texPath.data[0] == '*') {
                    // Embedded index like "*0" – for simplicity, skip and fallback
                    textureLoaded = false;
                } else {
                    std::wstring modelDir = GetDirectoryOf(filepath);
                    std::wstring texRel = UTF8ToWString(std::string(texPath.C_Str()));
                    std::wstring texFull = CombinePath(modelDir, texRel);
                    if (outTexture.loadFromFile(device, texFull)) {
                        textureLoaded = true;
                    }
                }
            }
        }
    }

    if (!textureLoaded) {
        // Fallback to white
        outTexture.createSolidColor(device, 255, 255, 255, 255);
    }

    return true;
}
