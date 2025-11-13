#pragma once
#include <DirectXMath.h>
#include "core/gfx/Model.hpp"
#include "Material.hpp"

// Drawable interface - bridge to Renderer
struct IDrawable {
    virtual ~IDrawable() = default;

    virtual DirectX::XMMATRIX world() const = 0;

    virtual const Model *model() const = 0;

    virtual const Material *material() const = 0; // currently per-entity uniform
};
