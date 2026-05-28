#include "Editor/Dx11LevelEditorRenderer.h"
#include "Editor/Heightmap.h"
#include "Engine/Renderer/Dx11Renderer.h"
#include "UI/UiStyle.h"

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

#include <Windows.h>
#include <commdlg.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

namespace
{
    constexpr int WindowWidth = 1600;
    constexpr int WindowHeight = 900;

    HWND g_windowHandle = nullptr;

    std::unique_ptr<StalkerOnline::Engine::Dx11Renderer> g_renderer;
    std::unique_ptr<StalkerOnline::Editor::Dx11LevelEditorRenderer> g_levelRenderer;

    StalkerOnline::Editor::Heightmap g_heightmap;
    StalkerOnline::Editor::LevelEditorRenderSettings g_renderSettings;

    std::atomic_bool g_running = true;

    char g_rawPath[260] = {};
    char g_savePath[260] = {};
    char g_statusText[512] = "Ready.";

    int g_rawWidth = 513;
    int g_rawHeight = 513;
    int g_rawFormatIndex = 1;
    int g_brushModeIndex = 0;

    float g_brushStrength = 0.45f;
    float g_flattenHeight = 0.5f;

    bool g_isDirty = false;

    StalkerOnline::Editor::RawHeightFormat FormatFromIndex(int index)
    {
        switch (index)
        {
            case 0:
                return StalkerOnline::Editor::RawHeightFormat::UInt8;

            case 1:
                return StalkerOnline::Editor::RawHeightFormat::UInt16LE;

            case 2:
                return StalkerOnline::Editor::RawHeightFormat::Float32LE;

            default:
                return StalkerOnline::Editor::RawHeightFormat::UInt16LE;
        }
    }

    StalkerOnline::Editor::TerrainBrushMode BrushModeFromIndex(int index)
    {
        switch (index)
        {
            case 0:
                return StalkerOnline::Editor::TerrainBrushMode::Raise;

            case 1:
                return StalkerOnline::Editor::TerrainBrushMode::Lower;

            case 2:
                return StalkerOnline::Editor::TerrainBrushMode::Smooth;

            case 3:
                return StalkerOnline::Editor::TerrainBrushMode::Flatten;

            default:
                return StalkerOnline::Editor::TerrainBrushMode::Raise;
        }
    }

    void SetStatus(const std::string& text)
    {
        std::snprintf(g_statusText, sizeof(g_statusText), "%s", text.c_str());
    }

    void CopyToBuffer(char* destination, std::size_t destinationSize, const std::string& value)
    {
        if (destination == nullptr || destinationSize == 0)
            return;

        std::snprintf(destination, destinationSize, "%s", value.c_str());
    }

    std::string ShowOpenRawDialog()
    {
        char fileName[MAX_PATH] = {};

        OPENFILENAMEA openFileName{};
        openFileName.lStructSize = sizeof(openFileName);
        openFileName.hwndOwner = g_windowHandle;
        openFileName.lpstrFilter = "RAW Heightmap (*.raw)\0*.raw\0All Files (*.*)\0*.*\0";
        openFileName.lpstrFile = fileName;
        openFileName.nMaxFile = MAX_PATH;
        openFileName.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        openFileName.lpstrDefExt = "raw";

        if (!GetOpenFileNameA(&openFileName))
            return {};

        return fileName;
    }

    std::string ShowSaveRawDialog()
    {
        char fileName[MAX_PATH] = {};

        if (g_savePath[0] != '\0')
            std::snprintf(fileName, sizeof(fileName), "%s", g_savePath);
        else if (g_rawPath[0] != '\0')
            std::snprintf(fileName, sizeof(fileName), "%s", g_rawPath);

        OPENFILENAMEA saveFileName{};
        saveFileName.lStructSize = sizeof(saveFileName);
        saveFileName.hwndOwner = g_windowHandle;
        saveFileName.lpstrFilter = "RAW Heightmap (*.raw)\0*.raw\0All Files (*.*)\0*.*\0";
        saveFileName.lpstrFile = fileName;
        saveFileName.nMaxFile = MAX_PATH;
        saveFileName.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        saveFileName.lpstrDefExt = "raw";

        if (!GetSaveFileNameA(&saveFileName))
            return {};

        return fileName;
    }

