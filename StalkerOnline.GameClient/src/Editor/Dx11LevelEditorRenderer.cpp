#include "Editor/Dx11LevelEditorRenderer.h"

#include <DirectXMath.h>
#include <Windows.h>
#include <d3dcompiler.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace StalkerOnline::Editor
{
    namespace
    {
        constexpr std::size_t MaxDynamicVertices = 98304;
        constexpr float Pi = 3.14159265358979323846f;

        using Color = std::array<float, 4>;

        struct Float3
        {
            float X = 0.0f;
            float Y = 0.0f;
            float Z = 0.0f;
        };

        struct CameraConstantBuffer
        {
            DirectX::XMMATRIX ViewProjection;
        };

        float SafeZoom(float zoom)
        {
            return std::clamp(zoom, 0.05f, 64.0f);
        }

        float CalculateOrthoWidth(
            std::uint32_t viewportWidth,
            std::uint32_t viewportHeight,
            const Heightmap& heightmap,
            const LevelEditorRenderSettings& settings)
        {
            const float aspectRatio = viewportHeight == 0
                ? 1.0f
                : static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);

            const float terrainWidth = static_cast<float>(heightmap.GetWidth() - 1) * settings.CellSize;
            const float terrainHeight = static_cast<float>(heightmap.GetHeight() - 1) * settings.CellSize;

            const float fittedWidth = std::max(
                terrainWidth * 1.15f,
                terrainHeight * 1.15f * aspectRatio);

            return std::max(1.0f, fittedWidth / SafeZoom(settings.Zoom));
        }

        DirectX::XMMATRIX BuildViewProjection(
            std::uint32_t viewportWidth,
            std::uint32_t viewportHeight,
            const Heightmap& heightmap,
            const LevelEditorRenderSettings& settings)
        {
            const float aspectRatio = viewportHeight == 0
                ? 1.0f
                : static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);

            const float orthoWidth = CalculateOrthoWidth(viewportWidth, viewportHeight, heightmap, settings);
            const float orthoHeight = orthoWidth / aspectRatio;
            const float cameraHeight = std::max(settings.HeightScale * 2.5f, 10000.0f);

            const DirectX::XMVECTOR eye = DirectX::XMVectorSet(
                settings.PanX,
                cameraHeight,
                settings.PanZ,
                1.0f);

            const DirectX::XMVECTOR target = DirectX::XMVectorSet(
                settings.PanX,
                0.0f,
                settings.PanZ,
                1.0f);

            const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

            const DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(eye, target, up);
            const DirectX::XMMATRIX projection = DirectX::XMMatrixOrthographicLH(
                orthoWidth,
                orthoHeight,
                1.0f,
                cameraHeight * 2.0f);

            return view * projection;
        }

        Color ScaleColor(const Color& color, float scale)
        {
            return
            {
                std::clamp(color[0] * scale, 0.0f, 1.0f),
                std::clamp(color[1] * scale, 0.0f, 1.0f),
                std::clamp(color[2] * scale, 0.0f, 1.0f),
                color[3]
            };
        }

        Color TerrainColor(float normalizedHeight)
        {
            const float value = std::clamp(normalizedHeight, 0.0f, 1.0f);

            if (value < 0.25f)
            {
                const float t = value / 0.25f;
                return
                {
                    0.070f + t * 0.035f,
                    0.120f + t * 0.070f,
                    0.105f + t * 0.045f,
                    1.0f
                };
            }

            if (value < 0.65f)
            {
                const float t = (value - 0.25f) / 0.40f;
                return
                {
                    0.115f + t * 0.180f,
                    0.175f + t * 0.155f,
                    0.120f + t * 0.085f,
                    1.0f
                };
            }

            const float t = (value - 0.65f) / 0.35f;
            return
            {
                0.290f + t * 0.340f,
                0.305f + t * 0.325f,
                0.225f + t * 0.310f,
                1.0f
            };
        }

        Float3 HeightmapPoint(
            const Heightmap& heightmap,
            const LevelEditorRenderSettings& settings,
            int x,
            int y)
        {
            const float centerX = static_cast<float>(heightmap.GetWidth() - 1) * 0.5f;
            const float centerY = static_cast<float>(heightmap.GetHeight() - 1) * 0.5f;

            return
            {
                (static_cast<float>(x) - centerX) * settings.CellSize,
                heightmap.GetHeightNormalized(x, y) * settings.HeightScale,
                (static_cast<float>(y) - centerY) * settings.CellSize
            };
        }

        void AddVertex(
            std::vector<Dx11LevelEditorRenderer::Vertex>& vertices,
            const Float3& position,
            const Color& color)
        {
            Dx11LevelEditorRenderer::Vertex vertex{};
            vertex.Position[0] = position.X;
            vertex.Position[1] = position.Y;
            vertex.Position[2] = position.Z;

            vertex.Color[0] = color[0];
            vertex.Color[1] = color[1];
            vertex.Color[2] = color[2];
            vertex.Color[3] = color[3];

            vertices.push_back(vertex);
        }

        void AddTriangle(
            std::vector<Dx11LevelEditorRenderer::Vertex>& vertices,
            const Float3& a,
            const Float3& b,
            const Float3& c,
            const Color& color)
        {
            AddVertex(vertices, a, color);
            AddVertex(vertices, b, color);
            AddVertex(vertices, c, color);
        }

        void AddLine(
            std::vector<Dx11LevelEditorRenderer::Vertex>& vertices,
            const Float3& a,
            const Float3& b,
            const Color& color)
        {
            AddVertex(vertices, a, color);
            AddVertex(vertices, b, color);
        }

        int CalculateRenderStep(const Heightmap& heightmap, const LevelEditorRenderSettings& settings)
        {
            const int maxAxis = std::max(heightmap.GetWidth(), heightmap.GetHeight());
            const int targetCells = std::clamp(settings.MaxRenderedCellsPerAxis, 32, 1024);

            return std::max(1, maxAxis / targetCells);
        }

        void BuildTerrainVertices(
            std::vector<Dx11LevelEditorRenderer::Vertex>& triangleVertices,
            std::vector<Dx11LevelEditorRenderer::Vertex>& lineVertices,
            const Heightmap& heightmap,
            const LevelEditorRenderSettings& settings)
        {
            if (!heightmap.IsValid())
                return;

            const int step = CalculateRenderStep(heightmap, settings);

            const std::size_t estimatedCells =
                static_cast<std::size_t>(heightmap.GetWidth() / step) *
                static_cast<std::size_t>(heightmap.GetHeight() / step);

            triangleVertices.reserve(std::min<std::size_t>(estimatedCells * 6, MaxDynamicVertices * 8));

            if (settings.ShowWireframe)
                lineVertices.reserve(std::min<std::size_t>(estimatedCells * 4, MaxDynamicVertices * 2));

            for (int y = 0; y < heightmap.GetHeight() - step; y += step)
            {
                for (int x = 0; x < heightmap.GetWidth() - step; x += step)
                {
                    const int x1 = std::min(x + step, heightmap.GetWidth() - 1);
                    const int y1 = std::min(y + step, heightmap.GetHeight() - 1);

                    const Float3 p00 = HeightmapPoint(heightmap, settings, x, y);
                    const Float3 p10 = HeightmapPoint(heightmap, settings, x1, y);
                    const Float3 p11 = HeightmapPoint(heightmap, settings, x1, y1);
                    const Float3 p01 = HeightmapPoint(heightmap, settings, x, y1);

                    const float averageHeight =
                        (heightmap.GetHeightNormalized(x, y) +
                        heightmap.GetHeightNormalized(x1, y) +
                        heightmap.GetHeightNormalized(x1, y1) +
                        heightmap.GetHeightNormalized(x, y1)) * 0.25f;

                    const Color color = TerrainColor(averageHeight);

                    AddTriangle(triangleVertices, p00, p10, p11, color);
                    AddTriangle(triangleVertices, p00, p11, p01, ScaleColor(color, 0.93f));

                    if (settings.ShowWireframe)
                    {
                        const Color lineColor{ 0.080f, 0.095f, 0.080f, 1.0f };
                        AddLine(lineVertices, p00, p10, lineColor);
                        AddLine(lineVertices, p00, p01, lineColor);
                    }
                }
            }
        }

        void AddBrushCircle(
            std::vector<Dx11LevelEditorRenderer::Vertex>& lineVertices,
            const Heightmap& heightmap,
            const LevelEditorRenderSettings& settings)
        {
            if (!settings.ShowBrush || !heightmap.IsValid())
                return;

            constexpr int segmentCount = 96;

            const float centerX = static_cast<float>(heightmap.GetWidth() - 1) * 0.5f;
            const float centerY = static_cast<float>(heightmap.GetHeight() - 1) * 0.5f;

            const float worldX = (settings.BrushX - centerX) * settings.CellSize;
            const float worldZ = (settings.BrushY - centerY) * settings.CellSize;
            const float radius = std::max(0.1f, settings.BrushRadiusCells * settings.CellSize);
            const float y = settings.HeightScale + settings.CellSize * 0.25f;

            const Color color{ 1.0f, 0.760f, 0.190f, 1.0f };

            for (int i = 0; i < segmentCount; ++i)
            {
                const float angleA = static_cast<float>(i) / static_cast<float>(segmentCount) * Pi * 2.0f;
                const float angleB = static_cast<float>(i + 1) / static_cast<float>(segmentCount) * Pi * 2.0f;

                AddLine(
                    lineVertices,
                    Float3{ worldX + std::cos(angleA) * radius, y, worldZ + std::sin(angleA) * radius },
                    Float3{ worldX + std::cos(angleB) * radius, y, worldZ + std::sin(angleB) * radius },
                    color);
            }
        }

        void DebugOutputBlob(ID3DBlob* blob)
        {
            if (blob == nullptr)
                return;

            const char* text = static_cast<const char*>(blob->GetBufferPointer());

            if (text == nullptr)
                return;

            OutputDebugStringA(text);
            OutputDebugStringA("\n");
        }

        bool CompileShader(
            const char* sourceCode,
            const char* entryPoint,
            const char* target,
            ID3DBlob** outputBlob)
        {
            if (sourceCode == nullptr || entryPoint == nullptr || target == nullptr || outputBlob == nullptr)
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
                errorBlob.GetAddressOf());

            if (FAILED(result))
            {
                DebugOutputBlob(errorBlob.Get());
                return false;
            }

            return true;
        }
    }

    Dx11LevelEditorRenderer::~Dx11LevelEditorRenderer()
    {
        Shutdown();
    }

    bool Dx11LevelEditorRenderer::Initialize(ID3D11Device* device)
    {
        if (m_initialized)
            return true;

        if (device == nullptr)
            return false;

        if (!CreateShaders(device))
            return false;

        if (!CreateBuffers(device))
            return false;

        if (!CreateStates(device))
            return false;

        m_initialized = true;
        return true;
    }

    void Dx11LevelEditorRenderer::Shutdown()
    {
        m_depthStencilState.Reset();
        m_rasterizerState.Reset();

        m_cameraConstantBuffer.Reset();
        m_dynamicVertexBuffer.Reset();

        m_inputLayout.Reset();
        m_pixelShader.Reset();
        m_vertexShader.Reset();

        m_initialized = false;
    }

    void Dx11LevelEditorRenderer::Render(
        ID3D11DeviceContext* deviceContext,
        std::uint32_t viewportWidth,
        std::uint32_t viewportHeight,
        const Heightmap& heightmap,
        const LevelEditorRenderSettings& settings)
    {
        if (!m_initialized || deviceContext == nullptr || viewportWidth == 0 || viewportHeight == 0)
            return;

        if (!heightmap.IsValid())
            return;

        D3D11_VIEWPORT viewport{};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = static_cast<float>(viewportWidth);
        viewport.Height = static_cast<float>(viewportHeight);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        deviceContext->RSSetViewports(1, &viewport);
        deviceContext->RSSetState(m_rasterizerState.Get());
        deviceContext->OMSetDepthStencilState(m_depthStencilState.Get(), 0);

        deviceContext->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        deviceContext->PSSetShader(m_pixelShader.Get(), nullptr, 0);
        deviceContext->IASetInputLayout(m_inputLayout.Get());

        ID3D11Buffer* cameraBuffers[] =
        {
            m_cameraConstantBuffer.Get()
        };

        deviceContext->VSSetConstantBuffers(0, 1, cameraBuffers);

        const DirectX::XMMATRIX viewProjection = BuildViewProjection(
            viewportWidth,
            viewportHeight,
            heightmap,
            settings);

        CameraConstantBuffer cameraData{};
        cameraData.ViewProjection = DirectX::XMMatrixTranspose(viewProjection);

        deviceContext->UpdateSubresource(
            m_cameraConstantBuffer.Get(),
            0,
            nullptr,
            &cameraData,
            0,
            0);

        std::vector<Vertex> triangleVertices;
        std::vector<Vertex> lineVertices;

        BuildTerrainVertices(triangleVertices, lineVertices, heightmap, settings);
        AddBrushCircle(lineVertices, heightmap, settings);

        UploadAndDraw(
            deviceContext,
            triangleVertices,
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
            3);

        UploadAndDraw(
            deviceContext,
            lineVertices,
            D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
            2);
    }

    bool Dx11LevelEditorRenderer::CreateShaders(ID3D11Device* device)
    {
        static const char* shaderCode = R"(
cbuffer CameraBuffer : register(b0)
{
    matrix ViewProjection;
};

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
    output.Position = mul(ViewProjection, float4(input.Position, 1.0f));
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

        if (!CompileShader(shaderCode, "VSMain", "vs_5_0", vertexShaderBlob.GetAddressOf()))
            return false;

        if (!CompileShader(shaderCode, "PSMain", "ps_5_0", pixelShaderBlob.GetAddressOf()))
            return false;

        const HRESULT createVertexShaderResult = device->CreateVertexShader(
            vertexShaderBlob->GetBufferPointer(),
            vertexShaderBlob->GetBufferSize(),
            nullptr,
            m_vertexShader.GetAddressOf());

        if (FAILED(createVertexShaderResult))
            return false;

        const HRESULT createPixelShaderResult = device->CreatePixelShader(
            pixelShaderBlob->GetBufferPointer(),
            pixelShaderBlob->GetBufferSize(),
            nullptr,
            m_pixelShader.GetAddressOf());

        if (FAILED(createPixelShaderResult))
            return false;

        const D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[]
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
            m_inputLayout.GetAddressOf());

        return SUCCEEDED(createInputLayoutResult);
    }

    bool Dx11LevelEditorRenderer::CreateBuffers(ID3D11Device* device)
    {
        D3D11_BUFFER_DESC vertexBufferDesc{};
        vertexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * MaxDynamicVertices);
        vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        vertexBufferDesc.MiscFlags = 0;
        vertexBufferDesc.StructureByteStride = sizeof(Vertex);

        const HRESULT createVertexBufferResult = device->CreateBuffer(
            &vertexBufferDesc,
            nullptr,
            m_dynamicVertexBuffer.GetAddressOf());

        if (FAILED(createVertexBufferResult))
            return false;

        D3D11_BUFFER_DESC cameraBufferDesc{};
        cameraBufferDesc.ByteWidth = sizeof(CameraConstantBuffer);
        cameraBufferDesc.Usage = D3D11_USAGE_DEFAULT;
        cameraBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cameraBufferDesc.CPUAccessFlags = 0;
        cameraBufferDesc.MiscFlags = 0;
        cameraBufferDesc.StructureByteStride = 0;

        const HRESULT createCameraBufferResult = device->CreateBuffer(
            &cameraBufferDesc,
            nullptr,
            m_cameraConstantBuffer.GetAddressOf());

        return SUCCEEDED(createCameraBufferResult);
    }

    bool Dx11LevelEditorRenderer::CreateStates(ID3D11Device* device)
    {
        D3D11_RASTERIZER_DESC rasterizerDesc{};
        rasterizerDesc.FillMode = D3D11_FILL_SOLID;
        rasterizerDesc.CullMode = D3D11_CULL_NONE;
        rasterizerDesc.FrontCounterClockwise = FALSE;
        rasterizerDesc.DepthBias = 0;
        rasterizerDesc.DepthBiasClamp = 0.0f;
        rasterizerDesc.SlopeScaledDepthBias = 0.0f;
        rasterizerDesc.DepthClipEnable = TRUE;
        rasterizerDesc.ScissorEnable = FALSE;
        rasterizerDesc.MultisampleEnable = FALSE;
        rasterizerDesc.AntialiasedLineEnable = FALSE;

        const HRESULT rasterizerResult = device->CreateRasterizerState(
            &rasterizerDesc,
            m_rasterizerState.GetAddressOf());

        if (FAILED(rasterizerResult))
            return false;

        D3D11_DEPTH_STENCIL_DESC depthDesc{};
        depthDesc.DepthEnable = TRUE;
        depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        depthDesc.StencilEnable = FALSE;

        const HRESULT depthResult = device->CreateDepthStencilState(
            &depthDesc,
            m_depthStencilState.GetAddressOf());

        return SUCCEEDED(depthResult);
    }

    bool Dx11LevelEditorRenderer::UploadAndDraw(
        ID3D11DeviceContext* deviceContext,
        const std::vector<Vertex>& vertices,
        D3D11_PRIMITIVE_TOPOLOGY topology,
        std::size_t primitiveVertexCount)
    {
        if (vertices.empty())
            return true;

        if (deviceContext == nullptr || primitiveVertexCount == 0)
            return false;

        std::size_t offset = 0;

        while (offset < vertices.size())
        {
            std::size_t batchCount = std::min(MaxDynamicVertices, vertices.size() - offset);
            batchCount -= batchCount % primitiveVertexCount;

            if (batchCount == 0)
                break;

            D3D11_MAPPED_SUBRESOURCE mappedResource{};

            const HRESULT mapResult = deviceContext->Map(
                m_dynamicVertexBuffer.Get(),
                0,
                D3D11_MAP_WRITE_DISCARD,
                0,
                &mappedResource);

            if (FAILED(mapResult))
                return false;

            std::memcpy(
                mappedResource.pData,
                vertices.data() + offset,
                batchCount * sizeof(Vertex));

            deviceContext->Unmap(m_dynamicVertexBuffer.Get(), 0);

            constexpr UINT stride = sizeof(Vertex);
            constexpr UINT vertexOffset = 0;

            ID3D11Buffer* vertexBuffers[]
            {
                m_dynamicVertexBuffer.Get()
            };

            deviceContext->IASetVertexBuffers(
                0,
                1,
                vertexBuffers,
                &stride,
                &vertexOffset);

            deviceContext->IASetPrimitiveTopology(topology);
            deviceContext->Draw(static_cast<UINT>(batchCount), 0);

            offset += batchCount;
        }

        return true;
    }
}
