#include "Engine/Renderer/Dx11WorldRenderer.h"

#include <DirectXMath.h>
#include <Windows.h>
#include <d3dcompiler.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace StalkerOnline::Engine
{
    namespace
    {
        constexpr std::size_t MaxDynamicVertices = 65536;
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

        Float3 ToRenderPosition(float gameX, float gameY, float gameZ)
        {
            return
            {
                gameX,
                gameZ,
                gameY
            };
        }

        void AddVertex(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            const Float3& position,
            const Color& color
        )
        {
            Dx11WorldRenderer::Vertex vertex{};
            vertex.Position[0] = position.X;
            vertex.Position[1] = position.Y;
            vertex.Position[2] = position.Z;

            vertex.Color[0] = color[0];
            vertex.Color[1] = color[1];
            vertex.Color[2] = color[2];
            vertex.Color[3] = color[3];

            vertices.push_back(vertex);
        }

        void AddLine(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            const Float3& a,
            const Float3& b,
            const Color& color
        )
        {
            AddVertex(vertices, a, color);
            AddVertex(vertices, b, color);
        }

        void AddTriangle(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            const Float3& a,
            const Float3& b,
            const Float3& c,
            const Color& color
        )
        {
            AddVertex(vertices, a, color);
            AddVertex(vertices, b, color);
            AddVertex(vertices, c, color);
        }

        void AddQuad(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            const Float3& a,
            const Float3& b,
            const Float3& c,
            const Float3& d,
            const Color& color
        )
        {
            AddTriangle(vertices, a, b, c, color);
            AddTriangle(vertices, a, c, d, color);
        }

        void AddCube(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            const Float3& center,
            const Float3& halfSize,
            const Color& baseColor
        )
        {
            const float minX = center.X - halfSize.X;
            const float maxX = center.X + halfSize.X;

            const float minY = center.Y - halfSize.Y;
            const float maxY = center.Y + halfSize.Y;

            const float minZ = center.Z - halfSize.Z;
            const float maxZ = center.Z + halfSize.Z;

            const Float3 p000{ minX, minY, minZ };
            const Float3 p001{ minX, minY, maxZ };
            const Float3 p010{ minX, maxY, minZ };
            const Float3 p011{ minX, maxY, maxZ };

            const Float3 p100{ maxX, minY, minZ };
            const Float3 p101{ maxX, minY, maxZ };
            const Float3 p110{ maxX, maxY, minZ };
            const Float3 p111{ maxX, maxY, maxZ };

            const Color topColor = ScaleColor(baseColor, 1.25f);
            const Color frontColor = ScaleColor(baseColor, 1.05f);
            const Color sideColor = ScaleColor(baseColor, 0.85f);
            const Color bottomColor = ScaleColor(baseColor, 0.55f);

            AddQuad(vertices, p001, p101, p111, p011, frontColor);
            AddQuad(vertices, p100, p000, p010, p110, sideColor);

            AddQuad(vertices, p000, p001, p011, p010, sideColor);
            AddQuad(vertices, p101, p100, p110, p111, sideColor);

            AddQuad(vertices, p010, p011, p111, p110, topColor);
            AddQuad(vertices, p000, p100, p101, p001, bottomColor);
        }

        Float3 TransformLocalPoint(
            const Float3& center,
            const Float3& local,
            float yawRadians
        )
        {
            const float cosYaw = std::cos(yawRadians);
            const float sinYaw = std::sin(yawRadians);

            return
            {
                center.X + local.X * cosYaw + local.Z * sinYaw,
                center.Y + local.Y,
                center.Z - local.X * sinYaw + local.Z * cosYaw
            };
        }

        void AddOrientedBox(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            const Float3& center,
            const Float3& halfSize,
            float yawRadians,
            const Color& baseColor
        )
        {
            const Float3 p000 = TransformLocalPoint(center, Float3{ -halfSize.X, -halfSize.Y, -halfSize.Z }, yawRadians);
            const Float3 p001 = TransformLocalPoint(center, Float3{ -halfSize.X, -halfSize.Y, halfSize.Z }, yawRadians);
            const Float3 p010 = TransformLocalPoint(center, Float3{ -halfSize.X, halfSize.Y, -halfSize.Z }, yawRadians);
            const Float3 p011 = TransformLocalPoint(center, Float3{ -halfSize.X, halfSize.Y, halfSize.Z }, yawRadians);

            const Float3 p100 = TransformLocalPoint(center, Float3{ halfSize.X, -halfSize.Y, -halfSize.Z }, yawRadians);
            const Float3 p101 = TransformLocalPoint(center, Float3{ halfSize.X, -halfSize.Y, halfSize.Z }, yawRadians);
            const Float3 p110 = TransformLocalPoint(center, Float3{ halfSize.X, halfSize.Y, -halfSize.Z }, yawRadians);
            const Float3 p111 = TransformLocalPoint(center, Float3{ halfSize.X, halfSize.Y, halfSize.Z }, yawRadians);

            const Color topColor = ScaleColor(baseColor, 1.22f);
            const Color frontColor = ScaleColor(baseColor, 1.03f);
            const Color sideColor = ScaleColor(baseColor, 0.84f);
            const Color bottomColor = ScaleColor(baseColor, 0.55f);

            AddQuad(vertices, p001, p101, p111, p011, frontColor);
            AddQuad(vertices, p100, p000, p010, p110, sideColor);

            AddQuad(vertices, p000, p001, p011, p010, sideColor);
            AddQuad(vertices, p101, p100, p110, p111, sideColor);

            AddQuad(vertices, p010, p011, p111, p110, topColor);
            AddQuad(vertices, p000, p100, p101, p001, bottomColor);
        }

        float TerrainHeight(float x, float z)
        {
            return
                std::sin(x * 0.18f) * 0.06f +
                std::cos(z * 0.15f) * 0.05f +
                std::sin((x + z) * 0.07f) * 0.04f;
        }

        Color TerrainColor(float x, float z)
        {
            const float variation =
                std::sin(x * 0.41f + z * 0.17f) * 0.5f +
                std::cos(x * 0.13f - z * 0.37f) * 0.5f;

            const float light = std::clamp(0.84f + variation * 0.10f, 0.68f, 0.98f);

            return
            {
                0.145f * light,
                0.165f * light,
                0.105f * light,
                1.0f
            };
        }

        void AddTerrain(std::vector<Dx11WorldRenderer::Vertex>& vertices)
        {
            constexpr int halfCellCount = 24;
            constexpr float cellSize = 2.0f;

            for (int z = -halfCellCount; z < halfCellCount; ++z)
            {
                for (int x = -halfCellCount; x < halfCellCount; ++x)
                {
                    const float x0 = static_cast<float>(x) * cellSize;
                    const float x1 = static_cast<float>(x + 1) * cellSize;
                    const float z0 = static_cast<float>(z) * cellSize;
                    const float z1 = static_cast<float>(z + 1) * cellSize;

                    const Float3 p00{ x0, TerrainHeight(x0, z0), z0 };
                    const Float3 p10{ x1, TerrainHeight(x1, z0), z0 };
                    const Float3 p11{ x1, TerrainHeight(x1, z1), z1 };
                    const Float3 p01{ x0, TerrainHeight(x0, z1), z1 };

                    AddQuad(
                        vertices,
                        p00,
                        p10,
                        p11,
                        p01,
                        TerrainColor((x0 + x1) * 0.5f, (z0 + z1) * 0.5f)
                    );
                }
            }
        }

        void AddRoad(std::vector<Dx11WorldRenderer::Vertex>& vertices)
        {
            constexpr int segmentCount = 46;
            constexpr float segmentLength = 2.0f;
            constexpr float y = 0.045f;

            for (int i = -segmentCount / 2; i < segmentCount / 2; ++i)
            {
                const float z0 = static_cast<float>(i) * segmentLength;
                const float z1 = static_cast<float>(i + 1) * segmentLength;
                const float bend = std::sin(static_cast<float>(i) * 0.22f) * 0.75f;
                const float nextBend = std::sin(static_cast<float>(i + 1) * 0.22f) * 0.75f;
                const float width = 3.2f + std::sin(static_cast<float>(i) * 0.61f) * 0.25f;

                const Color roadColor =
                {
                    0.225f,
                    0.205f + (i % 3 == 0 ? 0.014f : 0.0f),
                    0.155f,
                    1.0f
                };

                AddQuad(
                    vertices,
                    Float3{ bend - width, y, z0 },
                    Float3{ bend + width, y, z0 },
                    Float3{ nextBend + width, y, z1 },
                    Float3{ nextBend - width, y, z1 },
                    roadColor
                );
            }

            AddQuad(
                vertices,
                Float3{ -23.0f, y + 0.005f, 10.1f },
                Float3{ 23.0f, y + 0.005f, 10.1f },
                Float3{ 23.0f, y + 0.005f, 14.3f },
                Float3{ -23.0f, y + 0.005f, 14.3f },
                Color{ 0.190f, 0.180f, 0.135f, 1.0f }
            );
        }

        void AddGroundDisc(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            const Float3& center,
            float radius,
            const Color& color
        )
        {
            constexpr int segmentCount = 48;

            for (int i = 0; i < segmentCount; ++i)
            {
                const float angleA = static_cast<float>(i) / static_cast<float>(segmentCount) * Pi * 2.0f;
                const float angleB = static_cast<float>(i + 1) / static_cast<float>(segmentCount) * Pi * 2.0f;

                const Float3 pointA
                {
                    center.X + std::cos(angleA) * radius,
                    center.Y,
                    center.Z + std::sin(angleA) * radius
                };

                const Float3 pointB
                {
                    center.X + std::cos(angleB) * radius,
                    center.Y,
                    center.Z + std::sin(angleB) * radius
                };

                AddTriangle(vertices, center, pointA, pointB, color);
            }
        }

        void AddRuinBlock(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            const Float3& origin
        )
        {
            const Color concrete{ 0.315f, 0.330f, 0.300f, 1.0f };
            const Color rust{ 0.355f, 0.175f, 0.095f, 1.0f };

            AddCube(vertices, Float3{ origin.X - 2.7f, origin.Y + 1.25f, origin.Z }, Float3{ 0.22f, 1.25f, 2.8f }, concrete);
            AddCube(vertices, Float3{ origin.X + 2.7f, origin.Y + 1.00f, origin.Z - 0.45f }, Float3{ 0.22f, 1.00f, 2.15f }, concrete);
            AddCube(vertices, Float3{ origin.X, origin.Y + 1.05f, origin.Z - 2.7f }, Float3{ 2.7f, 1.05f, 0.22f }, concrete);

            AddCube(vertices, Float3{ origin.X - 0.9f, origin.Y + 0.25f, origin.Z + 1.6f }, Float3{ 0.85f, 0.25f, 0.55f }, ScaleColor(concrete, 0.65f));
            AddCube(vertices, Float3{ origin.X + 1.5f, origin.Y + 0.35f, origin.Z + 1.9f }, Float3{ 0.55f, 0.35f, 0.45f }, ScaleColor(concrete, 0.72f));

            AddCube(vertices, Float3{ origin.X + 0.6f, origin.Y + 1.9f, origin.Z - 2.95f }, Float3{ 0.12f, 0.12f, 0.8f }, rust);
            AddCube(vertices, Float3{ origin.X + 1.7f, origin.Y + 1.65f, origin.Z - 2.95f }, Float3{ 0.10f, 0.10f, 0.7f }, rust);
        }

        void AddFenceLine(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            float x,
            float zStart,
            float zEnd
        )
        {
            const Color postColor{ 0.300f, 0.255f, 0.185f, 1.0f };
            const Color railColor{ 0.235f, 0.215f, 0.160f, 1.0f };

            const float step = zStart < zEnd ? 3.0f : -3.0f;

            for (float z = zStart; (step > 0.0f ? z <= zEnd : z >= zEnd); z += step)
            {
                AddCube(vertices, Float3{ x, 0.55f, z }, Float3{ 0.10f, 0.55f, 0.10f }, postColor);
            }

            const float centerZ = (zStart + zEnd) * 0.5f;
            const float halfZ = std::fabs(zEnd - zStart) * 0.5f;

            AddCube(vertices, Float3{ x, 0.82f, centerZ }, Float3{ 0.07f, 0.07f, halfZ }, railColor);
            AddCube(vertices, Float3{ x, 0.42f, centerZ }, Float3{ 0.06f, 0.06f, halfZ }, ScaleColor(railColor, 0.82f));
        }

        void AddDeadTree(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            const Float3& root,
            float yawRadians,
            float scale
        )
        {
            const Color bark{ 0.185f, 0.140f, 0.095f, 1.0f };
            const Color branch{ 0.150f, 0.120f, 0.085f, 1.0f };

            AddOrientedBox(
                vertices,
                Float3{ root.X, root.Y + 1.05f * scale, root.Z },
                Float3{ 0.13f * scale, 1.05f * scale, 0.13f * scale },
                yawRadians + 0.10f,
                bark
            );

            AddOrientedBox(
                vertices,
                Float3{ root.X + std::sin(yawRadians) * 0.45f * scale, root.Y + 2.0f * scale, root.Z + std::cos(yawRadians) * 0.45f * scale },
                Float3{ 0.08f * scale, 0.55f * scale, 0.08f * scale },
                yawRadians + 0.65f,
                branch
            );

            AddOrientedBox(
                vertices,
                Float3{ root.X - std::sin(yawRadians) * 0.35f * scale, root.Y + 1.65f * scale, root.Z - std::cos(yawRadians) * 0.35f * scale },
                Float3{ 0.07f * scale, 0.48f * scale, 0.07f * scale },
                yawRadians - 0.85f,
                branch
            );
        }

        void AddAnomaly(
            std::vector<Dx11WorldRenderer::Vertex>& triangleVertices,
            std::vector<Dx11WorldRenderer::Vertex>& lineVertices,
            const Float3& center
        )
        {
            AddGroundDisc(
                triangleVertices,
                Float3{ center.X, center.Y + 0.075f, center.Z },
                2.6f,
                Color{ 0.090f, 0.170f, 0.150f, 1.0f }
            );

            AddGroundDisc(
                triangleVertices,
                Float3{ center.X, center.Y + 0.082f, center.Z },
                1.25f,
                Color{ 0.170f, 0.365f, 0.315f, 1.0f }
            );

            const Color lineColor{ 0.40f, 0.86f, 0.68f, 1.0f };

            for (int i = 0; i < 8; ++i)
            {
                const float angle = static_cast<float>(i) / 8.0f * Pi * 2.0f;
                const float radius = 1.3f + (i % 2 == 0 ? 0.4f : 0.0f);

                const Float3 base
                {
                    center.X + std::cos(angle) * radius,
                    center.Y + 0.14f,
                    center.Z + std::sin(angle) * radius
                };

                AddLine(
                    lineVertices,
                    base,
                    Float3{ center.X, center.Y + 1.7f, center.Z },
                    lineColor
                );
            }
        }

        void AddSectorMarkers(std::vector<Dx11WorldRenderer::Vertex>& vertices)
        {
            constexpr int halfLineCount = 4;
            constexpr float step = 10.0f;
            constexpr float halfSize = static_cast<float>(halfLineCount) * step;

            const Color markerColor{ 0.135f, 0.165f, 0.135f, 1.0f };
            const Color centerColor{ 0.250f, 0.320f, 0.240f, 1.0f };

            for (int i = -halfLineCount; i <= halfLineCount; ++i)
            {
                const float value = static_cast<float>(i) * step;
                const Color& color = i == 0 ? centerColor : markerColor;

                AddLine(vertices, Float3{ -halfSize, 0.09f, value }, Float3{ halfSize, 0.09f, value }, color);
                AddLine(vertices, Float3{ value, 0.09f, -halfSize }, Float3{ value, 0.09f, halfSize }, color);
            }
        }

        void AddWorldScene(
            std::vector<Dx11WorldRenderer::Vertex>& triangleVertices,
            std::vector<Dx11WorldRenderer::Vertex>& lineVertices
        )
        {
            AddTerrain(triangleVertices);
            AddRoad(triangleVertices);

            AddRuinBlock(triangleVertices, Float3{ -10.0f, 0.0f, 14.0f });
            AddRuinBlock(triangleVertices, Float3{ 13.0f, 0.0f, -11.0f });

            AddFenceLine(triangleVertices, -5.4f, -20.0f, 4.0f);
            AddFenceLine(triangleVertices, 5.8f, -18.0f, 6.0f);

            AddDeadTree(triangleVertices, Float3{ -15.0f, 0.0f, -7.0f }, 0.45f, 1.0f);
            AddDeadTree(triangleVertices, Float3{ -19.0f, 0.0f, 17.0f }, -0.95f, 0.75f);
            AddDeadTree(triangleVertices, Float3{ 18.0f, 0.0f, 7.0f }, 1.25f, 0.9f);

            AddCube(triangleVertices, Float3{ 7.8f, 0.28f, 15.7f }, Float3{ 0.75f, 0.28f, 0.55f }, Color{ 0.245f, 0.235f, 0.195f, 1.0f });
            AddCube(triangleVertices, Float3{ 9.1f, 0.34f, 16.4f }, Float3{ 0.45f, 0.34f, 0.72f }, Color{ 0.200f, 0.205f, 0.185f, 1.0f });
            AddCube(triangleVertices, Float3{ -7.8f, 0.24f, -13.8f }, Float3{ 0.65f, 0.24f, 0.38f }, Color{ 0.240f, 0.210f, 0.160f, 1.0f });

            AddAnomaly(triangleVertices, lineVertices, Float3{ 10.5f, 0.0f, -2.5f });
            AddSectorMarkers(lineVertices);
        }

        void AddCharacterModel(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            const Float3& groundPosition,
            float rotationZ,
            const Color& coatColor,
            const Color& accentColor
        )
        {
            const float yaw = DegToRad(rotationZ);
            const Color bootColor{ 0.055f, 0.060f, 0.050f, 1.0f };
            const Color trousers{ 0.155f, 0.185f, 0.135f, 1.0f };
            const Color backpack{ 0.105f, 0.110f, 0.085f, 1.0f };
            const Color mask{ 0.260f, 0.255f, 0.215f, 1.0f };
            const Color weapon{ 0.090f, 0.085f, 0.075f, 1.0f };

            const auto worldPoint = [&](float x, float y, float z)
            {
                return TransformLocalPoint(
                    groundPosition,
                    Float3{ x, y, z },
                    yaw
                );
            };

            AddOrientedBox(vertices, worldPoint(-0.18f, 0.14f, 0.02f), Float3{ 0.12f, 0.14f, 0.24f }, yaw, bootColor);
            AddOrientedBox(vertices, worldPoint(0.18f, 0.14f, 0.02f), Float3{ 0.12f, 0.14f, 0.24f }, yaw, bootColor);

            AddOrientedBox(vertices, worldPoint(-0.16f, 0.52f, 0.0f), Float3{ 0.11f, 0.36f, 0.11f }, yaw, trousers);
            AddOrientedBox(vertices, worldPoint(0.16f, 0.52f, 0.0f), Float3{ 0.11f, 0.36f, 0.11f }, yaw, trousers);

            AddOrientedBox(vertices, worldPoint(0.0f, 1.02f, 0.04f), Float3{ 0.34f, 0.43f, 0.21f }, yaw, coatColor);
            AddOrientedBox(vertices, worldPoint(0.0f, 1.00f, -0.25f), Float3{ 0.28f, 0.34f, 0.10f }, yaw, backpack);

            AddOrientedBox(vertices, worldPoint(-0.43f, 1.02f, 0.02f), Float3{ 0.075f, 0.34f, 0.09f }, yaw, ScaleColor(coatColor, 0.82f));
            AddOrientedBox(vertices, worldPoint(0.43f, 1.02f, 0.02f), Float3{ 0.075f, 0.34f, 0.09f }, yaw, ScaleColor(coatColor, 0.82f));

            AddOrientedBox(vertices, worldPoint(0.0f, 1.52f, 0.04f), Float3{ 0.18f, 0.18f, 0.17f }, yaw, mask);
            AddOrientedBox(vertices, worldPoint(0.0f, 1.72f, 0.02f), Float3{ 0.20f, 0.055f, 0.18f }, yaw, accentColor);

            AddOrientedBox(vertices, worldPoint(0.25f, 1.10f, 0.54f), Float3{ 0.045f, 0.050f, 0.45f }, yaw, weapon);
            AddOrientedBox(vertices, worldPoint(0.25f, 1.10f, 0.12f), Float3{ 0.070f, 0.090f, 0.13f }, yaw, accentColor);
        }

        Color ItemColorFromId(std::int32_t worldObjectId)
        {
            const int bucket = std::abs(worldObjectId) % 3;

            if (bucket == 0)
                return Color{ 0.780f, 0.560f, 0.165f, 1.0f };

            if (bucket == 1)
                return Color{ 0.385f, 0.560f, 0.640f, 1.0f };

            return Color{ 0.620f, 0.375f, 0.245f, 1.0f };
        }

        void AddWorldItemModel(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            const WorldRenderItem& item
        )
        {
            const float itemSize = std::clamp(item.Size, 0.15f, 2.0f);

            const Float3 itemPosition = ToRenderPosition(
                item.PositionX,
                item.PositionY,
                item.PositionZ
            );

            const Color itemColor = ItemColorFromId(item.WorldObjectId);

            AddCube(
                vertices,
                Float3{ itemPosition.X, itemPosition.Y + itemSize * 0.42f, itemPosition.Z },
                Float3{ itemSize * 1.20f, itemSize * 0.42f, itemSize * 0.78f },
                itemColor
            );

            AddCube(
                vertices,
                Float3{ itemPosition.X, itemPosition.Y + itemSize * 0.92f, itemPosition.Z },
                Float3{ itemSize * 0.92f, itemSize * 0.10f, itemSize * 0.58f },
                ScaleColor(itemColor, 1.18f)
            );
        }

		void AddScreenGradientQuad(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            float topY,
            float bottomY,
            const Color& topColor,
            const Color& bottomColor
        )
        {
            const float z = 0.999f;

            const Float3 leftTop{ -1.0f, topY, z };
            const Float3 rightTop{ 1.0f, topY, z };
            const Float3 rightBottom{ 1.0f, bottomY, z };
            const Float3 leftBottom{ -1.0f, bottomY, z };

            AddVertex(vertices, leftTop, topColor);
            AddVertex(vertices, rightTop, topColor);
            AddVertex(vertices, rightBottom, bottomColor);

            AddVertex(vertices, leftTop, topColor);
            AddVertex(vertices, rightBottom, bottomColor);
            AddVertex(vertices, leftBottom, bottomColor);
        }

        void AddScreenDisc(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            float centerX,
            float centerY,
            float radiusY,
            const Color& color,
            std::uint32_t viewportWidth,
            std::uint32_t viewportHeight
        )
        {
            if (viewportWidth == 0 || viewportHeight == 0)
                return;

            constexpr int segmentCount = 48;
            constexpr float discZ = 0.998f;

            const float aspectFix =
                static_cast<float>(viewportHeight) / static_cast<float>(viewportWidth);

            const float radiusX = radiusY * aspectFix;

            const Float3 center{ centerX, centerY, discZ };

            for (int i = 0; i < segmentCount; ++i)
            {
                const float angleA = static_cast<float>(i) / static_cast<float>(segmentCount) * Pi * 2.0f;
                const float angleB = static_cast<float>(i + 1) / static_cast<float>(segmentCount) * Pi * 2.0f;

                const Float3 pointA
                {
                    centerX + std::cos(angleA) * radiusX,
                    centerY + std::sin(angleA) * radiusY,
                    discZ
                };

                const Float3 pointB
                {
                    centerX + std::cos(angleB) * radiusX,
                    centerY + std::sin(angleB) * radiusY,
                    discZ
                };

                AddTriangle(vertices, center, pointA, pointB, color);
            }
        }

        void AddSkyAndSunScreenSpace(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            std::uint32_t viewportWidth,
            std::uint32_t viewportHeight
        )
        {
            const Color skyTop{ 0.035f, 0.055f, 0.075f, 1.0f };
            const Color skyMid{ 0.090f, 0.125f, 0.150f, 1.0f };
            const Color skyHorizon{ 0.145f, 0.160f, 0.145f, 1.0f };
            const Color groundFog{ 0.055f, 0.065f, 0.050f, 1.0f };

            AddScreenGradientQuad(vertices, 1.0f, 0.35f, skyTop, skyMid);
            AddScreenGradientQuad(vertices, 0.35f, -0.15f, skyMid, skyHorizon);
            AddScreenGradientQuad(vertices, -0.15f, -1.0f, skyHorizon, groundFog);

            const float sunX = 0.56f;
            const float sunY = 0.62f;

            AddScreenDisc(
                vertices,
                sunX,
                sunY,
                0.19f,
                Color{ 0.22f, 0.18f, 0.09f, 1.0f },
                viewportWidth,
                viewportHeight
            );

            AddScreenDisc(
                vertices,
                sunX,
                sunY,
                0.115f,
                Color{ 0.72f, 0.47f, 0.15f, 1.0f },
                viewportWidth,
                viewportHeight
            );

            AddScreenDisc(
                vertices,
                sunX,
                sunY,
                0.055f,
                Color{ 1.0f, 0.86f, 0.38f, 1.0f },
                viewportWidth,
                viewportHeight
            );
        }

        void AddPlayerForwardLine(
            std::vector<Dx11WorldRenderer::Vertex>& vertices,
            const Float3& playerPosition,
            float rotationZ
        )
        {
            const float yaw = DegToRad(rotationZ);

            const float forwardX = static_cast<float>(std::sin(yaw));
            const float forwardZ = static_cast<float>(std::cos(yaw));

            const Float3 start
            {
                playerPosition.X,
                playerPosition.Y + 1.15f,
                playerPosition.Z
            };

            const Float3 end
            {
                playerPosition.X + forwardX * 1.6f,
                playerPosition.Y + 1.15f,
                playerPosition.Z + forwardZ * 1.6f
            };

            const Color color{ 0.95f, 0.95f, 0.35f, 1.0f };

            AddLine(vertices, start, end, color);
        }

        DirectX::XMMATRIX BuildViewProjection(
            const WorldRenderPlayer& player,
            std::uint32_t viewportWidth,
            std::uint32_t viewportHeight,
            GameCameraMode cameraMode
        )
        {
            const float aspectRatio = viewportHeight == 0
                ? 1.0f
                : static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);

            Float3 playerGroundPosition = ToRenderPosition(
                player.PositionX,
                player.PositionY,
                player.PositionZ
            );

            if (!player.Valid)
            {
                playerGroundPosition = Float3{ 0.0f, 0.0f, 0.0f };
            }

            const float yaw = DegToRad(player.RotationZ);

            const float forwardX = static_cast<float>(std::sin(yaw));
            const float forwardZ = static_cast<float>(std::cos(yaw));

            DirectX::XMVECTOR eye{};
            DirectX::XMVECTOR target{};

            if (cameraMode == GameCameraMode::FirstPerson)
            {
                eye = DirectX::XMVectorSet(
                    playerGroundPosition.X,
                    playerGroundPosition.Y + 1.65f,
                    playerGroundPosition.Z,
                    1.0f
                );

                target = DirectX::XMVectorSet(
                    playerGroundPosition.X + forwardX * 10.0f,
                    playerGroundPosition.Y + 1.55f,
                    playerGroundPosition.Z + forwardZ * 10.0f,
                    1.0f
                );
            }
            else
            {
                eye = DirectX::XMVectorSet(
                    playerGroundPosition.X - forwardX * 7.5f,
                    playerGroundPosition.Y + 4.0f,
                    playerGroundPosition.Z - forwardZ * 7.5f,
                    1.0f
                );

                target = DirectX::XMVectorSet(
                    playerGroundPosition.X + forwardX * 2.0f,
                    playerGroundPosition.Y + 1.2f,
                    playerGroundPosition.Z + forwardZ * 2.0f,
                    1.0f
                );
            }

            const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

            const DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(
                eye,
                target,
                up
            );

            const DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(
                DegToRad(65.0f),
                aspectRatio,
                0.05f,
                500.0f
            );

            return view * projection;
        }

        void DebugOutputBlob(ID3DBlob* blob)
        {
            if (!blob)
                return;

            const char* text = static_cast<const char*>(blob->GetBufferPointer());

            if (!text)
                return;

            OutputDebugStringA(text);
            OutputDebugStringA("\n");
        }

        bool CompileShader(
            const char* sourceCode,
            const char* entryPoint,
            const char* target,
            ID3DBlob** outputBlob
        )
        {
            if (!sourceCode || !entryPoint || !target || !outputBlob)
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
                errorBlob.GetAddressOf()
            );

            if (FAILED(result))
            {
                DebugOutputBlob(errorBlob.Get());
                return false;
            }

            return true;
        }
    }

    Dx11WorldRenderer::~Dx11WorldRenderer()
    {
        Shutdown();
    }

    bool Dx11WorldRenderer::Initialize(ID3D11Device* device)
    {
        if (m_initialized)
            return true;

        if (!device)
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

    void Dx11WorldRenderer::Shutdown()
	{
		m_skyDepthStencilState.Reset();
		m_rasterizerState.Reset();

		m_cameraConstantBuffer.Reset();
		m_dynamicVertexBuffer.Reset();

		m_inputLayout.Reset();
		m_pixelShader.Reset();
		m_vertexShader.Reset();

		m_initialized = false;
	}

    void Dx11WorldRenderer::Render(
        ID3D11DeviceContext* deviceContext,
        std::uint32_t viewportWidth,
        std::uint32_t viewportHeight,
        const WorldRenderPlayer& player,
        const std::vector<WorldRenderRemotePlayer>& remotePlayers,
        const std::vector<WorldRenderItem>& items,
        GameCameraMode cameraMode
    )
    {
        if (!m_initialized)
            return;

        if (!deviceContext)
            return;

        if (viewportWidth == 0 || viewportHeight == 0)
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

		deviceContext->VSSetShader(m_vertexShader.Get(), nullptr, 0);
		deviceContext->PSSetShader(m_pixelShader.Get(), nullptr, 0);
		deviceContext->IASetInputLayout(m_inputLayout.Get());

		ID3D11Buffer* cameraBuffers[] =
		{
			m_cameraConstantBuffer.Get()
		};

		deviceContext->VSSetConstantBuffers(0, 1, cameraBuffers);

		CameraConstantBuffer skyCameraData{};
		skyCameraData.ViewProjection = DirectX::XMMatrixIdentity();

		deviceContext->UpdateSubresource(
			m_cameraConstantBuffer.Get(),
			0,
			nullptr,
			&skyCameraData,
			0,
			0
		);

		deviceContext->OMSetDepthStencilState(m_skyDepthStencilState.Get(), 0);

		std::vector<Vertex> skyVertices;
		skyVertices.reserve(256);

		AddSkyAndSunScreenSpace(
			skyVertices,
			viewportWidth,
			viewportHeight
		);

		UploadAndDraw(
			deviceContext,
			skyVertices,
			D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
		);

		deviceContext->OMSetDepthStencilState(nullptr, 0);

		const DirectX::XMMATRIX viewProjection = BuildViewProjection(
            player,
            viewportWidth,
            viewportHeight,
            cameraMode
        );

        CameraConstantBuffer cameraData{};
        cameraData.ViewProjection = DirectX::XMMatrixTranspose(viewProjection);

        deviceContext->UpdateSubresource(
            m_cameraConstantBuffer.Get(),
            0,
            nullptr,
            &cameraData,
            0,
            0
        );

        Float3 playerPosition = ToRenderPosition(
            player.PositionX,
            player.PositionY,
            player.PositionZ
        );

        if (!player.Valid)
            playerPosition = Float3{ 0.0f, 0.0f, 0.0f };

        std::vector<Vertex> triangleVertices;
        triangleVertices.reserve(32768);

        std::vector<Vertex> lineVertices;
        lineVertices.reserve(2048);

        AddWorldScene(triangleVertices, lineVertices);

        AddPlayerForwardLine(lineVertices, playerPosition, player.RotationZ);

        const Color localPlayerColor{ 0.210f, 0.430f, 0.255f, 1.0f };
        const Color localAccentColor{ 0.625f, 0.560f, 0.250f, 1.0f };
        const Color remotePlayerColor{ 0.315f, 0.390f, 0.440f, 1.0f };
        const Color remoteAccentColor{ 0.560f, 0.635f, 0.690f, 1.0f };

        if (cameraMode != GameCameraMode::FirstPerson)
        {
            AddCharacterModel(
                triangleVertices,
                playerPosition,
                player.RotationZ,
                localPlayerColor,
                localAccentColor
            );
        }

        for (const WorldRenderRemotePlayer& remotePlayer : remotePlayers)
        {
            if (!remotePlayer.IsAlive)
                continue;

            const Float3 remotePlayerPosition = ToRenderPosition(
                remotePlayer.PositionX,
                remotePlayer.PositionY,
                remotePlayer.PositionZ
            );

            AddCharacterModel(
                triangleVertices,
                remotePlayerPosition,
                remotePlayer.RotationZ,
                remotePlayerColor,
                remoteAccentColor
            );
        }

        for (const WorldRenderItem& item : items)
        {
            AddWorldItemModel(triangleVertices, item);
        }

        UploadAndDraw(
            deviceContext,
            triangleVertices,
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
        );

        UploadAndDraw(
            deviceContext,
            lineVertices,
            D3D11_PRIMITIVE_TOPOLOGY_LINELIST
        );
    }

    bool Dx11WorldRenderer::CreateShaders(ID3D11Device* device)
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
    output.Position = mul(ViewProjection, float4(input.Position, 1.0f));
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

        if (!CompileShader(
            shaderCode,
            "VSMain",
            "vs_5_0",
            vertexShaderBlob.GetAddressOf()
        ))
        {
            return false;
        }

        if (!CompileShader(
            shaderCode,
            "PSMain",
            "ps_5_0",
            pixelShaderBlob.GetAddressOf()
        ))
        {
            return false;
        }

        const HRESULT createVertexShaderResult = device->CreateVertexShader(
            vertexShaderBlob->GetBufferPointer(),
            vertexShaderBlob->GetBufferSize(),
            nullptr,
            m_vertexShader.GetAddressOf()
        );

        if (FAILED(createVertexShaderResult))
            return false;

        const HRESULT createPixelShaderResult = device->CreatePixelShader(
            pixelShaderBlob->GetBufferPointer(),
            pixelShaderBlob->GetBufferSize(),
            nullptr,
            m_pixelShader.GetAddressOf()
        );

        if (FAILED(createPixelShaderResult))
            return false;

        const D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[] =
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
            m_inputLayout.GetAddressOf()
        );

        return SUCCEEDED(createInputLayoutResult);
    }

    bool Dx11WorldRenderer::CreateBuffers(ID3D11Device* device)
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
            m_dynamicVertexBuffer.GetAddressOf()
        );

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
            m_cameraConstantBuffer.GetAddressOf()
        );

        return SUCCEEDED(createCameraBufferResult);
    }

    bool Dx11WorldRenderer::CreateStates(ID3D11Device* device)
	{
		if (!device)
			return false;

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
			m_rasterizerState.GetAddressOf()
		);

		if (FAILED(rasterizerResult))
			return false;

		D3D11_DEPTH_STENCIL_DESC skyDepthDesc{};
		skyDepthDesc.DepthEnable = FALSE;
		skyDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		skyDepthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		skyDepthDesc.StencilEnable = FALSE;

		const HRESULT skyDepthResult = device->CreateDepthStencilState(
			&skyDepthDesc,
			m_skyDepthStencilState.GetAddressOf()
		);

		return SUCCEEDED(skyDepthResult);
	}

    bool Dx11WorldRenderer::UploadAndDraw(
        ID3D11DeviceContext* deviceContext,
        const std::vector<Vertex>& vertices,
        D3D11_PRIMITIVE_TOPOLOGY topology
    )
    {
        if (vertices.empty())
            return true;

        if (vertices.size() > MaxDynamicVertices)
            return false;

        D3D11_MAPPED_SUBRESOURCE mappedResource{};

        const HRESULT mapResult = deviceContext->Map(
            m_dynamicVertexBuffer.Get(),
            0,
            D3D11_MAP_WRITE_DISCARD,
            0,
            &mappedResource
        );

        if (FAILED(mapResult))
            return false;

        std::memcpy(
            mappedResource.pData,
            vertices.data(),
            vertices.size() * sizeof(Vertex)
        );

        deviceContext->Unmap(m_dynamicVertexBuffer.Get(), 0);

        constexpr UINT stride = sizeof(Vertex);
        constexpr UINT offset = 0;

        ID3D11Buffer* vertexBuffers[] =
        {
            m_dynamicVertexBuffer.Get()
        };

        deviceContext->IASetVertexBuffers(
            0,
            1,
            vertexBuffers,
            &stride,
            &offset
        );

        deviceContext->IASetPrimitiveTopology(topology);

        deviceContext->Draw(
            static_cast<UINT>(vertices.size()),
            0
        );

        return true;
    }
}
