#include "Editor/EditorScene.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace StalkerOnline::Editor
{
    namespace
    {
        std::string EscapeJsonString(const std::string& value)
        {
            std::string result;
            result.reserve(value.size());

            for (char c : value)
            {
                switch (c)
                {
                    case '\\':
                        result += "\\\\";
                        break;

                    case '"':
                        result += "\\\"";
                        break;

                    case '\n':
                        result += "\\n";
                        break;

                    case '\r':
                        result += "\\r";
                        break;

                    case '\t':
                        result += "\\t";
                        break;

                    default:
                        result += c;
                        break;
                }
            }

            return result;
        }

        std::string UnescapeJsonString(const std::string& value)
        {
            std::string result;
            result.reserve(value.size());

            bool escaped = false;

            for (char c : value)
            {
                if (escaped)
                {
                    switch (c)
                    {
                        case 'n':
                            result += '\n';
                            break;

                        case 'r':
                            result += '\r';
                            break;

                        case 't':
                            result += '\t';
                            break;

                        case '\\':
                            result += '\\';
                            break;

                        case '"':
                            result += '"';
                            break;

                        default:
                            result += c;
                            break;
                    }

                    escaped = false;
                    continue;
                }

                if (c == '\\')
                {
                    escaped = true;
                    continue;
                }

                result += c;
            }

            return result;
        }

        std::string ReadTextFile(const std::string& path)
        {
            std::ifstream file(path, std::ios::binary);

            if (!file)
                return {};

            std::ostringstream stream;
            stream << file.rdbuf();

            return stream.str();
        }

        bool WriteTextFile(const std::string& path, const std::string& content)
        {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);

            if (!file)
                return false;

            file << content;
            return file.good();
        }

        void SkipSpaces(const std::string& text, std::size_t* position)
        {
            if (position == nullptr)
                return;

            while (*position < text.size())
            {
                const unsigned char c = static_cast<unsigned char>(text[*position]);

                if (!std::isspace(c))
                    break;

                ++(*position);
            }
        }

        std::size_t FindKeyColon(const std::string& text, const std::string& key)
        {
            const std::string token = "\"" + key + "\"";
            const std::size_t keyPosition = text.find(token);

            if (keyPosition == std::string::npos)
                return std::string::npos;

            return text.find(':', keyPosition + token.size());
        }

        std::size_t FindMatchingClose(
            const std::string& text,
            std::size_t openPosition,
            char openChar,
            char closeChar)
        {
            if (openPosition >= text.size() || text[openPosition] != openChar)
                return std::string::npos;

            int depth = 0;
            bool insideString = false;
            bool escaped = false;

            for (std::size_t i = openPosition; i < text.size(); ++i)
            {
                const char c = text[i];

                if (insideString)
                {
                    if (escaped)
                    {
                        escaped = false;
                        continue;
                    }

                    if (c == '\\')
                    {
                        escaped = true;
                        continue;
                    }

                    if (c == '"')
                        insideString = false;

                    continue;
                }

                if (c == '"')
                {
                    insideString = true;
                    continue;
                }

                if (c == openChar)
                    ++depth;
                else if (c == closeChar)
                {
                    --depth;

                    if (depth == 0)
                        return i;
                }
            }

            return std::string::npos;
        }

        bool ExtractObjectBlock(
            const std::string& text,
            const std::string& key,
            std::string* outBlock)
        {
            if (outBlock == nullptr)
                return false;

            const std::size_t colonPosition = FindKeyColon(text, key);

            if (colonPosition == std::string::npos)
                return false;

            std::size_t objectStart = text.find('{', colonPosition + 1);

            if (objectStart == std::string::npos)
                return false;

            const std::size_t objectEnd = FindMatchingClose(text, objectStart, '{', '}');

            if (objectEnd == std::string::npos || objectEnd <= objectStart)
                return false;

            *outBlock = text.substr(objectStart, objectEnd - objectStart + 1);
            return true;
        }

        bool ExtractArrayBlock(
            const std::string& text,
            const std::string& key,
            std::string* outBlock)
        {
            if (outBlock == nullptr)
                return false;

            const std::size_t colonPosition = FindKeyColon(text, key);

            if (colonPosition == std::string::npos)
                return false;

            std::size_t arrayStart = text.find('[', colonPosition + 1);

            if (arrayStart == std::string::npos)
                return false;

            const std::size_t arrayEnd = FindMatchingClose(text, arrayStart, '[', ']');

            if (arrayEnd == std::string::npos || arrayEnd <= arrayStart)
                return false;

            *outBlock = text.substr(arrayStart, arrayEnd - arrayStart + 1);
            return true;
        }

        bool ReadStringValue(
            const std::string& objectText,
            const std::string& key,
            std::string* outValue)
        {
            if (outValue == nullptr)
                return false;

            const std::size_t colonPosition = FindKeyColon(objectText, key);

            if (colonPosition == std::string::npos)
                return false;

            std::size_t position = colonPosition + 1;
            SkipSpaces(objectText, &position);

            if (position >= objectText.size() || objectText[position] != '"')
                return false;

            ++position;

            std::string value;
            bool escaped = false;

            while (position < objectText.size())
            {
                const char c = objectText[position++];

                if (escaped)
                {
                    value += '\\';
                    value += c;
                    escaped = false;
                    continue;
                }

                if (c == '\\')
                {
                    escaped = true;
                    continue;
                }

                if (c == '"')
                {
                    *outValue = UnescapeJsonString(value);
                    return true;
                }

                value += c;
            }

            return false;
        }

        bool ReadFloatValue(
            const std::string& objectText,
            const std::string& key,
            float* outValue)
        {
            if (outValue == nullptr)
                return false;

            const std::size_t colonPosition = FindKeyColon(objectText, key);

            if (colonPosition == std::string::npos)
                return false;

            std::size_t position = colonPosition + 1;
            SkipSpaces(objectText, &position);

            if (position >= objectText.size())
                return false;

            const std::string numberText = objectText.substr(position);
            char* endPointer = nullptr;
            const float value = std::strtof(numberText.c_str(), &endPointer);

            if (endPointer == numberText.c_str())
                return false;

            *outValue = value;
            return true;
        }

        bool ReadIntValue(
            const std::string& objectText,
            const std::string& key,
            int* outValue)
        {
            if (outValue == nullptr)
                return false;

            float value = 0.0f;

            if (!ReadFloatValue(objectText, key, &value))
                return false;

            *outValue = static_cast<int>(value);
            return true;
        }

        bool ReadUIntValue(
            const std::string& objectText,
            const std::string& key,
            std::uint32_t* outValue)
        {
            if (outValue == nullptr)
                return false;

            int value = 0;

            if (!ReadIntValue(objectText, key, &value))
                return false;

            if (value < 0)
                value = 0;

            *outValue = static_cast<std::uint32_t>(value);
            return true;
        }

        bool ReadBoolValue(
            const std::string& objectText,
            const std::string& key,
            bool* outValue)
        {
            if (outValue == nullptr)
                return false;

            const std::size_t colonPosition = FindKeyColon(objectText, key);

            if (colonPosition == std::string::npos)
                return false;

            std::size_t position = colonPosition + 1;
            SkipSpaces(objectText, &position);

            if (objectText.compare(position, 4, "true") == 0)
            {
                *outValue = true;
                return true;
            }

            if (objectText.compare(position, 5, "false") == 0)
            {
                *outValue = false;
                return true;
            }

            return false;
        }

        bool ReadVec3Value(
            const std::string& objectText,
            const std::string& key,
            EditorVec3* outValue)
        {
            if (outValue == nullptr)
                return false;

            const std::size_t colonPosition = FindKeyColon(objectText, key);

            if (colonPosition == std::string::npos)
                return false;

            std::size_t arrayStart = objectText.find('[', colonPosition + 1);

            if (arrayStart == std::string::npos)
                return false;

            const std::size_t arrayEnd = FindMatchingClose(objectText, arrayStart, '[', ']');

            if (arrayEnd == std::string::npos || arrayEnd <= arrayStart)
                return false;

            const std::string arrayText = objectText.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
            const char* cursor = arrayText.c_str();

            char* endPointer = nullptr;

            EditorVec3 value{};

            value.X = std::strtof(cursor, &endPointer);

            if (endPointer == cursor)
                return false;

            cursor = endPointer;

            while (*cursor == ',' || std::isspace(static_cast<unsigned char>(*cursor)))
                ++cursor;

            value.Y = std::strtof(cursor, &endPointer);

            if (endPointer == cursor)
                return false;

            cursor = endPointer;

            while (*cursor == ',' || std::isspace(static_cast<unsigned char>(*cursor)))
                ++cursor;

            value.Z = std::strtof(cursor, &endPointer);

            if (endPointer == cursor)
                return false;

            *outValue = value;
            return true;
        }

        void WriteVec3(std::ostringstream& stream, const EditorVec3& value)
        {
            stream
                << "["
                << value.X
                << ", "
                << value.Y
                << ", "
                << value.Z
                << "]";
        }
    }

    const char* EditorObjectTypeToText(EditorObjectType type)
    {
        switch (type)
        {
            case EditorObjectType::StaticMeshProxy:
                return "Static Mesh Proxy";

            case EditorObjectType::SpeedTreeProxy:
                return "SpeedTree Proxy";

            case EditorObjectType::PlayerSpawn:
                return "Player Spawn";

            case EditorObjectType::LootSpawner:
                return "Loot Spawner";

            case EditorObjectType::SafeZone:
                return "Safe Zone";

            case EditorObjectType::RadiationZone:
                return "Radiation Zone";

            case EditorObjectType::AnomalyZone:
                return "Anomaly Zone";

            case EditorObjectType::Light:
                return "Light";

            default:
                return "Unknown";
        }
    }

    const char* EditorObjectTypeToSerializedText(EditorObjectType type)
    {
        switch (type)
        {
            case EditorObjectType::StaticMeshProxy:
                return "static_mesh_proxy";

            case EditorObjectType::SpeedTreeProxy:
                return "speedtree_proxy";

            case EditorObjectType::PlayerSpawn:
                return "player_spawn";

            case EditorObjectType::LootSpawner:
                return "loot_spawner";

            case EditorObjectType::SafeZone:
                return "safe_zone";

            case EditorObjectType::RadiationZone:
                return "radiation_zone";

            case EditorObjectType::AnomalyZone:
                return "anomaly_zone";

            case EditorObjectType::Light:
                return "light";

            default:
                return "static_mesh_proxy";
        }
    }

    EditorObjectType EditorObjectTypeFromSerializedText(const std::string& text)
    {
        if (text == "speedtree_proxy")
            return EditorObjectType::SpeedTreeProxy;

        if (text == "player_spawn")
            return EditorObjectType::PlayerSpawn;

        if (text == "loot_spawner")
            return EditorObjectType::LootSpawner;

        if (text == "safe_zone")
            return EditorObjectType::SafeZone;

        if (text == "radiation_zone")
            return EditorObjectType::RadiationZone;

        if (text == "anomaly_zone")
            return EditorObjectType::AnomalyZone;

        if (text == "light")
            return EditorObjectType::Light;

        return EditorObjectType::StaticMeshProxy;
    }

    void EditorScene::Clear()
    {
        m_objects.clear();
        m_nextObjectId = 1;
        m_selectedObjectId = 0;
    }

    std::uint32_t EditorScene::AddObject(const EditorObject& object)
    {
        EditorObject newObject = object;

        if (newObject.Id == 0)
            newObject.Id = AllocateObjectId();
        else
            m_nextObjectId = (std::max)(m_nextObjectId, newObject.Id + 1);

        if (newObject.Name.empty())
            newObject.Name = std::string(EditorObjectTypeToText(newObject.Type)) + " " + std::to_string(newObject.Id);

        m_objects.push_back(newObject);
        m_selectedObjectId = newObject.Id;

        return newObject.Id;
    }

    bool EditorScene::RemoveObject(std::uint32_t objectId)
    {
        const auto oldSize = m_objects.size();

        m_objects.erase(
            std::remove_if(
                m_objects.begin(),
                m_objects.end(),
                [objectId](const EditorObject& object)
                {
                    return object.Id == objectId;
                }),
            m_objects.end());

        if (m_selectedObjectId == objectId)
            m_selectedObjectId = 0;

        return m_objects.size() != oldSize;
    }

    bool EditorScene::RemoveSelectedObject()
    {
        if (m_selectedObjectId == 0)
            return false;

        return RemoveObject(m_selectedObjectId);
    }

    void EditorScene::SelectObject(std::uint32_t objectId)
    {
        if (GetObject(objectId) == nullptr)
        {
            m_selectedObjectId = 0;
            return;
        }

        m_selectedObjectId = objectId;
    }

    void EditorScene::ClearSelection()
    {
        m_selectedObjectId = 0;
    }

    EditorObject* EditorScene::GetObjectMutable(std::uint32_t objectId)
    {
        for (EditorObject& object : m_objects)
        {
            if (object.Id == objectId)
                return &object;
        }

        return nullptr;
    }

    const EditorObject* EditorScene::GetObject(std::uint32_t objectId) const
    {
        for (const EditorObject& object : m_objects)
        {
            if (object.Id == objectId)
                return &object;
        }

        return nullptr;
    }

    EditorObject* EditorScene::GetSelectedObjectMutable()
    {
        return GetObjectMutable(m_selectedObjectId);
    }

    const EditorObject* EditorScene::GetSelectedObject() const
    {
        return GetObject(m_selectedObjectId);
    }

    const std::vector<EditorObject>& EditorScene::GetObjects() const
    {
        return m_objects;
    }

    std::uint32_t EditorScene::GetSelectedObjectId() const
    {
        return m_selectedObjectId;
    }

    std::uint32_t EditorScene::GetNextObjectId() const
    {
        return m_nextObjectId;
    }

    void EditorScene::SetTerrainInfo(
        const std::string& heightmapPath,
        int width,
        int height,
        float cellSizeX,
        float cellSizeY,
        float heightScale)
    {
        m_terrainHeightmapPath = heightmapPath;
        m_terrainWidth = width;
        m_terrainHeight = height;
        m_terrainCellSizeX = cellSizeX;
        m_terrainCellSizeY = cellSizeY;
        m_terrainHeightScale = heightScale;
    }

    const std::string& EditorScene::GetTerrainHeightmapPath() const
    {
        return m_terrainHeightmapPath;
    }

    int EditorScene::GetTerrainWidth() const
    {
        return m_terrainWidth;
    }

    int EditorScene::GetTerrainHeight() const
    {
        return m_terrainHeight;
    }

    float EditorScene::GetTerrainCellSizeX() const
    {
        return m_terrainCellSizeX;
    }

    float EditorScene::GetTerrainCellSizeY() const
    {
        return m_terrainCellSizeY;
    }

    float EditorScene::GetTerrainHeightScale() const
    {
        return m_terrainHeightScale;
    }

    bool EditorScene::SaveToFile(const std::string& path, std::string* outErrorMessage) const
    {
        if (path.empty())
        {
            if (outErrorMessage != nullptr)
                *outErrorMessage = "Level path is empty.";

            return false;
        }

        std::ostringstream stream;
        stream << std::fixed << std::setprecision(6);

        stream << "{\n";
        stream << "  \"version\": 1,\n";
        stream << "  \"terrain\": {\n";
        stream << "    \"heightmap\": \"" << EscapeJsonString(m_terrainHeightmapPath) << "\",\n";
        stream << "    \"width\": " << m_terrainWidth << ",\n";
        stream << "    \"height\": " << m_terrainHeight << ",\n";
        stream << "    \"cellSizeX\": " << m_terrainCellSizeX << ",\n";
        stream << "    \"cellSizeY\": " << m_terrainCellSizeY << ",\n";
        stream << "    \"heightScale\": " << m_terrainHeightScale << "\n";
        stream << "  },\n";

        stream << "  \"objects\": [\n";

        for (std::size_t i = 0; i < m_objects.size(); ++i)
        {
            const EditorObject& object = m_objects[i];

            stream << "    {\n";
            stream << "      \"id\": " << object.Id << ",\n";
            stream << "      \"type\": \"" << EditorObjectTypeToSerializedText(object.Type) << "\",\n";
            stream << "      \"name\": \"" << EscapeJsonString(object.Name) << "\",\n";
            stream << "      \"asset\": \"" << EscapeJsonString(object.AssetPath) << "\",\n";

            stream << "      \"position\": ";
            WriteVec3(stream, object.Position);
            stream << ",\n";

            stream << "      \"rotation\": ";
            WriteVec3(stream, object.Rotation);
            stream << ",\n";

            stream << "      \"scale\": ";
            WriteVec3(stream, object.Scale);
            stream << ",\n";

            stream << "      \"radius\": " << object.Radius << ",\n";
            stream << "      \"visible\": " << (object.Visible ? "true" : "false") << "\n";
            stream << "    }";

            if (i + 1 < m_objects.size())
                stream << ",";

            stream << "\n";
        }

        stream << "  ]\n";
        stream << "}\n";

        if (!WriteTextFile(path, stream.str()))
        {
            if (outErrorMessage != nullptr)
                *outErrorMessage = "Failed to write level file.";

            return false;
        }

        return true;
    }

    bool EditorScene::LoadFromFile(const std::string& path, std::string* outErrorMessage)
    {
        if (path.empty())
        {
            if (outErrorMessage != nullptr)
                *outErrorMessage = "Level path is empty.";

            return false;
        }

        const std::string content = ReadTextFile(path);

        if (content.empty())
        {
            if (outErrorMessage != nullptr)
                *outErrorMessage = "Failed to read level file or file is empty.";

            return false;
        }

        std::string terrainBlock;

        if (ExtractObjectBlock(content, "terrain", &terrainBlock))
        {
            std::string heightmapPath;
            int width = 0;
            int height = 0;
            float cellSizeX = 100.0f;
            float cellSizeY = 100.0f;
            float heightScale = 3000.0f;

            ReadStringValue(terrainBlock, "heightmap", &heightmapPath);
            ReadIntValue(terrainBlock, "width", &width);
            ReadIntValue(terrainBlock, "height", &height);
            ReadFloatValue(terrainBlock, "cellSizeX", &cellSizeX);
            ReadFloatValue(terrainBlock, "cellSizeY", &cellSizeY);
            ReadFloatValue(terrainBlock, "heightScale", &heightScale);

            SetTerrainInfo(
                heightmapPath,
                width,
                height,
                cellSizeX,
                cellSizeY,
                heightScale);
        }

        std::vector<EditorObject> loadedObjects;
        std::uint32_t highestObjectId = 0;

        std::string objectsArray;

        if (ExtractArrayBlock(content, "objects", &objectsArray))
        {
            std::size_t position = 0;

            while (position < objectsArray.size())
            {
                const std::size_t objectStart = objectsArray.find('{', position);

                if (objectStart == std::string::npos)
                    break;

                const std::size_t objectEnd = FindMatchingClose(objectsArray, objectStart, '{', '}');

                if (objectEnd == std::string::npos)
                    break;

                const std::string objectBlock = objectsArray.substr(objectStart, objectEnd - objectStart + 1);

                EditorObject object;

                std::string typeText;

                ReadUIntValue(objectBlock, "id", &object.Id);
                ReadStringValue(objectBlock, "type", &typeText);
                ReadStringValue(objectBlock, "name", &object.Name);
                ReadStringValue(objectBlock, "asset", &object.AssetPath);
                ReadVec3Value(objectBlock, "position", &object.Position);
                ReadVec3Value(objectBlock, "rotation", &object.Rotation);
                ReadVec3Value(objectBlock, "scale", &object.Scale);
                ReadFloatValue(objectBlock, "radius", &object.Radius);
                ReadBoolValue(objectBlock, "visible", &object.Visible);

                object.Type = EditorObjectTypeFromSerializedText(typeText);

                if (object.Id == 0)
                    object.Id = highestObjectId + 1;

                highestObjectId = (std::max)(highestObjectId, object.Id);

                if (object.Name.empty())
                    object.Name = std::string(EditorObjectTypeToText(object.Type)) + " " + std::to_string(object.Id);

                loadedObjects.push_back(object);

                position = objectEnd + 1;
            }
        }

        m_objects = std::move(loadedObjects);
        m_nextObjectId = highestObjectId + 1;
        m_selectedObjectId = 0;

        if (m_nextObjectId == 0)
            m_nextObjectId = 1;

        return true;
    }

    std::uint32_t EditorScene::AllocateObjectId()
    {
        const std::uint32_t id = m_nextObjectId;
        ++m_nextObjectId;

        if (m_nextObjectId == 0)
            m_nextObjectId = 1;

        return id;
    }
}