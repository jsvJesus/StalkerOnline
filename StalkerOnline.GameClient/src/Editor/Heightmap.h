#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace StalkerOnline::Editor
{
    enum class RawHeightFormat : std::uint8_t
    {
        UInt8 = 0,
        UInt16LE = 1,
        Float32LE = 2,
        R16 = 3
    };

    enum class TerrainBrushMode : std::uint8_t
    {
        Raise = 0,
        Lower = 1,
        Smooth = 2,
        Flatten = 3
    };

    struct RawHeightmapOptions
    {
        int Width = 513;
        int Height = 513;
        RawHeightFormat Format = RawHeightFormat::UInt16LE;
    };

    class Heightmap final
    {
    public:
        bool CreateFlat(int width, int height, float normalizedHeight, std::string* errorMessage = nullptr);

        bool LoadRaw(
            const std::string& path,
            const RawHeightmapOptions& options,
            std::string* errorMessage = nullptr);

        bool SaveRaw(
            const std::string& path,
            RawHeightFormat format,
            std::string* errorMessage = nullptr) const;

        bool IsValid() const;

        int GetWidth() const;
        int GetHeight() const;

        float GetHeightNormalized(int x, int y) const;
        void SetHeightNormalized(int x, int y, float value);

        const std::vector<float>& GetValues() const;

        void ApplyBrush(
            float centerX,
            float centerY,
            float radiusCells,
            float strengthPerSecond,
            float deltaTimeSeconds,
            TerrainBrushMode mode,
            float flattenHeight);

    private:
        int GetIndex(int x, int y) const;
        float CalculateNeighborAverage(int x, int y, const std::vector<float>& source) const;

    private:
        int m_width = 0;
        int m_height = 0;
        std::vector<float> m_values;
    };

    const char* RawHeightFormatToText(RawHeightFormat format);
    const char* TerrainBrushModeToText(TerrainBrushMode mode);

    bool TryDetectSquareRawDimensions(
        const std::string& path,
        RawHeightFormat format,
        int* outWidth,
        int* outHeight,
        std::string* errorMessage = nullptr);
}
