#pragma once

#include "Editor/EditorScene.h"
#include "Editor/Heightmap.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <vector>

namespace StalkerOnline::Editor
{
    struct LevelEditorRenderSettings
    {
        float CellSizeX = 100.0f;
        float CellSizeY = 100.0f;
        float HeightScale = 3000.0f;

        float CameraX = 0.0f;
        float CameraY = 22000.0f;
        float CameraZ = -36000.0f;

        float CameraYaw = 0.0f;
        float CameraPitch = -32.0f;
        float CameraSpeed = 7000.0f;

        float BrushX = 0.0f;
        float BrushY = 0.0f;
        float BrushRadiusCells = 16.0f;

        int MaxRenderedCellsPerAxis = 256;

        float PreviewMinHeight = 0.0f;
        float PreviewMaxHeight = 1.0f;

        bool NormalizeHeightPreview = true;
        bool ShowBrush = false;
        bool ShowWireframe = true;
    };

    class Dx11LevelEditorRenderer final
    {
    public:
        Dx11LevelEditorRenderer() = default;
        ~Dx11LevelEditorRenderer();

        Dx11LevelEditorRenderer(const Dx11LevelEditorRenderer&) = delete;
        Dx11LevelEditorRenderer& operator=(const Dx11LevelEditorRenderer&) = delete;

        bool Initialize(ID3D11Device* device);
        void Shutdown();

        void Render(
            ID3D11DeviceContext* deviceContext,
            std::uint32_t viewportWidth,
            std::uint32_t viewportHeight,
            const Heightmap& heightmap,
            const LevelEditorRenderSettings& settings,
            const EditorScene& scene);

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
            D3D11_PRIMITIVE_TOPOLOGY topology,
            std::size_t primitiveVertexCount);

    private:
        Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

        Microsoft::WRL::ComPtr<ID3D11Buffer> m_dynamicVertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_cameraConstantBuffer;

        Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizerState;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthStencilState;

        bool m_initialized = false;
    };
}