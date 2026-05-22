#include "NetworkClient.h"
#include "UI/UiStyle.h"

#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

#include <windows.h>
#include <d3d11.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <fstream>
#include <sstream>
#include <ws2tcpip.h>

namespace
{
    constexpr int WindowWidth = 1280;
    constexpr int WindowHeight = 800;

    constexpr const char* DefaultServerHost = "26.163.92.76";
    constexpr uint16_t DefaultServerPort = 7777;

    ID3D11Device* g_d3dDevice = nullptr;
    ID3D11DeviceContext* g_d3dDeviceContext = nullptr;
    IDXGISwapChain* g_swapChain = nullptr;
    ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

    std::unique_ptr<NetworkClient> g_client;

    StalkerOnline::UI::LoginScreenState g_loginState;
    StalkerOnline::UI::GameScreenState g_gameScreenState;

    std::atomic_bool g_running = true;
    std::atomic_bool g_authenticated = false;
    std::atomic_bool g_busy = false;
    std::atomic_bool g_connected = false;

    std::atomic_bool g_serverStatusKnown = false;
    std::atomic_bool g_serverOnline = false;
    std::atomic_bool g_serverStatusChecking = false;

    std::chrono::steady_clock::time_point g_nextServerStatusCheckTime{};
    std::string g_lastCheckedServerHost;
    uint16_t g_lastCheckedServerPort = 0;

    constexpr const char* RememberLoginFileName = "client_remember_login.ini";

    std::mutex g_statusMutex;
    std::string g_statusText = "Disconnected";

    float g_rotationZ = 0.0f;

    std::string WideToUtf8(const std::wstring& value)
    {
        if (value.empty())
            return {};

        const int requiredSize = WideCharToMultiByte(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0,
            nullptr,
            nullptr
        );

        if (requiredSize <= 0)
            return {};

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
            nullptr
        );

