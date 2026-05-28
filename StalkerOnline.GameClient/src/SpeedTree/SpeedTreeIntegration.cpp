#include "SpeedTree/SpeedTreeIntegration.h"

#include <cstring>

#if STALKERONLINE_WITH_SPEEDTREE
#include <Core/CoordSys.h>
#include <Core/Core.h>
#endif

#if STALKERONLINE_WITH_SPEEDTREE_DX11
#include <Renderers/DirectX11/DirectX11Renderer.h>
#endif

namespace StalkerOnline::SpeedTreeIntegration
{
    namespace
    {
        bool g_dx11RendererInitialized = false;

        std::string ToString(const char* text)
        {
            return text != nullptr ? text : "";
        }

#if STALKERONLINE_WITH_SPEEDTREE
        void EnsureCoordinateSystem()
        {
            static bool configured = false;

            if (configured)
                return;

            SpeedTree::CCoordSys::SetCoordSys(SpeedTree::CCoordSys::COORD_SYS_RIGHT_HANDED_Y_UP);
            configured = true;
        }
#endif
    }

    Dx11InitializationResult InitializeDx11Renderer(ID3D11Device* device, ID3D11DeviceContext* deviceContext)
    {
        Dx11InitializationResult result;

#if STALKERONLINE_WITH_SPEEDTREE_DX11
        if (device == nullptr || deviceContext == nullptr)
        {
            result.Message = "SpeedTree DX11 renderer init failed: D3D11 device/context is null.";
            return result;
        }

        EnsureCoordinateSystem();

        SpeedTree::DX11::SetDevice(device);
        SpeedTree::DX11::SetDeviceContext(deviceContext);

        g_dx11RendererInitialized = true;
        result.Success = true;
        result.Message = "SpeedTree DX11 renderer initialized.";
#else
        result.Message = "SpeedTree DX11 renderer is not linked in this build.";
#endif

        return result;
    }

    SdkStatus GetSdkStatus()
    {
        SdkStatus status;

#if STALKERONLINE_WITH_SPEEDTREE
        EnsureCoordinateSystem();

        status.Available = true;
        status.Authorized = SpeedTree::CCore::IsAuthorized();
        status.Version = ToString(SpeedTree::CCore::Version(false));
        status.CoordinateSystem = ToString(
            SpeedTree::CCoordSys::CoordSysName(SpeedTree::CCoordSys::GetCoordSysType()));
        status.Message = status.Authorized ? "SpeedTree SDK ready." : "SpeedTree SDK is not authorized.";

#if STALKERONLINE_WITH_SPEEDTREE_DX11
        status.DirectX11RendererAvailable = true;
        status.DirectX11RendererInitialized = g_dx11RendererInitialized;
#endif
#else
        status.Message = "SpeedTree SDK was not found at configure time.";
#endif

        return status;
    }

    TreeAssetInfo TryLoadTreeAsset(const std::string& path)
    {
        TreeAssetInfo result;
        result.FilePath = path;

        if (path.empty())
        {
            result.ErrorMessage = "SRT path is empty.";
            return result;
        }

#if STALKERONLINE_WITH_SPEEDTREE
        EnsureCoordinateSystem();

        SpeedTree::CCore tree;

        if (!tree.LoadTree(path.c_str()))
        {
            result.ErrorMessage = ToString(SpeedTree::CCore::GetError());

            if (result.ErrorMessage.empty())
                result.ErrorMessage = "SpeedTree failed to load SRT asset.";

            return result;
        }

        const SpeedTree::CExtents& extents = tree.GetExtents();
        const SpeedTree::Vec3& minBounds = extents.Min();
        const SpeedTree::Vec3& maxBounds = extents.Max();

        result.Loaded = true;
        result.MinBounds = { minBounds.x, minBounds.y, minBounds.z };
        result.MaxBounds = { maxBounds.x, maxBounds.y, maxBounds.z };
        result.Height = extents.GetHeight();
        result.Radius = extents.ComputeRadiusFromCenter3D();

#if STALKERONLINE_WITH_SPEEDTREE_DX11
        if (g_dx11RendererInitialized)
        {
            SpeedTree::CTreeRender renderTree;

            if (renderTree.LoadTree(path.c_str()))
            {
                const SpeedTree::CFixedString srtPath =
                    SpeedTree::CFixedString(renderTree.GetFilename()).Path();

                SpeedTree::CStaticArray<SpeedTree::CFixedString> searchPaths(
                    5,
                    "StalkerOnline::SpeedTreeIntegration::TryLoadTreeAsset",
                    false);

                searchPaths.push_back("");

                const char* shaderPath = renderTree.GetGeometry()->m_strShaderPath;

                if (shaderPath != nullptr && std::strlen(shaderPath) > 0)
                {
                    searchPaths.push_back(
                        srtPath +
                        SpeedTree::c_szFolderSeparator +
                        SpeedTree::CFixedString(shaderPath) +
                        SpeedTree::c_szFolderSeparator +
                        SpeedTree::CShaderTechnique::GetCompiledShaderFolder());
                }

                searchPaths.push_back(srtPath);
                searchPaths.push_back(srtPath + SpeedTree::CShaderTechnique::GetCompiledShaderFolder());

                SpeedTree::SAppState appState;
                appState.m_bMultisampling = false;
                appState.m_bAlphaToCoverage = false;
                appState.m_bDepthPrepass = false;
                appState.m_bDeferred = false;

                result.RenderInterfaceInitialized = renderTree.InitGfx(appState, searchPaths, 4, 1.0f);

                if (result.RenderInterfaceInitialized)
                {
                    result.RenderInterfaceMessage = "SpeedTree Render Interface graphics initialized.";
                    renderTree.ReleaseGfxResources();
                }
                else
                {
                    result.RenderInterfaceMessage = ToString(SpeedTree::CCore::GetError());

                    if (result.RenderInterfaceMessage.empty())
                        result.RenderInterfaceMessage = "SpeedTree Render Interface graphics init failed.";
                }
            }
            else
            {
                result.RenderInterfaceMessage = ToString(SpeedTree::CCore::GetError());

                if (result.RenderInterfaceMessage.empty())
                    result.RenderInterfaceMessage = "SpeedTree Render Interface tree load failed.";
            }
        }
        else
        {
            result.RenderInterfaceMessage = "SpeedTree DX11 renderer is linked but not initialized.";
        }
#endif
#else
        result.ErrorMessage = "SpeedTree SDK is not available in this build.";
#endif

        return result;
    }
}
