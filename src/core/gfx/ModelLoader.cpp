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
#include <cstring>

// Assimp (headers are placed directly under external/assimp)
#include <Importer.hpp>
#include <iostream>
#include <ostream>
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
                wprintf(L"Texture path is: %hs\n", texPath.data);

                // Helper lambda to upload an aiTexture to GPU
                auto loadAiTexture = [&](const aiTexture* tex) -> bool {
                    if (!tex) return false;
                    if (tex->mHeight == 0) {
                        // Compressed texture data in tex->pcData with size tex->mWidth
                        const void* bytes = reinterpret_cast<const void*>(tex->pcData);
                        size_t size = static_cast<size_t>(tex->mWidth);
                        const char* hint = tex->achFormatHint[0] ? tex->achFormatHint : nullptr;
                        return outTexture.loadFromMemory(device, bytes, size, hint);
                    } else {
                        // Uncompressed raw pixels in aiTexel array (RGBA 8-bit)
                        const uint32_t width = tex->mWidth;
                        const uint32_t height = tex->mHeight;
                        const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);
                        std::vector<unsigned char> rgba;
                        rgba.resize(count * 4);
                        for (size_t i = 0; i < count; ++i) {
                            const aiTexel& t = tex->pcData[i];
                            rgba[i * 4 + 0] = t.r;
                            rgba[i * 4 + 1] = t.g;
                            rgba[i * 4 + 2] = t.b;
                            rgba[i * 4 + 3] = t.a;
                        }
                        return outTexture.createFromRGBA(device, rgba.data(), width, height, width * 4);
                    }
                };

                if (texPath.data[0] == '*') {
                    // Embedded texture like "*0"
                    // parse index and fetch from scene->mTextures
                    wprintf(L"Loading embedded texture by index...\n");
                    int idx = 0;
                    try {
                        idx = std::stoi(std::string(texPath.C_Str() + 1));
                    } catch (...) { idx = 0; }
                    if (idx >= 0 && scene->mNumTextures > 0 && static_cast<unsigned int>(idx) < scene->mNumTextures) {
                        textureLoaded = loadAiTexture(scene->mTextures[idx]);
                    }
                } else {
                    // Named embedded texture path like "model.fbm/xxx.png"
                    // 1) Try matching full path against aiTexture::mFilename
                    if (!textureLoaded && scene->mNumTextures > 0) {
                        std::string wanted = std::string(texPath.C_Str());
                        for (unsigned int i = 0; i < scene->mNumTextures && !textureLoaded; ++i) {
                            const aiTexture* t = scene->mTextures[i];
                            if (!t) continue;
                            if (t->mFilename.length > 0) {
                                std::string tfull = t->mFilename.C_Str();
                                if (tfull == wanted) {
                                    wprintf(L"Loading embedded texture by full path match...\n");
                                    textureLoaded = loadAiTexture(t);
                                }
                            }
                        }
                    }

                    // 2) Try basename match against scene->mTextures
                    if (!textureLoaded && scene->mNumTextures > 0) {
                        std::string full = std::string(texPath.C_Str());
                        size_t slash = full.find_last_of("/\\");
                        std::string base = (slash == std::string::npos) ? full : full.substr(slash + 1);
                        for (unsigned int i = 0; i < scene->mNumTextures && !textureLoaded; ++i) {
                            const aiTexture* t = scene->mTextures[i];
                            if (!t) continue;
                            if (t->mFilename.length > 0) {
                                std::string tf = t->mFilename.C_Str();
                                size_t s2 = tf.find_last_of("/\\");
                                std::string tbase = (s2 == std::string::npos) ? tf : tf.substr(s2 + 1);
#ifdef _WIN32
                                if (_stricmp(tbase.c_str(), base.c_str()) == 0)
#else
                                if (strcasecmp(tbase.c_str(), base.c_str()) == 0)
#endif
                                {
                                    wprintf(L"Loading embedded texture by basename match...\n");
                                    textureLoaded = loadAiTexture(t);
                                }
                            }
                        }
                    }

                    // 3) Fallback to external disk file path
                    if (!textureLoaded) {
                        wprintf(L"Loading external texture from disk...\n");
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
    }
    else {
        wprintf(L"Model has no texture.");
    }

    if (!textureLoaded) {
        // Fallback to white
        outTexture.createSolidColor(device, 255, 255, 255, 255);
    }

    return true;
}
