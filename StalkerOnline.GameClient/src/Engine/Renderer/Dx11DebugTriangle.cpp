#include "Engine/Renderer/Dx11DebugTriangle.h"

#include <d3dcompiler.h>
#include <Windows.h>
#include <cstring>

namespace StalkerOnline::Engine
{
    namespace
    {
        void DebugOutputBlob(ID3DBlob* blob)
        {
            if (!blob)
                return;

            const char* text = static_cast<const char*>(blob->GetBufferPointer());
            if (!text)
                return;

            OutputDebugStringA(text);
            OutputDebugStringA("\n");
        }

        bool CompileShader(
            const char* sourceCode,
            const char* entryPoint,
            const char* target,
            ID3DBlob** outputBlob
        )
        {
            if (!sourceCode || !entryPoint || !target || !outputBlob)
                return false;

            UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;

#if defined(_DEBUG)
            flags |= D3DCOMPILE_DEBUG;
            flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

            Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

            const HRESULT result = D3DCompile(
                sourceCode,
                std::strlen(sourceCode),
                nullptr,
                nullptr,
                nullptr,
                entryPoint,
                target,
                flags,
                0,
                outputBlob,
                errorBlob.GetAddressOf()
            );

            if (FAILED(result))
            {
                DebugOutputBlob(errorBlob.Get());
                return false;
            }

            return true;
        }
    }

    Dx11DebugTriangle::~Dx11DebugTriangle()
    {
        Shutdown();
    }

    bool Dx11DebugTriangle::Initialize(ID3D11Device* device)
    {
        if (m_initialized)
            return true;

        if (!device)
            return false;

        if (!CreateShaders(device))
            return false;

        if (!CreateGeometry(device))
            return false;

        m_initialized = true;
        return true;
    }

    void Dx11DebugTriangle::Shutdown()
    {
        m_vertexBuffer.Reset();
        m_inputLayout.Reset();
        m_pixelShader.Reset();
        m_vertexShader.Reset();

        m_initialized = false;
    }

    void Dx11DebugTriangle::Render(
        ID3D11DeviceContext* deviceContext,
        std::uint32_t viewportWidth,
        std::uint32_t viewportHeight
    )
    {
        if (!m_initialized)
            return;

        if (!deviceContext)
            return;

        if (viewportWidth == 0 || viewportHeight == 0)
            return;

        D3D11_VIEWPORT viewport{};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = static_cast<float>(viewportWidth);
        viewport.Height = static_cast<float>(viewportHeight);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        deviceContext->RSSetViewports(1, &viewport);

        constexpr UINT stride = sizeof(Vertex);
        constexpr UINT offset = 0;

        ID3D11Buffer* vertexBuffers[] =
        {
            m_vertexBuffer.Get()
        };

        deviceContext->IASetInputLayout(m_inputLayout.Get());
        deviceContext->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
        deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        deviceContext->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        deviceContext->PSSetShader(m_pixelShader.Get(), nullptr, 0);

        deviceContext->Draw(3, 0);
    }

    bool Dx11DebugTriangle::CreateShaders(ID3D11Device* device)
    {
        static const char* shaderCode = R"(
struct VS_INPUT
{
    float3 Position : POSITION;
    float4 Color    : COLOR0;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR0;
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    output.Position = float4(input.Position, 1.0f);
    output.Color = input.Color;
    return output;
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    return input.Color;
}
)";

        Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBlob;

        if (!CompileShader(
            shaderCode,
            "VSMain",
            "vs_5_0",
            vertexShaderBlob.GetAddressOf()
        ))
        {
            return false;
        }

        if (!CompileShader(
            shaderCode,
            "PSMain",
            "ps_5_0",
            pixelShaderBlob.GetAddressOf()
        ))
        {
            return false;
        }

        const HRESULT createVertexShaderResult = device->CreateVertexShader(
            vertexShaderBlob->GetBufferPointer(),
            vertexShaderBlob->GetBufferSize(),
            nullptr,
            m_vertexShader.GetAddressOf()
        );

        if (FAILED(createVertexShaderResult))
            return false;

        const HRESULT createPixelShaderResult = device->CreatePixelShader(
            pixelShaderBlob->GetBufferPointer(),
            pixelShaderBlob->GetBufferSize(),
            nullptr,
            m_pixelShader.GetAddressOf()
        );

        if (FAILED(createPixelShaderResult))
            return false;

        const D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[] =
        {
            {
                "POSITION",
                0,
                DXGI_FORMAT_R32G32B32_FLOAT,
                0,
                0,
                D3D11_INPUT_PER_VERTEX_DATA,
                0
            },
            {
                "COLOR",
                0,
                DXGI_FORMAT_R32G32B32A32_FLOAT,
                0,
                12,
                D3D11_INPUT_PER_VERTEX_DATA,
                0
            }
        };

        const HRESULT createInputLayoutResult = device->CreateInputLayout(
            inputLayoutDesc,
            2,
            vertexShaderBlob->GetBufferPointer(),
            vertexShaderBlob->GetBufferSize(),
            m_inputLayout.GetAddressOf()
        );

        if (FAILED(createInputLayoutResult))
            return false;

        return true;
    }

    bool Dx11DebugTriangle::CreateGeometry(ID3D11Device* device)
    {
        const Vertex vertices[] =
        {
            {
                {  0.0f,  0.65f, 0.0f },
                {  0.20f, 0.85f, 0.30f, 1.0f }
            },
            {
                {  0.65f, -0.55f, 0.0f },
                {  0.90f, 0.75f, 0.25f, 1.0f }
            },
            {
                { -0.65f, -0.55f, 0.0f },
                {  0.20f, 0.55f, 0.95f, 1.0f }
            }
        };

        D3D11_BUFFER_DESC bufferDesc{};
        bufferDesc.ByteWidth = static_cast<UINT>(sizeof(vertices));
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags = 0;
        bufferDesc.MiscFlags = 0;
        bufferDesc.StructureByteStride = sizeof(Vertex);

        D3D11_SUBRESOURCE_DATA initialData{};
        initialData.pSysMem = vertices;

        const HRESULT result = device->CreateBuffer(
            &bufferDesc,
            &initialData,
            m_vertexBuffer.GetAddressOf()
        );

        return SUCCEEDED(result);
    }
}