        return result;
    }

    void SetStatus(const std::string& text)
    {
        std::lock_guard<std::mutex> lock(g_statusMutex);
        g_statusText = text;
    }

    void CopyToBuffer(char* destination, size_t destinationSize, const std::string& value)
    {
        if (!destination || destinationSize == 0)
            return;

        std::snprintf(destination, destinationSize, "%s", value.c_str());
    }

    uint16_t GetServerPortFromLoginState()
    {
        if (g_loginState.ServerPort < 1)
            return 7777;

        if (g_loginState.ServerPort > 65535)
            return 7777;

        return static_cast<uint16_t>(g_loginState.ServerPort);
    }

    void LoadRememberLogin()
    {
        std::ifstream file(RememberLoginFileName);

        if (!file.is_open())
            return;

        std::string line;
        bool remember = false;
        std::string host;
        std::string login;
        int port = 7777;

        while (std::getline(file, line))
        {
            const size_t separator = line.find('=');

            if (separator == std::string::npos)
                continue;

            const std::string key = line.substr(0, separator);
            const std::string value = line.substr(separator + 1);

            if (key == "remember")
            {
                remember = value == "1";
            }
            else if (key == "host")
            {
                host = value;
            }
            else if (key == "port")
            {
                try
                {
                    port = std::stoi(value);
                }
                catch (...)
                {
                    port = 7777;
                }
            }
            else if (key == "login")
            {
                login = value;
            }
        }

        if (!remember)
            return;

        g_loginState.RememberLogin = true;

        if (!host.empty())
            CopyToBuffer(g_loginState.ServerHost, sizeof(g_loginState.ServerHost), host);

        if (port >= 1 && port <= 65535)
            g_loginState.ServerPort = port;

        if (!login.empty())
            CopyToBuffer(g_loginState.Login, sizeof(g_loginState.Login), login);
    }

    void SaveRememberLogin(
        const std::string& host,
        uint16_t port,
        const std::string& login,
        bool remember
    )
    {
        if (!remember)
        {
            DeleteFileA(RememberLoginFileName);
            return;
        }

        std::ofstream file(RememberLoginFileName, std::ios::trunc);

        if (!file.is_open())
            return;

        file << "remember=1\n";
        file << "host=" << host << "\n";
        file << "port=" << port << "\n";
        file << "login=" << login << "\n";
    }

    bool CheckTcpServerOnline(
        const std::string& host,
        uint16_t port,
        int timeoutMs
    )
    {
        if (host.empty())
            return false;

        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        addrinfo* result = nullptr;
        const std::string portText = std::to_string(port);

        const int getAddrResult = getaddrinfo(
            host.c_str(),
            portText.c_str(),
            &hints,
            &result
        );

        if (getAddrResult != 0 || result == nullptr)
            return false;

        bool online = false;

        for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next)
        {
            SOCKET testSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

            if (testSocket == INVALID_SOCKET)
                continue;

            u_long nonBlocking = 1;
            ioctlsocket(testSocket, FIONBIO, &nonBlocking);

            const int connectResult = connect(
                testSocket,
                ptr->ai_addr,
                static_cast<int>(ptr->ai_addrlen)
            );

            if (connectResult == 0)
            {
                online = true;
                closesocket(testSocket);
                break;
            }

            const int error = WSAGetLastError();

            if (error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == WSAEINVAL)
            {
                fd_set writeSet;
                FD_ZERO(&writeSet);
                FD_SET(testSocket, &writeSet);

                fd_set errorSet;
                FD_ZERO(&errorSet);
                FD_SET(testSocket, &errorSet);

                timeval timeout{};
                timeout.tv_sec = timeoutMs / 1000;
                timeout.tv_usec = (timeoutMs % 1000) * 1000;

                const int selectResult = select(
                    0,
                    nullptr,
                    &writeSet,
                    &errorSet,
                    &timeout
                );

                if (selectResult > 0 && FD_ISSET(testSocket, &writeSet))
                {
                    int socketError = 0;
                    int socketErrorSize = sizeof(socketError);

                    getsockopt(
                        testSocket,
                        SOL_SOCKET,
                        SO_ERROR,
                        reinterpret_cast<char*>(&socketError),
                        &socketErrorSize
                    );

                    online = socketError == 0;
                }
            }

            closesocket(testSocket);

            if (online)
                break;
        }

        freeaddrinfo(result);

        return online;
    }

    void MaybeStartServerStatusCheck()
    {
        if (g_authenticated.load())
            return;

        if (g_serverStatusChecking.load())
            return;

        const std::string host = g_loginState.ServerHost;
        const uint16_t port = GetServerPortFromLoginState();

        if (host.empty())
        {
            g_serverStatusKnown.store(true);
            g_serverOnline.store(false);
            return;
        }

        const auto now = std::chrono::steady_clock::now();

        const bool serverChanged =
            host != g_lastCheckedServerHost ||
            port != g_lastCheckedServerPort;

        if (!serverChanged && now < g_nextServerStatusCheckTime)
            return;

        g_lastCheckedServerHost = host;
        g_lastCheckedServerPort = port;
        g_nextServerStatusCheckTime = now + std::chrono::seconds(5);

        g_serverStatusChecking.store(true);

        std::thread([host, port]()
        {
            const bool online = CheckTcpServerOnline(host, port, 800);

            g_serverOnline.store(online);
            g_serverStatusKnown.store(true);
            g_serverStatusChecking.store(false);

            if (!g_authenticated.load())
            {
                if (online)
                    SetStatus("Server online: " + host + ":" + std::to_string(port));
                else
                    SetStatus("Server offline: " + host + ":" + std::to_string(port));
            }
        }).detach();
    }

    uint16_t GetServerPortFromLoginState()
    {
        if (g_loginState.ServerPort < 1)
            return 7777;

        if (g_loginState.ServerPort > 65535)
            return 7777;

        return static_cast<uint16_t>(g_loginState.ServerPort);
    }

    void SyncUiStatus()
    {
        std::lock_guard<std::mutex> lock(g_statusMutex);

        std::snprintf(
            g_loginState.StatusText,
            sizeof(g_loginState.StatusText),
            "%s",
            g_statusText.c_str()
        );

        std::snprintf(
            g_gameScreenState.StatusText,
            sizeof(g_gameScreenState.StatusText),
            "%s",
            g_statusText.c_str()
        );

        g_loginState.IsConnected = g_connected.load();
        g_loginState.IsBusy = g_busy.load();

        g_loginState.ServerStatusKnown = g_serverStatusKnown.load();
        g_loginState.ServerOnline = g_serverOnline.load();
        g_loginState.IsCheckingServer = g_serverStatusChecking.load();
    }

    void CreateRenderTarget()
    {
        ID3D11Texture2D* backBuffer = nullptr;

        g_swapChain->GetBuffer(
            0,
            IID_PPV_ARGS(&backBuffer)
        );

        if (backBuffer)
        {
            g_d3dDevice->CreateRenderTargetView(
                backBuffer,
                nullptr,
                &g_mainRenderTargetView
            );

            backBuffer->Release();
        }
    }

    void CleanupRenderTarget()
    {
        if (g_mainRenderTargetView)
        {
            g_mainRenderTargetView->Release();
            g_mainRenderTargetView = nullptr;
        }
    }

    bool CreateDeviceD3D(HWND hwnd)
    {
        DXGI_SWAP_CHAIN_DESC swapChainDesc{};
        swapChainDesc.BufferCount = 2;
        swapChainDesc.BufferDesc.Width = 0;
        swapChainDesc.BufferDesc.Height = 0;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
        swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
        swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow = hwnd;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Windowed = TRUE;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT createDeviceFlags = 0;

        D3D_FEATURE_LEVEL featureLevel;
        const D3D_FEATURE_LEVEL featureLevelArray[2] =
        {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_0
        };

        HRESULT result = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createDeviceFlags,
            featureLevelArray,
            2,
            D3D11_SDK_VERSION,
            &swapChainDesc,
            &g_swapChain,
            &g_d3dDevice,
            &featureLevel,
            &g_d3dDeviceContext
        );

        if (FAILED(result))
            return false;

        CreateRenderTarget();
        return true;
    }

    void CleanupDeviceD3D()
    {
        CleanupRenderTarget();

        if (g_swapChain)
        {
            g_swapChain->Release();
            g_swapChain = nullptr;
        }

        if (g_d3dDeviceContext)
        {
            g_d3dDeviceContext->Release();
            g_d3dDeviceContext = nullptr;
        }

        if (g_d3dDevice)
        {
            g_d3dDevice->Release();
            g_d3dDevice = nullptr;
        }
    }

    void UpdateGameScreenStateFromNetwork()
    {
        if (!g_client)
            return;

        const bool connected = g_client->IsConnected();
        g_connected.store(connected);

        if (!connected && g_authenticated.load())
        {
            g_authenticated.store(false);
            SetStatus("Disconnected");
        }

        PlayerSnapshot player = g_client->GetPlayerSnapshot();

        if (player.Valid)
        {
            g_gameScreenState.Player.AccountId = player.AccountId;
            g_gameScreenState.Player.CharacterId = player.CharacterId;

            g_gameScreenState.Player.Health = player.Health;
            g_gameScreenState.Player.MaxHealth = player.MaxHealth;

            g_gameScreenState.Player.Stamina = player.Stamina;
            g_gameScreenState.Player.MaxStamina = player.MaxStamina;

            g_gameScreenState.Player.Hunger = player.Hunger;
            g_gameScreenState.Player.Thirst = player.Thirst;
            g_gameScreenState.Player.Radiation = player.Radiation;
            g_gameScreenState.Player.Toxicity = player.Toxicity;

            g_gameScreenState.Player.PosX = player.PositionX;
            g_gameScreenState.Player.PosY = player.PositionY;
            g_gameScreenState.Player.PosZ = player.PositionZ;

            g_rotationZ = player.RotationZ;
        }

        InventorySnapshotView inventory = g_client->GetInventorySnapshot();

        g_gameScreenState.Inventory.Valid = inventory.Valid;
        g_gameScreenState.Inventory.CharacterId = inventory.CharacterId;
        g_gameScreenState.Inventory.Capacity = inventory.Capacity;
        g_gameScreenState.Inventory.TotalWeight = inventory.TotalWeight;
        g_gameScreenState.Inventory.Items.clear();

        for (const InventoryItemView& item : inventory.Items)
        {
            StalkerOnline::UI::InventoryItemUi uiItem;
            uiItem.SlotIndex = item.SlotIndex;
            uiItem.ItemTemplateId = item.ItemTemplateId;
            uiItem.DisplayName = item.DisplayName;
            uiItem.Quantity = item.Quantity;
            uiItem.MaxStack = item.MaxStack;
            uiItem.WeightPerItem = item.WeightPerItem;

            g_gameScreenState.Inventory.Items.push_back(std::move(uiItem));
        }

        std::vector<WorldItemView> worldItems = g_client->GetWorldItemsSnapshot();

        g_gameScreenState.WorldItems.clear();

        for (const WorldItemView& item : worldItems)
        {
            StalkerOnline::UI::WorldItemUi uiItem;
            uiItem.WorldObjectId = item.WorldObjectId;
            uiItem.ItemTemplateId = item.ItemTemplateId;
            uiItem.DisplayName = item.DisplayName;
            uiItem.Quantity = item.Quantity;

            uiItem.PositionX = item.PositionX;
            uiItem.PositionY = item.PositionY;
            uiItem.PositionZ = item.PositionZ;

            uiItem.RotationX = item.RotationX;
            uiItem.RotationY = item.RotationY;
            uiItem.RotationZ = item.RotationZ;

            g_gameScreenState.WorldItems.push_back(std::move(uiItem));
        }
    }

    void RunLogin()
    {
        if (g_busy.load())
            return;

        const std::string host = g_loginState.ServerHost;
        const uint16_t port = GetServerPortFromLoginState();

        const std::string login = g_loginState.Login;
        const std::string password = g_loginState.Password;

        if (host.empty())
        {
            SetStatus("Server IP is empty");
            return;
        }

        if (login.empty())
        {
            SetStatus("Login is empty");
            return;
        }

        if (password.empty())
        {
            SetStatus("Password is empty");
            return;
        }

        g_busy.store(true);
        SetStatus("Connecting...");

        std::thread([host, port, login, password, rememberLogin]()
        {
            if (!g_client)
            {
                g_busy.store(false);
                SetStatus("Network client is not created");
                return;
            }

            if (!g_client->IsConnected())
            {
                if (!g_client->Connect(host, port))
                {
                    g_connected.store(false);
                    g_serverStatusKnown.store(true);
                    g_serverOnline.store(false);
                    g_busy.store(false);
                    SetStatus("Connect failed. Server offline: " + host + ":" + std::to_string(port));
                    return;
                }
            }

            g_connected.store(true);
            g_serverStatusKnown.store(true);
            g_serverOnline.store(true);
            SetStatus("Sending login request...");

            LoginResult result = g_client->Login(login, password);

            if (!result.Success)
            {
                g_authenticated.store(false);
                g_busy.store(false);
                SetStatus("Login failed: " + result.Message);
                g_client->Stop();
                g_connected.store(false);
                return;
            }

            g_authenticated.store(true);
            g_busy.store(false);

            SetStatus("Login success: " + result.Message);

            g_client->StartReceiveLoop();
            SaveRememberLogin(host, port, login, rememberLogin);
        }).detach();
    }

    void RunRegister()
    {
        if (g_busy.load())
            return;

        const std::string host = g_loginState.ServerHost;
        const uint16_t port = GetServerPortFromLoginState();

        const std::string login = g_loginState.Login;
        const std::string email = g_loginState.Email;
        const std::string password = g_loginState.Password;
        const bool rememberLogin = g_loginState.RememberLogin;

        if (host.empty())
        {
            SetStatus("Server IP is empty");
            return;
        }

        if (host.empty())
        {
            SetStatus("Server IP is empty");
            return;
        }

        if (login.empty())
        {
            SetStatus("Login is empty");
            return;
        }

        if (email.empty())
        {
            SetStatus("Email is empty");
            return;
        }

        if (password.empty())
        {
            SetStatus("Password is empty");
            return;
        }

        g_busy.store(true);
        SetStatus("Connecting...");

        std::thread([host, port, login, email, password, rememberLogin]()
        {
            if (!g_client)
            {
                g_busy.store(false);
                SetStatus("Network client is not created");
                return;
            }

            if (!g_client->IsConnected())
            {
                if (!g_client->Connect(host, port))
                {
                    g_connected.store(false);
                    g_serverStatusKnown.store(true);
                    g_serverOnline.store(false);
                    g_busy.store(false);
                    SetStatus("Connect failed. Server offline: " + host + ":" + std::to_string(port));
                    return;
                }
            }

            g_connected.store(true);
            g_serverStatusKnown.store(true);
            g_serverOnline.store(true);
            SetStatus("Sending register request...");

            RegisterResult result = g_client->Register(login, email, password);

            g_busy.store(false);

            if (!result.Success)
            {
                SetStatus("Register failed: " + result.Message);
                g_client->Stop();
                g_connected.store(false);
                return;
            }

            SetStatus("Register success. Account created: " + result.Login);
            SaveRememberLogin(host, port, login, rememberLogin);
        }).detach();
    }

    void RunDisconnect()
    {
        if (g_client)
            g_client->Stop();

        g_authenticated.store(false);
        g_connected.store(false);
        g_busy.store(false);

        SetStatus("Disconnected");
    }

    void SendMove(float directionX, float directionY)
    {
        if (!g_client || !g_client->IsConnected())
        {
            SetStatus("Move failed: not connected");
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
            SetStatus("Rotate failed: not connected");
            return;
        }

        g_client->SendMoveRequest(0.0f, 0.0f, g_rotationZ);
    }

    void PickupWorldItem(int32_t worldObjectId)
    {
        if (worldObjectId <= 0)
        {
            SetStatus("Pickup failed: invalid world item");
            return;
        }

        if (!g_client || !g_client->IsConnected())
        {
            SetStatus("Pickup failed: not connected");
            return;
        }

        g_client->SendPickupItemRequest(worldObjectId);

        SetStatus("Pickup request sent. WorldObjectId=" + std::to_string(worldObjectId));
    }

    void HandleKeyboardGameInput()
    {
        if (!g_authenticated.load())
            return;

        ImGuiIO& io = ImGui::GetIO();

        if (io.WantCaptureKeyboard)
            return;

        if (ImGui::IsKeyPressed(ImGuiKey_W))
            SendMove(0.0f, 1.0f);

        if (ImGui::IsKeyPressed(ImGuiKey_S))
            SendMove(0.0f, -1.0f);

        if (ImGui::IsKeyPressed(ImGuiKey_A))
            SendMove(-1.0f, 0.0f);

        if (ImGui::IsKeyPressed(ImGuiKey_D))
            SendMove(1.0f, 0.0f);

        if (ImGui::IsKeyPressed(ImGuiKey_Q))
            RotatePlayer(-15.0f);

        if (ImGui::IsKeyPressed(ImGuiKey_E))
            RotatePlayer(15.0f);

        if (ImGui::IsKeyPressed(ImGuiKey_F))
            PickupWorldItem(g_gameScreenState.SelectedWorldItemId);
    }

    void HandleGameScreenActions(const StalkerOnline::UI::GameScreenActions& actions)
    {
        if (actions.MoveUpPressed)
            SendMove(0.0f, 1.0f);

        if (actions.MoveDownPressed)
            SendMove(0.0f, -1.0f);

        if (actions.MoveLeftPressed)
            SendMove(-1.0f, 0.0f);

        if (actions.MoveRightPressed)
            SendMove(1.0f, 0.0f);

        if (actions.RotateLeftPressed)
            RotatePlayer(-15.0f);

        if (actions.RotateRightPressed)
            RotatePlayer(15.0f);

        if (actions.PickupPressed)
            PickupWorldItem(actions.PickupWorldObjectId);

        if (actions.DisconnectPressed)
            RunDisconnect();
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
);

