#include "Editor/Heightmap.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>

namespace StalkerOnline::Editor
{
    namespace
    {
        constexpr float DefaultHeight = 0.5f;

        int BytesPerSample(RawHeightFormat format)
        {
            switch (format)
            {
                case RawHeightFormat::UInt8:
                    return 1;

                case RawHeightFormat::UInt16LE:
                    return 2;

                case RawHeightFormat::Float32LE:
                    return 4;

                case RawHeightFormat::R16:
                    return 2;

                default:
                    return 0;
            }
        }

        void SetError(std::string* errorMessage, const std::string& text)
        {
            if (errorMessage != nullptr)
                *errorMessage = text;
        }

        bool ValidateSize(int width, int height, std::string* errorMessage)
        {
            if (width < 2 || height < 2)
            {
                SetError(errorMessage, "Heightmap size must be at least 2x2.");
                return false;
            }

            if (width > 8192 || height > 8192)
            {
                SetError(errorMessage, "Heightmap size is too large. Max supported size is 8192x8192.");
                return false;
            }

            return true;
        }

        float ClampHeight(float value)
        {
            if (std::isnan(value) || std::isinf(value))
                return DefaultHeight;

            return std::clamp(value, 0.0f, 1.0f);
        }
    }

    bool Heightmap::CreateFlat(int width, int height, float normalizedHeight, std::string* errorMessage)
    {
        if (!ValidateSize(width, height, errorMessage))
            return false;

        m_width = width;
        m_height = height;
        m_values.assign(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height), ClampHeight(normalizedHeight));

