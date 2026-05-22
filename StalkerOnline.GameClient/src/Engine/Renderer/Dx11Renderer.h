#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <cstdint>

namespace StalkerOnline::Engine
{
    class Dx11Renderer final
    {
    public:
        Dx11Renderer() = default;
        ~Dx11Renderer();

        Dx11Renderer(const Dx11Renderer&) = delete;
        Dx11Renderer& operator=(const Dx11Renderer&) = delete;

        bool Initialize(HWND windowHandle);
        void Shutdown();

        void BeginFrame(const float clearColor[4]);
        void EndFrame(bool enableVSync);

        void Resize(std::uint32_t width, std::uint32_t height);

        ID3D11Device* GetDevice() const;
        ID3D11DeviceContext* GetDeviceContext() const;
        IDXGISwapChain* GetSwapChain() const;
        ID3D11RenderTargetView* GetMainRenderTargetView() const;

        std::uint32_t GetWidth() const;
        std::uint32_t GetHeight() const;

    private:
        bool CreateRenderTarget();
        void CleanupRenderTarget();

    private:
        HWND m_windowHandle = nullptr;

        Microsoft::WRL::ComPtr<ID3D11Device> m_device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_deviceContext;
        Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_mainRenderTargetView;

        std::uint32_t m_width = 1280;
        std::uint32_t m_height = 800;

        bool m_initialized = false;
    };
}