#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <vector>

namespace StalkerOnline::Engine
{
    enum class GameCameraMode : std::uint8_t
    {
        ThirdPerson = 0,
        FirstPerson = 1
    };

    struct WorldRenderPlayer
    {
        bool Valid = false;

        float PositionX = 0.0f;
        float PositionY = 0.0f;
        float PositionZ = 0.0f;

        float RotationZ = 0.0f;
    };

    struct WorldRenderItem
    {
        std::int32_t WorldObjectId = 0;

        float PositionX = 0.0f;
        float PositionY = 0.0f;
        float PositionZ = 0.0f;

        float Size = 0.35f;
    };

    struct WorldRenderRemotePlayer
    {
        std::int32_t CharacterId = 0;

        float PositionX = 0.0f;
        float PositionY = 0.0f;
        float PositionZ = 0.0f;

        float RotationZ = 0.0f;

        bool IsAlive = true;
    };

    class Dx11WorldRenderer final
    {
    public:
        Dx11WorldRenderer() = default;
        ~Dx11WorldRenderer();

        Dx11WorldRenderer(const Dx11WorldRenderer&) = delete;
        Dx11WorldRenderer& operator=(const Dx11WorldRenderer&) = delete;

        bool Initialize(ID3D11Device* device);
        void Shutdown();

        void Render(
            ID3D11DeviceContext* deviceContext,
            std::uint32_t viewportWidth,
            std::uint32_t viewportHeight,
            const WorldRenderPlayer& player,
            const std::vector<WorldRenderRemotePlayer>& remotePlayers,
            const std::vector<WorldRenderItem>& items,
            GameCameraMode cameraMode
        );

    public:
        struct Vertex
        {
            float Position[3];
            float Color[4];
        };

    private:
        bool CreateShaders(ID3D11Device* device);
        bool CreateBuffers(ID3D11Device* device);
        bool CreateStates(ID3D11Device* device);

        bool UploadAndDraw(
            ID3D11DeviceContext* deviceContext,
            const std::vector<Vertex>& vertices,
            D3D11_PRIMITIVE_TOPOLOGY topology
        );

    private:
        Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

        Microsoft::WRL::ComPtr<ID3D11Buffer> m_dynamicVertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_cameraConstantBuffer;

        Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizerState;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_skyDepthStencilState;

        bool m_initialized = false;
    };
}
