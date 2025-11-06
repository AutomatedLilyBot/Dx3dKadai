#pragma once
#include <string>
#include <wrl/client.h>
#include <d3d11.h>

#include "Mesh.hpp"
#include "Texture.hpp"

// A simple Assimp-based model loader that loads a single-mesh FBX (or other formats)
// Populates a Mesh (VertexPNCT) and a Texture (diffuse). Falls back to a white texture.
class ModelLoader {
public:
    // Load a model from file. Returns true on success.
    // - device: D3D11 device to create GPU resources
    // - filepath: absolute or relative path to the model file (e.g., L"asset/your.fbx")
    // - outMesh: output mesh (vertex/index buffers)
    // - outTexture: output texture (diffuse); creates a solid white fallback if texture not found
    static bool LoadFBX(ID3D11Device *device,
                        const std::wstring &filepath,
                        Mesh &outMesh,
                        Texture &outTexture);
};
