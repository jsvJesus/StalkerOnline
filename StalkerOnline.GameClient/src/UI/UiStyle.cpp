#include "UI/UiStyle.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace StalkerOnline::UI
{
    static ImVec4 ColorFromBytes(int r, int g, int b, int a = 255)
    {
        return ImVec4(
            static_cast<float>(r) / 255.0f,
            static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f,
            static_cast<float>(a) / 255.0f
        );
    }

    static ImU32 U32(int r, int g, int b, int a = 255)
    {
        return IM_COL32(r, g, b, a);
    }

    static float Clamp01(float value)
    {
        if (value < 0.0f)
            return 0.0f;

        if (value > 1.0f)
            return 1.0f;

        return value;
    }

    static void DrawTextShadow(
        ImDrawList* drawList,
        const ImVec2& position,
        ImU32 color,
        const char* text
    )
    {
        drawList->AddText(ImVec2(position.x + 1.0f, position.y + 1.0f), U32(0, 0, 0, 210), text);
        drawList->AddText(position, color, text);
    }

    static void DrawSeparatorLine(ImDrawList* drawList, const ImVec2& a, const ImVec2& b)
    {
        drawList->AddLine(a, b, U32(114, 103, 76, 180), 1.0f);
        drawList->AddLine(ImVec2(a.x, a.y + 1.0f), ImVec2(b.x, b.y + 1.0f), U32(0, 0, 0, 160), 1.0f);
    }

    void ApplyStalkerDarkStyle()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        style.WindowPadding = ImVec2(14.0f, 12.0f);
        style.FramePadding = ImVec2(10.0f, 7.0f);
        style.CellPadding = ImVec2(8.0f, 5.0f);
        style.ItemSpacing = ImVec2(10.0f, 8.0f);
        style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);

        style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
        style.IndentSpacing = 20.0f;
        style.ScrollbarSize = 13.0f;
        style.GrabMinSize = 10.0f;

        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.TabBorderSize = 1.0f;

        style.WindowRounding = 3.0f;
        style.ChildRounding = 3.0f;
        style.FrameRounding = 2.0f;
        style.PopupRounding = 3.0f;
        style.ScrollbarRounding = 2.0f;
        style.GrabRounding = 2.0f;
        style.TabRounding = 2.0f;

        style.WindowTitleAlign = ImVec2(0.02f, 0.50f);
        style.ButtonTextAlign = ImVec2(0.50f, 0.50f);
        style.SelectableTextAlign = ImVec2(0.00f, 0.50f);

        ImVec4* colors = style.Colors;

        colors[ImGuiCol_Text] = ColorFromBytes(216, 210, 188);
        colors[ImGuiCol_TextDisabled] = ColorFromBytes(105, 101, 91);

        colors[ImGuiCol_WindowBg] = ColorFromBytes(12, 13, 11, 238);
        colors[ImGuiCol_ChildBg] = ColorFromBytes(18, 18, 15, 220);
        colors[ImGuiCol_PopupBg] = ColorFromBytes(14, 14, 12, 245);

        colors[ImGuiCol_Border] = ColorFromBytes(96, 86, 58, 190);
        colors[ImGuiCol_BorderShadow] = ColorFromBytes(0, 0, 0, 160);

        colors[ImGuiCol_FrameBg] = ColorFromBytes(29, 30, 25, 230);
        colors[ImGuiCol_FrameBgHovered] = ColorFromBytes(53, 55, 45, 245);
        colors[ImGuiCol_FrameBgActive] = ColorFromBytes(79, 75, 52, 255);

        colors[ImGuiCol_TitleBg] = ColorFromBytes(20, 21, 17);
        colors[ImGuiCol_TitleBgActive] = ColorFromBytes(34, 33, 24);
        colors[ImGuiCol_TitleBgCollapsed] = ColorFromBytes(10, 10, 9, 220);

        colors[ImGuiCol_MenuBarBg] = ColorFromBytes(18, 19, 16);

        colors[ImGuiCol_ScrollbarBg] = ColorFromBytes(9, 9, 8, 210);
        colors[ImGuiCol_ScrollbarGrab] = ColorFromBytes(70, 68, 52, 220);
        colors[ImGuiCol_ScrollbarGrabHovered] = ColorFromBytes(95, 91, 63, 240);
        colors[ImGuiCol_ScrollbarGrabActive] = ColorFromBytes(126, 113, 71, 255);

        colors[ImGuiCol_CheckMark] = ColorFromBytes(192, 164, 83);
        colors[ImGuiCol_SliderGrab] = ColorFromBytes(145, 122, 70);
        colors[ImGuiCol_SliderGrabActive] = ColorFromBytes(201, 161, 72);

        colors[ImGuiCol_Button] = ColorFromBytes(42, 43, 35, 240);
        colors[ImGuiCol_ButtonHovered] = ColorFromBytes(73, 72, 51, 255);
        colors[ImGuiCol_ButtonActive] = ColorFromBytes(121, 97, 49, 255);

        colors[ImGuiCol_Header] = ColorFromBytes(47, 48, 39, 230);
        colors[ImGuiCol_HeaderHovered] = ColorFromBytes(80, 78, 55, 245);
        colors[ImGuiCol_HeaderActive] = ColorFromBytes(113, 91, 51, 255);

        colors[ImGuiCol_Separator] = ColorFromBytes(87, 79, 55, 210);
        colors[ImGuiCol_SeparatorHovered] = ColorFromBytes(139, 119, 68, 240);
        colors[ImGuiCol_SeparatorActive] = ColorFromBytes(193, 151, 66, 255);

        colors[ImGuiCol_ResizeGrip] = ColorFromBytes(89, 81, 55, 90);
        colors[ImGuiCol_ResizeGripHovered] = ColorFromBytes(153, 125, 66, 160);
        colors[ImGuiCol_ResizeGripActive] = ColorFromBytes(211, 156, 55, 210);

        colors[ImGuiCol_Tab] = ColorFromBytes(30, 31, 26, 240);
        colors[ImGuiCol_TabHovered] = ColorFromBytes(80, 78, 55, 255);
        colors[ImGuiCol_TabActive] = ColorFromBytes(54, 53, 39, 255);
        colors[ImGuiCol_TabUnfocused] = ColorFromBytes(19, 20, 17, 230);
        colors[ImGuiCol_TabUnfocusedActive] = ColorFromBytes(34, 35, 29, 240);

        colors[ImGuiCol_PlotLines] = ColorFromBytes(173, 152, 91);
        colors[ImGuiCol_PlotLinesHovered] = ColorFromBytes(228, 177, 63);
        colors[ImGuiCol_PlotHistogram] = ColorFromBytes(163, 130, 66);
        colors[ImGuiCol_PlotHistogramHovered] = ColorFromBytes(222, 161, 54);

        colors[ImGuiCol_TableHeaderBg] = ColorFromBytes(31, 32, 27);
        colors[ImGuiCol_TableBorderStrong] = ColorFromBytes(89, 82, 59);
        colors[ImGuiCol_TableBorderLight] = ColorFromBytes(49, 48, 39);
        colors[ImGuiCol_TableRowBg] = ColorFromBytes(0, 0, 0, 0);
        colors[ImGuiCol_TableRowBgAlt] = ColorFromBytes(255, 255, 255, 9);

        colors[ImGuiCol_TextSelectedBg] = ColorFromBytes(126, 98, 47, 120);
        colors[ImGuiCol_DragDropTarget] = ColorFromBytes(224, 166, 54, 230);
        colors[ImGuiCol_NavHighlight] = ColorFromBytes(196, 150, 55, 180);
        colors[ImGuiCol_NavWindowingHighlight] = ColorFromBytes(255, 255, 255, 70);
        colors[ImGuiCol_NavWindowingDimBg] = ColorFromBytes(0, 0, 0, 120);
        colors[ImGuiCol_ModalWindowDimBg] = ColorFromBytes(0, 0, 0, 175);
    }

    void DrawZoneBackground(float timeSeconds)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImDrawList* drawList = ImGui::GetBackgroundDrawList();

        const ImVec2 pos = viewport->Pos;
        const ImVec2 size = viewport->Size;

        const ImVec2 p0 = pos;
        const ImVec2 p1 = ImVec2(pos.x + size.x, pos.y + size.y);

        drawList->AddRectFilledMultiColor(
            p0,
            p1,
            U32(8, 10, 8, 255),
            U32(17, 19, 14, 255),
            U32(7, 8, 7, 255),
            U32(18, 15, 10, 255)
        );

        const float pulse = 0.5f + 0.5f * std::sin(timeSeconds * 0.7f);

        drawList->AddCircleFilled(
            ImVec2(pos.x + size.x * 0.72f, pos.y + size.y * 0.28f),
            size.x * 0.34f,
            U32(102, 76, 29, static_cast<int>(28.0f + pulse * 18.0f)),
            96
        );

        drawList->AddCircleFilled(
            ImVec2(pos.x + size.x * 0.18f, pos.y + size.y * 0.75f),
            size.x * 0.28f,
            U32(52, 71, 45, 30),
            96
        );

        for (int i = 0; i < 38; ++i)
        {
            const float k = static_cast<float>(i);
            const float x = pos.x + std::fmod(k * 113.0f + timeSeconds * 8.0f, size.x);
            const float y = pos.y + std::fmod(k * 47.0f + timeSeconds * 13.0f, size.y);
            const float alpha = 18.0f + 18.0f * Clamp01(std::sin(timeSeconds + k) * 0.5f + 0.5f);

            drawList->AddCircleFilled(
                ImVec2(x, y),
                1.0f + std::fmod(k, 3.0f),
                U32(180, 164, 115, static_cast<int>(alpha)),
                8
            );
        }

        for (float y = pos.y; y < pos.y + size.y; y += 4.0f)
        {
            drawList->AddLine(
                ImVec2(pos.x, y),
                ImVec2(pos.x + size.x, y),
                U32(0, 0, 0, 22),
                1.0f
            );
        }

        const float vignetteThickness = 220.0f;

        drawList->AddRectFilledMultiColor(
            p0,
            ImVec2(p1.x, p0.y + vignetteThickness),
            U32(0, 0, 0, 190),
            U32(0, 0, 0, 190),
            U32(0, 0, 0, 0),
            U32(0, 0, 0, 0)
        );

        drawList->AddRectFilledMultiColor(
            ImVec2(p0.x, p1.y - vignetteThickness),
            p1,
            U32(0, 0, 0, 0),
            U32(0, 0, 0, 0),
            U32(0, 0, 0, 210),
            U32(0, 0, 0, 210)
        );

        drawList->AddRectFilledMultiColor(
            p0,
            ImVec2(p0.x + vignetteThickness, p1.y),
            U32(0, 0, 0, 210),
            U32(0, 0, 0, 0),
            U32(0, 0, 0, 0),
            U32(0, 0, 0, 210)
        );

        drawList->AddRectFilledMultiColor(
            ImVec2(p1.x - vignetteThickness, p0.y),
            p1,
            U32(0, 0, 0, 0),
            U32(0, 0, 0, 220),
            U32(0, 0, 0, 220),
            U32(0, 0, 0, 0)
        );
    }

    bool StalkerButton(
        const char* label,
        const ImVec2& size,
        bool danger,
        bool disabled
    )
    {
        if (disabled)
            ImGui::BeginDisabled(true);

        const ImVec4 normal = danger
            ? ColorFromBytes(74, 31, 25, 245)
            : ColorFromBytes(45, 47, 38, 245);

        const ImVec4 hovered = danger
            ? ColorFromBytes(122, 45, 32, 255)
            : ColorFromBytes(78, 78, 54, 255);

        const ImVec4 active = danger
            ? ColorFromBytes(168, 57, 36, 255)
            : ColorFromBytes(128, 101, 49, 255);

        ImGui::PushStyleColor(ImGuiCol_Button, normal);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
        ImGui::PushStyleColor(ImGuiCol_Border, ColorFromBytes(121, 105, 67, 210));

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 8.0f));

        const bool pressed = ImGui::Button(label, size);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        if (disabled)
            ImGui::EndDisabled();

        return pressed && !disabled;
    }

    bool BeginStalkerPanel(
        const char* id,
        const char* title,
        const ImVec2& position,
        const ImVec2& size,
        ImGuiWindowFlags extraFlags
    )
    {
        ImGui::SetNextWindowPos(position, ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoTitleBar |
            extraFlags;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ColorFromBytes(13, 14, 12, 232));
        ImGui::PushStyleColor(ImGuiCol_Border, ColorFromBytes(105, 94, 62, 210));

        const bool opened = ImGui::Begin(id, nullptr, flags);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 windowSize = ImGui::GetWindowSize();

        drawList->AddRectFilled(
            windowPos,
            ImVec2(windowPos.x + windowSize.x, windowPos.y + 34.0f),
            U32(32, 32, 25, 240),
            4.0f,
            ImDrawFlags_RoundCornersTop
        );

        drawList->AddRect(
            ImVec2(windowPos.x + 1.0f, windowPos.y + 1.0f),
            ImVec2(windowPos.x + windowSize.x - 1.0f, windowPos.y + windowSize.y - 1.0f),
            U32(0, 0, 0, 180),
            4.0f,
            0,
            1.0f
        );

        drawList->AddLine(
            ImVec2(windowPos.x, windowPos.y + 34.0f),
            ImVec2(windowPos.x + windowSize.x, windowPos.y + 34.0f),
            U32(146, 119, 62, 185),
            1.0f
        );

        DrawTextShadow(
            drawList,
            ImVec2(windowPos.x + 14.0f, windowPos.y + 9.0f),
            U32(219, 197, 141, 255),
            title
        );

        ImGui::SetCursorPosY(46.0f);

        return opened;
    }

    void EndStalkerPanel()
    {
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }

    void DrawLoginRegisterScreen(
        LoginScreenState& state,
        bool& outLoginPressed,
        bool& outRegisterPressed,
        bool& outExitPressed
    )
    {
        outLoginPressed = false;
        outRegisterPressed = false;
        outExitPressed = false;

        ImGuiViewport* viewport = ImGui::GetMainViewport();

        const ImVec2 panelSize(560.0f, state.RegisterMode ? 665.0f : 610.0f);
        const ImVec2 panelPos(
            viewport->Pos.x + viewport->Size.x * 0.5f - panelSize.x * 0.5f,
            viewport->Pos.y + viewport->Size.y * 0.5f - panelSize.y * 0.5f
        );

        if (!BeginStalkerPanel("##StalkerLoginPanel", "AUTHORIZATION TERMINAL", panelPos, panelSize))
        {
            EndStalkerPanel();
            return;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 windowSize = ImGui::GetWindowSize();

        const char* logo = "STALKER ONLINE";
        const ImVec2 logoSize = ImGui::CalcTextSize(logo);
        const ImVec2 logoPos(
            windowPos.x + windowSize.x * 0.5f - logoSize.x * 0.5f,
            windowPos.y + 60.0f
        );

        DrawTextShadow(drawList, logoPos, U32(226, 194, 105, 255), logo);

        ImGui::SetCursorPosY(92.0f);

        ImGui::TextColored(ColorFromBytes(145, 137, 108), "SERVER STATUS");
        ImGui::SameLine();

        if (state.IsCheckingServer)
        {
            ImGui::TextColored(ColorFromBytes(210, 175, 70), "CHECKING...");
        }
        else if (!state.ServerStatusKnown)
        {
            ImGui::TextColored(ColorFromBytes(150, 145, 125), "UNKNOWN");
        }
        else if (state.ServerOnline)
        {
            ImGui::TextColored(ColorFromBytes(105, 181, 96), "ONLINE");
        }
        else
        {
            ImGui::TextColored(ColorFromBytes(187, 79, 61), "OFFLINE");
        }

        ImGui::TextColored(ColorFromBytes(139, 133, 112), "%s", state.StatusText);

        DrawSeparatorLine(
            drawList,
            ImVec2(windowPos.x + 16.0f, ImGui::GetCursorScreenPos().y + 8.0f),
            ImVec2(windowPos.x + windowSize.x - 16.0f, ImGui::GetCursorScreenPos().y + 8.0f)
        );

        ImGui::Dummy(ImVec2(1.0f, 18.0f));

        ImGui::TextColored(ColorFromBytes(197, 179, 126), "SERVER IP");

        ImGui::PushItemWidth(340.0f);
        ImGui::InputText("##ServerHostInput", state.ServerHost, sizeof(state.ServerHost));
        ImGui::PopItemWidth();

        ImGui::SameLine();

        ImGui::TextColored(ColorFromBytes(197, 179, 126), "PORT");

        ImGui::SameLine();

        ImGui::PushItemWidth(90.0f);
        ImGui::InputInt("##ServerPortInput", &state.ServerPort, 0, 0);
        ImGui::PopItemWidth();

        if (state.ServerPort < 1)
            state.ServerPort = 1;

        if (state.ServerPort > 65535)
            state.ServerPort = 65535;

        ImGui::Dummy(ImVec2(1.0f, 10.0f));

        ImGui::TextColored(ColorFromBytes(197, 179, 126), "LOGIN");
        ImGui::InputText("##LoginInput", state.Login, sizeof(state.Login));

        if (state.RegisterMode)
        {
            ImGui::TextColored(ColorFromBytes(197, 179, 126), "EMAIL");
            ImGui::InputText("##EmailInput", state.Email, sizeof(state.Email));
        }

        ImGui::TextColored(ColorFromBytes(197, 179, 126), "PASSWORD");
        ImGui::InputText(
            "##PasswordInput",
            state.Password,
            sizeof(state.Password),
            ImGuiInputTextFlags_Password
        );

        ImGui::Dummy(ImVec2(1.0f, 6.0f));

        ImGui::Checkbox("Remember login", &state.RememberLogin);

        ImGui::Dummy(ImVec2(1.0f, 12.0f));

        const float buttonWidth = windowSize.x - 32.0f;

        if (!state.RegisterMode)
        {
            if (StalkerButton("LOGIN", ImVec2(buttonWidth, 38.0f), false, state.IsBusy || !state.ServerOnline))
                outLoginPressed = true;

            if (StalkerButton("CREATE NEW ACCOUNT", ImVec2(buttonWidth, 34.0f), false, state.IsBusy || !state.ServerOnline))
                state.RegisterMode = true;
        }
        else
        {
            if (StalkerButton("REGISTER ACCOUNT", ImVec2(buttonWidth, 38.0f), false, state.IsBusy || !state.ServerOnline))
                outRegisterPressed = true;

            if (StalkerButton("BACK TO LOGIN", ImVec2(buttonWidth, 34.0f), false, state.IsBusy))
                state.RegisterMode = false;
        }

        ImGui::Dummy(ImVec2(1.0f, 8.0f));

        if (StalkerButton("EXIT CLIENT", ImVec2(buttonWidth, 34.0f), true, state.IsBusy))
            outExitPressed = true;

        EndStalkerPanel();
    }

    static void DrawStatBar(
        const char* label,
        float value,
        float maxValue,
        ImU32 fillColor
    )
    {
        const float width = ImGui::GetContentRegionAvail().x;
        const float height = 18.0f;

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float ratio = maxValue > 0.0f ? Clamp01(value / maxValue) : 0.0f;

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilled(
            pos,
            ImVec2(pos.x + width, pos.y + height),
            U32(17, 18, 15, 255),
            2.0f
        );

        drawList->AddRectFilled(
            pos,
            ImVec2(pos.x + width * ratio, pos.y + height),
            fillColor,
            2.0f
        );

        drawList->AddRect(
            pos,
            ImVec2(pos.x + width, pos.y + height),
            U32(88, 80, 57, 220),
            2.0f
        );

        char text[128];
        std::snprintf(text, sizeof(text), "%s %.0f / %.0f", label, value, maxValue);

        const ImVec2 textSize = ImGui::CalcTextSize(text);
        DrawTextShadow(
            drawList,
            ImVec2(pos.x + width * 0.5f - textSize.x * 0.5f, pos.y + 1.0f),
            U32(230, 223, 198, 255),
            text
        );

        ImGui::Dummy(ImVec2(width, height + 6.0f));
    }

    static void DrawInventorySlot(int index)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 pos = ImGui::GetCursorScreenPos();

        const float size = 48.0f;

        const bool hasItem = index == 1 || index == 5 || index == 9 || index == 14;

        drawList->AddRectFilled(
            pos,
            ImVec2(pos.x + size, pos.y + size),
            hasItem ? U32(48, 50, 38, 245) : U32(18, 19, 16, 240),
            2.0f
        );

        drawList->AddRect(
            pos,
            ImVec2(pos.x + size, pos.y + size),
            hasItem ? U32(161, 132, 66, 230) : U32(69, 63, 47, 190),
            2.0f,
            0,
            1.0f
        );

        if (hasItem)
        {
            drawList->AddCircleFilled(
                ImVec2(pos.x + size * 0.5f, pos.y + size * 0.5f),
                12.0f,
                U32(122, 112, 75, 230),
                16
            );

            drawList->AddCircle(
                ImVec2(pos.x + size * 0.5f, pos.y + size * 0.5f),
                12.0f,
                U32(214, 177, 84, 220),
                16,
                1.0f
            );
        }

        ImGui::Dummy(ImVec2(size, size));
    }

    void DrawGameHudMock(
        const PlayerDebugStats& stats,
        bool& outOpenInventoryPressed,
        bool& outDisconnectPressed
    )
    {
        outOpenInventoryPressed = false;
        outDisconnectPressed = false;

        ImGuiViewport* viewport = ImGui::GetMainViewport();

        if (BeginStalkerPanel(
            "##PlayerStatsPanel",
            "PLAYER STATUS",
            ImVec2(viewport->Pos.x + 22.0f, viewport->Pos.y + 22.0f),
            ImVec2(330.0f, 250.0f)
        ))
        {
            DrawStatBar("Health", stats.Health, stats.MaxHealth, U32(126, 42, 35, 235));
            DrawStatBar("Stamina", stats.Stamina, stats.MaxStamina, U32(71, 126, 58, 235));

            ImGui::TextColored(ColorFromBytes(194, 180, 130), "Hunger: %.0f", stats.Hunger);
            ImGui::TextColored(ColorFromBytes(194, 180, 130), "Thirst: %.0f", stats.Thirst);
            ImGui::TextColored(ColorFromBytes(194, 180, 130), "Radiation: %.0f", stats.Radiation);
            ImGui::TextColored(ColorFromBytes(194, 180, 130), "Toxicity: %.0f", stats.Toxicity);

            ImGui::TextColored(
                ColorFromBytes(133, 127, 106),
                "Position: %.1f / %.1f / %.1f",
                stats.PosX,
                stats.PosY,
                stats.PosZ
            );
        }

        EndStalkerPanel();

        const ImVec2 invSize(330.0f, 330.0f);
        const ImVec2 invPos(
            viewport->Pos.x + viewport->Size.x - invSize.x - 22.0f,
            viewport->Pos.y + 22.0f
        );

        if (BeginStalkerPanel("##QuickInventoryPanel", "QUICK INVENTORY", invPos, invSize))
        {
            constexpr int columns = 5;

            for (int i = 0; i < 20; ++i)
            {
                DrawInventorySlot(i);

                if ((i + 1) % columns != 0)
                    ImGui::SameLine();
            }

            ImGui::Dummy(ImVec2(1.0f, 8.0f));

            if (StalkerButton("OPEN INVENTORY", ImVec2(-1.0f, 34.0f)))
                outOpenInventoryPressed = true;
        }

        EndStalkerPanel();

        const ImVec2 menuSize(300.0f, 130.0f);
        const ImVec2 menuPos(
            viewport->Pos.x + viewport->Size.x * 0.5f - menuSize.x * 0.5f,
            viewport->Pos.y + viewport->Size.y - menuSize.y - 22.0f
        );

        if (BeginStalkerPanel("##BottomMenuPanel", "CLIENT", menuPos, menuSize))
        {
            ImGui::TextColored(ColorFromBytes(151, 143, 113), "W/A/S/D movement will be here later.");

            if (StalkerButton("DISCONNECT", ImVec2(-1.0f, 34.0f), true))
                outDisconnectPressed = true;
        }

        EndStalkerPanel();
    }

    static const InventoryItemUi* FindInventoryItemAtSlot(
        const InventorySnapshotUi& inventory,
        int32_t slotIndex
    )
    {
        for (const InventoryItemUi& item : inventory.Items)
        {
            if (item.SlotIndex == slotIndex)
                return &item;
        }

        return nullptr;
    }

    static bool HasWorldItem(
        const std::vector<WorldItemUi>& items,
        int32_t worldObjectId
    )
    {
        if (worldObjectId <= 0)
            return false;

        for (const WorldItemUi& item : items)
        {
            if (item.WorldObjectId == worldObjectId)
                return true;
        }

        return false;
    }

    static bool DrawRealInventorySlot(
        const InventoryItemUi* item,
        int32_t slotIndex,
        bool selected
    )
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 pos = ImGui::GetCursorScreenPos();

        constexpr float slotSize = 54.0f;

        const bool hasItem = item != nullptr;

        ImU32 bgColor = hasItem ? U32(45, 47, 36, 245) : U32(17, 18, 15, 235);
        ImU32 borderColor = hasItem ? U32(163, 132, 66, 235) : U32(67, 61, 46, 190);

        if (selected)
        {
            bgColor = U32(70, 62, 38, 255);
            borderColor = U32(235, 185, 70, 255);
        }

        drawList->AddRectFilled(
            pos,
            ImVec2(pos.x + slotSize, pos.y + slotSize),
            bgColor,
            2.0f
        );

        drawList->AddRect(
            pos,
            ImVec2(pos.x + slotSize, pos.y + slotSize),
            borderColor,
            2.0f,
            0,
            selected ? 2.0f : 1.0f
        );

        char slotText[32];
        std::snprintf(slotText, sizeof(slotText), "%d", slotIndex);

        drawList->AddText(
            ImVec2(pos.x + 4.0f, pos.y + 3.0f),
            U32(102, 98, 82, 220),
            slotText
        );

        if (hasItem)
        {
            drawList->AddRectFilled(
                ImVec2(pos.x + 13.0f, pos.y + 15.0f),
                ImVec2(pos.x + slotSize - 13.0f, pos.y + slotSize - 13.0f),
                U32(103, 94, 62, 230),
                3.0f
            );

            drawList->AddRect(
                ImVec2(pos.x + 13.0f, pos.y + 15.0f),
                ImVec2(pos.x + slotSize - 13.0f, pos.y + slotSize - 13.0f),
                U32(219, 177, 78, 210),
                3.0f,
                0,
                1.0f
            );

            char qtyText[32];
            std::snprintf(qtyText, sizeof(qtyText), "x%d", item->Quantity);

            const ImVec2 qtySize = ImGui::CalcTextSize(qtyText);

            drawList->AddText(
                ImVec2(pos.x + slotSize - qtySize.x - 4.0f, pos.y + slotSize - 17.0f),
                U32(235, 225, 190, 255),
                qtyText
            );
        }

        ImGui::InvisibleButton("##InventorySlot", ImVec2(slotSize, slotSize));

        bool clicked = false;

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            clicked = true;

        if (hasItem && ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::TextColored(ColorFromBytes(226, 194, 105), "%s", item->DisplayName.c_str());
            ImGui::Separator();
            ImGui::Text("Template: %s", item->ItemTemplateId.c_str());
            ImGui::Text("Slot: %d", item->SlotIndex);
            ImGui::Text("Quantity: %d / %d", item->Quantity, item->MaxStack);
            ImGui::Text("Weight: %.2f", item->WeightPerItem);
            ImGui::TextColored(ColorFromBytes(180, 160, 95), "Left click: select slot");
            ImGui::EndTooltip();
        }

        return clicked;
    }

    static void DrawInventoryGrid(
        GameScreenState& state,
        GameScreenActions& actions
    )
    {
        const InventorySnapshotUi& inventory = state.Inventory;

        const int32_t capacity = inventory.Capacity > 0 ? inventory.Capacity : 20;
        constexpr int32_t columns = 5;

        if (state.SelectedInventorySlotIndex >= capacity)
            state.SelectedInventorySlotIndex = -1;

        ImGui::BeginChild(
            "##InventoryGridChild",
            ImVec2(0.0f, 260.0f),
            true,
            ImGuiWindowFlags_AlwaysVerticalScrollbar
        );

        for (int32_t slot = 0; slot < capacity; ++slot)
        {
            ImGui::PushID(slot);

            const InventoryItemUi* item = FindInventoryItemAtSlot(inventory, slot);
            const bool selected = state.SelectedInventorySlotIndex == slot;

            if (DrawRealInventorySlot(item, slot, selected))
            {
                if (item != nullptr)
                    state.SelectedInventorySlotIndex = slot;
                else if (selected)
                    state.SelectedInventorySlotIndex = -1;
            }

            ImGui::PopID();

            if ((slot + 1) % columns != 0)
                ImGui::SameLine();
        }

        ImGui::EndChild();

        const InventoryItemUi* selectedItem = nullptr;

        if (state.SelectedInventorySlotIndex >= 0)
            selectedItem = FindInventoryItemAtSlot(inventory, state.SelectedInventorySlotIndex);

        ImGui::Dummy(ImVec2(1.0f, 6.0f));

        if (selectedItem != nullptr)
        {
            ImGui::TextColored(
                ColorFromBytes(226, 194, 105),
                "Selected: slot %d | %s x%d",
                selectedItem->SlotIndex,
                selectedItem->DisplayName.c_str(),
                selectedItem->Quantity
            );
        }
        else
        {
            ImGui::TextColored(
                ColorFromBytes(130, 126, 108),
                "Selected: none"
            );
        }

        const bool canDrop = selectedItem != nullptr;

        if (StalkerButton("DROP SELECTED ITEM", ImVec2(-1.0f, 34.0f), true, !canDrop))
        {
            actions.DropPressed = true;
            actions.DropSlotIndex = state.SelectedInventorySlotIndex;
            actions.DropQuantity = 1;
        }
    }

    static void DrawWorldItemsList(
        GameScreenState& state,
        GameScreenActions& actions
    )
    {
        if (!HasWorldItem(state.WorldItems, state.SelectedWorldItemId))
            state.SelectedWorldItemId = 0;

        ImGui::BeginChild(
            "##WorldItemsChild",
            ImVec2(0.0f, 230.0f),
            true,
            ImGuiWindowFlags_AlwaysVerticalScrollbar
        );

        if (state.WorldItems.empty())
        {
            ImGui::TextColored(ColorFromBytes(130, 126, 108), "No visible world items.");
        }
        else
        {
            for (const WorldItemUi& item : state.WorldItems)
            {
                char label[256];

                std::snprintf(
                    label,
                    sizeof(label),
                    "%s x%d  |  id=%d  |  %.1f %.1f %.1f",
                    item.DisplayName.c_str(),
                    item.Quantity,
                    item.WorldObjectId,
                    item.PositionX,
                    item.PositionY,
                    item.PositionZ
                );

                const bool selected = state.SelectedWorldItemId == item.WorldObjectId;

                ImGui::PushID(item.WorldObjectId);

                if (ImGui::Selectable(label, selected))
                    state.SelectedWorldItemId = item.WorldObjectId;

                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::TextColored(ColorFromBytes(226, 194, 105), "%s", item.DisplayName.c_str());
                    ImGui::Separator();
                    ImGui::Text("Template: %s", item.ItemTemplateId.c_str());
                    ImGui::Text("WorldObjectId: %d", item.WorldObjectId);
                    ImGui::Text("Quantity: %d", item.Quantity);
                    ImGui::Text("Position: %.2f / %.2f / %.2f", item.PositionX, item.PositionY, item.PositionZ);
                    ImGui::EndTooltip();
                }

                ImGui::PopID();
            }
        }

        ImGui::EndChild();

        const bool canPickup = state.SelectedWorldItemId > 0;

        if (StalkerButton("PICKUP SELECTED ITEM", ImVec2(-1.0f, 34.0f), false, !canPickup))
        {
            actions.PickupPressed = true;
            actions.PickupWorldObjectId = state.SelectedWorldItemId;
        }
    }

    void DrawGameScreen(
        GameScreenState& state,
        GameScreenActions& actions
    )
    {
        actions = GameScreenActions{};

        ImGuiViewport* viewport = ImGui::GetMainViewport();

        const ImVec2 leftPanelPos(
            viewport->Pos.x + 22.0f,
            viewport->Pos.y + 22.0f
        );

        const ImVec2 leftPanelSize(
            350.0f,
            330.0f
        );

        if (BeginStalkerPanel(
            "##PlayerStatusPanel",
            "PLAYER STATUS",
            leftPanelPos,
            leftPanelSize
        ))
        {
            DrawStatBar("Health", state.Player.Health, state.Player.MaxHealth, U32(126, 42, 35, 235));
            DrawStatBar("Stamina", state.Player.Stamina, state.Player.MaxStamina, U32(71, 126, 58, 235));

            ImGui::Dummy(ImVec2(1.0f, 4.0f));

            ImGui::TextColored(ColorFromBytes(194, 180, 130), "AccountId: %d", state.Player.AccountId);
            ImGui::TextColored(ColorFromBytes(194, 180, 130), "CharacterId: %d", state.Player.CharacterId);
            ImGui::TextColored(ColorFromBytes(194, 180, 130), "Nearby players: %d", state.NearbyPlayerCount);

            ImGui::Separator();

            ImGui::TextColored(ColorFromBytes(194, 180, 130), "Hunger: %.0f", state.Player.Hunger);
            ImGui::TextColored(ColorFromBytes(194, 180, 130), "Thirst: %.0f", state.Player.Thirst);
            ImGui::TextColored(ColorFromBytes(194, 180, 130), "Radiation: %.0f", state.Player.Radiation);
            ImGui::TextColored(ColorFromBytes(194, 180, 130), "Toxicity: %.0f", state.Player.Toxicity);

            ImGui::Separator();

            ImGui::TextColored(
                ColorFromBytes(133, 127, 106),
                "Position: %.1f / %.1f / %.1f",
                state.Player.PosX,
                state.Player.PosY,
                state.Player.PosZ
            );
        }

        EndStalkerPanel();

        const ImVec2 worldPanelPos(
            viewport->Pos.x + 22.0f,
            viewport->Pos.y + 372.0f
        );

        const ImVec2 worldPanelSize(
            470.0f,
            355.0f
        );

        if (BeginStalkerPanel(
            "##WorldItemsPanel",
            "VISIBLE WORLD ITEMS",
            worldPanelPos,
            worldPanelSize
        ))
        {
            DrawWorldItemsList(state, actions);
        }

        EndStalkerPanel();

        const ImVec2 inventoryPanelSize(
            430.0f,
            420.0f
        );

        const ImVec2 inventoryPanelPos(
            viewport->Pos.x + viewport->Size.x - inventoryPanelSize.x - 22.0f,
            viewport->Pos.y + 22.0f
        );

        if (BeginStalkerPanel(
            "##InventoryPanel",
            "PLAYER INVENTORY",
            inventoryPanelPos,
            inventoryPanelSize
        ))
        {
            if (!state.Inventory.Valid)
            {
                ImGui::TextColored(ColorFromBytes(170, 95, 74), "Inventory not loaded.");
            }
            else
            {
                ImGui::TextColored(
                    ColorFromBytes(194, 180, 130),
                    "CharacterId: %d",
                    state.Inventory.CharacterId
                );

                ImGui::TextColored(
                    ColorFromBytes(194, 180, 130),
                    "Items: %d / %d",
                    static_cast<int>(state.Inventory.Items.size()),
                    state.Inventory.Capacity
                );

                ImGui::TextColored(
                    ColorFromBytes(194, 180, 130),
                    "Total weight: %.2f",
                    state.Inventory.TotalWeight
                );

                ImGui::Dummy(ImVec2(1.0f, 6.0f));

                DrawInventoryGrid(state, actions);
            }
        }

        EndStalkerPanel();

        const ImVec2 controlsPanelSize(
            430.0f,
            235.0f
        );

        const ImVec2 controlsPanelPos(
            viewport->Pos.x + viewport->Size.x - controlsPanelSize.x - 22.0f,
            viewport->Pos.y + 462.0f
        );

        if (BeginStalkerPanel(
            "##ControlsPanel",
            "CLIENT CONTROLS",
            controlsPanelPos,
            controlsPanelSize
        ))
        {
            ImGui::TextColored(ColorFromBytes(151, 143, 113), "Movement: W/A/S/D | Rotate: Q/E | Camera: C | F pickup | G drop");

            ImGui::Dummy(ImVec2(1.0f, 6.0f));

            if (StalkerButton("W / MOVE UP", ImVec2(126.0f, 34.0f)))
                actions.MoveUpPressed = true;

            ImGui::SameLine();

            if (StalkerButton("S / MOVE DOWN", ImVec2(126.0f, 34.0f)))
                actions.MoveDownPressed = true;

            ImGui::SameLine();

            if (StalkerButton("A / LEFT", ImVec2(126.0f, 34.0f)))
                actions.MoveLeftPressed = true;

            if (StalkerButton("D / RIGHT", ImVec2(126.0f, 34.0f)))
                actions.MoveRightPressed = true;

            ImGui::SameLine();

            if (StalkerButton("Q / ROT -15", ImVec2(126.0f, 34.0f)))
                actions.RotateLeftPressed = true;

            ImGui::SameLine();

            if (StalkerButton("E / ROT +15", ImVec2(126.0f, 34.0f)))
                actions.RotateRightPressed = true;

            ImGui::Dummy(ImVec2(1.0f, 8.0f));

            ImGui::TextWrapped("%s", state.StatusText);

            ImGui::Dummy(ImVec2(1.0f, 6.0f));

            if (StalkerButton("DISCONNECT", ImVec2(-1.0f, 34.0f), true))
                actions.DisconnectPressed = true;
        }

        EndStalkerPanel();
    }
}