    void CreateFlatLevel()
    {
        std::string errorMessage;

        if (!g_heightmap.CreateFlat(g_rawWidth, g_rawHeight, g_flattenHeight, &errorMessage))
        {
            SetStatus("New level failed: " + errorMessage);
            return;
        }

        g_renderSettings.PanX = 0.0f;
        g_renderSettings.PanZ = 0.0f;
        g_renderSettings.Zoom = 1.0f;
        g_isDirty = true;

        SetStatus(
            "New flat RAW heightmap: " +
            std::to_string(g_heightmap.GetWidth()) +
            "x" +
            std::to_string(g_heightmap.GetHeight()) +
            ".");
    }

    void LoadRawHeightmap(const std::string& path)
    {
        if (path.empty())
            return;

        StalkerOnline::Editor::RawHeightmapOptions options;
        options.Width = g_rawWidth;
        options.Height = g_rawHeight;
        options.Format = FormatFromIndex(g_rawFormatIndex);

        std::string errorMessage;

        if (!g_heightmap.LoadRaw(path, options, &errorMessage))
        {
            SetStatus("Load RAW failed: " + errorMessage);
            return;
        }

        CopyToBuffer(g_rawPath, sizeof(g_rawPath), path);
        CopyToBuffer(g_savePath, sizeof(g_savePath), path);

        g_renderSettings.PanX = 0.0f;
        g_renderSettings.PanZ = 0.0f;
        g_renderSettings.Zoom = 1.0f;
        g_isDirty = false;

        SetStatus("Loaded RAW heightmap: " + path);
    }

    void SaveRawHeightmap(const std::string& path)
    {
        if (path.empty())
        {
            SetStatus("Save failed: path is empty.");
            return;
        }

        std::string errorMessage;

        if (!g_heightmap.SaveRaw(path, FormatFromIndex(g_rawFormatIndex), &errorMessage))
        {
            SetStatus("Save RAW failed: " + errorMessage);
            return;
        }

        CopyToBuffer(g_savePath, sizeof(g_savePath), path);
        g_isDirty = false;

        SetStatus("Saved RAW heightmap: " + path);
    }

    float CalculateOrthoWidth(std::uint32_t viewportWidth, std::uint32_t viewportHeight)
    {
        if (!g_heightmap.IsValid())
            return 1.0f;

        const float aspectRatio = viewportHeight == 0
            ? 1.0f
            : static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);

        const float terrainWidth = static_cast<float>(g_heightmap.GetWidth() - 1) * g_renderSettings.CellSize;
        const float terrainHeight = static_cast<float>(g_heightmap.GetHeight() - 1) * g_renderSettings.CellSize;

        const float fittedWidth = std::max(
            terrainWidth * 1.15f,
            terrainHeight * 1.15f * aspectRatio);

