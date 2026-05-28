#include "NetworkClient.h"
#include "UI/UiStyle.h"
#include "Engine/Renderer/Dx11Renderer.h"
#include "Engine/Renderer/Dx11WorldRenderer.h"
#include "SpeedTree/SpeedTreeIntegration.h"

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
#include <cmath>
#include <ws2tcpip.h>

namespace
{
    constexpr int WindowWidth = 1280;
    constexpr int WindowHeight = 800;

    constexpr const char* DefaultServerHost = "26.163.92.76";
    constexpr uint16_t DefaultServerPort = 7777;

    std::unique_ptr<StalkerOnline::Engine::Dx11Renderer> g_renderer;
    std::unique_ptr<StalkerOnline::Engine::Dx11WorldRenderer> g_worldRenderer;
    StalkerOnline::Engine::GameCameraMode g_cameraMode = StalkerOnline::Engine::GameCameraMode::ThirdPerson;

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
    uint16_t GetServerPortFromLoginState();

    std::mutex g_statusMutex;
    std::string g_statusText = "Disconnected";

    float g_rotationZ = 0.0f;

    constexpr float MouseLookSensitivity = 0.14f;
    constexpr float KeyboardRotationSpeed = 120.0f;
    constexpr float MoveSendIntervalSeconds = 0.05f;

    std::chrono::steady_clock::time_point g_lastMoveSendTime = std::chrono::steady_clock::now();

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

