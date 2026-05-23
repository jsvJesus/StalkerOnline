#include "Engine/Renderer/Dx11WorldRenderer.h"

#include <DirectXMath.h>
#include <Windows.h>
#include <d3dcompiler.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace StalkerOnline::Engine
{
    namespace
    {
        constexpr std::size_t MaxDynamicVertices = 65536;
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

        Float3 ToRenderPosition(float gameX, float gameY, float gameZ)
        {
            return
            {
                gameX,
                gameZ,
                gameY
            };
        }

        void AddVertex(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            const Float3& position,
            const Color& color
        )
        {
            Dx11WorldRenderer::Vertex vertex{};
            vertex.Position[0] = position.X;
            vertex.Position[1] = position.Y;
            vertex.Position[2] = position.Z;

            vertex.Color[0] = color[0];
            vertex.Color[1] = color[1];
            vertex.Color[2] = color[2];
            vertex.Color[3] = color[3];

            vertices.push_back(vertex);
        }

        void AddLine(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            const Float3& a,
            const Float3& b,
            const Color& color
        )
        {
            AddVertex(vertices, a, color);
            AddVertex(vertices, b, color);
        }

        void AddTriangle(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            const Float3& a,
            const Float3& b,
            const Float3& c,
            const Color& color
        )
        {
            AddVertex(vertices, a, color);
            AddVertex(vertices, b, color);
            AddVertex(vertices, c, color);
        }

        void AddQuad(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            const Float3& a,
            const Float3& b,
            const Float3& c,
            const Float3& d,
            const Color& color
        )
        {
            AddTriangle(vertices, a, b, c, color);
            AddTriangle(vertices, a, c, d, color);
        }

        void AddCube(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            const Float3& center,
            const Float3& halfSize,
            const Color& baseColor
        )
        {
            const float minX = center.X - halfSize.X;
            const float maxX = center.X + halfSize.X;

            const float minY = center.Y - halfSize.Y;
            const float maxY = center.Y + halfSize.Y;

            const float minZ = center.Z - halfSize.Z;
            const float maxZ = center.Z + halfSize.Z;

            const Float3 p000{ minX, minY, minZ };
            const Float3 p001{ minX, minY, maxZ };
            const Float3 p010{ minX, maxY, minZ };
            const Float3 p011{ minX, maxY, maxZ };

            const Float3 p100{ maxX, minY, minZ };
            const Float3 p101{ maxX, minY, maxZ };
            const Float3 p110{ maxX, maxY, minZ };
            const Float3 p111{ maxX, maxY, maxZ };

            const Color topColor = ScaleColor(baseColor, 1.25f);
            const Color frontColor = ScaleColor(baseColor, 1.05f);
            const Color sideColor = ScaleColor(baseColor, 0.85f);
            const Color bottomColor = ScaleColor(baseColor, 0.55f);

            AddQuad(vertices, p001, p101, p111, p011, frontColor);
            AddQuad(vertices, p100, p000, p010, p110, sideColor);

            AddQuad(vertices, p000, p001, p011, p010, sideColor);
            AddQuad(vertices, p101, p100, p110, p111, sideColor);

            AddQuad(vertices, p010, p011, p111, p110, topColor);
            AddQuad(vertices, p000, p100, p101, p001, bottomColor);
        }

        void AddGrid(std::vector<Dx11WorldRenderer::Vertex>& vertices)
        {
            constexpr int halfLineCount = 30;
            constexpr float step = 1.0f;
            constexpr float halfSize = static_cast<float>(halfLineCount) * step;

            const Color gridColor{ 0.18f, 0.22f, 0.18f, 1.0f };
            const Color centerColor{ 0.32f, 0.42f, 0.32f, 1.0f };
            const Color xAxisColor{ 0.55f, 0.18f, 0.16f, 1.0f };
            const Color zAxisColor{ 0.18f, 0.32f, 0.55f, 1.0f };

            for (int i = -halfLineCount; i <= halfLineCount; ++i)
            {
                const float value = static_cast<float>(i) * step;

                const Color& lineColor = (i == 0) ? centerColor : gridColor;

                AddLine(
                    vertices,
                    Float3{ -halfSize, 0.0f, value },
                    Float3{ halfSize, 0.0f, value },
                    lineColor
                );

                AddLine(
                    vertices,
                    Float3{ value, 0.0f, -halfSize },
                    Float3{ value, 0.0f, halfSize },
                    lineColor
                );
            }

            AddLine(
                vertices,
                Float3{ -halfSize, 0.025f, 0.0f },
                Float3{ halfSize, 0.025f, 0.0f },
                xAxisColor
            );

            AddLine(
                vertices,
                Float3{ 0.0f, 0.03f, -halfSize },
                Float3{ 0.0f, 0.03f, halfSize },
                zAxisColor
            );
        }

        void AddPlayerForwardLine(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            const Float3& playerPosition,
            float rotationZ
        )
        {
            const float yaw = DegToRad(rotationZ);

            const float forwardX = static_cast<float>(std::sin(yaw));
            const float forwardZ = static_cast<float>(std::cos(yaw));

            const Float3 start
            {
                playerPosition.X,
                playerPosition.Y + 1.15f,
                playerPosition.Z
            };

            const Float3 end
            {
                playerPosition.X + forwardX * 1.6f,
                playerPosition.Y + 1.15f,
                playerPosition.Z + forwardZ * 1.6f
            };

            const Color color{ 0.95f, 0.95f, 0.35f, 1.0f };

            AddLine(vertices, start, end, color);
        }

        DirectX::XMMATRIX BuildViewProjection(
            const WorldRenderPlayer& player,
            std::uint32_t viewportWidth,
            std::uint32_t viewportHeight,
            GameCameraMode cameraMode
        )
        {
            const float aspectRatio = viewportHeight == 0
                ? 1.0f
                : static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);

            Float3 playerGroundPosition = ToRenderPosition(
                player.PositionX,
                player.PositionY,
                player.PositionZ
            );

            if (!player.Valid)
            {
                playerGroundPosition = Float3{ 0.0f, 0.0f, 0.0f };
            }

            const float yaw = DegToRad(player.RotationZ);

            const float forwardX = static_cast<float>(std::sin(yaw));
            const float forwardZ = static_cast<float>(std::cos(yaw));

            DirectX::XMVECTOR eye{};
            DirectX::XMVECTOR target{};

            if (cameraMode == GameCameraMode::FirstPerson)
            {
                eye = DirectX::XMVectorSet(
                    playerGroundPosition.X,
                    playerGroundPosition.Y + 1.65f,
                    playerGroundPosition.Z,
                    1.0f
                );

                target = DirectX::XMVectorSet(
                    playerGroundPosition.X + forwardX * 10.0f,
                    playerGroundPosition.Y + 1.55f,
                    playerGroundPosition.Z + forwardZ * 10.0f,
                    1.0f
                );
            }
            else
            {
                eye = DirectX::XMVectorSet(
                    playerGroundPosition.X - forwardX * 7.5f,
                    playerGroundPosition.Y + 4.0f,
                    playerGroundPosition.Z - forwardZ * 7.5f,
                    1.0f
                );

                target = DirectX::XMVectorSet(
                    playerGroundPosition.X + forwardX * 2.0f,
                    playerGroundPosition.Y + 1.2f,
                    playerGroundPosition.Z + forwardZ * 2.0f,
                    1.0f
                );
            }

            const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

            const DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(
                eye,
                target,
                up
            );

            const DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(
                DegToRad(65.0f),
                aspectRatio,
                0.05f,
                500.0f
            );

            return view * projection;
        }

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

    Dx11WorldRenderer::~Dx11WorldRenderer()
    {
        Shutdown();
    }

    bool Dx11WorldRenderer::Initialize(ID3D11Device* device)
    {
        if (m_initialized)
            return true;

        if (!device)
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

    void Dx11WorldRenderer::Shutdown()
    {
        m_rasterizerState.Reset();
        m_cameraConstantBuffer.Reset();
        m_dynamicVertexBuffer.Reset();

        m_inputLayout.Reset();
        m_pixelShader.Reset();
        m_vertexShader.Reset();

        m_initialized = false;
    }

    void Dx11WorldRenderer::Render(
        ID3D11DeviceContext* deviceContext,
        std::uint32_t viewportWidth,
        std::uint32_t viewportHeight,
        const WorldRenderPlayer& player,
        const std::vector<WorldRenderItem>& items,
        GameCameraMode cameraMode
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
        deviceContext->RSSetState(m_rasterizerState.Get());

        const DirectX::XMMATRIX viewProjection = BuildViewProjection(
            player,
            viewportWidth,
            viewportHeight,
            cameraMode
        );

        CameraConstantBuffer cameraData{};
        cameraData.ViewProjection = DirectX::XMMatrixTranspose(viewProjection);

        deviceContext->UpdateSubresource(
            m_cameraConstantBuffer.Get(),
            0,
            nullptr,
            &cameraData,
            0,
            0
        );

        ID3D11Buffer* cameraBuffers[] =
        {
            m_cameraConstantBuffer.Get()
        };

        deviceContext->VSSetConstantBuffers(0, 1, cameraBuffers);

        deviceContext->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        deviceContext->PSSetShader(m_pixelShader.Get(), nullptr, 0);
        deviceContext->IASetInputLayout(m_inputLayout.Get());

        std::vector<Vertex> lineVertices;
        lineVertices.reserve(512);

        AddGrid(lineVertices);

        Float3 playerPosition = ToRenderPosition(
            player.PositionX,
            player.PositionY,
            player.PositionZ
        );

        if (!player.Valid)
            playerPosition = Float3{ 0.0f, 0.0f, 0.0f };

        AddPlayerForwardLine(lineVertices, playerPosition, player.RotationZ);

        UploadAndDraw(
            deviceContext,
            lineVertices,
            D3D11_PRIMITIVE_TOPOLOGY_LINELIST
        );

        std::vector<Vertex> triangleVertices;
        triangleVertices.reserve(4096);

        const Color playerColor{ 0.22f, 0.72f, 0.32f, 1.0f };
        const Color itemColor{ 0.90f, 0.68f, 0.20f, 1.0f };

        const Float3 playerCenter
        {
            playerPosition.X,
            playerPosition.Y + 0.9f,
            playerPosition.Z
        };

        AddCube(
            triangleVertices,
            playerCenter,
            Float3{ 0.35f, 0.9f, 0.35f },
            playerColor
        );

        for (const WorldRenderItem& item : items)
        {
            const float itemSize = std::clamp(item.Size, 0.15f, 2.0f);

            const Float3 itemPosition = ToRenderPosition(
                item.PositionX,
                item.PositionY,
                item.PositionZ
            );

            const Float3 itemCenter
            {
                itemPosition.X,
                itemPosition.Y + itemSize * 0.5f,
                itemPosition.Z
            };

            AddCube(
                triangleVertices,
                itemCenter,
                Float3{ itemSize, itemSize, itemSize },
                itemColor
            );
        }

        UploadAndDraw(
            deviceContext,
            triangleVertices,
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
        );
    }

    bool Dx11WorldRenderer::CreateShaders(ID3D11Device* device)
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

        return SUCCEEDED(createInputLayoutResult);
    }

    bool Dx11WorldRenderer::CreateBuffers(ID3D11Device* device)
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
            m_dynamicVertexBuffer.GetAddressOf()
        );

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
            m_cameraConstantBuffer.GetAddressOf()
        );

        return SUCCEEDED(createCameraBufferResult);
    }

    bool Dx11WorldRenderer::CreateStates(ID3D11Device* device)
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

        const HRESULT result = device->CreateRasterizerState(
            &rasterizerDesc,
            m_rasterizerState.GetAddressOf()
        );

        return SUCCEEDED(result);
    }

    bool Dx11WorldRenderer::UploadAndDraw(
        ID3D11DeviceContext* deviceContext,
        const std::vector<Vertex>& vertices,
        D3D11_PRIMITIVE_TOPOLOGY topology
    )
    {
        if (vertices.empty())
            return true;

        if (vertices.size() > MaxDynamicVertices)
            return false;

        D3D11_MAPPED_SUBRESOURCE mappedResource{};

        const HRESULT mapResult = deviceContext->Map(
            m_dynamicVertexBuffer.Get(),
            0,
            D3D11_MAP_WRITE_DISCARD,
            0,
            &mappedResource
        );

        if (FAILED(mapResult))
            return false;

        std::memcpy(
            mappedResource.pData,
            vertices.data(),
            vertices.size() * sizeof(Vertex)
        );

        deviceContext->Unmap(m_dynamicVertexBuffer.Get(), 0);

        constexpr UINT stride = sizeof(Vertex);
        constexpr UINT offset = 0;

        ID3D11Buffer* vertexBuffers[] =
        {
            m_dynamicVertexBuffer.Get()
        };

        deviceContext->IASetVertexBuffers(
            0,
            1,
            vertexBuffers,
            &stride,
            &offset
        );

        deviceContext->IASetPrimitiveTopology(topology);

        deviceContext->Draw(
            static_cast<UINT>(vertices.size()),
            0
        );

        return true;
    }
}