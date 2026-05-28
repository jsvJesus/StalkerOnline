#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace StalkerOnline::Editor
{
    struct EditorVec3
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Z = 0.0f;
    };

    enum class EditorObjectType : std::uint32_t
    {
        StaticMeshProxy = 0,
        SpeedTreeProxy = 1,
        PlayerSpawn = 2,
        LootSpawner = 3,
        SafeZone = 4,
        RadiationZone = 5,
        AnomalyZone = 6,
        Light = 7
    };

    const char* EditorObjectTypeToText(EditorObjectType type);
    const char* EditorObjectTypeToSerializedText(EditorObjectType type);
    EditorObjectType EditorObjectTypeFromSerializedText(const std::string& text);

    struct EditorObject
    {
        std::uint32_t Id = 0;

        EditorObjectType Type = EditorObjectType::StaticMeshProxy;

        std::string Name;
        std::string AssetPath;

        EditorVec3 Position;
        EditorVec3 Rotation;
        EditorVec3 Scale = { 1.0f, 1.0f, 1.0f };

        float Radius = 100.0f;
        bool Visible = true;
    };

    class EditorScene final
    {
    public:
        EditorScene() = default;

        void Clear();

        std::uint32_t AddObject(const EditorObject& object);
        bool RemoveObject(std::uint32_t objectId);
        bool RemoveSelectedObject();

        void SelectObject(std::uint32_t objectId);
        void ClearSelection();

        EditorObject* GetObjectMutable(std::uint32_t objectId);
        const EditorObject* GetObject(std::uint32_t objectId) const;

        EditorObject* GetSelectedObjectMutable();
        const EditorObject* GetSelectedObject() const;

        const std::vector<EditorObject>& GetObjects() const;

        std::uint32_t GetSelectedObjectId() const;
        std::uint32_t GetNextObjectId() const;

        void SetTerrainInfo(
            const std::string& heightmapPath,
            int width,
            int height,
            float cellSizeX,
            float cellSizeY,
            float heightScale);

        const std::string& GetTerrainHeightmapPath() const;
        int GetTerrainWidth() const;
        int GetTerrainHeight() const;
        float GetTerrainCellSizeX() const;
        float GetTerrainCellSizeY() const;
        float GetTerrainHeightScale() const;

        bool SaveToFile(const std::string& path, std::string* outErrorMessage = nullptr) const;
        bool LoadFromFile(const std::string& path, std::string* outErrorMessage = nullptr);

    private:
        std::uint32_t AllocateObjectId();

    private:
        std::vector<EditorObject> m_objects;

        std::uint32_t m_nextObjectId = 1;
        std::uint32_t m_selectedObjectId = 0;

        std::string m_terrainHeightmapPath;
        int m_terrainWidth = 0;
        int m_terrainHeight = 0;
        float m_terrainCellSizeX = 100.0f;
        float m_terrainCellSizeY = 100.0f;
        float m_terrainHeightScale = 3000.0f;
    };
}