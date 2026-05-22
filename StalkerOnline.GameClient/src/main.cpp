#include "NetworkClient.h"
#include "UI/UiStyle.h"
#include <windows.h>

#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
    constexpr int WindowWidth = 1120;
    constexpr int WindowHeight = 760;

    constexpr int IdHostEdit = 1001;
    constexpr int IdPortEdit = 1002;
    constexpr int IdLoginEdit = 1003;
    constexpr int IdEmailEdit = 1004;
    constexpr int IdPasswordEdit = 1005;

    constexpr int IdLoginButton = 2001;
    constexpr int IdRegisterButton = 2002;
    constexpr int IdDisconnectButton = 2003;

    constexpr int IdPlayerInfo = 3001;
    constexpr int IdInventoryList = 3002;
    constexpr int IdWorldItemsList = 3003;
    constexpr int IdLogEdit = 3004;

    constexpr int IdMoveUpButton = 4001;
    constexpr int IdMoveDownButton = 4002;
    constexpr int IdMoveLeftButton = 4003;
    constexpr int IdMoveRightButton = 4004;
    constexpr int IdRotateLeftButton = 4005;
    constexpr int IdRotateRightButton = 4006;
    constexpr int IdPickupButton = 4007;

    constexpr UINT WmAppendLog = WM_APP + 1;
    constexpr UINT WmSetAuthControlsEnabled = WM_APP + 2;
    constexpr UINT WmRefreshGameUi = WM_APP + 3;

    HWND g_mainWindow = nullptr;

    HWND g_hostEdit = nullptr;
    HWND g_portEdit = nullptr;
    HWND g_loginEdit = nullptr;
    HWND g_emailEdit = nullptr;
    HWND g_passwordEdit = nullptr;

    HWND g_playerInfo = nullptr;
    HWND g_inventoryList = nullptr;
    HWND g_worldItemsList = nullptr;
    HWND g_logEdit = nullptr;

    std::unique_ptr<NetworkClient> g_client;

    float g_rotationZ = 0.0f;

    std::string WideToUtf8(const std::wstring& value)
    {
        if (value.empty())
            return "";

        int requiredSize = WideCharToMultiByte(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0,
            nullptr,
            nullptr);

        if (requiredSize <= 0)
            return "";

        std::string result;
        result.resize(static_cast<size_t>(requiredSize));

        WideCharToMultiByte(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            requiredSize,
            nullptr,
            nullptr);

        return result;
    }

    std::wstring Utf8ToWide(const std::string& value)
    {
        if (value.empty())
            return L"";

        int requiredSize = MultiByteToWideChar(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0);

        if (requiredSize <= 0)
            return L"";

        std::wstring result;
        result.resize(static_cast<size_t>(requiredSize));

        MultiByteToWideChar(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            requiredSize);

        return result;
    }

    std::wstring GetWindowTextValue(HWND hwnd)
    {
        int length = GetWindowTextLengthW(hwnd);

        if (length <= 0)
            return L"";

        std::vector<wchar_t> buffer;
        buffer.resize(static_cast<size_t>(length) + 1);

        GetWindowTextW(hwnd, buffer.data(), length + 1);

        return std::wstring(buffer.data());
    }

    uint16_t ReadPort()
    {
        std::wstring portText = GetWindowTextValue(g_portEdit);

        try
        {
            int port = std::stoi(portText);

            if (port <= 0 || port > 65535)
                return 7777;

            return static_cast<uint16_t>(port);
        }
        catch (...)
        {
            return 7777;
        }
    }

    void AppendLogDirect(const std::wstring& text)
    {
        if (!g_logEdit)
            return;

        int length = GetWindowTextLengthW(g_logEdit);

        SendMessageW(
            g_logEdit,
            EM_SETSEL,
            static_cast<WPARAM>(length),
            static_cast<LPARAM>(length));

        std::wstring line = text + L"\r\n";

        SendMessageW(
            g_logEdit,
            EM_REPLACESEL,
            FALSE,
            reinterpret_cast<LPARAM>(line.c_str()));
    }

    void PostLog(const std::wstring& text)
    {
        if (!g_mainWindow)
            return;

        auto* heapText = new std::wstring(text);

        PostMessageW(
            g_mainWindow,
            WmAppendLog,
            0,
            reinterpret_cast<LPARAM>(heapText));
    }

    void PostRefreshGameUi()
    {
        if (!g_mainWindow)
            return;

        PostMessageW(g_mainWindow, WmRefreshGameUi, 0, 0);
    }

    void SetAuthControlsEnabledDirect(bool enabled)
    {
        EnableWindow(g_hostEdit, enabled);
        EnableWindow(g_portEdit, enabled);
        EnableWindow(g_loginEdit, enabled);
        EnableWindow(g_emailEdit, enabled);
        EnableWindow(g_passwordEdit, enabled);

        EnableWindow(GetDlgItem(g_mainWindow, IdLoginButton), enabled);
        EnableWindow(GetDlgItem(g_mainWindow, IdRegisterButton), enabled);
    }

    void SetGameControlsEnabledDirect(bool enabled)
    {
        EnableWindow(GetDlgItem(g_mainWindow, IdMoveUpButton), enabled);
        EnableWindow(GetDlgItem(g_mainWindow, IdMoveDownButton), enabled);
        EnableWindow(GetDlgItem(g_mainWindow, IdMoveLeftButton), enabled);
        EnableWindow(GetDlgItem(g_mainWindow, IdMoveRightButton), enabled);
        EnableWindow(GetDlgItem(g_mainWindow, IdRotateLeftButton), enabled);
        EnableWindow(GetDlgItem(g_mainWindow, IdRotateRightButton), enabled);
        EnableWindow(GetDlgItem(g_mainWindow, IdPickupButton), enabled);
        EnableWindow(g_worldItemsList, enabled);
    }

    void PostSetAuthControlsEnabled(bool enabled)
    {
        PostMessageW(
            g_mainWindow,
            WmSetAuthControlsEnabled,
            static_cast<WPARAM>(enabled ? 1 : 0),
            0);
    }

    void RefreshPlayerUi()
    {
        PlayerSnapshot player = g_client->GetPlayerSnapshot();

        std::wstringstream ss;

        if (!player.Valid)
        {
            ss << L"Player: not loaded";
        }
        else
        {
            ss
                << L"Player: " << Utf8ToWide(player.Nickname)
                << L" | CharacterId: " << player.CharacterId
                << L" | Pos: X=" << player.PositionX
                << L" Y=" << player.PositionY
                << L" Z=" << player.PositionZ
                << L" | RotZ=" << player.RotationZ
                << L"\r\n"
                << L"HP: " << player.Health << L"/" << player.MaxHealth
                << L" | Stamina: " << player.Stamina << L"/" << player.MaxStamina
                << L" | Hunger: " << player.Hunger
                << L" | Thirst: " << player.Thirst
                << L" | Rad: " << player.Radiation
                << L" | Toxic: " << player.Toxicity
                << L" | Alive: " << (player.IsAlive ? L"true" : L"false");

            g_rotationZ = player.RotationZ;
        }

        SetWindowTextW(g_playerInfo, ss.str().c_str());
    }

    void RefreshInventoryUi()
    {
        InventorySnapshotView inventory = g_client->GetInventorySnapshot();

        SendMessageW(g_inventoryList, LB_RESETCONTENT, 0, 0);

        if (!inventory.Valid)
        {
            SendMessageW(g_inventoryList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Inventory not loaded"));
            return;
        }

        std::wstringstream header;
        header
            << L"Items: " << inventory.Items.size()
            << L"/" << inventory.Capacity
            << L" | Weight: " << inventory.TotalWeight;

        SendMessageW(g_inventoryList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(header.str().c_str()));
        SendMessageW(g_inventoryList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"--------------------------------"));

        for (const InventoryItemView& item : inventory.Items)
        {
            std::wstringstream ss;
            ss
                << L"Slot " << item.SlotIndex
                << L": " << Utf8ToWide(item.DisplayName)
                << L" [" << Utf8ToWide(item.ItemTemplateId) << L"]"
                << L" x" << item.Quantity << L"/" << item.MaxStack
                << L" | W=" << item.WeightPerItem;

            SendMessageW(g_inventoryList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(ss.str().c_str()));
        }
    }

    void RefreshWorldItemsUi()
    {
        std::vector<WorldItemView> items = g_client->GetWorldItemsSnapshot();

        SendMessageW(g_worldItemsList, LB_RESETCONTENT, 0, 0);

        if (items.empty())
        {
            SendMessageW(g_worldItemsList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"No visible world items"));
            SendMessageW(g_worldItemsList, LB_SETITEMDATA, 0, static_cast<LPARAM>(0));
            return;
        }

        for (const WorldItemView& item : items)
        {
            std::wstringstream ss;
            ss
                << L"Id " << item.WorldObjectId
                << L": " << Utf8ToWide(item.DisplayName)
                << L" x" << item.Quantity
                << L" | Pos=(" << item.PositionX
                << L", " << item.PositionY
                << L", " << item.PositionZ << L")";

            LRESULT index = SendMessageW(
                g_worldItemsList,
                LB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(ss.str().c_str()));

            if (index != LB_ERR && index != LB_ERRSPACE)
            {
                SendMessageW(
                    g_worldItemsList,
                    LB_SETITEMDATA,
                    static_cast<WPARAM>(index),
                    static_cast<LPARAM>(item.WorldObjectId));
            }
        }

        SendMessageW(g_worldItemsList, LB_SETCURSEL, 0, 0);
    }

    void RefreshGameUi()
    {
        if (!g_client)
            return;

        RefreshPlayerUi();
        RefreshInventoryUi();
        RefreshWorldItemsUi();

        bool connected = g_client->IsConnected();
        SetGameControlsEnabledDirect(connected);
    }

    void RunLogin()
    {
        std::wstring hostWide = GetWindowTextValue(g_hostEdit);
        std::wstring loginWide = GetWindowTextValue(g_loginEdit);
        std::wstring passwordWide = GetWindowTextValue(g_passwordEdit);

        std::string host = WideToUtf8(hostWide);
        std::string login = WideToUtf8(loginWide);
        std::string password = WideToUtf8(passwordWide);

        uint16_t port = ReadPort();

        if (host.empty())
        {
            PostLog(L"[UI] Host is empty.");
            return;
        }

        if (login.empty())
        {
            PostLog(L"[UI] Login is empty.");
            return;
        }

        if (password.empty())
        {
            PostLog(L"[UI] Password is empty.");
            return;
        }

        PostSetAuthControlsEnabled(false);

        std::thread([host, port, login, password]()
        {
            PostLog(L"[CLIENT] Connecting...");

            if (!g_client->Connect(host, port))
            {
                PostLog(L"[CLIENT] Connect failed.");
                PostSetAuthControlsEnabled(true);
                PostRefreshGameUi();
                return;
            }

            LoginResult result = g_client->Login(login, password);

            PostLog(
                L"[LOGIN] Success=" +
                std::wstring(result.Success ? L"true" : L"false") +
                L", Message=" +
                Utf8ToWide(result.Message));

            if (!result.Success)
            {
                PostSetAuthControlsEnabled(true);
                PostRefreshGameUi();
                return;
            }

            g_client->StartReceiveLoop();
            PostRefreshGameUi();

        }).detach();
    }

    void RunRegister()
    {
        std::wstring hostWide = GetWindowTextValue(g_hostEdit);
        std::wstring loginWide = GetWindowTextValue(g_loginEdit);
        std::wstring emailWide = GetWindowTextValue(g_emailEdit);
        std::wstring passwordWide = GetWindowTextValue(g_passwordEdit);

        std::string host = WideToUtf8(hostWide);
        std::string login = WideToUtf8(loginWide);
        std::string email = WideToUtf8(emailWide);
        std::string password = WideToUtf8(passwordWide);

        uint16_t port = ReadPort();

        if (host.empty())
        {
            PostLog(L"[UI] Host is empty.");
            return;
        }

        if (login.empty())
        {
            PostLog(L"[UI] Login is empty.");
            return;
        }

        if (email.empty())
        {
            PostLog(L"[UI] Email is empty.");
            return;
        }

        if (password.empty())
        {
            PostLog(L"[UI] Password is empty.");
            return;
        }

        PostSetAuthControlsEnabled(false);

        std::thread([host, port, login, email, password]()
        {
            PostLog(L"[CLIENT] Connecting...");

            if (!g_client->Connect(host, port))
            {
                PostLog(L"[CLIENT] Connect failed.");
                PostSetAuthControlsEnabled(true);
                PostRefreshGameUi();
                return;
            }

            RegisterResult result = g_client->Register(login, email, password);

            std::wstringstream ss;
            ss
                << L"[REGISTER] Success=" << (result.Success ? L"true" : L"false")
                << L", AccountId=" << result.AccountId
                << L", Login=" << Utf8ToWide(result.Login)
                << L", Message=" << Utf8ToWide(result.Message);

            PostLog(ss.str());

            PostSetAuthControlsEnabled(true);
            PostRefreshGameUi();

        }).detach();
    }

    void RunDisconnect()
    {
        if (g_client)
            g_client->Stop();

        PostSetAuthControlsEnabled(true);
        PostLog(L"[CLIENT] Disconnected.");
        PostRefreshGameUi();
    }

    void SendMove(float directionX, float directionY)
    {
        if (!g_client || !g_client->IsConnected())
        {
            PostLog(L"[MOVE] Not connected.");
            return;
        }

        g_client->SendMoveRequest(directionX, directionY, g_rotationZ);
    }

    void RotatePlayer(float delta)
    {
        g_rotationZ += delta;

        if (g_rotationZ > 360.0f)
            g_rotationZ -= 360.0f;

        if (g_rotationZ < -360.0f)
            g_rotationZ += 360.0f;

        if (!g_client || !g_client->IsConnected())
        {
            PostLog(L"[ROTATE] Not connected.");
            return;
        }

        g_client->SendMoveRequest(0.0f, 0.0f, g_rotationZ);
    }

    void PickupSelectedWorldItem()
    {
        if (!g_client || !g_client->IsConnected())
        {
            PostLog(L"[PICKUP] Not connected.");
            return;
        }

        LRESULT selectedIndex = SendMessageW(g_worldItemsList, LB_GETCURSEL, 0, 0);

        if (selectedIndex == LB_ERR)
        {
            PostLog(L"[PICKUP] No selected world item.");
            return;
        }

        LRESULT itemData = SendMessageW(
            g_worldItemsList,
            LB_GETITEMDATA,
            static_cast<WPARAM>(selectedIndex),
            0);

        if (itemData == LB_ERR || itemData <= 0)
        {
            PostLog(L"[PICKUP] Invalid selected item.");
            return;
        }

        int32_t worldObjectId = static_cast<int32_t>(itemData);

        g_client->SendPickupItemRequest(worldObjectId);
    }

    HWND CreateLabel(
        HWND parent,
        const wchar_t* text,
        int x,
        int y,
        int w,
        int h)
    {
        return CreateWindowExW(
            0,
            L"STATIC",
            text,
            WS_CHILD | WS_VISIBLE,
            x,
            y,
            w,
            h,
            parent,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);
    }

    HWND CreateStaticBox(
        HWND parent,
        int id,
        const wchar_t* text,
        int x,
        int y,
        int w,
        int h)
    {
        return CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"STATIC",
            text,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            x,
            y,
            w,
            h,
            parent,
            reinterpret_cast<HMENU>(static_cast<intptr_t>(id)),
            GetModuleHandleW(nullptr),
            nullptr);
    }

    HWND CreateEdit(
        HWND parent,
        int id,
        const wchar_t* text,
        int x,
        int y,
        int w,
        int h,
        bool password = false)
    {
        DWORD style =
            WS_CHILD |
            WS_VISIBLE |
            WS_BORDER |
            ES_AUTOHSCROLL;

        if (password)
            style |= ES_PASSWORD;

        return CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            text,
            style,
            x,
            y,
            w,
            h,
            parent,
            reinterpret_cast<HMENU>(static_cast<intptr_t>(id)),
            GetModuleHandleW(nullptr),
            nullptr);
    }

    HWND CreateButton(
        HWND parent,
        int id,
        const wchar_t* text,
        int x,
        int y,
        int w,
        int h)
    {
        return CreateWindowExW(
            0,
            L"BUTTON",
            text,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x,
            y,
            w,
            h,
            parent,
            reinterpret_cast<HMENU>(static_cast<intptr_t>(id)),
            GetModuleHandleW(nullptr),
            nullptr);
    }

    HWND CreateListBox(
        HWND parent,
        int id,
        int x,
        int y,
        int w,
        int h)
    {
        return CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"LISTBOX",
            L"",
            WS_CHILD |
            WS_VISIBLE |
            WS_BORDER |
            WS_VSCROLL |
            LBS_NOTIFY,
            x,
            y,
            w,
            h,
            parent,
            reinterpret_cast<HMENU>(static_cast<intptr_t>(id)),
            GetModuleHandleW(nullptr),
            nullptr);
    }

    void ApplyDefaultFont(HWND hwnd)
    {
        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }

    void ApplyFontToChildren(HWND parent)
    {
        ApplyDefaultFont(parent);

        HWND child = GetWindow(parent, GW_CHILD);

        while (child)
        {
            ApplyDefaultFont(child);
            child = GetWindow(child, GW_HWNDNEXT);
        }
    }

    void CreateUi(HWND hwnd)
    {
        CreateLabel(hwnd, L"Server IP:", 20, 20, 90, 24);
        g_hostEdit = CreateEdit(hwnd, IdHostEdit, L"26.163.92.76", 120, 18, 180, 26);

        CreateLabel(hwnd, L"Port:", 320, 20, 50, 24);
        g_portEdit = CreateEdit(hwnd, IdPortEdit, L"7777", 370, 18, 80, 26);

        CreateLabel(hwnd, L"Login:", 20, 60, 90, 24);
        g_loginEdit = CreateEdit(hwnd, IdLoginEdit, L"", 120, 58, 250, 26);

        CreateLabel(hwnd, L"Email:", 400, 60, 50, 24);
        g_emailEdit = CreateEdit(hwnd, IdEmailEdit, L"", 460, 58, 250, 26);

        CreateLabel(hwnd, L"Password:", 20, 100, 90, 24);
        g_passwordEdit = CreateEdit(hwnd, IdPasswordEdit, L"", 120, 98, 250, 26, true);

        CreateButton(hwnd, IdLoginButton, L"Login", 390, 96, 110, 32);
        CreateButton(hwnd, IdRegisterButton, L"Register", 515, 96, 110, 32);
        CreateButton(hwnd, IdDisconnectButton, L"Disconnect", 640, 96, 120, 32);

        g_playerInfo = CreateStaticBox(
            hwnd,
            IdPlayerInfo,
            L"Player: not loaded",
            20,
            150,
            1060,
            70);

        CreateLabel(hwnd, L"Visible World Items", 20, 235, 220, 24);
        g_worldItemsList = CreateListBox(hwnd, IdWorldItemsList, 20, 260, 520, 210);

        CreateLabel(hwnd, L"Inventory", 560, 235, 220, 24);
        g_inventoryList = CreateListBox(hwnd, IdInventoryList, 560, 260, 520, 210);

        CreateLabel(hwnd, L"Movement", 20, 490, 150, 24);

        CreateButton(hwnd, IdMoveUpButton, L"W / Up", 125, 490, 90, 32);
        CreateButton(hwnd, IdMoveLeftButton, L"A / Left", 25, 530, 90, 32);
        CreateButton(hwnd, IdMoveDownButton, L"S / Down", 125, 530, 90, 32);
        CreateButton(hwnd, IdMoveRightButton, L"D / Right", 225, 530, 90, 32);

        CreateButton(hwnd, IdRotateLeftButton, L"Rotate -15", 340, 490, 100, 32);
        CreateButton(hwnd, IdRotateRightButton, L"Rotate +15", 450, 490, 100, 32);
        CreateButton(hwnd, IdPickupButton, L"Pickup Selected", 340, 530, 210, 32);

        CreateLabel(hwnd, L"Log", 580, 490, 100, 24);

        g_logEdit = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD |
            WS_VISIBLE |
            WS_BORDER |
            ES_MULTILINE |
            ES_AUTOVSCROLL |
            ES_READONLY |
            WS_VSCROLL,
            580,
            515,
            500,
            170,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<intptr_t>(IdLogEdit)),
            GetModuleHandleW(nullptr),
            nullptr);

        ApplyFontToChildren(hwnd);

        SetGameControlsEnabledDirect(false);

        AppendLogDirect(L"Stalker Online Game Client started.");
        AppendLogDirect(L"Login/Register ready. Game UI enabled after login.");
    }

    LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
    {
        switch (message)
        {
            case WM_CREATE:
            {
                g_mainWindow = hwnd;

                g_client = std::make_unique<NetworkClient>(
                    [](const std::wstring& text)
                    {
                        PostLog(text);
                    },
                    []()
                    {
                        PostRefreshGameUi();
                    });

                CreateUi(hwnd);
                return 0;
            }

            case WM_COMMAND:
            {
                int controlId = LOWORD(wParam);

                switch (controlId)
                {
                    case IdLoginButton:
                        RunLogin();
                        return 0;

                    case IdRegisterButton:
                        RunRegister();
                        return 0;

                    case IdDisconnectButton:
                        RunDisconnect();
                        return 0;

                    case IdMoveUpButton:
                        SendMove(0.0f, 1.0f);
                        return 0;

                    case IdMoveDownButton:
                        SendMove(0.0f, -1.0f);
                        return 0;

                    case IdMoveLeftButton:
                        SendMove(-1.0f, 0.0f);
                        return 0;

                    case IdMoveRightButton:
                        SendMove(1.0f, 0.0f);
                        return 0;

                    case IdRotateLeftButton:
                        RotatePlayer(-15.0f);
                        return 0;

                    case IdRotateRightButton:
                        RotatePlayer(15.0f);
                        return 0;

                    case IdPickupButton:
                        PickupSelectedWorldItem();
                        return 0;

                    default:
                        break;
                }

                break;
            }

            case WM_KEYDOWN:
            {
                switch (wParam)
                {
                    case 'W':
                        SendMove(0.0f, 1.0f);
                        return 0;

                    case 'S':
                        SendMove(0.0f, -1.0f);
                        return 0;

                    case 'A':
                        SendMove(-1.0f, 0.0f);
                        return 0;

                    case 'D':
                        SendMove(1.0f, 0.0f);
                        return 0;

                    case 'Q':
                        RotatePlayer(-15.0f);
                        return 0;

                    case 'E':
                        RotatePlayer(15.0f);
                        return 0;

                    case 'F':
                        PickupSelectedWorldItem();
                        return 0;

                    default:
                        break;
                }

                break;
            }

            case WmAppendLog:
            {
                auto* text = reinterpret_cast<std::wstring*>(lParam);

                if (text)
                {
                    AppendLogDirect(*text);
                    delete text;
                }

                return 0;
            }

            case WmSetAuthControlsEnabled:
            {
                bool enabled = wParam != 0;
                SetAuthControlsEnabledDirect(enabled);
                return 0;
            }

            case WmRefreshGameUi:
            {
                RefreshGameUi();
                return 0;
            }

            case WM_DESTROY:
            {
                if (g_client)
                {
                    g_client->Stop();
                    g_client.reset();
                }

                PostQuitMessage(0);
                return 0;
            }

            default:
                break;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    PWSTR,
    int nCmdShow)
{
    const wchar_t* className = L"StalkerOnlineGameClientWindow";

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.lpszClassName = className;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    RegisterClassW(&windowClass);

    HWND hwnd = CreateWindowExW(
        0,
        className,
        L"Stalker Online - Game Client",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        WindowWidth,
        WindowHeight,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hwnd)
        return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG message{};

    while (GetMessageW(&message, nullptr, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return 0;
}