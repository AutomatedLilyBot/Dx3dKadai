#pragma once
#include <string>
#include <wrl/client.h>
#include <d3d11.h>

#include "Mesh.hpp"
#include "Texture.hpp"
#include "Model.hpp"

// Assimp-based model loader. Legacy single-mesh API is kept for compatibility.
class ModelLoader {
public:
    // Legacy: Load only the first mesh and one diffuse texture. Returns true on success.
    static bool LoadFBX(ID3D11Device *device,
                        const std::wstring &filepath,
                        Mesh &outMesh,
                        Texture &outTexture);

    // New: Load full model (all meshes, per-material diffuse textures, and node transforms)
    static bool LoadFBX(ID3D11Device *device,
                        const std::wstring &filepath,
                        Model &outModel);
};