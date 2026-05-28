#pragma once

#include <array>
#include <string>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace StalkerOnline::SpeedTreeIntegration
{
    struct SdkStatus
    {
        bool Available = false;
        bool Authorized = false;
        bool DirectX11RendererAvailable = false;
        bool DirectX11RendererInitialized = false;
        std::string Version;
        std::string CoordinateSystem;
        std::string Message;
    };

    struct Dx11InitializationResult
    {
        bool Success = false;
        std::string Message;
    };

    struct TreeAssetInfo
    {
        bool Loaded = false;
        bool RenderInterfaceInitialized = false;
        std::string FilePath;
        std::string ErrorMessage;
        std::string RenderInterfaceMessage;

        std::array<float, 3> MinBounds = { 0.0f, 0.0f, 0.0f };
        std::array<float, 3> MaxBounds = { 0.0f, 0.0f, 0.0f };
        float Height = 0.0f;
        float Radius = 0.0f;
    };

    Dx11InitializationResult InitializeDx11Renderer(ID3D11Device* device, ID3D11DeviceContext* deviceContext);
    SdkStatus GetSdkStatus();
    TreeAssetInfo TryLoadTreeAsset(const std::string& path);
}
