#pragma once

#include <imgui.h>

#include <cstdint>
#include <string>
#include <vector>

namespace StalkerOnline::UI
{
    struct LoginScreenState
    {
        bool RegisterMode = false;
        bool RememberLogin = true;

        char ServerHost[128] = "26.163.92.76";
        int ServerPort = 7777;

        char Login[64] = {};
        char Email[128] = {};
        char Password[64] = {};

        char StatusText[256] = "Disconnected";
        bool IsConnected = false;
        bool IsBusy = false;
    };

    struct PlayerDebugStats
    {
        int AccountId = 0;
        int CharacterId = 0;

        float Health = 100.0f;
        float MaxHealth = 100.0f;

        float Stamina = 100.0f;
        float MaxStamina = 100.0f;

        float Hunger = 0.0f;
        float Thirst = 0.0f;
        float Radiation = 0.0f;
        float Toxicity = 0.0f;

        float PosX = 0.0f;
        float PosY = 0.0f;
        float PosZ = 0.0f;
    };

    struct InventoryItemUi
    {
        int32_t SlotIndex = 0;

        std::string ItemTemplateId;
        std::string DisplayName;

        int32_t Quantity = 0;
        int32_t MaxStack = 1;

        float WeightPerItem = 0.0f;
    };

    struct InventorySnapshotUi
    {
        bool Valid = false;

        int32_t CharacterId = 0;
        int32_t Capacity = 0;

        float TotalWeight = 0.0f;

        std::vector<InventoryItemUi> Items;
    };

    struct WorldItemUi
    {
        int32_t WorldObjectId = 0;

        std::string ItemTemplateId;
        std::string DisplayName;

        int32_t Quantity = 0;

        float PositionX = 0.0f;
        float PositionY = 0.0f;
        float PositionZ = 0.0f;

        float RotationX = 0.0f;
        float RotationY = 0.0f;
        float RotationZ = 0.0f;
    };

    struct GameScreenState
    {
        PlayerDebugStats Player;
        InventorySnapshotUi Inventory;
        std::vector<WorldItemUi> WorldItems;

        int32_t SelectedWorldItemId = 0;

        char StatusText[256] = "Ready";
    };

    struct GameScreenActions
    {
        bool MoveUpPressed = false;
        bool MoveDownPressed = false;
        bool MoveLeftPressed = false;
        bool MoveRightPressed = false;

        bool RotateLeftPressed = false;
        bool RotateRightPressed = false;

        bool PickupPressed = false;
        int32_t PickupWorldObjectId = 0;

        bool DisconnectPressed = false;
    };

    void ApplyStalkerDarkStyle();

    void DrawZoneBackground(float timeSeconds);

    bool StalkerButton(
        const char* label,
        const ImVec2& size = ImVec2(0.0f, 0.0f),
        bool danger = false,
        bool disabled = false
    );

    bool BeginStalkerPanel(
        const char* id,
        const char* title,
        const ImVec2& position,
        const ImVec2& size,
        ImGuiWindowFlags extraFlags = 0
    );

    void EndStalkerPanel();

    void DrawLoginRegisterScreen(
        LoginScreenState& state,
        bool& outLoginPressed,
        bool& outRegisterPressed,
        bool& outExitPressed
    );

    void DrawGameHudMock(
        const PlayerDebugStats& stats,
        bool& outOpenInventoryPressed,
        bool& outDisconnectPressed
    );

    void DrawGameScreen(
        GameScreenState& state,
        GameScreenActions& actions
    );
}