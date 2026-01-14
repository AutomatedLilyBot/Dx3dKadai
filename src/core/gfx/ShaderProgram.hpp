#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <string>
#include <vector>


#pragma comment(lib, "d3dcompiler.lib")


class ShaderProgram {
public:
    bool compileFromSource(
        ID3D11Device *device,
        const std::string &hlsl,
        const D3D11_INPUT_ELEMENT_DESC *layout,
        UINT layoutCount,
        const char *vsEntry = "VSMain",
        const char *psEntry = "PSMain",
        const char *vsProfile = "vs_5_0",
        const char *psProfile = "ps_5_0");

    bool compileFromFile(
        ID3D11Device *device,
        const std::wstring &hlslPath,
        const D3D11_INPUT_ELEMENT_DESC *layout,
        UINT layoutCount,
        LPCSTR vsEntry = "VSMain",
        LPCSTR psEntry = "PSMain",
        const char *vsProfile = "vs_5_0",
        const char *psProfile = "ps_5_0");


    void bind(ID3D11DeviceContext *ctx);


    ID3D11VertexShader *vs() const { return m_vs.Get(); }
    ID3D11PixelShader *ps() const { return m_ps.Get(); }
    ID3D11InputLayout *inputLayout() const { return m_layout.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_ps;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_layout;
};
