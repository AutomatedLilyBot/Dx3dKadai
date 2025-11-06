//
// Created by zyzyz on 2025/10/22.
//

#include "core/gfx/ShaderProgram.hpp"


static bool CompileShader(const std::string &src, const char *entry, const char *profile, ID3DBlob **blob) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    Microsoft::WRL::ComPtr<ID3DBlob> code, err;
    HRESULT hr = D3DCompile(
        src.data(), src.size(), nullptr, nullptr, nullptr,
        entry, profile, flags, 0, code.GetAddressOf(), err.GetAddressOf());
    if (FAILED(hr)) {
        if (err) OutputDebugStringA((const char *) err->GetBufferPointer());
        return false;
    }
    *blob = code.Detach();
    return true;
}

static bool CompileFile(const std::wstring &path, LPCSTR entry, const char *profile, ID3DBlob **blob) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    Microsoft::WRL::ComPtr<ID3DBlob> code, err;
    HRESULT hr = D3DCompileFromFile(
        path.c_str(),
        nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, // 允许在 .hlsl 里用 #include
        entry, profile, flags, 0,
        code.GetAddressOf(), err.GetAddressOf());
    if (FAILED(hr)) {
        if (err) {
            const char *errMsg = (const char *) err->GetBufferPointer();
            OutputDebugStringA(errMsg);
            printf("Shader compile error:\n%s\n", errMsg);
        }
        std::wstring msg = L"[HLSL] Compile failed: " + path + L"\n";
        OutputDebugStringW(msg.c_str());
        wprintf(L"%s", msg.c_str());
        return false;
    }
    *blob = code.Detach();
    return true;
}


bool ShaderProgram::compileFromSource(ID3D11Device *device,
                                      const std::string &hlsl,
                                      const D3D11_INPUT_ELEMENT_DESC *layout,
                                      UINT layoutCount,
                                      const char *vsEntry,
                                      const char *psEntry,
                                      const char *vsProfile,
                                      const char *psProfile) {
    Microsoft::WRL::ComPtr<ID3DBlob> vsb, psb;
    if (!CompileShader(hlsl, vsEntry, vsProfile, vsb.GetAddressOf())) return false;
    if (!CompileShader(hlsl, psEntry, psProfile, psb.GetAddressOf())) return false;


    if (FAILED(device->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, m_vs.GetAddressOf())))
        return false;
    if (FAILED(device->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, m_ps.GetAddressOf())))
        return false;


    if (FAILED(device->CreateInputLayout(layout, layoutCount, vsb->GetBufferPointer(), vsb->GetBufferSize(),
                                         m_layout.GetAddressOf())))
        return false;


    return true;
}

bool ShaderProgram::compileFromFile(ID3D11Device *device, const std::wstring &hlslPath,
                                    const D3D11_INPUT_ELEMENT_DESC *layout, UINT layoutCount, LPCSTR vsEntry,
                                    LPCSTR psEntry,
                                    const char *vsProfile, const char *psProfile) {
    Microsoft::WRL::ComPtr<ID3DBlob> vsb, psb;
    if (!CompileFile(hlslPath, vsEntry, vsProfile, vsb.GetAddressOf())) return false;
    if (!CompileFile(hlslPath, psEntry, psProfile, psb.GetAddressOf())) return false;

    if (FAILED(device->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, m_vs.GetAddressOf())))
        return false;
    if (FAILED(device->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, m_ps.GetAddressOf())))
        return false;
    if (FAILED(device->CreateInputLayout(layout, layoutCount, vsb->GetBufferPointer(), vsb->GetBufferSize(),
                                         m_layout.GetAddressOf())))
        return false;

    return true;
}


void ShaderProgram::bind(ID3D11DeviceContext *ctx) {
    ctx->IASetInputLayout(m_layout.Get());
    ctx->VSSetShader(m_vs.Get(), nullptr, 0);
    ctx->PSSetShader(m_ps.Get(), nullptr, 0);
}