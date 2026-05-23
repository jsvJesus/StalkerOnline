#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>

namespace StalkerOnline::Engine
{
    class Dx11DebugTriangle final
    {
    public:
        Dx11DebugTriangle() = default;
        ~Dx11DebugTriangle();

        Dx11DebugTriangle(const Dx11DebugTriangle&) = delete;
        Dx11DebugTriangle& operator=(const Dx11DebugTriangle&) = delete;

        bool Initialize(ID3D11Device* device);
        void Shutdown();

        void Render(
            ID3D11DeviceContext* deviceContext,
            std::uint32_t viewportWidth,
            std::uint32_t viewportHeight
        );

    private:
        struct Vertex
        {
            float Position[3];
            float Color[4];
        };

    private:
        bool CreateShaders(ID3D11Device* device);
        bool CreateGeometry(ID3D11Device* device);

    private:
        Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;

        bool m_initialized = false;
    };
}