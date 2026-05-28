#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Editor/Dx11LevelEditorRenderer.h"
#include "Editor/EditorScene.h"
#include "Editor/Heightmap.h"
#include "Engine/Renderer/Dx11Renderer.h"
#include "SpeedTree/SpeedTreeIntegration.h"
#include "UI/UiStyle.h"

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

#include <Windows.h>
#include <commdlg.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cctype>
#include <cmath>
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
    StalkerOnline::Editor::EditorScene g_editorScene;

    std::atomic_bool g_running = true;

    char g_rawPath[260] = {};
    char g_savePath[260] = {};
    char g_levelPath[260] = {};
    char g_speedTreeAssetPath[260] = {};
    char g_statusText[512] = "Ready.";

    int g_rawWidth = 513;
    int g_rawHeight = 513;
    int g_rawFormatIndex = 0;
    int g_brushModeIndex = 0;

    float g_brushStrength = 0.45f;
    float g_flattenHeight = 0.5f;
    float g_ueZScale = 13.498139f;

    bool g_autoDetectSquareSize = true;
    bool g_isDirty = false;

    enum class StudioTab : int
    {
        Settings = 0,
        Terrain,
        Objects,
        Materials,
        Environment,
        Collections,
        Decorators,
        Roads,
        Gameplay,
        PostFX,
        ColorCorrection
    };

    StudioTab g_activeStudioTab = StudioTab::Settings;

    bool g_showCameraPanel = true;
    bool g_showMapPanel = false;
    bool g_showShadowsPanel = false;
    bool g_showMiscPanel = false;

    float g_cameraZNear = 0.10f;
    float g_cameraZFar = 3000.0f;
    float g_timeOfDay = 12.0f;
    float g_editorSpeed = 5.0f;

    float g_heightMin = 0.0f;
    float g_heightMax = 1.0f;

    StalkerOnline::SpeedTreeIntegration::TreeAssetInfo g_speedTreeAsset;
    void LoadRawHeightmap(const std::string& path);
    void CreatePreviewIslandLevel();

    StalkerOnline::Editor::RawHeightFormat FormatFromIndex(int index)
    {
        switch (index)
        {
            case 0:
                return StalkerOnline::Editor::RawHeightFormat::R16;

            case 1:
                return StalkerOnline::Editor::RawHeightFormat::UInt8;

            case 2:
                return StalkerOnline::Editor::RawHeightFormat::UInt16LE;

            case 3:
                return StalkerOnline::Editor::RawHeightFormat::Float32LE;

            default:
                return StalkerOnline::Editor::RawHeightFormat::R16;
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

    bool EndsWithCaseInsensitive(const std::string& value, const std::string& suffix)
    {
        if (suffix.size() > value.size())
            return false;

        const std::size_t offset = value.size() - suffix.size();

        for (std::size_t i = 0; i < suffix.size(); ++i)
        {
            const auto a = static_cast<unsigned char>(value[offset + i]);
            const auto b = static_cast<unsigned char>(suffix[i]);

            if (std::tolower(a) != std::tolower(b))
                return false;
        }

        return true;
    }

    void SelectFormatFromPath(const std::string& path)
    {
        if (EndsWithCaseInsensitive(path, ".r16"))
            g_rawFormatIndex = 0;
    }

    void RefreshHeightStats()
    {
        if (!g_heightmap.IsValid() || g_heightmap.GetValues().empty())
        {
            g_heightMin = 0.0f;
            g_heightMax = 1.0f;
        }
        else
        {
            const auto [minIt, maxIt] = std::minmax_element(
                g_heightmap.GetValues().begin(),
                g_heightmap.GetValues().end());

            g_heightMin = *minIt;
            g_heightMax = *maxIt;
        }

        g_renderSettings.PreviewMinHeight = g_heightMin;
        g_renderSettings.PreviewMaxHeight = g_heightMax;
    }

    bool AutoDetectRawDimensions(const std::string& path, bool updateStatusOnFailure)
    {
        int detectedWidth = 0;
        int detectedHeight = 0;
        std::string errorMessage;

        if (!StalkerOnline::Editor::TryDetectSquareRawDimensions(
            path,
            FormatFromIndex(g_rawFormatIndex),
            &detectedWidth,
            &detectedHeight,
            &errorMessage))
        {
            if (updateStatusOnFailure)
                SetStatus("Auto size failed: " + errorMessage);

            return false;
        }

        g_rawWidth = detectedWidth;
        g_rawHeight = detectedHeight;

        SetStatus(
            "Auto-detected terrain size: " +
            std::to_string(g_rawWidth) +
            "x" +
            std::to_string(g_rawHeight) +
            ".");

        return true;
    }

    void ResetCameraToHeightmap()
    {
        if (!g_heightmap.IsValid())
            return;

        const float terrainWidth = static_cast<float>(g_heightmap.GetWidth() - 1) * g_renderSettings.CellSizeX;
        const float terrainHeight = static_cast<float>(g_heightmap.GetHeight() - 1) * g_renderSettings.CellSizeY;
        const float maxTerrainAxis = (std::max)(terrainWidth, terrainHeight);

        g_renderSettings.CameraX = 0.0f;
        g_renderSettings.CameraY = (std::max)(g_renderSettings.HeightScale * 2.5f, maxTerrainAxis * 0.42f);
        g_renderSettings.CameraZ = -(std::max)(1000.0f, maxTerrainAxis * 0.72f);
        g_renderSettings.CameraYaw = 0.0f;
        g_renderSettings.CameraPitch = -32.0f;
        g_renderSettings.CameraSpeed = (std::max)(700.0f, maxTerrainAxis * 0.14f);
    }

    std::string ShowOpenRawDialog()
    {
        char fileName[MAX_PATH] = {};

        OPENFILENAMEA openFileName{};
        openFileName.lStructSize = sizeof(openFileName);
        openFileName.hwndOwner = g_windowHandle;
        openFileName.lpstrFilter =
            "Terrain Heightmaps (*.r16;*.raw)\0*.r16;*.raw\0"
            "UE5 R16 Heightmap (*.r16)\0*.r16\0"
            "RAW Heightmap (*.raw)\0*.raw\0"
            "All Files (*.*)\0*.*\0";
        openFileName.lpstrFile = fileName;
        openFileName.nMaxFile = MAX_PATH;
        openFileName.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        openFileName.lpstrDefExt = "r16";

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
        saveFileName.lpstrFilter =
            "UE5 R16 Heightmap (*.r16)\0*.r16\0"
            "RAW Heightmap (*.raw)\0*.raw\0"
            "All Files (*.*)\0*.*\0";
        saveFileName.lpstrFile = fileName;
        saveFileName.nMaxFile = MAX_PATH;
        saveFileName.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        saveFileName.lpstrDefExt = "r16";

        if (!GetSaveFileNameA(&saveFileName))
            return {};

        return fileName;
    }

    std::string ShowOpenLevelDialog()
    {
        char fileName[MAX_PATH] = {};

        OPENFILENAMEA openFileName{};
        openFileName.lStructSize = sizeof(openFileName);
        openFileName.hwndOwner = g_windowHandle;
        openFileName.lpstrFilter =
            "Stalker Online Level (*.sollevel)\0*.sollevel\0"
            "All Files (*.*)\0*.*\0";
        openFileName.lpstrFile = fileName;
        openFileName.nMaxFile = MAX_PATH;
        openFileName.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        openFileName.lpstrDefExt = "sollevel";

        if (!GetOpenFileNameA(&openFileName))
            return {};

        return fileName;
    }

    std::string ShowSaveLevelDialog()
    {
        char fileName[MAX_PATH] = {};

        if (g_levelPath[0] != '\0')
            std::snprintf(fileName, sizeof(fileName), "%s", g_levelPath);

        OPENFILENAMEA saveFileName{};
        saveFileName.lStructSize = sizeof(saveFileName);
        saveFileName.hwndOwner = g_windowHandle;
        saveFileName.lpstrFilter =
            "Stalker Online Level (*.sollevel)\0*.sollevel\0"
            "All Files (*.*)\0*.*\0";
        saveFileName.lpstrFile = fileName;
        saveFileName.nMaxFile = MAX_PATH;
        saveFileName.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        saveFileName.lpstrDefExt = "sollevel";

        if (!GetSaveFileNameA(&saveFileName))
            return {};

        return fileName;
    }

    void SyncSceneTerrainInfo()
    {
        const int terrainWidth = g_heightmap.IsValid() ? g_heightmap.GetWidth() : g_rawWidth;
        const int terrainHeight = g_heightmap.IsValid() ? g_heightmap.GetHeight() : g_rawHeight;

        g_editorScene.SetTerrainInfo(
            g_rawPath,
            terrainWidth,
            terrainHeight,
            g_renderSettings.CellSizeX,
            g_renderSettings.CellSizeY,
            g_renderSettings.HeightScale);
    }

    void SaveLevelFile(const std::string& path)
    {
        if (path.empty())
        {
            SetStatus("Save level failed: path is empty.");
            return;
        }

        SyncSceneTerrainInfo();

        std::string errorMessage;

        if (!g_editorScene.SaveToFile(path, &errorMessage))
        {
            SetStatus("Save level failed: " + errorMessage);
            return;
        }

        CopyToBuffer(g_levelPath, sizeof(g_levelPath), path);
        g_isDirty = false;

        SetStatus("Saved level: " + path);
    }

    void LoadLevelFile(const std::string& path)
    {
        if (path.empty())
            return;

        std::string errorMessage;

        if (!g_editorScene.LoadFromFile(path, &errorMessage))
        {
            SetStatus("Load level failed: " + errorMessage);
            return;
        }

        CopyToBuffer(g_levelPath, sizeof(g_levelPath), path);

        g_renderSettings.CellSizeX = g_editorScene.GetTerrainCellSizeX();
        g_renderSettings.CellSizeY = g_editorScene.GetTerrainCellSizeY();
        g_renderSettings.HeightScale = g_editorScene.GetTerrainHeightScale();
        g_ueZScale = g_renderSettings.HeightScale / 512.0f;

        if (!g_editorScene.GetTerrainHeightmapPath().empty())
        {
            CopyToBuffer(g_rawPath, sizeof(g_rawPath), g_editorScene.GetTerrainHeightmapPath());
            LoadRawHeightmap(g_editorScene.GetTerrainHeightmapPath());
        }

        g_isDirty = false;
        SetStatus("Loaded level: " + path);
    }

    std::string ShowOpenSpeedTreeDialog()
    {
        char fileName[MAX_PATH] = {};

        OPENFILENAMEA openFileName{};
        openFileName.lStructSize = sizeof(openFileName);
        openFileName.hwndOwner = g_windowHandle;
        openFileName.lpstrFilter =
            "SpeedTree Assets (*.srt)\0*.srt\0"
            "All Files (*.*)\0*.*\0";
        openFileName.lpstrFile = fileName;
        openFileName.nMaxFile = MAX_PATH;
        openFileName.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        openFileName.lpstrDefExt = "srt";

        if (!GetOpenFileNameA(&openFileName))
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

        RefreshHeightStats();
        ResetCameraToHeightmap();
        g_isDirty = true;
        g_editorScene.Clear();
        SyncSceneTerrainInfo();

        SetStatus(
            "New flat RAW heightmap: " +
            std::to_string(g_heightmap.GetWidth()) +
            "x" +
            std::to_string(g_heightmap.GetHeight()) +
            ".");
    }

    float SimpleNoise2D(float x, float y)
    {
        const float value =
            std::sin(x * 12.9898f + y * 78.233f) *
            43758.5453f;

        return value - std::floor(value);
    }

    float SmoothStep(float edge0, float edge1, float x)
    {
        const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    void AddPreviewObject(
        StalkerOnline::Editor::EditorObjectType type,
        const std::string& name,
        float x,
        float y,
        float z,
        float radius)
    {
        StalkerOnline::Editor::EditorObject object;
        object.Type = type;
        object.Name = name;
        object.Position = { x, y, z };
        object.Rotation = { 0.0f, SimpleNoise2D(x * 0.001f, z * 0.001f) * 360.0f, 0.0f };
        object.Scale = { 1.0f, 1.0f, 1.0f };
        object.Radius = radius;
        object.Visible = true;

        g_editorScene.AddObject(object);
    }

    void CreatePreviewIslandLevel()
    {
        g_rawWidth = 513;
        g_rawHeight = 513;
        g_renderSettings.CellSizeX = 100.0f;
        g_renderSettings.CellSizeY = 100.0f;
        g_renderSettings.HeightScale = 9000.0f;
        g_ueZScale = g_renderSettings.HeightScale / 512.0f;

        std::string errorMessage;

        if (!g_heightmap.CreateFlat(g_rawWidth, g_rawHeight, 0.35f, &errorMessage))
        {
            SetStatus("Preview island failed: " + errorMessage);
            return;
        }

        const float centerX = static_cast<float>(g_rawWidth - 1) * 0.5f;
        const float centerY = static_cast<float>(g_rawHeight - 1) * 0.5f;

        for (int y = 0; y < g_rawHeight; ++y)
        {
            for (int x = 0; x < g_rawWidth; ++x)
            {
                const float nx = (static_cast<float>(x) - centerX) / centerX;
                const float ny = (static_cast<float>(y) - centerY) / centerY;
                const float distance = std::sqrt(nx * nx + ny * ny);

                const float islandMask = 1.0f - SmoothStep(0.62f, 1.05f, distance);

                float height = 0.30f;

                const float hillA =
                    std::exp(-((nx + 0.25f) * (nx + 0.25f) + (ny - 0.15f) * (ny - 0.15f)) * 8.0f);

                const float hillB =
                    std::exp(-((nx - 0.30f) * (nx - 0.30f) + (ny + 0.20f) * (ny + 0.20f)) * 14.0f);

                const float ridge =
                    std::exp(-((nx + ny * 0.35f) * (nx + ny * 0.35f)) * 22.0f) * 0.20f;

                const float noise =
                    (SimpleNoise2D(static_cast<float>(x) * 0.045f, static_cast<float>(y) * 0.045f) - 0.5f) * 0.035f;

                height += islandMask * (hillA * 0.42f + hillB * 0.28f + ridge + noise);

                const float beach = SmoothStep(0.66f, 0.92f, distance);
                height = height * (1.0f - beach) + 0.315f * beach;

                g_heightmap.SetHeightNormalized(x, y, std::clamp(height, 0.0f, 1.0f));
            }
        }

        CopyToBuffer(g_rawPath, sizeof(g_rawPath), "");
        CopyToBuffer(g_savePath, sizeof(g_savePath), "");
        CopyToBuffer(g_levelPath, sizeof(g_levelPath), "");

        g_editorScene.Clear();

        RefreshHeightStats();
        ResetCameraToHeightmap();

        for (int i = 0; i < 40; ++i)
        {
            const float a = static_cast<float>(i) * 0.73f;
            const float r = 2500.0f + SimpleNoise2D(a, a * 2.0f) * 9500.0f;

            const float x = std::cos(a) * r;
            const float z = std::sin(a) * r;

            AddPreviewObject(
                StalkerOnline::Editor::EditorObjectType::SpeedTreeProxy,
                "Tree Proxy",
                x,
                450.0f,
                z,
                90.0f);
        }

        for (int i = 0; i < 18; ++i)
        {
            const float a = static_cast<float>(i) * 1.37f;
            const float r = 1200.0f + SimpleNoise2D(a * 2.0f, a) * 6500.0f;

            const float x = std::cos(a) * r;
            const float z = std::sin(a) * r;

            AddPreviewObject(
                StalkerOnline::Editor::EditorObjectType::StaticMeshProxy,
                "Building Proxy",
                x,
                500.0f,
                z,
                150.0f);
        }

        AddPreviewObject(
            StalkerOnline::Editor::EditorObjectType::PlayerSpawn,
            "Player Spawn",
            0.0f,
            700.0f,
            -1800.0f,
            100.0f);

        AddPreviewObject(
            StalkerOnline::Editor::EditorObjectType::SafeZone,
            "Safe Zone",
            -1800.0f,
            450.0f,
            900.0f,
            850.0f);

        SyncSceneTerrainInfo();

        g_isDirty = true;

        SetStatus("Created preview island level.");
    }

    void LoadRawHeightmap(const std::string& path)
    {
        if (path.empty())
            return;

        SelectFormatFromPath(path);

        if (g_autoDetectSquareSize)
            AutoDetectRawDimensions(path, false);

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

        RefreshHeightStats();
        ResetCameraToHeightmap();
        g_isDirty = false;
        SyncSceneTerrainInfo();

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
        CopyToBuffer(g_rawPath, sizeof(g_rawPath), path);
        SyncSceneTerrainInfo();

        SetStatus("Saved RAW heightmap: " + path);
    }

    void LoadSpeedTreeAsset(const std::string& path)
    {
        if (path.empty())
        {
            SetStatus("SpeedTree load failed: SRT path is empty.");
            return;
        }

        g_speedTreeAsset = StalkerOnline::SpeedTreeIntegration::TryLoadTreeAsset(path);

        if (!g_speedTreeAsset.Loaded)
        {
            SetStatus("SpeedTree load failed: " + g_speedTreeAsset.ErrorMessage);
            return;
        }

        CopyToBuffer(g_speedTreeAssetPath, sizeof(g_speedTreeAssetPath), path);
        SetStatus("Loaded SpeedTree asset: " + path);
    }

    struct EditorVector3
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Z = 0.0f;
    };

    float DegToRad(float degrees)
    {
        return degrees * 3.14159265358979323846f / 180.0f;
    }

    EditorVector3 Add(EditorVector3 a, EditorVector3 b)
    {
        return { a.X + b.X, a.Y + b.Y, a.Z + b.Z };
    }

    EditorVector3 Scale(EditorVector3 vector, float scale)
    {
        return { vector.X * scale, vector.Y * scale, vector.Z * scale };
    }

    EditorVector3 Cross(EditorVector3 a, EditorVector3 b)
    {
        return
        {
            a.Y * b.Z - a.Z * b.Y,
            a.Z * b.X - a.X * b.Z,
            a.X * b.Y - a.Y * b.X
        };
    }

    EditorVector3 Normalize(EditorVector3 vector)
    {
        const float length = std::sqrt(vector.X * vector.X + vector.Y * vector.Y + vector.Z * vector.Z);

        if (length <= 0.00001f)
            return {};

        return
        {
            vector.X / length,
            vector.Y / length,
            vector.Z / length
        };
    }

    EditorVector3 CameraForward()
    {
        const float yaw = DegToRad(g_renderSettings.CameraYaw);
        const float pitch = DegToRad(std::clamp(g_renderSettings.CameraPitch, -89.0f, 89.0f));
        const float cosPitch = std::cos(pitch);

        return Normalize(
            {
                std::sin(yaw) * cosPitch,
                std::sin(pitch),
                std::cos(yaw) * cosPitch
            });
    }

    EditorVector3 CameraRight()
    {
        constexpr EditorVector3 worldUp{ 0.0f, 1.0f, 0.0f };
        return Normalize(Cross(worldUp, CameraForward()));
    }

    EditorVector3 CameraUp()
    {
        return Normalize(Cross(CameraForward(), CameraRight()));
    }

    float HeightForEditorPreview(float rawHeight)
    {
        const float clampedRawHeight = std::clamp(rawHeight, 0.0f, 1.0f);

        if (!g_renderSettings.NormalizeHeightPreview)
            return clampedRawHeight;

        const float minHeight = std::clamp(g_renderSettings.PreviewMinHeight, 0.0f, 1.0f);
        const float maxHeight = std::clamp(g_renderSettings.PreviewMaxHeight, 0.0f, 1.0f);
        const float range = maxHeight - minHeight;

        if (range <= 0.000001f)
            return 0.5f;

        return std::clamp((clampedRawHeight - minHeight) / range, 0.0f, 1.0f);
    }

    StalkerOnline::Editor::EditorVec3 MakePlacementPosition()
    {
        if (g_heightmap.IsValid())
        {
            const float centerX = static_cast<float>(g_heightmap.GetWidth() - 1) * 0.5f;
            const float centerY = static_cast<float>(g_heightmap.GetHeight() - 1) * 0.5f;

            const int sampleX = std::clamp(static_cast<int>(std::round(g_renderSettings.BrushX)), 0, g_heightmap.GetWidth() - 1);
            const int sampleY = std::clamp(static_cast<int>(std::round(g_renderSettings.BrushY)), 0, g_heightmap.GetHeight() - 1);

            const float rawHeight = g_heightmap.GetHeightNormalized(sampleX, sampleY);
            const float previewHeight = HeightForEditorPreview(rawHeight);

            return
            {
                (g_renderSettings.BrushX - centerX) * g_renderSettings.CellSizeX,
                (previewHeight - 0.5f) * g_renderSettings.HeightScale,
                (g_renderSettings.BrushY - centerY) * g_renderSettings.CellSizeY
            };
        }

        const EditorVector3 forward = CameraForward();

        return
        {
            g_renderSettings.CameraX + forward.X * 800.0f,
            g_renderSettings.CameraY + forward.Y * 800.0f,
            g_renderSettings.CameraZ + forward.Z * 800.0f
        };
    }

    void AddEditorObjectAtBrush(
        StalkerOnline::Editor::EditorObjectType type,
        const std::string& name,
        const std::string& assetPath,
        float radius)
    {
        StalkerOnline::Editor::EditorObject object;
        object.Type = type;
        object.Name = name;
        object.AssetPath = assetPath;
        object.Position = MakePlacementPosition();
        object.Radius = radius;
        object.Scale = { 1.0f, 1.0f, 1.0f };

        const std::uint32_t objectId = g_editorScene.AddObject(object);
        g_editorScene.SelectObject(objectId);

        g_isDirty = true;

        SetStatus("Added object: " + name);
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

        const float ndcX = localX / static_cast<float>(viewportWidth) * 2.0f - 1.0f;
        const float ndcY = 1.0f - localY / static_cast<float>(viewportHeight) * 2.0f;

        constexpr float fovRadians = 60.0f * 3.14159265358979323846f / 180.0f;
        const float aspectRatio = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);
        const float tanHalfFov = std::tan(fovRadians * 0.5f);

        const EditorVector3 forward = CameraForward();
        const EditorVector3 right = CameraRight();
        const EditorVector3 up = CameraUp();

        const EditorVector3 rayDirection = Normalize(
            Add(
                forward,
                Add(
                    Scale(right, ndcX * aspectRatio * tanHalfFov),
                    Scale(up, ndcY * tanHalfFov))));

        if (std::fabs(rayDirection.Y) <= 0.00001f)
            return false;

        const float t = -g_renderSettings.CameraY / rayDirection.Y;

        if (t <= 0.0f)
            return false;

        const float worldX = g_renderSettings.CameraX + rayDirection.X * t;
        const float worldZ = g_renderSettings.CameraZ + rayDirection.Z * t;

        *outHeightmapX = worldX / g_renderSettings.CellSizeX + static_cast<float>(g_heightmap.GetWidth() - 1) * 0.5f;
        *outHeightmapY = worldZ / g_renderSettings.CellSizeY + static_cast<float>(g_heightmap.GetHeight() - 1) * 0.5f;

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

            if (ImGui::IsKeyPressed(ImGuiKey_F))
                ResetCameraToHeightmap();

            EditorVector3 moveDirection{};

            if (ImGui::IsKeyDown(ImGuiKey_W))
                moveDirection = Add(moveDirection, CameraForward());

            if (ImGui::IsKeyDown(ImGuiKey_S))
                moveDirection = Add(moveDirection, Scale(CameraForward(), -1.0f));

            if (ImGui::IsKeyDown(ImGuiKey_D))
                moveDirection = Add(moveDirection, CameraRight());

            if (ImGui::IsKeyDown(ImGuiKey_A))
                moveDirection = Add(moveDirection, Scale(CameraRight(), -1.0f));

            if (ImGui::IsKeyDown(ImGuiKey_E))
                moveDirection.Y += 1.0f;

            if (ImGui::IsKeyDown(ImGuiKey_Q))
                moveDirection.Y -= 1.0f;

            moveDirection = Normalize(moveDirection);

            const float speedMultiplier = io.KeyShift ? 4.0f : 1.0f;
            const float cameraStep = g_renderSettings.CameraSpeed * speedMultiplier * (std::max)(io.DeltaTime, 0.001f);

            g_renderSettings.CameraX += moveDirection.X * cameraStep;
            g_renderSettings.CameraY += moveDirection.Y * cameraStep;
            g_renderSettings.CameraZ += moveDirection.Z * cameraStep;
        }

        if (!io.WantCaptureMouse)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
            {
                g_renderSettings.CameraYaw += io.MouseDelta.x * 0.14f;
                g_renderSettings.CameraPitch = std::clamp(
                    g_renderSettings.CameraPitch - io.MouseDelta.y * 0.14f,
                    -85.0f,
                    85.0f);
            }

            if (io.MouseWheel != 0.0f)
            {
                const float speedFactor = io.MouseWheel > 0.0f ? 1.12f : 0.89f;
                g_renderSettings.CameraSpeed = std::clamp(g_renderSettings.CameraSpeed * speedFactor, 50.0f, 1000000.0f);
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
                (std::max)(io.DeltaTime, 0.001f),
                brushMode,
                g_flattenHeight);

            RefreshHeightStats();
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
            CreatePreviewIslandLevel();

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

        if (ImGui::Button("Open Level"))
        {
            const std::string path = ShowOpenLevelDialog();

            if (!path.empty())
                LoadLevelFile(path);
        }

        ImGui::SameLine();

        if (ImGui::Button("Save Level"))
        {
            if (g_levelPath[0] != '\0')
            {
                SaveLevelFile(g_levelPath);
            }
            else
            {
                const std::string path = ShowSaveLevelDialog();

                if (!path.empty())
                    SaveLevelFile(path);
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Save Level As"))
        {
            const std::string path = ShowSaveLevelDialog();

            if (!path.empty())
                SaveLevelFile(path);
        }

        ImGui::SameLine();

        if (ImGui::Button("Reset Camera"))
            ResetCameraToHeightmap();

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
            "UE5 .r16 / RAW16 unsigned LE",
            "8-bit unsigned",
            "16-bit unsigned LE",
            "32-bit float LE"
        };

        ImGui::Combo("RAW format", &g_rawFormatIndex, formats, IM_ARRAYSIZE(formats));
        ImGui::Checkbox("Auto square size", &g_autoDetectSquareSize);

        if (ImGui::Button("Detect Size From File"))
            AutoDetectRawDimensions(g_rawPath, true);

        ImGui::Separator();

        ImGui::DragFloat("Scale X", &g_renderSettings.CellSizeX, 0.1f, 0.01f, 10000.0f, "%.6f");
        ImGui::DragFloat("Scale Y", &g_renderSettings.CellSizeY, 0.1f, 0.01f, 10000.0f, "%.6f");

        if (ImGui::DragFloat("UE Z scale", &g_ueZScale, 0.01f, 0.001f, 10000.0f, "%.6f"))
            g_renderSettings.HeightScale = g_ueZScale * 512.0f;

        if (ImGui::DragFloat("Height range", &g_renderSettings.HeightScale, 10.0f, 1.0f, 1000000.0f, "%.1f"))
            g_ueZScale = g_renderSettings.HeightScale / 512.0f;

        ImGui::SliderInt("Render cells", &g_renderSettings.MaxRenderedCellsPerAxis, 32, 512);
        ImGui::Checkbox("Normalize preview", &g_renderSettings.NormalizeHeightPreview);
        ImGui::Checkbox("Wireframe", &g_renderSettings.ShowWireframe);

        ImGui::Separator();

        if (g_heightmap.IsValid())
        {
            ImGui::Text("Loaded: %d x %d", g_heightmap.GetWidth(), g_heightmap.GetHeight());
            ImGui::Text("Format: %s", StalkerOnline::Editor::RawHeightFormatToText(FormatFromIndex(g_rawFormatIndex)));
            ImGui::Text("Height min/max: %.6f / %.6f", g_heightMin, g_heightMax);

            const float rawRange = g_heightMax - g_heightMin;

            if (rawRange <= 0.000001f)
            {
                ImGui::TextColored(
                    ImVec4(0.95f, 0.42f, 0.30f, 1.0f),
                    "Heightmap looks flat or unreadable.");
            }
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

        ImGui::DragFloat("Camera speed", &g_renderSettings.CameraSpeed, 100.0f, 50.0f, 1000000.0f, "%.1f");
        ImGui::DragFloat3("Camera pos", &g_renderSettings.CameraX, 100.0f, -10000000.0f, 10000000.0f, "%.1f");
        ImGui::DragFloat("Yaw", &g_renderSettings.CameraYaw, 0.25f, -3600.0f, 3600.0f, "%.1f");
        ImGui::DragFloat("Pitch", &g_renderSettings.CameraPitch, 0.25f, -85.0f, 85.0f, "%.1f");

        if (ImGui::Button("Reset Camera"))
            ResetCameraToHeightmap();

        ImGui::Separator();

        ImGui::Text("Brush: %.1f / %.1f", g_renderSettings.BrushX, g_renderSettings.BrushY);

        if (g_heightmap.IsValid())
        {
            const int sampleX = std::clamp(static_cast<int>(g_renderSettings.BrushX), 0, g_heightmap.GetWidth() - 1);
            const int sampleY = std::clamp(static_cast<int>(g_renderSettings.BrushY), 0, g_heightmap.GetHeight() - 1);
            ImGui::Text("Height: %.4f", g_heightmap.GetHeightNormalized(sampleX, sampleY));
        }

        ImGui::TextColored(ImVec4(0.74f, 0.70f, 0.52f, 1.0f), "LMB sculpt | Shift+LMB lower | RMB look | Wheel speed");
        ImGui::TextColored(ImVec4(0.74f, 0.70f, 0.52f, 1.0f), "Camera: W/A/S/D, Q/E vertical, Shift faster, F reset");
        ImGui::TextColored(ImVec4(0.74f, 0.70f, 0.52f, 1.0f), "Brush: 1 Raise, 2 Lower, 3 Smooth, 4 Flatten");

        ImGui::End();
    }

    void DrawSpeedTreePanel()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float width = 350.0f;

        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x - width - 16.0f, viewport->Pos.y + 434.0f));
        ImGui::SetNextWindowSize(ImVec2(width, 230.0f));

        ImGui::Begin("SpeedTree", nullptr, ImGuiWindowFlags_NoCollapse);

        const StalkerOnline::SpeedTreeIntegration::SdkStatus sdkStatus =
            StalkerOnline::SpeedTreeIntegration::GetSdkStatus();

        ImGui::TextColored(
            sdkStatus.Available ? ImVec4(0.55f, 0.75f, 0.45f, 1.0f) : ImVec4(0.90f, 0.35f, 0.25f, 1.0f),
            "%s",
            sdkStatus.Available ? "SDK linked" : "SDK missing");

        if (!sdkStatus.Version.empty())
            ImGui::TextWrapped("%s", sdkStatus.Version.c_str());

        if (!sdkStatus.CoordinateSystem.empty())
            ImGui::Text("Coord: %s", sdkStatus.CoordinateSystem.c_str());

        ImGui::Text(
            "DX11 renderer: %s / %s",
            sdkStatus.DirectX11RendererAvailable ? "linked" : "not linked",
            sdkStatus.DirectX11RendererInitialized ? "initialized" : "not initialized");

        ImGui::Separator();

        ImGui::InputText("SRT path", g_speedTreeAssetPath, sizeof(g_speedTreeAssetPath));

        if (ImGui::Button("Open SRT"))
        {
            const std::string path = ShowOpenSpeedTreeDialog();

            if (!path.empty())
                LoadSpeedTreeAsset(path);
        }

        ImGui::SameLine();

        if (ImGui::Button("Load Path"))
            LoadSpeedTreeAsset(g_speedTreeAssetPath);

        ImGui::Separator();

        if (g_speedTreeAsset.Loaded)
        {
            ImGui::Text("Height: %.3f", g_speedTreeAsset.Height);
            ImGui::Text("Radius: %.3f", g_speedTreeAsset.Radius);
            ImGui::Text(
                "Render Interface: %s",
                g_speedTreeAsset.RenderInterfaceInitialized ? "initialized" : "not initialized");

            if (!g_speedTreeAsset.RenderInterfaceMessage.empty())
                ImGui::TextWrapped("%s", g_speedTreeAsset.RenderInterfaceMessage.c_str());

            ImGui::Text(
                "Min: %.2f %.2f %.2f",
                g_speedTreeAsset.MinBounds[0],
                g_speedTreeAsset.MinBounds[1],
                g_speedTreeAsset.MinBounds[2]);
            ImGui::Text(
                "Max: %.2f %.2f %.2f",
                g_speedTreeAsset.MaxBounds[0],
                g_speedTreeAsset.MaxBounds[1],
                g_speedTreeAsset.MaxBounds[2]);
        }
        else if (!g_speedTreeAsset.ErrorMessage.empty())
        {
            ImGui::TextColored(ImVec4(0.90f, 0.35f, 0.25f, 1.0f), "%s", g_speedTreeAsset.ErrorMessage.c_str());
        }

        ImGui::End();
    }

    void DrawScenePanel()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + 16.0f, viewport->Pos.y + 444.0f));
        ImGui::SetNextWindowSize(ImVec2(360.0f, 380.0f));

        ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoCollapse);

        if (ImGui::Button("Add Cube"))
        {
            AddEditorObjectAtBrush(
                StalkerOnline::Editor::EditorObjectType::StaticMeshProxy,
                "Static Mesh Proxy",
                "",
                100.0f);
        }

        ImGui::SameLine();

        if (ImGui::Button("Add Spawn"))
        {
            AddEditorObjectAtBrush(
                StalkerOnline::Editor::EditorObjectType::PlayerSpawn,
                "Player Spawn",
                "",
                100.0f);
        }

        if (ImGui::Button("Add Loot"))
        {
            AddEditorObjectAtBrush(
                StalkerOnline::Editor::EditorObjectType::LootSpawner,
                "Loot Spawner",
                "",
                100.0f);
        }

        ImGui::SameLine();

        if (ImGui::Button("Add Tree Proxy"))
        {
            AddEditorObjectAtBrush(
                StalkerOnline::Editor::EditorObjectType::SpeedTreeProxy,
                "SpeedTree Proxy",
                g_speedTreeAssetPath,
                100.0f);
        }

        if (ImGui::Button("Add Safe Zone"))
        {
            AddEditorObjectAtBrush(
                StalkerOnline::Editor::EditorObjectType::SafeZone,
                "Safe Zone",
                "",
                600.0f);
        }

        ImGui::SameLine();

        if (ImGui::Button("Add Rad Zone"))
        {
            AddEditorObjectAtBrush(
                StalkerOnline::Editor::EditorObjectType::RadiationZone,
                "Radiation Zone",
                "",
                600.0f);
        }

        if (ImGui::Button("Add Anomaly"))
        {
            AddEditorObjectAtBrush(
                StalkerOnline::Editor::EditorObjectType::AnomalyZone,
                "Anomaly Zone",
                "",
                250.0f);
        }

        ImGui::SameLine();

        if (ImGui::Button("Add Light"))
        {
            AddEditorObjectAtBrush(
                StalkerOnline::Editor::EditorObjectType::Light,
                "Light",
                "",
                100.0f);
        }

        ImGui::Separator();

        ImGui::Text("Objects: %d", static_cast<int>(g_editorScene.GetObjects().size()));

        ImGui::BeginChild("##SceneObjectsList", ImVec2(0.0f, 0.0f), true);

        for (const StalkerOnline::Editor::EditorObject& object : g_editorScene.GetObjects())
        {
            char label[256]{};

            std::snprintf(
                label,
                sizeof(label),
                "#%u %s - %s",
                object.Id,
                StalkerOnline::Editor::EditorObjectTypeToText(object.Type),
                object.Name.c_str());

            const bool selected = object.Id == g_editorScene.GetSelectedObjectId();

            if (ImGui::Selectable(label, selected))
                g_editorScene.SelectObject(object.Id);
        }

        ImGui::EndChild();

        ImGui::End();
    }

    void DrawDetailsPanel()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float width = 350.0f;

        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x - width - 16.0f, viewport->Pos.y + 680.0f));
        ImGui::SetNextWindowSize(ImVec2(width, 205.0f));

        ImGui::Begin("Details", nullptr, ImGuiWindowFlags_NoCollapse);

        StalkerOnline::Editor::EditorObject* selectedObject =
            g_editorScene.GetSelectedObjectMutable();

        if (selectedObject == nullptr)
        {
            ImGui::TextColored(ImVec4(0.74f, 0.70f, 0.52f, 1.0f), "No object selected.");
            ImGui::End();
            return;
        }

        ImGui::Text("Selected ID: %u", selectedObject->Id);

        char nameBuffer[128]{};
        std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", selectedObject->Name.c_str());

        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
        {
            selectedObject->Name = nameBuffer;
            g_isDirty = true;
        }

        const char* types[]
        {
            "Static Mesh Proxy",
            "SpeedTree Proxy",
            "Player Spawn",
            "Loot Spawner",
            "Safe Zone",
            "Radiation Zone",
            "Anomaly Zone",
            "Light"
        };

        int typeIndex = static_cast<int>(selectedObject->Type);

        if (ImGui::Combo("Type", &typeIndex, types, IM_ARRAYSIZE(types)))
        {
            typeIndex = std::clamp(typeIndex, 0, static_cast<int>(IM_ARRAYSIZE(types)) - 1);
            selectedObject->Type = static_cast<StalkerOnline::Editor::EditorObjectType>(typeIndex);
            g_isDirty = true;
        }

        char assetBuffer[260]{};
        std::snprintf(assetBuffer, sizeof(assetBuffer), "%s", selectedObject->AssetPath.c_str());

        if (ImGui::InputText("Asset", assetBuffer, sizeof(assetBuffer)))
        {
            selectedObject->AssetPath = assetBuffer;
            g_isDirty = true;
        }

        if (ImGui::DragFloat3("Position", &selectedObject->Position.X, 10.0f, -10000000.0f, 10000000.0f, "%.1f"))
            g_isDirty = true;

        if (ImGui::DragFloat3("Rotation", &selectedObject->Rotation.X, 0.5f, -3600.0f, 3600.0f, "%.1f"))
            g_isDirty = true;

        if (ImGui::DragFloat3("Scale", &selectedObject->Scale.X, 0.01f, 0.01f, 1000.0f, "%.2f"))
            g_isDirty = true;

        if (ImGui::DragFloat("Radius", &selectedObject->Radius, 5.0f, 1.0f, 100000.0f, "%.1f"))
            g_isDirty = true;

        if (ImGui::Checkbox("Visible", &selectedObject->Visible))
            g_isDirty = true;

        if (ImGui::Button("Delete Selected"))
        {
            g_editorScene.RemoveSelectedObject();
            g_isDirty = true;
        }

        ImGui::End();
    }

    bool StudioTabButton(const char* label, StudioTab tab, float width)
    {
        const bool active = g_activeStudioTab == tab;

        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.16f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.24f, 0.24f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.32f, 0.30f, 1.0f));
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.015f, 0.015f, 0.015f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.11f, 0.12f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.20f, 0.20f, 1.0f));
        }

        const bool clicked = ImGui::Button(label, ImVec2(width, 24.0f));

        ImGui::PopStyleColor(3);

        if (clicked)
            g_activeStudioTab = tab;

        return clicked;
    }

    bool StudioSmallButton(const char* label, bool active, float width = 76.0f)
    {
        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.23f, 0.24f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.85f, 0.12f, 0.10f, 1.0f));
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.025f, 0.025f, 0.025f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, active ? 1.5f : 1.0f);

        const bool clicked = ImGui::Button(label, ImVec2(width, 23.0f));

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

        return clicked;
    }

    void DrawStudioTopTabs()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y));
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 36.0f));

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.55f, 0.67f, 0.74f, 0.95f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 5.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 0.0f));

        ImGui::Begin("##StudioTopTabs", nullptr, flags);

        StudioTabButton("Settings", StudioTab::Settings, 140.0f);
        ImGui::SameLine();
        StudioTabButton("Terrain", StudioTab::Terrain, 140.0f);
        ImGui::SameLine();
        StudioTabButton("Objects", StudioTab::Objects, 140.0f);
        ImGui::SameLine();
        StudioTabButton("Materials", StudioTab::Materials, 140.0f);
        ImGui::SameLine();
        StudioTabButton("Environment", StudioTab::Environment, 140.0f);
        ImGui::SameLine();
        StudioTabButton("Collections", StudioTab::Collections, 140.0f);
        ImGui::SameLine();
        StudioTabButton("Decorators", StudioTab::Decorators, 140.0f);
        ImGui::SameLine();
        StudioTabButton("Roads", StudioTab::Roads, 140.0f);
        ImGui::SameLine();
        StudioTabButton("Gameplay", StudioTab::Gameplay, 140.0f);
        ImGui::SameLine();
        StudioTabButton("Post FX", StudioTab::PostFX, 140.0f);
        ImGui::SameLine();
        StudioTabButton("Color Correction", StudioTab::ColorCorrection, 170.0f);

        ImGui::End();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    void DrawStudioSettingsOverlay()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + 4.0f, viewport->Pos.y + 42.0f));
        ImGui::SetNextWindowSize(ImVec2(350.0f, 94.0f));

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.14f, 0.15f, 0.76f));

        ImGui::Begin("##StudioSettingsOverlay", nullptr, flags);

        StudioSmallButton("System Settings", true, 110.0f);
        ImGui::SameLine();
        StudioSmallButton("Options Menu", false, 110.0f);

        ImGui::Spacing();

        if (StudioSmallButton("SAVE MAP", false, 94.0f))
        {
            if (g_levelPath[0] != '\0')
                SaveLevelFile(g_levelPath);
            else
            {
                const std::string path = ShowSaveLevelDialog();

                if (!path.empty())
                    SaveLevelFile(path);
            }
        }

        ImGui::SameLine();

        if (StudioSmallButton("SAVE GLOBAL", false, 110.0f))
        {
            if (g_levelPath[0] != '\0')
                SaveLevelFile(g_levelPath);
            else
            {
                const std::string path = ShowSaveLevelDialog();

                if (!path.empty())
                    SaveLevelFile(path);
            }
        }

        ImGui::End();

        ImGui::PopStyleColor();

        const float width = 320.0f;

        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x - width - 8.0f, viewport->Pos.y + 42.0f));
        ImGui::SetNextWindowSize(ImVec2(width, 94.0f));

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.14f, 0.15f, 0.76f));

        ImGui::Begin("##StudioCameraOverlay", nullptr, flags);

        if (StudioSmallButton("Camera", g_showCameraPanel, 70.0f))
            g_showCameraPanel = !g_showCameraPanel;

        ImGui::SameLine();

        if (StudioSmallButton("Map", g_showMapPanel, 70.0f))
            g_showMapPanel = !g_showMapPanel;

        ImGui::SameLine();

        if (StudioSmallButton("Shadows", g_showShadowsPanel, 80.0f))
            g_showShadowsPanel = !g_showShadowsPanel;

        ImGui::SameLine();

        if (StudioSmallButton("Misc", g_showMiscPanel, 60.0f))
            g_showMiscPanel = !g_showMiscPanel;

        if (g_showCameraPanel)
        {
            ImGui::SliderFloat("Camera Z Near", &g_cameraZNear, 0.01f, 10.0f, "%.2f");
            ImGui::SliderFloat("Camera Z Far", &g_cameraZFar, 100.0f, 100000.0f, "%.2f");
        }

        ImGui::End();

        ImGui::PopStyleColor();
    }

    void DrawStudioTerrainToolbar()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + 4.0f, viewport->Pos.y + 42.0f));
        ImGui::SetNextWindowSize(ImVec2(620.0f, 54.0f));

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.14f, 0.15f, 0.76f));

        ImGui::Begin("##StudioTerrainToolbar", nullptr, flags);

        if (StudioSmallButton("New Flat", false, 80.0f))
            CreateFlatLevel();

        ImGui::SameLine();

        if (StudioSmallButton("New Island", false, 90.0f))
            CreatePreviewIslandLevel();

        ImGui::SameLine();

        if (StudioSmallButton("Open RAW", false, 90.0f))
        {
            const std::string path = ShowOpenRawDialog();

            if (!path.empty())
                LoadRawHeightmap(path);
        }

        ImGui::SameLine();

        if (StudioSmallButton("Save RAW", false, 90.0f))
            SaveRawHeightmap(g_savePath[0] != '\0' ? g_savePath : g_rawPath);

        ImGui::SameLine();

        if (StudioSmallButton("Reset Camera", false, 115.0f))
            ResetCameraToHeightmap();

        ImGui::End();

        ImGui::PopStyleColor();
    }

    void DrawStudioObjectsToolbar()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + 4.0f, viewport->Pos.y + 42.0f));
        ImGui::SetNextWindowSize(ImVec2(760.0f, 54.0f));

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.14f, 0.15f, 0.76f));

        ImGui::Begin("##StudioObjectsToolbar", nullptr, flags);

        if (StudioSmallButton("Add Cube", false, 86.0f))
        {
            AddEditorObjectAtBrush(
                StalkerOnline::Editor::EditorObjectType::StaticMeshProxy,
                "Static Mesh Proxy",
                "",
                100.0f);
        }

        ImGui::SameLine();

        if (StudioSmallButton("Add Spawn", false, 90.0f))
        {
            AddEditorObjectAtBrush(
                StalkerOnline::Editor::EditorObjectType::PlayerSpawn,
                "Player Spawn",
                "",
                100.0f);
        }

        ImGui::SameLine();

        if (StudioSmallButton("Add Loot", false, 80.0f))
        {
            AddEditorObjectAtBrush(
                StalkerOnline::Editor::EditorObjectType::LootSpawner,
                "Loot Spawner",
                "",
                100.0f);
        }

        ImGui::SameLine();

        if (StudioSmallButton("Add Tree", false, 80.0f))
        {
            AddEditorObjectAtBrush(
                StalkerOnline::Editor::EditorObjectType::SpeedTreeProxy,
                "SpeedTree Proxy",
                g_speedTreeAssetPath,
                100.0f);
        }

        ImGui::SameLine();

        if (StudioSmallButton("Safe Zone", false, 90.0f))
        {
            AddEditorObjectAtBrush(
                StalkerOnline::Editor::EditorObjectType::SafeZone,
                "Safe Zone",
                "",
                600.0f);
        }

        ImGui::SameLine();

        if (StudioSmallButton("Rad Zone", false, 90.0f))
        {
            AddEditorObjectAtBrush(
                StalkerOnline::Editor::EditorObjectType::RadiationZone,
                "Radiation Zone",
                "",
                600.0f);
        }

        ImGui::SameLine();

        if (StudioSmallButton("Anomaly", false, 86.0f))
        {
            AddEditorObjectAtBrush(
                StalkerOnline::Editor::EditorObjectType::AnomalyZone,
                "Anomaly Zone",
                "",
                250.0f);
        }

        ImGui::End();

        ImGui::PopStyleColor();
    }

    void DrawStudioPlaceholderPanel(const char* title)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + 4.0f, viewport->Pos.y + 42.0f));
        ImGui::SetNextWindowSize(ImVec2(360.0f, 72.0f));

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.14f, 0.15f, 0.76f));

        ImGui::Begin("##StudioPlaceholderPanel", nullptr, flags);

        ImGui::Text("%s", title);
        ImGui::TextColored(ImVec4(0.74f, 0.70f, 0.52f, 1.0f), "This tab will be implemented as a real editor module.");

        ImGui::End();

        ImGui::PopStyleColor();
    }

    void DrawStudioBottomBar()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - 24.0f));
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 24.0f));

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.14f, 0.15f, 0.78f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 3.0f));

        ImGui::Begin("##StudioBottomBar", nullptr, flags);

        ImGuiIO& io = ImGui::GetIO();

        ImGui::Text(
            "SO_EDITOR FPS %.1f [%.2fms] Objs:[%d] Cam:[%.1f %.1f %.1f] Dirty:%s",
            io.Framerate,
            io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f,
            static_cast<int>(g_editorScene.GetObjects().size()),
            g_renderSettings.CameraX,
            g_renderSettings.CameraY,
            g_renderSettings.CameraZ,
            g_isDirty ? "yes" : "no");

        ImGui::SameLine(viewport->Size.x - 520.0f);
        ImGui::Text("Time Of Day");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        ImGui::SliderFloat("##StudioTimeOfDay", &g_timeOfDay, 0.0f, 24.0f, "%.2f");

        ImGui::SameLine();
        ImGui::Text("Speed");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);

        if (ImGui::SliderFloat("##StudioSpeed", &g_editorSpeed, 1.0f, 20.0f, "%.0f"))
            g_renderSettings.CameraSpeed = 1400.0f * g_editorSpeed;

        ImGui::End();

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    void DrawEditorUi()
    {
        DrawStudioTopTabs();

        switch (g_activeStudioTab)
        {
            case StudioTab::Settings:
                DrawStudioSettingsOverlay();
                break;

            case StudioTab::Terrain:
                DrawStudioTerrainToolbar();
                DrawImportPanel();
                DrawSculptPanel();
                break;

            case StudioTab::Objects:
                DrawStudioObjectsToolbar();
                DrawScenePanel();
                DrawDetailsPanel();
                break;

            case StudioTab::Environment:
                DrawSpeedTreePanel();
                DrawSculptPanel();
                break;

            case StudioTab::Materials:
                DrawStudioPlaceholderPanel("Materials");
                break;

            case StudioTab::Collections:
                DrawStudioPlaceholderPanel("Collections");
                break;

            case StudioTab::Decorators:
                DrawStudioPlaceholderPanel("Decorators");
                break;

            case StudioTab::Roads:
                DrawStudioPlaceholderPanel("Roads");
                break;

            case StudioTab::Gameplay:
                DrawStudioPlaceholderPanel("Gameplay");
                break;

            case StudioTab::PostFX:
                DrawStudioPlaceholderPanel("Post FX");
                break;

            case StudioTab::ColorCorrection:
                DrawStudioPlaceholderPanel("Color Correction");
                break;

            default:
                DrawStudioSettingsOverlay();
                break;
        }

        DrawStudioBottomBar();
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

    const StalkerOnline::SpeedTreeIntegration::Dx11InitializationResult speedTreeDx11Init =
        StalkerOnline::SpeedTreeIntegration::InitializeDx11Renderer(
            g_renderer->GetDevice(),
            g_renderer->GetDeviceContext());

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

    if (!speedTreeDx11Init.Success)
        SetStatus(speedTreeDx11Init.Message);

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

        const float clearColor[4] = { 0.46f, 0.63f, 0.76f, 1.0f };

        g_renderer->BeginFrame(clearColor);

        g_levelRenderer->Render(
            g_renderer->GetDeviceContext(),
            g_renderer->GetWidth(),
            g_renderer->GetHeight(),
            g_heightmap,
            g_renderSettings,
            g_editorScene);

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
