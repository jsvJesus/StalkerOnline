cmake_minimum_required(VERSION 3.17)

project(StalkerOnlineGameClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")

add_executable(StalkerOnline.GameClient WIN32
    src/main.cpp

    src/Packet.h

    src/NetworkClient.h
    src/NetworkClient.cpp

    src/UI/UiStyle.h
    src/UI/UiStyle.cpp

    src/Engine/Renderer/Dx11Renderer.h
    src/Engine/Renderer/Dx11Renderer.cpp
    src/Engine/Renderer/Dx11DebugTriangle.h
    src/Engine/Renderer/Dx11DebugTriangle.cpp

    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp

    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

target_include_directories(StalkerOnline.GameClient PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_compile_definitions(StalkerOnline.GameClient PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
)

target_link_libraries(StalkerOnline.GameClient PRIVATE
    ws2_32
    d3d11
    dxgi
    d3dcompiler
)

target_compile_features(StalkerOnline.GameClient PRIVATE cxx_std_20)

if (MSVC)
    target_compile_options(StalkerOnline.GameClient PRIVATE /W4 /permissive-)
endif()