#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <stdexcept>


#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")


class RenderDeviceD3D11 {
public:
    bool initialize(HWND hwnd, unsigned width, unsigned height, bool enableDebug = false);

    void shutdown();

    void clear(float r, float g, float b, float a);

    void present();

    void resize(unsigned width, unsigned height);


    // Getters
    ID3D11Device *device() const { return m_device.Get(); }
    ID3D11DeviceContext *context() const { return m_context.Get(); }


    ID3D11RenderTargetView *rtv() const { return m_rtv.Get(); }
    ID3D11DepthStencilView *dsv() const { return m_dsv.Get(); }


    unsigned width() const { return m_width; }
    unsigned height() const { return m_height; }

private:
    void createBackbufferViews();

private:
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;


    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depth;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_dsv;


    D3D11_VIEWPORT m_viewport{};
    unsigned m_width = 0;
    unsigned m_height = 0;
};
