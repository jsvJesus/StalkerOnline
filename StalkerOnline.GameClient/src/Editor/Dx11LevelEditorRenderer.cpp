#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Editor/Dx11LevelEditorRenderer.h"

#include <DirectXMath.h>
#include <Windows.h>
#include <d3dcompiler.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace StalkerOnline::Editor
{
    namespace
    {
        constexpr std::size_t MaxDynamicVertices = 98304;
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

        Float3 CameraForward(const LevelEditorRenderSettings& settings)
        {
            const float yaw = DegToRad(settings.CameraYaw);
            const float pitch = DegToRad(std::clamp(settings.CameraPitch, -89.0f, 89.0f));
            const float cosPitch = std::cos(pitch);

            return
            {
                std::sin(yaw) * cosPitch,
                std::sin(pitch),
                std::cos(yaw) * cosPitch
            };
        }

        float CalculateFarPlane(const Heightmap& heightmap, const LevelEditorRenderSettings& settings)
        {
            const float terrainWidth = static_cast<float>(heightmap.GetWidth() - 1) * settings.CellSizeX;
            const float terrainHeight = static_cast<float>(heightmap.GetHeight() - 1) * settings.CellSizeY;
            const float maxTerrainAxis = (std::max)(terrainWidth, terrainHeight);

            return (std::max)(maxTerrainAxis * 4.0f, settings.HeightScale * 12.0f);
        }

        DirectX::XMMATRIX BuildViewProjection(
            std::uint32_t viewportWidth,
            std::uint32_t viewportHeight,
            const Heightmap& heightmap,
            const LevelEditorRenderSettings& settings)
        {
            const float aspectRatio = viewportHeight == 0
                ? 1.0f
                : static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);

            const Float3 forward = CameraForward(settings);

            const DirectX::XMVECTOR eye = DirectX::XMVectorSet(
                settings.CameraX,
                settings.CameraY,
                settings.CameraZ,
                1.0f);

            const DirectX::XMVECTOR target = DirectX::XMVectorSet(
                settings.CameraX + forward.X,
                settings.CameraY + forward.Y,
                settings.CameraZ + forward.Z,
                1.0f);

            const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

            const DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(eye, target, up);
            const DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(
                DegToRad(60.0f),
                aspectRatio,
                (std::max)(1.0f, (std::min)(settings.CellSizeX, settings.CellSizeY) * 0.02f),
                CalculateFarPlane(heightmap, settings));

            return view * projection;
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

        Color LerpColor(const Color& a, const Color& b, float t)
        {
            const float clampedT = std::clamp(t, 0.0f, 1.0f);

            return
            {
                a[0] + (b[0] - a[0]) * clampedT,
                a[1] + (b[1] - a[1]) * clampedT,
                a[2] + (b[2] - a[2]) * clampedT,
                a[3] + (b[3] - a[3]) * clampedT
            };
        }

        Color TerrainColor(float normalizedHeight)
        {
            const float h = std::clamp(normalizedHeight, 0.0f, 1.0f);

            const Color wetSand    { 0.34f, 0.32f, 0.22f, 1.0f };
            const Color beachSand  { 0.72f, 0.63f, 0.40f, 1.0f };
            const Color dryGrass   { 0.34f, 0.42f, 0.20f, 1.0f };
            const Color deepGrass  { 0.14f, 0.28f, 0.13f, 1.0f };
            const Color hillGreen  { 0.25f, 0.36f, 0.16f, 1.0f };
            const Color rock       { 0.34f, 0.34f, 0.30f, 1.0f };
            const Color highRock   { 0.52f, 0.52f, 0.48f, 1.0f };

            if (h < 0.30f)
                return wetSand;

            if (h < 0.38f)
                return LerpColor(wetSand, beachSand, (h - 0.30f) / 0.08f);

            if (h < 0.52f)
                return LerpColor(beachSand, dryGrass, (h - 0.38f) / 0.14f);

            if (h < 0.68f)
                return LerpColor(dryGrass, deepGrass, (h - 0.52f) / 0.16f);

            if (h < 0.82f)
                return LerpColor(deepGrass, hillGreen, (h - 0.68f) / 0.14f);

            if (h < 0.94f)
                return LerpColor(hillGreen, rock, (h - 0.82f) / 0.12f);

            return LerpColor(rock, highRock, (h - 0.94f) / 0.06f);
        }

        float HeightForPreview(
            float rawHeight,
            const LevelEditorRenderSettings& settings)
        {
            const float clampedRawHeight = std::clamp(rawHeight, 0.0f, 1.0f);

            if (!settings.NormalizeHeightPreview)
                return clampedRawHeight;

            const float minHeight = std::clamp(settings.PreviewMinHeight, 0.0f, 1.0f);
            const float maxHeight = std::clamp(settings.PreviewMaxHeight, 0.0f, 1.0f);
            const float range = maxHeight - minHeight;

            if (range <= 0.000001f)
                return 0.5f;

            return std::clamp((clampedRawHeight - minHeight) / range, 0.0f, 1.0f);
        }

        Float3 HeightmapPoint(
            const Heightmap& heightmap,
            const LevelEditorRenderSettings& settings,
            int x,
            int y)
        {
            const float centerX = static_cast<float>(heightmap.GetWidth() - 1) * 0.5f;
            const float centerY = static_cast<float>(heightmap.GetHeight() - 1) * 0.5f;

            return
            {
                (static_cast<float>(x) - centerX) * settings.CellSizeX,
                (HeightForPreview(heightmap.GetHeightNormalized(x, y), settings) - 0.5f) * settings.HeightScale,
                (static_cast<float>(y) - centerY) * settings.CellSizeY
            };
        }

        Float3 Subtract(const Float3& a, const Float3& b)
        {
            return
            {
                a.X - b.X,
                a.Y - b.Y,
                a.Z - b.Z
            };
        }

        Float3 CrossProduct(const Float3& a, const Float3& b)
        {
            return
            {
                a.Y * b.Z - a.Z * b.Y,
                a.Z * b.X - a.X * b.Z,
                a.X * b.Y - a.Y * b.X
            };
        }

        float DotProduct(const Float3& a, const Float3& b)
        {
            return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
        }

        Float3 NormalizeFloat3(const Float3& value)
        {
            const float length = std::sqrt(
                value.X * value.X +
                value.Y * value.Y +
                value.Z * value.Z);

            if (length <= 0.000001f)
                return { 0.0f, 1.0f, 0.0f };

            return
            {
                value.X / length,
                value.Y / length,
                value.Z / length
            };
        }

        Float3 CalculateTriangleNormal(
            const Float3& a,
            const Float3& b,
            const Float3& c)
        {
            Float3 normal = NormalizeFloat3(
                CrossProduct(
                    Subtract(c, a),
                    Subtract(b, a)));

            if (normal.Y < 0.0f)
            {
                normal.X = -normal.X;
                normal.Y = -normal.Y;
                normal.Z = -normal.Z;
            }

            return normal;
        }

        Color ApplySunLight(const Color& baseColor, const Float3& normal)
        {
            const Float3 sunDirection = NormalizeFloat3({ -0.45f, 0.82f, -0.35f });

            const float ndotl = std::clamp(DotProduct(normal, sunDirection), 0.0f, 1.0f);
            const float ambient = 0.42f;
            const float diffuse = 0.78f * ndotl;
            const float light = std::clamp(ambient + diffuse, 0.20f, 1.25f);

            const float skyBounce = std::clamp(normal.Y, 0.0f, 1.0f) * 0.08f;

            return
            {
                std::clamp(baseColor[0] * light + skyBounce * 0.20f, 0.0f, 1.0f),
                std::clamp(baseColor[1] * light + skyBounce * 0.28f, 0.0f, 1.0f),
                std::clamp(baseColor[2] * light + skyBounce * 0.34f, 0.0f, 1.0f),
                baseColor[3]
            };
        }

        void AddVertex(
            std::vector<Dx11LevelEditorRenderer::Vertex>& vertices,
            const Float3& position,
            const Color& color)
        {
            Dx11LevelEditorRenderer::Vertex vertex{};
            vertex.Position[0] = position.X;
            vertex.Position[1] = position.Y;
            vertex.Position[2] = position.Z;

            vertex.Color[0] = color[0];
            vertex.Color[1] = color[1];
            vertex.Color[2] = color[2];
            vertex.Color[3] = color[3];

            vertices.push_back(vertex);
        }

        void AddTriangle(
            std::vector<Dx11LevelEditorRenderer::Vertex>& vertices,
            const Float3& a,
            const Float3& b,
            const Float3& c,
            const Color& color)
        {
            AddVertex(vertices, a, color);
            AddVertex(vertices, b, color);
            AddVertex(vertices, c, color);
        }

        void AddLine(
            std::vector<Dx11LevelEditorRenderer::Vertex>& vertices,
            const Float3& a,
            const Float3& b,
            const Color& color)
        {
            AddVertex(vertices, a, color);
            AddVertex(vertices, b, color);
        }

        int CalculateRenderStep(const Heightmap& heightmap, const LevelEditorRenderSettings& settings)
        {
            const int maxAxis = (std::max)(heightmap.GetWidth(), heightmap.GetHeight());
            const int targetCells = std::clamp(settings.MaxRenderedCellsPerAxis, 32, 1024);

            return (std::max)(1, maxAxis / targetCells);
        }

        void BuildTerrainVertices(
            std::vector<Dx11LevelEditorRenderer::Vertex>& triangleVertices,
            std::vector<Dx11LevelEditorRenderer::Vertex>& lineVertices,
            const Heightmap& heightmap,
            const LevelEditorRenderSettings& settings)
        {
            if (!heightmap.IsValid())
                return;

            const int step = CalculateRenderStep(heightmap, settings);

            const std::size_t estimatedCells =
                static_cast<std::size_t>(heightmap.GetWidth() / step) *
                static_cast<std::size_t>(heightmap.GetHeight() / step);

            triangleVertices.reserve(std::min<std::size_t>(estimatedCells * 6, MaxDynamicVertices * 8));

            if (settings.ShowWireframe)
                lineVertices.reserve(std::min<std::size_t>(estimatedCells * 4, MaxDynamicVertices * 2));

            for (int y = 0; y < heightmap.GetHeight() - step; y += step)
            {
                for (int x = 0; x < heightmap.GetWidth() - step; x += step)
                {
                    const int x1 = (std::min)(x + step, heightmap.GetWidth() - 1);
                    const int y1 = (std::min)(y + step, heightmap.GetHeight() - 1);

                    const Float3 p00 = HeightmapPoint(heightmap, settings, x, y);
                    const Float3 p10 = HeightmapPoint(heightmap, settings, x1, y);
                    const Float3 p11 = HeightmapPoint(heightmap, settings, x1, y1);
                    const Float3 p01 = HeightmapPoint(heightmap, settings, x, y1);

                    const float averageHeight =
                        (heightmap.GetHeightNormalized(x, y) +
                        heightmap.GetHeightNormalized(x1, y) +
                        heightmap.GetHeightNormalized(x1, y1) +
                        heightmap.GetHeightNormalized(x, y1)) * 0.25f;

                    const float previewHeight = HeightForPreview(averageHeight, settings);
                    const Color baseColor = TerrainColor(previewHeight);

                    const Float3 normalA = CalculateTriangleNormal(p00, p10, p11);
                    const Float3 normalB = CalculateTriangleNormal(p00, p11, p01);

                    const Color colorA = ApplySunLight(baseColor, normalA);
                    const Color colorB = ApplySunLight(ScaleColor(baseColor, 0.96f), normalB);

                    AddTriangle(triangleVertices, p00, p10, p11, colorA);
                    AddTriangle(triangleVertices, p00, p11, p01, colorB);

                    if (settings.ShowWireframe)
                    {
                        const Color lineColor{ 0.080f, 0.095f, 0.080f, 1.0f };
                        AddLine(lineVertices, p00, p10, lineColor);
                        AddLine(lineVertices, p00, p01, lineColor);
                    }
                }
            }
        }

                void AddWaterPlane(
            std::vector<Dx11LevelEditorRenderer::Vertex>& triangleVertices,
            const Heightmap& heightmap,
            const LevelEditorRenderSettings& settings)
        {
            if (!heightmap.IsValid())
                return;

            const float terrainWidth = static_cast<float>(heightmap.GetWidth() - 1) * settings.CellSizeX;
            const float terrainHeight = static_cast<float>(heightmap.GetHeight() - 1) * settings.CellSizeY;

            const float minTerrainX = -terrainWidth * 0.5f;
            const float maxTerrainX = terrainWidth * 0.5f;
            const float minTerrainZ = -terrainHeight * 0.5f;
            const float maxTerrainZ = terrainHeight * 0.5f;

            const float maxAxis = (std::max)(terrainWidth, terrainHeight);
            const float margin = maxAxis * 0.55f;

            const float minX = minTerrainX - margin;
            const float maxX = maxTerrainX + margin;
            const float minZ = minTerrainZ - margin;
            const float maxZ = maxTerrainZ + margin;

            /*
                Важно:
                preview island сейчас имеет берег примерно 0.315.
                Если вода выше 0.315 — она накрывает карту.
                Поэтому держим waterRawLevel ниже берега.
            */
            const float waterRawLevel = 0.292f;

            const float waterY =
                (HeightForPreview(waterRawLevel, settings) - 0.5f) * settings.HeightScale - 20.0f;

            constexpr int grid = 96;

            const Color shallowWater { 0.045f, 0.310f, 0.340f, 1.0f };
            const Color deepWater    { 0.018f, 0.095f, 0.160f, 1.0f };
            const Color farWater     { 0.015f, 0.070f, 0.125f, 1.0f };

            const float heightmapCenterX = static_cast<float>(heightmap.GetWidth() - 1) * 0.5f;
            const float heightmapCenterY = static_cast<float>(heightmap.GetHeight() - 1) * 0.5f;

            auto shouldDrawWaterCell = [&](float worldX, float worldZ)
            {
                const bool outsideTerrain =
                    worldX < minTerrainX ||
                    worldX > maxTerrainX ||
                    worldZ < minTerrainZ ||
                    worldZ > maxTerrainZ;

                if (outsideTerrain)
                    return true;

                const float heightmapX = worldX / settings.CellSizeX + heightmapCenterX;
                const float heightmapY = worldZ / settings.CellSizeY + heightmapCenterY;

                const int sampleX = std::clamp(
                    static_cast<int>(std::round(heightmapX)),
                    0,
                    heightmap.GetWidth() - 1);

                const int sampleY = std::clamp(
                    static_cast<int>(std::round(heightmapY)),
                    0,
                    heightmap.GetHeight() - 1);

                const float rawHeight = heightmap.GetHeightNormalized(sampleX, sampleY);

                /*
                    Рисуем воду внутри terrain только если земля реально ниже воды.
                    Сейчас для preview island это почти не сработает, и это правильно:
                    остров не должен быть залит.
                */
                return rawHeight <= waterRawLevel;
            };

            for (int z = 0; z < grid; ++z)
            {
                for (int x = 0; x < grid; ++x)
                {
                    const float tx0 = static_cast<float>(x) / static_cast<float>(grid);
                    const float tx1 = static_cast<float>(x + 1) / static_cast<float>(grid);
                    const float tz0 = static_cast<float>(z) / static_cast<float>(grid);
                    const float tz1 = static_cast<float>(z + 1) / static_cast<float>(grid);

                    const float wx0 = minX + (maxX - minX) * tx0;
                    const float wx1 = minX + (maxX - minX) * tx1;
                    const float wz0 = minZ + (maxZ - minZ) * tz0;
                    const float wz1 = minZ + (maxZ - minZ) * tz1;

                    const float centerWorldX = (wx0 + wx1) * 0.5f;
                    const float centerWorldZ = (wz0 + wz1) * 0.5f;

                    if (!shouldDrawWaterCell(centerWorldX, centerWorldZ))
                        continue;

                    const float normalizedDistanceFromCenter =
                        std::sqrt(
                            (centerWorldX / (maxAxis * 0.5f)) * (centerWorldX / (maxAxis * 0.5f)) +
                            (centerWorldZ / (maxAxis * 0.5f)) * (centerWorldZ / (maxAxis * 0.5f)));

                    const float wave =
                        (std::sin(centerWorldX * 0.0012f + centerWorldZ * 0.0017f) + 1.0f) * 0.5f;

                    Color waterColor =
                        LerpColor(
                            shallowWater,
                            deepWater,
                            std::clamp(normalizedDistanceFromCenter * 0.55f + wave * 0.08f, 0.0f, 1.0f));

                    waterColor =
                        LerpColor(
                            waterColor,
                            farWater,
                            std::clamp((normalizedDistanceFromCenter - 1.0f) * 0.65f, 0.0f, 1.0f));

                    const float smallWaveA = std::sin(wx0 * 0.0011f + wz0 * 0.0019f) * 5.0f;
                    const float smallWaveB = std::sin(wx1 * 0.0011f + wz1 * 0.0019f) * 5.0f;

                    const Float3 p00{ wx0, waterY + smallWaveA, wz0 };
                    const Float3 p10{ wx1, waterY + smallWaveB, wz0 };
                    const Float3 p11{ wx1, waterY + smallWaveA, wz1 };
                    const Float3 p01{ wx0, waterY + smallWaveB, wz1 };

                    AddTriangle(triangleVertices, p00, p10, p11, waterColor);
                    AddTriangle(triangleVertices, p00, p11, p01, ScaleColor(waterColor, 0.94f));
                }
            }
        }

        void AddReferenceGrid(
            std::vector<Dx11LevelEditorRenderer::Vertex>& lineVertices,
            const Heightmap& heightmap,
            const LevelEditorRenderSettings& settings)
        {
            if (!heightmap.IsValid())
                return;

            const float terrainWidth = static_cast<float>(heightmap.GetWidth() - 1) * settings.CellSizeX;
            const float terrainHeight = static_cast<float>(heightmap.GetHeight() - 1) * settings.CellSizeY;

            const float minX = -terrainWidth * 0.5f;
            const float maxX = terrainWidth * 0.5f;
            const float minZ = -terrainHeight * 0.5f;
            const float maxZ = terrainHeight * 0.5f;

            const float y = (std::min)(settings.CellSizeX, settings.CellSizeY) * 0.08f;
            const Color gridColor{ 0.210f, 0.250f, 0.185f, 1.0f };
            const Color majorGridColor{ 0.330f, 0.360f, 0.230f, 1.0f };
            const Color xAxisColor{ 0.650f, 0.280f, 0.190f, 1.0f };
            const Color zAxisColor{ 0.220f, 0.390f, 0.670f, 1.0f };

            constexpr int gridDivisions = 16;

            for (int i = 0; i <= gridDivisions; ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(gridDivisions);
                const float x = minX + (maxX - minX) * t;
                const float z = minZ + (maxZ - minZ) * t;
                const bool majorLine = i == 0 || i == gridDivisions || i == gridDivisions / 2;
                const Color color = majorLine ? majorGridColor : gridColor;

                AddLine(lineVertices, { x, y, minZ }, { x, y, maxZ }, color);
                AddLine(lineVertices, { minX, y, z }, { maxX, y, z }, color);
            }

            AddLine(lineVertices, { minX, y * 1.5f, 0.0f }, { maxX, y * 1.5f, 0.0f }, xAxisColor);
            AddLine(lineVertices, { 0.0f, y * 1.5f, minZ }, { 0.0f, y * 1.5f, maxZ }, zAxisColor);
        }

        void AddBrushCircle(
            std::vector<Dx11LevelEditorRenderer::Vertex>& lineVertices,
            const Heightmap& heightmap,
            const LevelEditorRenderSettings& settings)
        {
            if (!settings.ShowBrush || !heightmap.IsValid())
                return;

            constexpr int segmentCount = 96;

            const float centerX = static_cast<float>(heightmap.GetWidth() - 1) * 0.5f;
            const float centerY = static_cast<float>(heightmap.GetHeight() - 1) * 0.5f;

            const float radiusCells = (std::max)(0.1f, settings.BrushRadiusCells);

            const Color color{ 1.0f, 0.760f, 0.190f, 1.0f };

            auto makeCirclePoint = [&](float angle)
            {
                const float sampleX = settings.BrushX + std::cos(angle) * radiusCells;
                const float sampleY = settings.BrushY + std::sin(angle) * radiusCells;

                const int heightSampleX = std::clamp(static_cast<int>(std::round(sampleX)), 0, heightmap.GetWidth() - 1);
                    const int heightSampleY = std::clamp(static_cast<int>(std::round(sampleY)), 0, heightmap.GetHeight() - 1);

                return Float3
                {
                    (sampleX - centerX) * settings.CellSizeX,
                    (HeightForPreview(heightmap.GetHeightNormalized(heightSampleX, heightSampleY), settings) - 0.5f) * settings.HeightScale + (std::min)(settings.CellSizeX, settings.CellSizeY) * 0.35f,
                    (sampleY - centerY) * settings.CellSizeY
                };
            };

            for (int i = 0; i < segmentCount; ++i)
            {
                const float angleA = static_cast<float>(i) / static_cast<float>(segmentCount) * Pi * 2.0f;
                const float angleB = static_cast<float>(i + 1) / static_cast<float>(segmentCount) * Pi * 2.0f;

                AddLine(
                    lineVertices,
                    makeCirclePoint(angleA),
                    makeCirclePoint(angleB),
                    color);
            }
        }

        Color ObjectColor(EditorObjectType type)
        {
            switch (type)
            {
                case EditorObjectType::StaticMeshProxy:
                    return { 1.0f, 0.12f, 0.16f, 1.0f };

                case EditorObjectType::SpeedTreeProxy:
                    return { 0.12f, 0.50f, 0.12f, 1.0f };

                case EditorObjectType::PlayerSpawn:
                    return { 0.20f, 0.42f, 1.0f, 1.0f };

                case EditorObjectType::LootSpawner:
                    return { 1.0f, 0.75f, 0.12f, 1.0f };

                case EditorObjectType::SafeZone:
                    return { 0.15f, 1.0f, 0.45f, 1.0f };

                case EditorObjectType::RadiationZone:
                    return { 0.75f, 1.0f, 0.12f, 1.0f };

                case EditorObjectType::AnomalyZone:
                    return { 0.80f, 0.18f, 1.0f, 1.0f };

                case EditorObjectType::Light:
                    return { 1.0f, 0.95f, 0.45f, 1.0f };

                default:
                    return { 1.0f, 0.12f, 0.16f, 1.0f };
            }
        }

        Float3 ObjectHalfSize(const EditorObject& object)
        {
            const float radius = (std::max)(1.0f, object.Radius);

            switch (object.Type)
            {
                case EditorObjectType::StaticMeshProxy:
                    return { 125.0f, 125.0f, 125.0f };

                case EditorObjectType::SpeedTreeProxy:
                    return { 90.0f, 420.0f, 90.0f };

                case EditorObjectType::PlayerSpawn:
                    return { 45.0f, 90.0f, 45.0f };

                case EditorObjectType::LootSpawner:
                    return { 65.0f, 45.0f, 65.0f };

                case EditorObjectType::SafeZone:
                case EditorObjectType::RadiationZone:
                    return { radius, 60.0f, radius };

                case EditorObjectType::AnomalyZone:
                    return { radius, 120.0f, radius };

                case EditorObjectType::Light:
                    return { 40.0f, 40.0f, 40.0f };

                default:
                    return { 100.0f, 100.0f, 100.0f };
            }
        }

        Float3 TransformObjectPoint(const Float3& localPoint, const EditorObject& object)
        {
            const DirectX::XMMATRIX transform =
                DirectX::XMMatrixScaling(
                    object.Scale.X,
                    object.Scale.Y,
                    object.Scale.Z) *
                DirectX::XMMatrixRotationRollPitchYaw(
                    DegToRad(object.Rotation.X),
                    DegToRad(object.Rotation.Y),
                    DegToRad(object.Rotation.Z)) *
                DirectX::XMMatrixTranslation(
                    object.Position.X,
                    object.Position.Y,
                    object.Position.Z);

            const DirectX::XMVECTOR localVector = DirectX::XMVectorSet(
                localPoint.X,
                localPoint.Y,
                localPoint.Z,
                1.0f);

            const DirectX::XMVECTOR worldVector =
                DirectX::XMVector3TransformCoord(localVector, transform);

            DirectX::XMFLOAT3 result{};
            DirectX::XMStoreFloat3(&result, worldVector);

            return { result.x, result.y, result.z };
        }

        void AddObjectBox(
            std::vector<Dx11LevelEditorRenderer::Vertex>& lineVertices,
            const EditorObject& object,
            const Color& color)
        {
            const Float3 halfSize = ObjectHalfSize(object);

            const Float3 localCorners[8]
            {
                { -halfSize.X, -halfSize.Y, -halfSize.Z },
                {  halfSize.X, -halfSize.Y, -halfSize.Z },
                {  halfSize.X, -halfSize.Y,  halfSize.Z },
                { -halfSize.X, -halfSize.Y,  halfSize.Z },

                { -halfSize.X,  halfSize.Y, -halfSize.Z },
                {  halfSize.X,  halfSize.Y, -halfSize.Z },
                {  halfSize.X,  halfSize.Y,  halfSize.Z },
                { -halfSize.X,  halfSize.Y,  halfSize.Z }
            };

            Float3 corners[8]{};

            for (int i = 0; i < 8; ++i)
                corners[i] = TransformObjectPoint(localCorners[i], object);

            const int edges[12][2]
            {
                { 0, 1 },
                { 1, 2 },
                { 2, 3 },
                { 3, 0 },

                { 4, 5 },
                { 5, 6 },
                { 6, 7 },
                { 7, 4 },

                { 0, 4 },
                { 1, 5 },
                { 2, 6 },
                { 3, 7 }
            };

            for (const auto& edge : edges)
                AddLine(lineVertices, corners[edge[0]], corners[edge[1]], color);

            const Float3 objectCenter
            {
                object.Position.X,
                object.Position.Y,
                object.Position.Z
            };

            const Float3 objectTop
            {
                object.Position.X,
                object.Position.Y + halfSize.Y * object.Scale.Y,
                object.Position.Z
            };

            AddLine(lineVertices, objectCenter, objectTop, color);
        }

        Color ObjectFillColor(EditorObjectType type)
        {
            switch (type)
            {
                case EditorObjectType::StaticMeshProxy:
                    return { 0.33f, 0.25f, 0.19f, 1.0f };

                case EditorObjectType::SpeedTreeProxy:
                    return { 0.08f, 0.25f, 0.08f, 1.0f };

                case EditorObjectType::PlayerSpawn:
                    return { 0.08f, 0.18f, 0.60f, 1.0f };

                case EditorObjectType::LootSpawner:
                    return { 0.55f, 0.38f, 0.10f, 1.0f };

                case EditorObjectType::SafeZone:
                    return { 0.04f, 0.34f, 0.16f, 1.0f };

                case EditorObjectType::RadiationZone:
                    return { 0.38f, 0.48f, 0.05f, 1.0f };

                case EditorObjectType::AnomalyZone:
                    return { 0.36f, 0.08f, 0.50f, 1.0f };

                case EditorObjectType::Light:
                    return { 0.75f, 0.62f, 0.20f, 1.0f };

                default:
                    return { 0.35f, 0.25f, 0.20f, 1.0f };
            }
        }

        void AddSolidBox(
            std::vector<Dx11LevelEditorRenderer::Vertex>& triangleVertices,
            const EditorObject& object,
            const Color& baseColor)
        {
            const Float3 halfSize = ObjectHalfSize(object);

            const Float3 localCorners[8]
            {
                { -halfSize.X, -halfSize.Y, -halfSize.Z },
                {  halfSize.X, -halfSize.Y, -halfSize.Z },
                {  halfSize.X, -halfSize.Y,  halfSize.Z },
                { -halfSize.X, -halfSize.Y,  halfSize.Z },

                { -halfSize.X,  halfSize.Y, -halfSize.Z },
                {  halfSize.X,  halfSize.Y, -halfSize.Z },
                {  halfSize.X,  halfSize.Y,  halfSize.Z },
                { -halfSize.X,  halfSize.Y,  halfSize.Z }
            };

            Float3 corners[8]{};

            for (int i = 0; i < 8; ++i)
                corners[i] = TransformObjectPoint(localCorners[i], object);

            const int faces[12][3]
            {
                { 0, 1, 2 }, { 0, 2, 3 },
                { 4, 6, 5 }, { 4, 7, 6 },
                { 0, 4, 5 }, { 0, 5, 1 },
                { 1, 5, 6 }, { 1, 6, 2 },
                { 2, 6, 7 }, { 2, 7, 3 },
                { 3, 7, 4 }, { 3, 4, 0 }
            };

            for (int i = 0; i < 12; ++i)
            {
                const Float3 a = corners[faces[i][0]];
                const Float3 b = corners[faces[i][1]];
                const Float3 c = corners[faces[i][2]];

                const Float3 normal = CalculateTriangleNormal(a, b, c);
                const Color color = ApplySunLight(baseColor, normal);

                AddTriangle(triangleVertices, a, b, c, color);
            }
        }

        void AddTreeProxySolid(
            std::vector<Dx11LevelEditorRenderer::Vertex>& triangleVertices,
            const EditorObject& object)
        {
            const float radius = 120.0f * object.Scale.X;
            const float trunkRadius = 28.0f * object.Scale.X;
            const float trunkHeight = 210.0f * object.Scale.Y;
            const float treeHeight = 760.0f * object.Scale.Y;

            const Float3 base
            {
                object.Position.X,
                object.Position.Y,
                object.Position.Z
            };

            const Color trunkColor { 0.20f, 0.12f, 0.065f, 1.0f };
            const Color leafColorA { 0.045f, 0.18f, 0.055f, 1.0f };
            const Color leafColorB { 0.065f, 0.28f, 0.075f, 1.0f };

            EditorObject trunk = object;
            trunk.Type = EditorObjectType::StaticMeshProxy;
            trunk.Position = { base.X, base.Y + trunkHeight * 0.5f, base.Z };
            trunk.Scale = { 1.0f, 1.0f, 1.0f };
            trunk.Radius = trunkRadius;

            const Float3 trunkHalf{ trunkRadius, trunkHeight * 0.5f, trunkRadius };

            const Float3 trunkCorners[8]
            {
                { -trunkHalf.X, -trunkHalf.Y, -trunkHalf.Z },
                {  trunkHalf.X, -trunkHalf.Y, -trunkHalf.Z },
                {  trunkHalf.X, -trunkHalf.Y,  trunkHalf.Z },
                { -trunkHalf.X, -trunkHalf.Y,  trunkHalf.Z },

                { -trunkHalf.X,  trunkHalf.Y, -trunkHalf.Z },
                {  trunkHalf.X,  trunkHalf.Y, -trunkHalf.Z },
                {  trunkHalf.X,  trunkHalf.Y,  trunkHalf.Z },
                { -trunkHalf.X,  trunkHalf.Y,  trunkHalf.Z }
            };

            Float3 tc[8]{};

            for (int i = 0; i < 8; ++i)
                tc[i] = TransformObjectPoint(trunkCorners[i], trunk);

            AddTriangle(triangleVertices, tc[0], tc[1], tc[2], trunkColor);
            AddTriangle(triangleVertices, tc[0], tc[2], tc[3], trunkColor);
            AddTriangle(triangleVertices, tc[4], tc[6], tc[5], trunkColor);
            AddTriangle(triangleVertices, tc[4], tc[7], tc[6], trunkColor);
            AddTriangle(triangleVertices, tc[0], tc[4], tc[5], trunkColor);
            AddTriangle(triangleVertices, tc[0], tc[5], tc[1], trunkColor);
            AddTriangle(triangleVertices, tc[1], tc[5], tc[6], trunkColor);
            AddTriangle(triangleVertices, tc[1], tc[6], tc[2], trunkColor);
            AddTriangle(triangleVertices, tc[2], tc[6], tc[7], trunkColor);
            AddTriangle(triangleVertices, tc[2], tc[7], tc[3], trunkColor);
            AddTriangle(triangleVertices, tc[3], tc[7], tc[4], trunkColor);
            AddTriangle(triangleVertices, tc[3], tc[4], tc[0], trunkColor);

            const Float3 foliageBase[4]
            {
                { base.X - radius, base.Y + trunkHeight, base.Z - radius },
                { base.X + radius, base.Y + trunkHeight, base.Z - radius },
                { base.X + radius, base.Y + trunkHeight, base.Z + radius },
                { base.X - radius, base.Y + trunkHeight, base.Z + radius }
            };

            const Float3 top
            {
                base.X,
                base.Y + treeHeight,
                base.Z
            };

            AddTriangle(triangleVertices, foliageBase[0], foliageBase[1], top, leafColorA);
            AddTriangle(triangleVertices, foliageBase[1], foliageBase[2], top, leafColorB);
            AddTriangle(triangleVertices, foliageBase[2], foliageBase[3], top, leafColorA);
            AddTriangle(triangleVertices, foliageBase[3], foliageBase[0], top, leafColorB);
            AddTriangle(triangleVertices, foliageBase[0], foliageBase[2], foliageBase[1], ScaleColor(leafColorA, 0.75f));
            AddTriangle(triangleVertices, foliageBase[0], foliageBase[3], foliageBase[2], ScaleColor(leafColorB, 0.75f));
        }

        void BuildSceneSolidVertices(
            std::vector<Dx11LevelEditorRenderer::Vertex>& triangleVertices,
            const EditorScene& scene)
        {
            for (const EditorObject& object : scene.GetObjects())
            {
                if (!object.Visible)
                    continue;

                if (object.Type == EditorObjectType::SpeedTreeProxy)
                {
                    AddTreeProxySolid(triangleVertices, object);
                    continue;
                }

                if (object.Type == EditorObjectType::SafeZone ||
                    object.Type == EditorObjectType::RadiationZone ||
                    object.Type == EditorObjectType::AnomalyZone)
                {
                    continue;
                }

                AddSolidBox(
                    triangleVertices,
                    object,
                    ObjectFillColor(object.Type));
            }
        }

        void BuildSceneObjectVertices(
            std::vector<Dx11LevelEditorRenderer::Vertex>& lineVertices,
            const EditorScene& scene)
        {
            const std::uint32_t selectedObjectId = scene.GetSelectedObjectId();

            for (const EditorObject& object : scene.GetObjects())
            {
                if (!object.Visible)
                    continue;

                Color color = ObjectColor(object.Type);

                if (object.Id == selectedObjectId)
                    color = { 1.0f, 0.90f, 0.20f, 1.0f };

                AddObjectBox(lineVertices, object, color);
            }
        }

        void DebugOutputBlob(ID3DBlob* blob)
        {
            if (blob == nullptr)
                return;

            const char* text = static_cast<const char*>(blob->GetBufferPointer());

            if (text == nullptr)
                return;

            OutputDebugStringA(text);
            OutputDebugStringA("\n");
        }

        bool CompileShader(
            const char* sourceCode,
            const char* entryPoint,
            const char* target,
            ID3DBlob** outputBlob)
        {
            if (sourceCode == nullptr || entryPoint == nullptr || target == nullptr || outputBlob == nullptr)
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
                errorBlob.GetAddressOf());

            if (FAILED(result))
            {
                DebugOutputBlob(errorBlob.Get());
                return false;
            }

            return true;
        }
    }

    Dx11LevelEditorRenderer::~Dx11LevelEditorRenderer()
    {
        Shutdown();
    }

    bool Dx11LevelEditorRenderer::Initialize(ID3D11Device* device)
    {
        if (m_initialized)
            return true;

        if (device == nullptr)
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

    void Dx11LevelEditorRenderer::Shutdown()
    {
        m_depthStencilState.Reset();
        m_rasterizerState.Reset();

        m_cameraConstantBuffer.Reset();
        m_dynamicVertexBuffer.Reset();

        m_inputLayout.Reset();
        m_pixelShader.Reset();
        m_vertexShader.Reset();

        m_initialized = false;
    }

    void Dx11LevelEditorRenderer::Render(
        ID3D11DeviceContext* deviceContext,
        std::uint32_t viewportWidth,
        std::uint32_t viewportHeight,
        const Heightmap& heightmap,
        const LevelEditorRenderSettings& settings,
        const EditorScene& scene)
    {
        if (!m_initialized || deviceContext == nullptr || viewportWidth == 0 || viewportHeight == 0)
            return;

        if (!heightmap.IsValid())
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
        deviceContext->OMSetDepthStencilState(m_depthStencilState.Get(), 0);

        deviceContext->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        deviceContext->PSSetShader(m_pixelShader.Get(), nullptr, 0);
        deviceContext->IASetInputLayout(m_inputLayout.Get());

        ID3D11Buffer* cameraBuffers[]
        {
            m_cameraConstantBuffer.Get()
        };

        deviceContext->VSSetConstantBuffers(0, 1, cameraBuffers);

        const DirectX::XMMATRIX viewProjection = BuildViewProjection(
            viewportWidth,
            viewportHeight,
            heightmap,
            settings);

        CameraConstantBuffer cameraData{};
        cameraData.ViewProjection = DirectX::XMMatrixTranspose(viewProjection);

        deviceContext->UpdateSubresource(
            m_cameraConstantBuffer.Get(),
            0,
            nullptr,
            &cameraData,
            0,
            0);

        std::vector<Vertex> triangleVertices;
        std::vector<Vertex> lineVertices;

        BuildTerrainVertices(triangleVertices, lineVertices, heightmap, settings);

        if (settings.ShowWater)
            AddWaterPlane(triangleVertices, heightmap, settings);

        BuildSceneSolidVertices(triangleVertices, scene);

        if (settings.ShowDebugGrid)
            AddReferenceGrid(lineVertices, heightmap, settings);

        AddBrushCircle(lineVertices, heightmap, settings);

        if (settings.ShowObjectWireframe)
            BuildSceneObjectVertices(lineVertices, scene);

        UploadAndDraw(
            deviceContext,
            triangleVertices,
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
            3);

        UploadAndDraw(
            deviceContext,
            lineVertices,
            D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
            2);
    }

    bool Dx11LevelEditorRenderer::CreateShaders(ID3D11Device* device)
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
                output.Position = mul(float4(input.Position, 1.0f), ViewProjection);
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

        if (!CompileShader(shaderCode, "VSMain", "vs_5_0", vertexShaderBlob.GetAddressOf()))
            return false;

        if (!CompileShader(shaderCode, "PSMain", "ps_5_0", pixelShaderBlob.GetAddressOf()))
            return false;

        const HRESULT createVertexShaderResult = device->CreateVertexShader(
            vertexShaderBlob->GetBufferPointer(),
            vertexShaderBlob->GetBufferSize(),
            nullptr,
            m_vertexShader.GetAddressOf());

        if (FAILED(createVertexShaderResult))
            return false;

        const HRESULT createPixelShaderResult = device->CreatePixelShader(
            pixelShaderBlob->GetBufferPointer(),
            pixelShaderBlob->GetBufferSize(),
            nullptr,
            m_pixelShader.GetAddressOf());

        if (FAILED(createPixelShaderResult))
            return false;

        const D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[]
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
            m_inputLayout.GetAddressOf());

        return SUCCEEDED(createInputLayoutResult);
    }

    bool Dx11LevelEditorRenderer::CreateBuffers(ID3D11Device* device)
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
            m_dynamicVertexBuffer.GetAddressOf());

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
            m_cameraConstantBuffer.GetAddressOf());

        return SUCCEEDED(createCameraBufferResult);
    }

    bool Dx11LevelEditorRenderer::CreateStates(ID3D11Device* device)
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

        const HRESULT rasterizerResult = device->CreateRasterizerState(
            &rasterizerDesc,
            m_rasterizerState.GetAddressOf());

        if (FAILED(rasterizerResult))
            return false;

        D3D11_DEPTH_STENCIL_DESC depthDesc{};
        depthDesc.DepthEnable = TRUE;
        depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        depthDesc.StencilEnable = FALSE;

        const HRESULT depthResult = device->CreateDepthStencilState(
            &depthDesc,
            m_depthStencilState.GetAddressOf());

        return SUCCEEDED(depthResult);
    }

    bool Dx11LevelEditorRenderer::UploadAndDraw(
        ID3D11DeviceContext* deviceContext,
        const std::vector<Vertex>& vertices,
        D3D11_PRIMITIVE_TOPOLOGY topology,
        std::size_t primitiveVertexCount)
    {
        if (vertices.empty())
            return true;

        if (deviceContext == nullptr || primitiveVertexCount == 0)
            return false;

        std::size_t offset = 0;

        while (offset < vertices.size())
        {
            std::size_t batchCount = (std::min)(MaxDynamicVertices, vertices.size() - offset);
            batchCount -= batchCount % primitiveVertexCount;

            if (batchCount == 0)
                break;

            D3D11_MAPPED_SUBRESOURCE mappedResource{};

            const HRESULT mapResult = deviceContext->Map(
                m_dynamicVertexBuffer.Get(),
                0,
                D3D11_MAP_WRITE_DISCARD,
                0,
                &mappedResource);

            if (FAILED(mapResult))
                return false;

            std::memcpy(
                mappedResource.pData,
                vertices.data() + offset,
                batchCount * sizeof(Vertex));

            deviceContext->Unmap(m_dynamicVertexBuffer.Get(), 0);

            constexpr UINT stride = sizeof(Vertex);
            constexpr UINT vertexOffset = 0;

            ID3D11Buffer* vertexBuffers[]
            {
                m_dynamicVertexBuffer.Get()
            };

            deviceContext->IASetVertexBuffers(
                0,
                1,
                vertexBuffers,
                &stride,
                &vertexOffset);

            deviceContext->IASetPrimitiveTopology(topology);
            deviceContext->Draw(static_cast<UINT>(batchCount), 0);

            offset += batchCount;
        }

        return true;
    }
}
