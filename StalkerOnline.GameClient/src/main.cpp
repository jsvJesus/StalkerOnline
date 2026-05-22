#include "NetworkClient.h"

#include <windows.h>

#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
    constexpr int WindowWidth = 920;
    constexpr int WindowHeight = 620;

    constexpr int IdHostEdit = 1001;
    constexpr int IdPortEdit = 1002;
    constexpr int IdLoginEdit = 1003;
    constexpr int IdEmailEdit = 1004;
    constexpr int IdPasswordEdit = 1005;

    constexpr int IdLoginButton = 2001;
    constexpr int IdRegisterButton = 2002;
    constexpr int IdDisconnectButton = 2003;

    constexpr int IdLogEdit = 3001;

    constexpr UINT WmAppendLog = WM_APP + 1;
    constexpr UINT WmSetControlsEnabled = WM_APP + 2;

    HWND g_mainWindow = nullptr;

    HWND g_hostEdit = nullptr;
    HWND g_portEdit = nullptr;
    HWND g_loginEdit = nullptr;
    HWND g_emailEdit = nullptr;
    HWND g_passwordEdit = nullptr;
    HWND g_logEdit = nullptr;

    std::unique_ptr<NetworkClient> g_client;

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

    void SetControlsEnabledDirect(bool enabled)
    {
        EnableWindow(g_hostEdit, enabled);
        EnableWindow(g_portEdit, enabled);
        EnableWindow(g_loginEdit, enabled);
        EnableWindow(g_emailEdit, enabled);
        EnableWindow(g_passwordEdit, enabled);

        EnableWindow(GetDlgItem(g_mainWindow, IdLoginButton), enabled);
        EnableWindow(GetDlgItem(g_mainWindow, IdRegisterButton), enabled);
    }

    void PostSetControlsEnabled(bool enabled)
    {
        PostMessageW(
            g_mainWindow,
            WmSetControlsEnabled,
            static_cast<WPARAM>(enabled ? 1 : 0),
            0);
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

        PostSetControlsEnabled(false);

        std::thread([host, port, login, password]()
        {
            PostLog(L"[CLIENT] Connecting...");

            if (!g_client->Connect(host, port))
            {
                PostLog(L"[CLIENT] Connect failed.");
                PostSetControlsEnabled(true);
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
                PostSetControlsEnabled(true);
                return;
            }

            g_client->StartReceiveLoop();

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

        PostSetControlsEnabled(false);

        std::thread([host, port, login, email, password]()
        {
            PostLog(L"[CLIENT] Connecting...");

            if (!g_client->Connect(host, port))
            {
                PostLog(L"[CLIENT] Connect failed.");
                PostSetControlsEnabled(true);
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

            PostSetControlsEnabled(true);

        }).detach();
    }

    void RunDisconnect()
    {
        if (g_client)
            g_client->Stop();

        PostSetControlsEnabled(true);
        PostLog(L"[CLIENT] Disconnected.");
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

        CreateLabel(hwnd, L"Login:", 20, 65, 90, 24);
        g_loginEdit = CreateEdit(hwnd, IdLoginEdit, L"", 120, 63, 250, 26);

        CreateLabel(hwnd, L"Email:", 20, 105, 90, 24);
        g_emailEdit = CreateEdit(hwnd, IdEmailEdit, L"", 120, 103, 250, 26);

        CreateLabel(hwnd, L"Password:", 20, 145, 90, 24);
        g_passwordEdit = CreateEdit(hwnd, IdPasswordEdit, L"", 120, 143, 250, 26, true);

        CreateButton(hwnd, IdLoginButton, L"Login", 20, 190, 110, 32);
        CreateButton(hwnd, IdRegisterButton, L"Register", 145, 190, 110, 32);
        CreateButton(hwnd, IdDisconnectButton, L"Disconnect", 270, 190, 120, 32);

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
            20,
            245,
            860,
            320,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<intptr_t>(IdLogEdit)),
            GetModuleHandleW(nullptr),
            nullptr);

        ApplyFontToChildren(hwnd);

        AppendLogDirect(L"Stalker Online Game Client started.");
        AppendLogDirect(L"Real C++ client window. Login/Register ready.");
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

            case WmSetControlsEnabled:
            {
                bool enabled = wParam != 0;
                SetControlsEnabledDirect(enabled);
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