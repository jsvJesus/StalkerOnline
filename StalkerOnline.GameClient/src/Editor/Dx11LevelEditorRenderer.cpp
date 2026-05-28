#ifndef NOMINMAX
#define NOMINMAX
#endif

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

        float DegToRad(float degrees)
        {
            return degrees * Pi / 180.0f;
        }

        Float3 CameraForward(const LevelEditorRenderSettings& settings)
        {
            const float yaw = DegToRad(settings.CameraYaw);
            const float pitch = DegToRad(std::clamp(settings.CameraPitch, -89.0f, 89.0f));
            const float cosPitch = std::cos(pitch);

            return
            {
                std::sin(yaw) * cosPitch,
                std::sin(pitch),
                std::cos(yaw) * cosPitch
            };
        }

        float CalculateFarPlane(const Heightmap& heightmap, const LevelEditorRenderSettings& settings)
        {
            const float terrainWidth = static_cast<float>(heightmap.GetWidth() - 1) * settings.CellSizeX;
            const float terrainHeight = static_cast<float>(heightmap.GetHeight() - 1) * settings.CellSizeY;
            const float maxTerrainAxis = (std::max)(terrainWidth, terrainHeight);

            return (std::max)(maxTerrainAxis * 4.0f, settings.HeightScale * 12.0f);
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

            const Float3 forward = CameraForward(settings);

            const DirectX::XMVECTOR eye = DirectX::XMVectorSet(
                settings.CameraX,
                settings.CameraY,
                settings.CameraZ,
                1.0f);

            const DirectX::XMVECTOR target = DirectX::XMVectorSet(
                settings.CameraX + forward.X,
                settings.CameraY + forward.Y,
                settings.CameraZ + forward.Z,
                1.0f);

            const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

            const DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(eye, target, up);
            const DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(
                DegToRad(60.0f),
                aspectRatio,
                (std::max)(1.0f, (std::min)(settings.CellSizeX, settings.CellSizeY) * 0.02f),
                CalculateFarPlane(heightmap, settings));

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
                    0.100f + t * 0.055f,
                    0.165f + t * 0.095f,
                    0.135f + t * 0.060f,
                    1.0f
                };
            }

            if (value < 0.65f)
            {
                const float t = (value - 0.25f) / 0.40f;
                return
                {
                    0.165f + t * 0.205f,
                    0.245f + t * 0.175f,
                    0.150f + t * 0.100f,
                    1.0f
                };
            }

            const float t = (value - 0.65f) / 0.35f;
            return
            {
                0.350f + t * 0.360f,
                0.390f + t * 0.330f,
                0.250f + t * 0.320f,
                1.0f
            };
        }

        float HeightForPreview(
            float rawHeight,
            const LevelEditorRenderSettings& settings)
        {
            const float clampedRawHeight = std::clamp(rawHeight, 0.0f, 1.0f);

            if (!settings.NormalizeHeightPreview)
                return clampedRawHeight;

            const float minHeight = std::clamp(settings.PreviewMinHeight, 0.0f, 1.0f);
            const float maxHeight = std::clamp(settings.PreviewMaxHeight, 0.0f, 1.0f);
            const float range = maxHeight - minHeight;

            if (range <= 0.000001f)
                return 0.5f;

            return std::clamp((clampedRawHeight - minHeight) / range, 0.0f, 1.0f);
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
                (static_cast<float>(x) - centerX) * settings.CellSizeX,
                (HeightForPreview(heightmap.GetHeightNormalized(x, y), settings) - 0.5f) * settings.HeightScale,
                (static_cast<float>(y) - centerY) * settings.CellSizeY
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
            const int maxAxis = (std::max)(heightmap.GetWidth(), heightmap.GetHeight());
            const int targetCells = std::clamp(settings.MaxRenderedCellsPerAxis, 32, 1024);

            return (std::max)(1, maxAxis / targetCells);
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
                    const int x1 = (std::min)(x + step, heightmap.GetWidth() - 1);
                    const int y1 = (std::min)(y + step, heightmap.GetHeight() - 1);

                    const Float3 p00 = HeightmapPoint(heightmap, settings, x, y);
                    const Float3 p10 = HeightmapPoint(heightmap, settings, x1, y);
                    const Float3 p11 = HeightmapPoint(heightmap, settings, x1, y1);
                    const Float3 p01 = HeightmapPoint(heightmap, settings, x, y1);

                    const float averageHeight =
                        (heightmap.GetHeightNormalized(x, y) +
                        heightmap.GetHeightNormalized(x1, y) +
                        heightmap.GetHeightNormalized(x1, y1) +
                        heightmap.GetHeightNormalized(x, y1)) * 0.25f;

                    const Color color = TerrainColor(HeightForPreview(averageHeight, settings));

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

        void AddReferenceGrid(
            std::vector<Dx11LevelEditorRenderer::Vertex>& lineVertices,
            const Heightmap& heightmap,
            const LevelEditorRenderSettings& settings)
        {
            if (!heightmap.IsValid())
                return;

            const float terrainWidth = static_cast<float>(heightmap.GetWidth() - 1) * settings.CellSizeX;
            const float terrainHeight = static_cast<float>(heightmap.GetHeight() - 1) * settings.CellSizeY;

            const float minX = -terrainWidth * 0.5f;
            const float maxX = terrainWidth * 0.5f;
            const float minZ = -terrainHeight * 0.5f;
            const float maxZ = terrainHeight * 0.5f;

            const float y = (std::min)(settings.CellSizeX, settings.CellSizeY) * 0.08f;
            const Color gridColor{ 0.210f, 0.250f, 0.185f, 1.0f };
            const Color majorGridColor{ 0.330f, 0.360f, 0.230f, 1.0f };
            const Color xAxisColor{ 0.650f, 0.280f, 0.190f, 1.0f };
            const Color zAxisColor{ 0.220f, 0.390f, 0.670f, 1.0f };

            constexpr int gridDivisions = 16;

            for (int i = 0; i <= gridDivisions; ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(gridDivisions);
                const float x = minX + (maxX - minX) * t;
                const float z = minZ + (maxZ - minZ) * t;
                const bool majorLine = i == 0 || i == gridDivisions || i == gridDivisions / 2;
                const Color color = majorLine ? majorGridColor : gridColor;

                AddLine(lineVertices, { x, y, minZ }, { x, y, maxZ }, color);
                AddLine(lineVertices, { minX, y, z }, { maxX, y, z }, color);
            }

            AddLine(lineVertices, { minX, y * 1.5f, 0.0f }, { maxX, y * 1.5f, 0.0f }, xAxisColor);
            AddLine(lineVertices, { 0.0f, y * 1.5f, minZ }, { 0.0f, y * 1.5f, maxZ }, zAxisColor);
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

            const float radiusCells = (std::max)(0.1f, settings.BrushRadiusCells);

            const Color color{ 1.0f, 0.760f, 0.190f, 1.0f };

            auto makeCirclePoint = [&](float angle)
            {
                const float sampleX = settings.BrushX + std::cos(angle) * radiusCells;
                const float sampleY = settings.BrushY + std::sin(angle) * radiusCells;

                const int heightSampleX = std::clamp(static_cast<int>(std::round(sampleX)), 0, heightmap.GetWidth() - 1);
                    const int heightSampleY = std::clamp(static_cast<int>(std::round(sampleY)), 0, heightmap.GetHeight() - 1);

                return Float3
                {
                    (sampleX - centerX) * settings.CellSizeX,
                    (HeightForPreview(heightmap.GetHeightNormalized(heightSampleX, heightSampleY), settings) - 0.5f) * settings.HeightScale + (std::min)(settings.CellSizeX, settings.CellSizeY) * 0.35f,
                    (sampleY - centerY) * settings.CellSizeY
                };
            };

            for (int i = 0; i < segmentCount; ++i)
            {
                const float angleA = static_cast<float>(i) / static_cast<float>(segmentCount) * Pi * 2.0f;
                const float angleB = static_cast<float>(i + 1) / static_cast<float>(segmentCount) * Pi * 2.0f;

                AddLine(
                    lineVertices,
                    makeCirclePoint(angleA),
                    makeCirclePoint(angleB),
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
        AddReferenceGrid(lineVertices, heightmap, settings);
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
            std::size_t batchCount = (std::min)(MaxDynamicVertices, vertices.size() - offset);
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