        g_gameScreenState.NearbyPlayerCount =
            static_cast<int32_t>(g_client->GetRemotePlayersSnapshot().size());
    }

    void RunLogin()
    {
        if (g_busy.load())
            return;

        const std::string host = g_loginState.ServerHost;
        const uint16_t port = GetServerPortFromLoginState();

        const std::string login = g_loginState.Login;
        const std::string password = g_loginState.Password;
        const bool rememberLogin = g_loginState.RememberLogin;

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

            SaveRememberLogin(host, port, login, rememberLogin);

            g_client->StartReceiveLoop();
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

            SaveRememberLogin(host, port, login, rememberLogin);

            SetStatus("Register success. Account created: " + result.Login);
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

    float NormalizeRotationZ(float value)
    {
        while (value >= 360.0f)
            value -= 360.0f;

        while (value < 0.0f)
            value += 360.0f;

        return value;
    }

    float DegreesToRadians(float degrees)
    {
        return degrees * 3.14159265358979323846f / 180.0f;
    }

    float ClampMoveDeltaTime(float deltaTime)
    {
        if (deltaTime < 0.016f)
            return 0.016f;

        if (deltaTime > 0.10f)
            return 0.10f;

        return deltaTime;
    }

    void SendMove(float directionX, float directionY, float deltaTime = 0.05f)
    {
        if (!g_client || !g_client->IsConnected())
            return;

        const float length = std::sqrt(directionX * directionX + directionY * directionY);

        if (length > 1.0f)
        {
            directionX /= length;
            directionY /= length;
        }

        g_client->SendMoveRequest(
            directionX,
            directionY,
            g_rotationZ,
            ClampMoveDeltaTime(deltaTime)
        );
    }

    void SendLocalMove(float forwardAmount, float rightAmount, float deltaTime = 0.05f)
    {
        const float yaw = DegreesToRadians(g_rotationZ);

        const float forwardX = std::sin(yaw);
        const float forwardY = std::cos(yaw);

        const float rightX = std::cos(yaw);
        const float rightY = -std::sin(yaw);

        const float worldDirectionX = forwardX * forwardAmount + rightX * rightAmount;
        const float worldDirectionY = forwardY * forwardAmount + rightY * rightAmount;

        SendMove(worldDirectionX, worldDirectionY, deltaTime);
    }

    void SendRotationOnly(float deltaTime = 0.05f)
    {
        if (!g_client || !g_client->IsConnected())
            return;

        g_client->SendMoveRequest(
            0.0f,
            0.0f,
            g_rotationZ,
            ClampMoveDeltaTime(deltaTime)
        );
    }

    void RotatePlayer(float delta)
    {
        g_rotationZ = NormalizeRotationZ(g_rotationZ + delta);
        SendRotationOnly(0.05f);
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

    void DropInventoryItem(int32_t slotIndex, int32_t quantity)
    {
        if (slotIndex < 0)
        {
            SetStatus("Drop failed: invalid inventory slot");
            return;
        }

        if (quantity <= 0)
            quantity = 1;

        if (!g_client || !g_client->IsConnected())
        {
            SetStatus("Drop failed: not connected");
            return;
        }

        g_client->SendDropItemRequest(slotIndex, quantity);

        SetStatus(
            "Drop request sent. Slot=" +
            std::to_string(slotIndex) +
            ", Quantity=" +
            std::to_string(quantity)
        );
    }

    void ToggleCameraMode()
    {
        if (g_cameraMode == StalkerOnline::Engine::GameCameraMode::ThirdPerson)
        {
            g_cameraMode = StalkerOnline::Engine::GameCameraMode::FirstPerson;
            SetStatus("Camera mode: FPS");
        }
        else
        {
            g_cameraMode = StalkerOnline::Engine::GameCameraMode::ThirdPerson;
            SetStatus("Camera mode: TPS");
        }
    }

    void RenderWorld3D()
    {
        if (!g_worldRenderer)
            return;

        if (!g_authenticated.load())
            return;

        StalkerOnline::Engine::WorldRenderPlayer renderPlayer;
        renderPlayer.Valid = g_gameScreenState.Player.CharacterId > 0;

        renderPlayer.PositionX = g_gameScreenState.Player.PosX;
        renderPlayer.PositionY = g_gameScreenState.Player.PosY;
        renderPlayer.PositionZ = g_gameScreenState.Player.PosZ;

        renderPlayer.RotationZ = g_rotationZ;

        std::vector<StalkerOnline::Engine::WorldRenderItem> renderItems;
        renderItems.reserve(g_gameScreenState.WorldItems.size());

        for (const StalkerOnline::UI::WorldItemUi& item : g_gameScreenState.WorldItems)
        {
            StalkerOnline::Engine::WorldRenderItem renderItem;
            renderItem.WorldObjectId = item.WorldObjectId;

            renderItem.PositionX = item.PositionX;
            renderItem.PositionY = item.PositionY;
            renderItem.PositionZ = item.PositionZ;

            renderItem.Size = 0.28f;

            renderItems.push_back(renderItem);
        }

        std::vector<RemotePlayerView> remotePlayers = g_client->GetRemotePlayersSnapshot();

        std::vector<StalkerOnline::Engine::WorldRenderRemotePlayer> renderRemotePlayers;
        renderRemotePlayers.reserve(remotePlayers.size());

        for (const RemotePlayerView& remotePlayer : remotePlayers)
        {
            StalkerOnline::Engine::WorldRenderRemotePlayer renderRemotePlayer;
            renderRemotePlayer.CharacterId = remotePlayer.CharacterId;

            renderRemotePlayer.PositionX = remotePlayer.PositionX;
            renderRemotePlayer.PositionY = remotePlayer.PositionY;
            renderRemotePlayer.PositionZ = remotePlayer.PositionZ;

            renderRemotePlayer.RotationZ = remotePlayer.RotationZ;
            renderRemotePlayer.IsAlive = remotePlayer.IsAlive;

            renderRemotePlayers.push_back(renderRemotePlayer);
        }

        g_worldRenderer->Render(
            g_renderer->GetDeviceContext(),
            g_renderer->GetWidth(),
            g_renderer->GetHeight(),
            renderPlayer,
            renderRemotePlayers,
            renderItems,
            g_cameraMode
        );
    }

    void HandleKeyboardGameInput()
    {
        if (!g_authenticated.load())
            return;

        ImGuiIO& io = ImGui::GetIO();

        bool rotationChanged = false;

        if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            const float mouseDeltaX = io.MouseDelta.x;

            if (std::fabs(mouseDeltaX) > 0.001f)
            {
                g_rotationZ = NormalizeRotationZ(
                    g_rotationZ + mouseDeltaX * MouseLookSensitivity
                );

                rotationChanged = true;
            }
        }

        if (!io.WantCaptureKeyboard)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_C))
                ToggleCameraMode();

            if (ImGui::IsKeyPressed(ImGuiKey_F))
                PickupWorldItem(g_gameScreenState.SelectedWorldItemId);

            if (ImGui::IsKeyPressed(ImGuiKey_G))
                DropInventoryItem(g_gameScreenState.SelectedInventorySlotIndex, 1);

            if (ImGui::IsKeyDown(ImGuiKey_Q))
            {
                g_rotationZ = NormalizeRotationZ(
                    g_rotationZ - KeyboardRotationSpeed * io.DeltaTime
                );

                rotationChanged = true;
            }

            if (ImGui::IsKeyDown(ImGuiKey_E))
            {
                g_rotationZ = NormalizeRotationZ(
                    g_rotationZ + KeyboardRotationSpeed * io.DeltaTime
                );

                rotationChanged = true;
            }
        }

        float forwardAmount = 0.0f;
        float rightAmount = 0.0f;

        if (!io.WantCaptureKeyboard)
        {
            if (ImGui::IsKeyDown(ImGuiKey_W))
                forwardAmount += 1.0f;

            if (ImGui::IsKeyDown(ImGuiKey_S))
                forwardAmount -= 1.0f;

            if (ImGui::IsKeyDown(ImGuiKey_D))
                rightAmount += 1.0f;

            if (ImGui::IsKeyDown(ImGuiKey_A))
                rightAmount -= 1.0f;
        }

        const bool hasMovement =
            std::fabs(forwardAmount) > 0.001f ||
            std::fabs(rightAmount) > 0.001f;

        if (!hasMovement && !rotationChanged)
            return;

        const auto now = std::chrono::steady_clock::now();
        const float elapsedSeconds = std::chrono::duration<float>(now - g_lastMoveSendTime).count();

        if (elapsedSeconds < MoveSendIntervalSeconds)
           return;

        g_lastMoveSendTime = now;

        if (hasMovement)
        {
            SendLocalMove(
                forwardAmount,
                rightAmount,
                elapsedSeconds
            );
        }
        else
        {
            SendRotationOnly(elapsedSeconds);
        }
    }

    void HandleGameScreenActions(const StalkerOnline::UI::GameScreenActions& actions)
    {
        if (actions.MoveUpPressed)
            SendLocalMove(1.0f, 0.0f);

        if (actions.MoveDownPressed)
            SendLocalMove(-1.0f, 0.0f);

        if (actions.MoveLeftPressed)
            SendLocalMove(0.0f, -1.0f);

        if (actions.MoveRightPressed)
            SendLocalMove(0.0f, 1.0f);

        if (actions.RotateLeftPressed)
            RotatePlayer(-15.0f);

        if (actions.RotateRightPressed)
            RotatePlayer(15.0f);

        if (actions.PickupPressed)
            PickupWorldItem(actions.PickupWorldObjectId);

        if (actions.DropPressed)
            DropInventoryItem(actions.DropSlotIndex, actions.DropQuantity);

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
    		if (g_renderer && wParam != SIZE_MINIMIZED)
    		{
       		 	const std::uint32_t width = static_cast<std::uint32_t>(LOWORD(lParam));
        		const std::uint32_t height = static_cast<std::uint32_t>(HIWORD(lParam));

        		g_renderer->Resize(width, height);
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

    g_renderer = std::make_unique<StalkerOnline::Engine::Dx11Renderer>();

	if (!g_renderer->Initialize(hwnd))
	{
    	g_renderer.reset();
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
    ImGui_ImplDX11_Init(
    	g_renderer->GetDevice(),
    	g_renderer->GetDeviceContext()
	);

    StalkerOnline::SpeedTreeIntegration::InitializeDx11Renderer(
        g_renderer->GetDevice(),
        g_renderer->GetDeviceContext()
    );

    g_worldRenderer = std::make_unique<StalkerOnline::Engine::Dx11WorldRenderer>();

    if (!g_worldRenderer->Initialize(g_renderer->GetDevice()))
    {
        MessageBoxW(
            hwnd,
            L"Failed to initialize Dx11WorldRenderer.",
            L"Stalker Online",
            MB_ICONERROR
        );

        g_worldRenderer.reset();

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        if (g_renderer)
        {
            g_renderer->Shutdown();
            g_renderer.reset();
        }

        DestroyWindow(hwnd);
        UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
        return 1;
    }

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

    const StalkerOnline::SpeedTreeIntegration::SdkStatus speedTreeStatus =
        StalkerOnline::SpeedTreeIntegration::GetSdkStatus();

    SetStatus(
        "Ready. Server: " +
        std::string(g_loginState.ServerHost) +
        ":" +
        std::to_string(g_loginState.ServerPort) +
        (speedTreeStatus.Available ? " | SpeedTree SDK ready" : " | SpeedTree SDK missing")
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

        if (!g_authenticated.load())
        {
            StalkerOnline::UI::DrawZoneBackground(static_cast<float>(ImGui::GetTime()));
        }

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

		g_renderer->BeginFrame(clearColor);

        RenderWorld3D();

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_renderer->EndFrame(true);
    }

    if (g_client)
    {
        g_client->Stop();
        g_client.reset();
    }
    
    if (g_worldRenderer)
    {
        g_worldRenderer->Shutdown();
        g_worldRenderer.reset();
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (g_renderer)
	{
    	g_renderer->Shutdown();
    	g_renderer.reset();
	}

    DestroyWindow(hwnd);
    UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);

    return 0;
}