        return std::max(1.0f, fittedWidth / std::clamp(g_renderSettings.Zoom, 0.05f, 64.0f));
    }

    bool ScreenToHeightmap(
        float screenX,
        float screenY,
        float* outHeightmapX,
        float* outHeightmapY)
    {
        if (!g_renderer || !g_heightmap.IsValid() || outHeightmapX == nullptr || outHeightmapY == nullptr)
            return false;

        const std::uint32_t viewportWidth = g_renderer->GetWidth();
        const std::uint32_t viewportHeight = g_renderer->GetHeight();

        if (viewportWidth == 0 || viewportHeight == 0)
            return false;

        const float localX = screenX;
        const float localY = screenY;

        if (localX < 0.0f || localY < 0.0f || localX >= static_cast<float>(viewportWidth) || localY >= static_cast<float>(viewportHeight))
            return false;

        const float aspectRatio = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);
        const float orthoWidth = CalculateOrthoWidth(viewportWidth, viewportHeight);
        const float orthoHeight = orthoWidth / aspectRatio;

        const float ndcX = localX / static_cast<float>(viewportWidth) * 2.0f - 1.0f;
        const float ndcY = 1.0f - localY / static_cast<float>(viewportHeight) * 2.0f;

        const float worldX = g_renderSettings.PanX + ndcX * orthoWidth * 0.5f;
        const float worldZ = g_renderSettings.PanZ + ndcY * orthoHeight * 0.5f;

        *outHeightmapX = worldX / g_renderSettings.CellSize + static_cast<float>(g_heightmap.GetWidth() - 1) * 0.5f;
        *outHeightmapY = worldZ / g_renderSettings.CellSize + static_cast<float>(g_heightmap.GetHeight() - 1) * 0.5f;

        return
            *outHeightmapX >= 0.0f &&
            *outHeightmapY >= 0.0f &&
            *outHeightmapX <= static_cast<float>(g_heightmap.GetWidth() - 1) &&
            *outHeightmapY <= static_cast<float>(g_heightmap.GetHeight() - 1);
    }

    void HandleEditorInput()
    {
        ImGuiIO& io = ImGui::GetIO();

        if (!g_heightmap.IsValid())
            return;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float mouseX = io.MousePos.x - viewport->Pos.x;
        const float mouseY = io.MousePos.y - viewport->Pos.y;

        float heightmapX = 0.0f;
        float heightmapY = 0.0f;
        const bool mouseOverHeightmap = ScreenToHeightmap(mouseX, mouseY, &heightmapX, &heightmapY);

        g_renderSettings.BrushX = heightmapX;
        g_renderSettings.BrushY = heightmapY;
        g_renderSettings.ShowBrush = mouseOverHeightmap && !io.WantCaptureMouse;

        if (!io.WantCaptureKeyboard)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_1))
                g_brushModeIndex = 0;

            if (ImGui::IsKeyPressed(ImGuiKey_2))
                g_brushModeIndex = 1;

            if (ImGui::IsKeyPressed(ImGuiKey_3))
                g_brushModeIndex = 2;

            if (ImGui::IsKeyPressed(ImGuiKey_4))
                g_brushModeIndex = 3;
        }

        if (!io.WantCaptureMouse)
        {
            if (io.MouseWheel != 0.0f)
            {
                const float zoomFactor = io.MouseWheel > 0.0f ? 1.12f : 0.89f;
                g_renderSettings.Zoom = std::clamp(g_renderSettings.Zoom * zoomFactor, 0.05f, 64.0f);
            }

            if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
            {
                const std::uint32_t viewportWidth = g_renderer->GetWidth();
                const std::uint32_t viewportHeight = g_renderer->GetHeight();

                if (viewportWidth > 0 && viewportHeight > 0)
                {
                    const float aspectRatio = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);
                    const float orthoWidth = CalculateOrthoWidth(viewportWidth, viewportHeight);
                    const float orthoHeight = orthoWidth / aspectRatio;

                    g_renderSettings.PanX -= io.MouseDelta.x / static_cast<float>(viewportWidth) * orthoWidth;
                    g_renderSettings.PanZ += io.MouseDelta.y / static_cast<float>(viewportHeight) * orthoHeight;
                }
            }
        }

        if (mouseOverHeightmap && !io.WantCaptureMouse && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            StalkerOnline::Editor::TerrainBrushMode brushMode = BrushModeFromIndex(g_brushModeIndex);

            if (io.KeyShift)
                brushMode = StalkerOnline::Editor::TerrainBrushMode::Lower;

            g_heightmap.ApplyBrush(
                heightmapX,
                heightmapY,
                g_renderSettings.BrushRadiusCells,
                g_brushStrength,
                std::max(io.DeltaTime, 0.001f),
                brushMode,
                g_flattenHeight);

            g_isDirty = true;
        }
    }

    void DrawTopToolbar()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 42.0f));

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::Begin("##LevelEditorTopToolbar", nullptr, flags);

        if (ImGui::Button("New Flat"))
            CreateFlatLevel();

        ImGui::SameLine();

        if (ImGui::Button("Open RAW"))
        {
            const std::string path = ShowOpenRawDialog();

            if (!path.empty())
                LoadRawHeightmap(path);
        }

        ImGui::SameLine();

        if (ImGui::Button("Load Path"))
            LoadRawHeightmap(g_rawPath);

        ImGui::SameLine();

        if (ImGui::Button("Save"))
            SaveRawHeightmap(g_savePath[0] != '\0' ? g_savePath : g_rawPath);

        ImGui::SameLine();

        if (ImGui::Button("Save As"))
        {
            const std::string path = ShowSaveRawDialog();

            if (!path.empty())
                SaveRawHeightmap(path);
        }

        ImGui::SameLine();

        ImGui::TextColored(
            g_isDirty ? ImVec4(0.92f, 0.68f, 0.22f, 1.0f) : ImVec4(0.55f, 0.75f, 0.45f, 1.0f),
            "%s",
            g_isDirty ? "Dirty" : "Saved");

        ImGui::SameLine();
        ImGui::Text("| %s", g_statusText);

        ImGui::End();
    }

    void DrawImportPanel()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + 16.0f, viewport->Pos.y + 58.0f));
        ImGui::SetNextWindowSize(ImVec2(360.0f, 370.0f));

        ImGui::Begin("RAW Heightmap", nullptr, ImGuiWindowFlags_NoCollapse);

        ImGui::InputText("RAW path", g_rawPath, sizeof(g_rawPath));
        ImGui::InputText("Save path", g_savePath, sizeof(g_savePath));

        ImGui::Separator();

        ImGui::InputInt("Width", &g_rawWidth);
        ImGui::InputInt("Height", &g_rawHeight);
        g_rawWidth = std::clamp(g_rawWidth, 2, 8192);
        g_rawHeight = std::clamp(g_rawHeight, 2, 8192);

        const char* formats[] =
        {
            "8-bit unsigned",
            "16-bit unsigned LE",
            "32-bit float LE"
        };

        ImGui::Combo("RAW format", &g_rawFormatIndex, formats, IM_ARRAYSIZE(formats));

        ImGui::Separator();

        ImGui::DragFloat("Cell size", &g_renderSettings.CellSize, 1.0f, 1.0f, 1000.0f, "%.1f");
        ImGui::DragFloat("Height scale", &g_renderSettings.HeightScale, 10.0f, 1.0f, 20000.0f, "%.1f");
        ImGui::SliderInt("Render cells", &g_renderSettings.MaxRenderedCellsPerAxis, 32, 512);
        ImGui::Checkbox("Wireframe", &g_renderSettings.ShowWireframe);

        ImGui::Separator();

        if (g_heightmap.IsValid())
        {
            ImGui::Text("Loaded: %d x %d", g_heightmap.GetWidth(), g_heightmap.GetHeight());
            ImGui::Text("Format: %s", StalkerOnline::Editor::RawHeightFormatToText(FormatFromIndex(g_rawFormatIndex)));
        }
        else
        {
            ImGui::TextColored(ImVec4(0.90f, 0.35f, 0.25f, 1.0f), "No heightmap loaded.");
        }

        ImGui::End();
    }

    void DrawSculptPanel()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float width = 350.0f;

        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x - width - 16.0f, viewport->Pos.y + 58.0f));
        ImGui::SetNextWindowSize(ImVec2(width, 360.0f));

        ImGui::Begin("Sculpt", nullptr, ImGuiWindowFlags_NoCollapse);

        const char* brushModes[] =
        {
            "Raise",
            "Lower",
            "Smooth",
            "Flatten"
        };

        ImGui::Combo("Mode", &g_brushModeIndex, brushModes, IM_ARRAYSIZE(brushModes));
        ImGui::SliderFloat("Radius", &g_renderSettings.BrushRadiusCells, 1.0f, 128.0f, "%.1f cells");
        ImGui::SliderFloat("Strength", &g_brushStrength, 0.01f, 2.0f, "%.2f / sec");
        ImGui::SliderFloat("Flatten height", &g_flattenHeight, 0.0f, 1.0f, "%.3f");

        ImGui::Separator();

        ImGui::DragFloat("Zoom", &g_renderSettings.Zoom, 0.02f, 0.05f, 64.0f, "%.2f");
        ImGui::DragFloat("Pan X", &g_renderSettings.PanX, 10.0f, -1000000.0f, 1000000.0f, "%.1f");
        ImGui::DragFloat("Pan Z", &g_renderSettings.PanZ, 10.0f, -1000000.0f, 1000000.0f, "%.1f");

        ImGui::Separator();

        ImGui::Text("Brush: %.1f / %.1f", g_renderSettings.BrushX, g_renderSettings.BrushY);

        if (g_heightmap.IsValid())
        {
            const int sampleX = std::clamp(static_cast<int>(g_renderSettings.BrushX), 0, g_heightmap.GetWidth() - 1);
            const int sampleY = std::clamp(static_cast<int>(g_renderSettings.BrushY), 0, g_heightmap.GetHeight() - 1);
            ImGui::Text("Height: %.4f", g_heightmap.GetHeightNormalized(sampleX, sampleY));
        }

        ImGui::TextColored(ImVec4(0.74f, 0.70f, 0.52f, 1.0f), "LMB sculpt | Shift+LMB lower | MMB pan | Wheel zoom");
        ImGui::TextColored(ImVec4(0.74f, 0.70f, 0.52f, 1.0f), "Hotkeys: 1 Raise, 2 Lower, 3 Smooth, 4 Flatten");

        ImGui::End();
    }

    void DrawEditorUi()
    {
        DrawTopToolbar();
        DrawImportPanel();
        DrawSculptPanel();
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
);