LRESULT WINAPI WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam))
        return true;

    switch (message)
    {
        case WM_SIZE:
        {
            if (g_d3dDevice != nullptr && wParam != SIZE_MINIMIZED)
            {
                CleanupRenderTarget();

                g_swapChain->ResizeBuffers(
                    0,
                    static_cast<UINT>(LOWORD(lParam)),
                    static_cast<UINT>(HIWORD(lParam)),
                    DXGI_FORMAT_UNKNOWN,
                    0
                );

                CreateRenderTarget();
            }

            return 0;
        }

        case WM_SYSCOMMAND:
        {
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;

            break;
        }

        case WM_DESTROY:
        {
            g_running.store(false);
            PostQuitMessage(0);
            return 0;
        }

        default:
            break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    PWSTR,
    int nCmdShow
)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.style = CS_CLASSDC;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = hInstance;
    windowClass.hIcon = nullptr;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszMenuName = nullptr;
    windowClass.lpszClassName = L"StalkerOnlineGameClientWindow";
    windowClass.hIconSm = nullptr;

    RegisterClassExW(&windowClass);

    HWND hwnd = CreateWindowW(
        windowClass.lpszClassName,
        L"Stalker Online - Game Client",
        WS_OVERLAPPEDWINDOW,
        100,
        100,
        WindowWidth,
        WindowHeight,
        nullptr,
        nullptr,
        windowClass.hInstance,
        nullptr
    );

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    StalkerOnline::UI::ApplyStalkerDarkStyle();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_d3dDevice, g_d3dDeviceContext);

    LoadRememberLogin();
    g_client = std::make_unique<NetworkClient>(
        [](const std::wstring& text)
        {
            SetStatus(WideToUtf8(text));
        },
        []()
        {
        }
    );

    SetStatus(
        "Ready. Server: " +
        std::string(g_loginState.ServerHost) +
        ":" +
        std::to_string(g_loginState.ServerPort)
    );

    MSG message{};

    while (g_running.load())
    {
        while (PeekMessageW(&message, nullptr, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);

            if (message.message == WM_QUIT)
                g_running.store(false);
        }

        if (!g_running.load())
            break;

        MaybeStartServerStatusCheck();
        UpdateGameScreenStateFromNetwork();
        SyncUiStatus();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        StalkerOnline::UI::DrawZoneBackground(static_cast<float>(ImGui::GetTime()));

        bool loginPressed = false;
        bool registerPressed = false;
        bool exitPressed = false;

        if (!g_authenticated.load())
        {
            StalkerOnline::UI::DrawLoginRegisterScreen(
                g_loginState,
                loginPressed,
                registerPressed,
                exitPressed
            );

            if (loginPressed)
                RunLogin();

            if (registerPressed)
                RunRegister();

            if (exitPressed)
                g_running.store(false);
        }
        else
        {
            HandleKeyboardGameInput();

            StalkerOnline::UI::GameScreenActions actions;

            StalkerOnline::UI::DrawGameScreen(
                g_gameScreenState,
                actions
            );

            HandleGameScreenActions(actions);
        }

        ImGui::Render();

        const float clearColor[4] = { 0.015f, 0.017f, 0.014f, 1.0f };

        g_d3dDeviceContext->OMSetRenderTargets(
            1,
            &g_mainRenderTargetView,
            nullptr
        );

        g_d3dDeviceContext->ClearRenderTargetView(
            g_mainRenderTargetView,
            clearColor
        );

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_swapChain->Present(1, 0);
    }

    if (g_client)
    {
        g_client->Stop();
        g_client.reset();
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();

    DestroyWindow(hwnd);
    UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);

    return 0;
}