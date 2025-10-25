//
// Created by zyzyz on 2025/10/22.
//

#include "core/gfx/RenderDeviceD3D11.hpp"
#include <dxgi1_2.h>

bool RenderDeviceD3D11::initialize(HWND hwnd, unsigned width, unsigned height, bool enableDebug)
{
m_width = width; m_height = height;


unsigned int flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
if (enableDebug) flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif


D3D_FEATURE_LEVEL flOut{};
const D3D_FEATURE_LEVEL fls[] = {
D3D_FEATURE_LEVEL_11_1,
D3D_FEATURE_LEVEL_11_0
};


HRESULT hr = D3D11CreateDevice(
nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
flags, fls, _countof(fls), D3D11_SDK_VERSION,
m_device.ReleaseAndGetAddressOf(), &flOut, m_context.ReleaseAndGetAddressOf());
if (FAILED(hr)) return false;


// Create swapchain via factory from device
Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDev;
if (FAILED(m_device.As(&dxgiDev))) return false;


Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
if (FAILED(dxgiDev->GetAdapter(&adapter))) return false;


Microsoft::WRL::ComPtr<IDXGIFactory> factory;
if (FAILED(adapter->GetParent(__uuidof(IDXGIFactory), &factory))) return false;


DXGI_SWAP_CHAIN_DESC scd{};
scd.BufferCount = 2;
scd.BufferDesc.Width = width;
scd.BufferDesc.Height = height;
scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
scd.OutputWindow = hwnd;
scd.SampleDesc.Count = 1;
scd.Windowed = TRUE;
scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; // Simple; flip model requires Factory2


if (FAILED(factory->CreateSwapChain(m_device.Get(), &scd, m_swapChain.ReleaseAndGetAddressOf()))) return false;


createBackbufferViews();


// Viewport
m_viewport = { 0.0f, 0.0f, static_cast<FLOAT>(m_width), static_cast<FLOAT>(m_height), 0.0f, 1.0f };
m_context->RSSetViewports(1, &m_viewport);


// Default rasterizer: cull none (to keep the quad visible regardless of winding)
Microsoft::WRL::ComPtr<ID3D11RasterizerState> rs;
D3D11_RASTERIZER_DESC rd{};
rd.FillMode = D3D11_FILL_SOLID;
rd.CullMode = D3D11_CULL_NONE;
rd.DepthClipEnable = TRUE;
m_device->CreateRasterizerState(&rd, rs.GetAddressOf());
m_context->RSSetState(rs.Get());


// Depth state
Microsoft::WRL::ComPtr<ID3D11DepthStencilState> dss;
D3D11_DEPTH_STENCIL_DESC dsd{};
dsd.DepthEnable = TRUE;
dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
dsd.DepthFunc = D3D11_COMPARISON_LESS;
m_device->CreateDepthStencilState(&dsd, dss.GetAddressOf());
m_context->OMSetDepthStencilState(dss.Get(), 0);


return true;
}

void RenderDeviceD3D11::shutdown()
{
m_dsv.Reset();
m_depth.Reset();
m_rtv.Reset();
m_swapChain.Reset();
m_context.Reset();
m_device.Reset();
}


void RenderDeviceD3D11::createBackbufferViews()
{
// RTV
Microsoft::WRL::ComPtr<ID3D11Texture2D> back;
if (FAILED(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(back.GetAddressOf()))))
throw std::runtime_error("GetBuffer failed");
if (FAILED(m_device->CreateRenderTargetView(back.Get(), nullptr, m_rtv.ReleaseAndGetAddressOf())))
throw std::runtime_error("Create RTV failed");


// DSV
D3D11_TEXTURE2D_DESC dsDesc{};
dsDesc.Width = m_width;
dsDesc.Height = m_height;
dsDesc.MipLevels = 1;
dsDesc.ArraySize = 1;
dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
dsDesc.SampleDesc.Count = 1;
dsDesc.Usage = D3D11_USAGE_DEFAULT;
dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;


if (FAILED(m_device->CreateTexture2D(&dsDesc, nullptr, m_depth.ReleaseAndGetAddressOf())))
throw std::runtime_error("Create depth texture failed");
if (FAILED(m_device->CreateDepthStencilView(m_depth.Get(), nullptr, m_dsv.ReleaseAndGetAddressOf())))
throw std::runtime_error("Create DSV failed");
}


void RenderDeviceD3D11::clear(float r, float g, float b, float a)
{
const float col[4] = { r, g, b, a };
m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), m_dsv.Get());
m_context->ClearRenderTargetView(m_rtv.Get(), col);
m_context->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}


void RenderDeviceD3D11::present()
{
m_swapChain->Present(1, 0);
}


void RenderDeviceD3D11::resize(unsigned width, unsigned height)
{
if (!m_swapChain) return;
m_context->OMSetRenderTargets(0, nullptr, nullptr);
m_rtv.Reset();
m_dsv.Reset();
m_depth.Reset();


m_width = width; m_height = height;
m_swapChain->ResizeBuffers(0, m_width, m_height, DXGI_FORMAT_UNKNOWN, 0);
createBackbufferViews();


m_viewport = { 0.0f, 0.0f, static_cast<FLOAT>(m_width), static_cast<FLOAT>(m_height), 0.0f, 1.0f };
m_context->RSSetViewports(1, &m_viewport);
}