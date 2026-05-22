#pragma once

#include <imgui.h>

namespace StalkerOnline::UI
{
    struct LoginScreenState
    {
        bool RegisterMode = false;
        bool RememberLogin = true;

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
}