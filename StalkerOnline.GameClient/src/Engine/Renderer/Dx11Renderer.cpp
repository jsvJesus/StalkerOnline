#include "Engine/Renderer/Dx11Renderer.h"

namespace StalkerOnline::Engine
{
    Dx11Renderer::~Dx11Renderer()
    {
        Shutdown();
    }

    bool Dx11Renderer::Initialize(HWND windowHandle)
    {
        if (m_initialized)
            return true;

        if (!windowHandle)
            return false;

        m_windowHandle = windowHandle;

        RECT clientRect{};
        GetClientRect(m_windowHandle, &clientRect);

        m_width = static_cast<std::uint32_t>(clientRect.right - clientRect.left);
        m_height = static_cast<std::uint32_t>(clientRect.bottom - clientRect.top);

        if (m_width == 0)
            m_width = 1280;

        if (m_height == 0)
            m_height = 800;

        DXGI_SWAP_CHAIN_DESC swapChainDesc{};
        swapChainDesc.BufferCount = 2;
        swapChainDesc.BufferDesc.Width = m_width;
        swapChainDesc.BufferDesc.Height = m_height;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
        swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
        swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow = m_windowHandle;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Windowed = TRUE;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT createDeviceFlags = 0;

        D3D_FEATURE_LEVEL featureLevel{};
        const D3D_FEATURE_LEVEL featureLevelArray[] =
        {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_0
        };

        const UINT featureLevelCount = static_cast<UINT>(
            sizeof(featureLevelArray) / sizeof(featureLevelArray[0])
        );

        const HRESULT result = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createDeviceFlags,
            featureLevelArray,
            featureLevelCount,
            D3D11_SDK_VERSION,
            &swapChainDesc,
            m_swapChain.GetAddressOf(),
            m_device.GetAddressOf(),
            &featureLevel,
            m_deviceContext.GetAddressOf()
        );

        if (FAILED(result))
            return false;

        if (!CreateRenderTarget())
        {
            Shutdown();
            return false;
        }

        m_initialized = true;
        return true;
    }

    void Dx11Renderer::Shutdown()
    {
        CleanupRenderTarget();

        if (m_deviceContext)
            m_deviceContext->Flush();

        m_swapChain.Reset();
        m_deviceContext.Reset();
        m_device.Reset();

        m_windowHandle = nullptr;
        m_initialized = false;
    }

    void Dx11Renderer::BeginFrame(const float clearColor[4])
    {
        if (!m_initialized)
            return;

        if (!m_deviceContext || !m_mainRenderTargetView)
            return;

        ID3D11RenderTargetView* renderTarget = m_mainRenderTargetView.Get();

        m_deviceContext->OMSetRenderTargets(
            1,
            &renderTarget,
            nullptr
        );

        m_deviceContext->ClearRenderTargetView(
            m_mainRenderTargetView.Get(),
            clearColor
        );
    }

    void Dx11Renderer::EndFrame(bool enableVSync)
    {
        if (!m_initialized || !m_swapChain)
            return;

        m_swapChain->Present(enableVSync ? 1 : 0, 0);
    }

    void Dx11Renderer::Resize(std::uint32_t width, std::uint32_t height)
    {
        if (!m_initialized)
            return;

        if (width == 0 || height == 0)
            return;

        if (!m_swapChain)
            return;

        m_width = width;
        m_height = height;

        CleanupRenderTarget();

        const HRESULT result = m_swapChain->ResizeBuffers(
            0,
            m_width,
            m_height,
            DXGI_FORMAT_UNKNOWN,
            0
        );

        if (FAILED(result))
            return;

        CreateRenderTarget();
    }

    ID3D11Device* Dx11Renderer::GetDevice() const
    {
        return m_device.Get();
    }

    ID3D11DeviceContext* Dx11Renderer::GetDeviceContext() const
    {
        return m_deviceContext.Get();
    }

    IDXGISwapChain* Dx11Renderer::GetSwapChain() const
    {
        return m_swapChain.Get();
    }

    ID3D11RenderTargetView* Dx11Renderer::GetMainRenderTargetView() const
    {
        return m_mainRenderTargetView.Get();
    }

    std::uint32_t Dx11Renderer::GetWidth() const
    {
        return m_width;
    }

    std::uint32_t Dx11Renderer::GetHeight() const
    {
        return m_height;
    }

    bool Dx11Renderer::CreateRenderTarget()
    {
        if (!m_swapChain || !m_device)
            return false;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;

        const HRESULT getBufferResult = m_swapChain->GetBuffer(
            0,
            IID_PPV_ARGS(backBuffer.GetAddressOf())
        );

        if (FAILED(getBufferResult) || !backBuffer)
            return false;

        const HRESULT createViewResult = m_device->CreateRenderTargetView(
            backBuffer.Get(),
            nullptr,
            m_mainRenderTargetView.GetAddressOf()
        );

        return SUCCEEDED(createViewResult);
    }

    void Dx11Renderer::CleanupRenderTarget()
    {
        m_mainRenderTargetView.Reset();
    }
}