        return true;
    }

    bool Heightmap::LoadRaw(
        const std::string& path,
        const RawHeightmapOptions& options,
        std::string* errorMessage)
    {
        if (path.empty())
        {
            SetError(errorMessage, "RAW path is empty.");
            return false;
        }

        if (!ValidateSize(options.Width, options.Height, errorMessage))
            return false;

        const int bytesPerSample = BytesPerSample(options.Format);

        if (bytesPerSample <= 0)
        {
            SetError(errorMessage, "Unsupported RAW format.");
            return false;
        }

        const std::uint64_t sampleCount =
            static_cast<std::uint64_t>(options.Width) * static_cast<std::uint64_t>(options.Height);

        const std::uint64_t expectedByteCount = sampleCount * static_cast<std::uint64_t>(bytesPerSample);

        if (expectedByteCount > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()))
        {
            SetError(errorMessage, "RAW file is too large to load.");
            return false;
        }

        std::ifstream file(path, std::ios::binary | std::ios::ate);

        if (!file.is_open())
        {
            SetError(errorMessage, "Failed to open RAW file.");
            return false;
        }

        const std::streamoff actualByteCount = file.tellg();

        if (actualByteCount < 0 || static_cast<std::uint64_t>(actualByteCount) != expectedByteCount)
        {
            SetError(errorMessage, "RAW file size does not match width, height and format.");
            return false;
        }

        file.seekg(0, std::ios::beg);

        std::vector<unsigned char> bytes;
        bytes.resize(static_cast<std::size_t>(expectedByteCount));

        if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        {
            SetError(errorMessage, "Failed to read RAW file data.");
            return false;
        }

        m_width = options.Width;
        m_height = options.Height;
        m_values.resize(static_cast<std::size_t>(sampleCount));

        for (std::uint64_t i = 0; i < sampleCount; ++i)
        {
            const std::size_t byteOffset = static_cast<std::size_t>(i) * static_cast<std::size_t>(bytesPerSample);

            if (options.Format == RawHeightFormat::UInt8)
            {
                m_values[static_cast<std::size_t>(i)] =
                    static_cast<float>(bytes[byteOffset]) / 255.0f;
            }
            else if (options.Format == RawHeightFormat::UInt16LE || options.Format == RawHeightFormat::R16)
            {
                const std::uint16_t value =
                    static_cast<std::uint16_t>(bytes[byteOffset]) |
                    static_cast<std::uint16_t>(bytes[byteOffset + 1] << 8);

                m_values[static_cast<std::size_t>(i)] =
                    static_cast<float>(value) / 65535.0f;
            }
            else if (options.Format == RawHeightFormat::Float32LE)
            {
                std::uint32_t bits =
                    static_cast<std::uint32_t>(bytes[byteOffset]) |
                    (static_cast<std::uint32_t>(bytes[byteOffset + 1]) << 8) |
                    (static_cast<std::uint32_t>(bytes[byteOffset + 2]) << 16) |
                    (static_cast<std::uint32_t>(bytes[byteOffset + 3]) << 24);

                float value = 0.0f;
                std::memcpy(&value, &bits, sizeof(float));
                m_values[static_cast<std::size_t>(i)] = ClampHeight(value);
            }
        }

        return true;
    }

    bool Heightmap::SaveRaw(
        const std::string& path,
        RawHeightFormat format,
        std::string* errorMessage) const
    {
        if (path.empty())
        {
            SetError(errorMessage, "RAW save path is empty.");
            return false;
        }

        if (!IsValid())
        {
            SetError(errorMessage, "No heightmap is loaded.");
            return false;
        }

        std::ofstream file(path, std::ios::binary | std::ios::trunc);

        if (!file.is_open())
        {
            SetError(errorMessage, "Failed to open RAW file for writing.");
            return false;
        }

        for (float normalizedValue : m_values)
        {
            const float value = ClampHeight(normalizedValue);

            if (format == RawHeightFormat::UInt8)
            {
                const auto output = static_cast<unsigned char>(std::round(value * 255.0f));
                file.write(reinterpret_cast<const char*>(&output), sizeof(output));
            }
            else if (format == RawHeightFormat::UInt16LE || format == RawHeightFormat::R16)
            {
                const auto output = static_cast<std::uint16_t>(std::round(value * 65535.0f));
                const unsigned char bytes[2]
                {
                    static_cast<unsigned char>(output & 0xFF),
                    static_cast<unsigned char>((output >> 8) & 0xFF)
                };

                file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
            }
            else if (format == RawHeightFormat::Float32LE)
            {
                std::uint32_t bits = 0;
                std::memcpy(&bits, &value, sizeof(float));

                const unsigned char bytes[4]
                {
                    static_cast<unsigned char>(bits & 0xFF),
                    static_cast<unsigned char>((bits >> 8) & 0xFF),
                    static_cast<unsigned char>((bits >> 16) & 0xFF),
                    static_cast<unsigned char>((bits >> 24) & 0xFF)
                };

                file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
            }
            else
            {
                SetError(errorMessage, "Unsupported RAW save format.");
                return false;
            }
        }

        if (!file.good())
        {
            SetError(errorMessage, "Failed while writing RAW file.");
            return false;
        }

        return true;
    }

    bool Heightmap::IsValid() const
    {
        return m_width > 1 &&
            m_height > 1 &&
            m_values.size() == static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
    }

    int Heightmap::GetWidth() const
    {
        return m_width;
    }

    int Heightmap::GetHeight() const
    {
        return m_height;
    }

    float Heightmap::GetHeightNormalized(int x, int y) const
    {
        if (!IsValid())
            return 0.0f;

        x = std::clamp(x, 0, m_width - 1);
        y = std::clamp(y, 0, m_height - 1);

        return m_values[static_cast<std::size_t>(GetIndex(x, y))];
    }

    void Heightmap::SetHeightNormalized(int x, int y, float value)
    {
        if (!IsValid())
            return;

        if (x < 0 || y < 0 || x >= m_width || y >= m_height)
            return;

        m_values[static_cast<std::size_t>(GetIndex(x, y))] = ClampHeight(value);
    }

    const std::vector<float>& Heightmap::GetValues() const
    {
        return m_values;
    }

    void Heightmap::ApplyBrush(
        float centerX,
        float centerY,
        float radiusCells,
        float strengthPerSecond,
        float deltaTimeSeconds,
        TerrainBrushMode mode,
        float flattenHeight)
    {
        if (!IsValid())
            return;

        if (radiusCells <= 0.01f || strengthPerSecond <= 0.0f || deltaTimeSeconds <= 0.0f)
            return;

        const int minX = std::max(0, static_cast<int>(std::floor(centerX - radiusCells)));
        const int maxX = std::min(m_width - 1, static_cast<int>(std::ceil(centerX + radiusCells)));
        const int minY = std::max(0, static_cast<int>(std::floor(centerY - radiusCells)));
        const int maxY = std::min(m_height - 1, static_cast<int>(std::ceil(centerY + radiusCells)));

        const float radiusSquared = radiusCells * radiusCells;
        const float amount = strengthPerSecond * deltaTimeSeconds;
        const float targetFlattenHeight = ClampHeight(flattenHeight);

        std::vector<float> originalValues;

        if (mode == TerrainBrushMode::Smooth)
            originalValues = m_values;

        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                const float dx = static_cast<float>(x) - centerX;
                const float dy = static_cast<float>(y) - centerY;
                const float distanceSquared = dx * dx + dy * dy;

                if (distanceSquared > radiusSquared)
                    continue;

                const float distance = std::sqrt(distanceSquared);
                const float falloff = std::clamp(1.0f - distance / radiusCells, 0.0f, 1.0f);
                const float weightedAmount = amount * falloff;

                const int index = GetIndex(x, y);
                const float oldHeight = m_values[static_cast<std::size_t>(index)];

                float newHeight = oldHeight;

                if (mode == TerrainBrushMode::Raise)
                {
                    newHeight = oldHeight + weightedAmount;
                }
                else if (mode == TerrainBrushMode::Lower)
                {
                    newHeight = oldHeight - weightedAmount;
                }
                else if (mode == TerrainBrushMode::Smooth)
                {
                    const float average = CalculateNeighborAverage(x, y, originalValues);
                    newHeight = oldHeight + (average - oldHeight) * std::clamp(weightedAmount * 4.0f, 0.0f, 1.0f);
                }
                else if (mode == TerrainBrushMode::Flatten)
                {
                    newHeight = oldHeight + (targetFlattenHeight - oldHeight) * std::clamp(weightedAmount * 2.0f, 0.0f, 1.0f);
                }

                m_values[static_cast<std::size_t>(index)] = ClampHeight(newHeight);
            }
        }
    }

    int Heightmap::GetIndex(int x, int y) const
    {
        return y * m_width + x;
    }

    float Heightmap::CalculateNeighborAverage(int x, int y, const std::vector<float>& source) const
    {
        float total = 0.0f;
        int count = 0;

        for (int offsetY = -1; offsetY <= 1; ++offsetY)
        {
            for (int offsetX = -1; offsetX <= 1; ++offsetX)
            {
                const int sampleX = x + offsetX;
                const int sampleY = y + offsetY;

                if (sampleX < 0 || sampleY < 0 || sampleX >= m_width || sampleY >= m_height)
                    continue;

                total += source[static_cast<std::size_t>(GetIndex(sampleX, sampleY))];
                count++;
            }
        }

        if (count <= 0)
            return GetHeightNormalized(x, y);

        return total / static_cast<float>(count);
    }

    const char* RawHeightFormatToText(RawHeightFormat format)
    {
        switch (format)
        {
            case RawHeightFormat::UInt8:
                return "RAW 8-bit unsigned";

            case RawHeightFormat::UInt16LE:
                return "RAW 16-bit unsigned LE";

            case RawHeightFormat::Float32LE:
                return "RAW 32-bit float LE";

            case RawHeightFormat::R16:
                return "UE5 R16 / RAW16 unsigned LE";

            default:
                return "Unknown";
        }
    }

    const char* TerrainBrushModeToText(TerrainBrushMode mode)
    {
        switch (mode)
        {
            case TerrainBrushMode::Raise:
                return "Raise";

            case TerrainBrushMode::Lower:
                return "Lower";

            case TerrainBrushMode::Smooth:
                return "Smooth";

            case TerrainBrushMode::Flatten:
                return "Flatten";

            default:
                return "Unknown";
        }
    }

    bool TryDetectSquareRawDimensions(
        const std::string& path,
        RawHeightFormat format,
        int* outWidth,
        int* outHeight,
        std::string* errorMessage)
    {
        if (outWidth == nullptr || outHeight == nullptr)
        {
            SetError(errorMessage, "Dimension output pointer is null.");
            return false;
        }

        if (path.empty())
        {
            SetError(errorMessage, "RAW path is empty.");
            return false;
        }

        const int bytesPerSample = BytesPerSample(format);

        if (bytesPerSample <= 0)
        {
            SetError(errorMessage, "Unsupported RAW format.");
            return false;
        }

        std::ifstream file(path, std::ios::binary | std::ios::ate);

        if (!file.is_open())
        {
            SetError(errorMessage, "Failed to open RAW file for dimension detection.");
            return false;
        }

        const std::streamoff byteCount = file.tellg();

        if (byteCount <= 0)
        {
            SetError(errorMessage, "RAW file is empty.");
            return false;
        }

        const std::uint64_t totalBytes = static_cast<std::uint64_t>(byteCount);

        if (totalBytes % static_cast<std::uint64_t>(bytesPerSample) != 0)
        {
            SetError(errorMessage, "RAW file size is not divisible by format sample size.");
            return false;
        }

        const std::uint64_t sampleCount = totalBytes / static_cast<std::uint64_t>(bytesPerSample);
        const auto side = static_cast<std::uint64_t>(std::llround(std::sqrt(static_cast<long double>(sampleCount))));

        if (side < 2 || side * side != sampleCount)
        {
            SetError(errorMessage, "RAW file is not a square heightmap for the selected format.");
            return false;
        }

        if (side > 8192)
        {
            SetError(errorMessage, "Detected RAW size is too large. Max supported size is 8192x8192.");
            return false;
        }

        *outWidth = static_cast<int>(side);
        *outHeight = static_cast<int>(side);

        return true;
    }
}