LRESULT WINAPI WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam))
        return true;

    switch (message)
    {
        case WM_SIZE:
        {
            if (g_renderer && wParam != SIZE_MINIMIZED)
            {
                const std::uint32_t width = static_cast<std::uint32_t>(LOWORD(lParam));
                const std::uint32_t height = static_cast<std::uint32_t>(HIWORD(lParam));

                g_renderer->Resize(width, height);
            }

            return 0;
        }

        case WM_SYSCOMMAND:
        {
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;

            break;
        }

        case WM_DESTROY:
        {
            g_running.store(false);
            PostQuitMessage(0);
            return 0;
        }

        default:
            break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    PWSTR,
    int nCmdShow
)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.style = CS_CLASSDC;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = hInstance;
    windowClass.hIcon = nullptr;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszMenuName = nullptr;
    windowClass.lpszClassName = L"StalkerOnlineLevelEditorWindow";
    windowClass.hIconSm = nullptr;

    RegisterClassExW(&windowClass);

    g_windowHandle = CreateWindowW(
        windowClass.lpszClassName,
        L"Stalker Online - Level Editor",
        WS_OVERLAPPEDWINDOW,
        80,
        80,
        WindowWidth,
        WindowHeight,
        nullptr,
        nullptr,
        windowClass.hInstance,
        nullptr);

    g_renderer = std::make_unique<StalkerOnline::Engine::Dx11Renderer>();

    if (!g_renderer->Initialize(g_windowHandle))
    {
        g_renderer.reset();
        UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
        return 1;
    }

    ShowWindow(g_windowHandle, nCmdShow);
    UpdateWindow(g_windowHandle);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    StalkerOnline::UI::ApplyStalkerDarkStyle();

    ImGui_ImplWin32_Init(g_windowHandle);
    ImGui_ImplDX11_Init(
        g_renderer->GetDevice(),
        g_renderer->GetDeviceContext());

    g_levelRenderer = std::make_unique<StalkerOnline::Editor::Dx11LevelEditorRenderer>();

    if (!g_levelRenderer->Initialize(g_renderer->GetDevice()))
    {
        MessageBoxW(
            g_windowHandle,
            L"Failed to initialize Dx11LevelEditorRenderer.",
            L"Stalker Online Level Editor",
            MB_ICONERROR);

        g_levelRenderer.reset();

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        g_renderer->Shutdown();
        g_renderer.reset();

        DestroyWindow(g_windowHandle);
        UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
        return 1;
    }

    CreateFlatLevel();

    MSG message{};

    while (g_running.load())
    {
        while (PeekMessageW(&message, nullptr, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);

            if (message.message == WM_QUIT)
                g_running.store(false);
        }

        if (!g_running.load())
            break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        HandleEditorInput();
        DrawEditorUi();

        ImGui::Render();

        const float clearColor[4] = { 0.020f, 0.024f, 0.020f, 1.0f };

        g_renderer->BeginFrame(clearColor);

        g_levelRenderer->Render(
            g_renderer->GetDeviceContext(),
            g_renderer->GetWidth(),
            g_renderer->GetHeight(),
            g_heightmap,
            g_renderSettings);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_renderer->EndFrame(true);
    }

    if (g_levelRenderer)
    {
        g_levelRenderer->Shutdown();
        g_levelRenderer.reset();
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (g_renderer)
    {
        g_renderer->Shutdown();
        g_renderer.reset();
    }

    DestroyWindow(g_windowHandle);
    UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);

    return 0;
}
