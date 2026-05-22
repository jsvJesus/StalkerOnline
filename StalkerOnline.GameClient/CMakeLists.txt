cmake_minimum_required(VERSION 3.17)

project(StalkerOnlineGameClient LANGUAGES CXX)

add_executable(StalkerOnline.GameClient WIN32
    src/main.cpp
    src/Packet.h
    src/NetworkClient.h
    src/NetworkClient.cpp
)

target_compile_features(StalkerOnline.GameClient PRIVATE cxx_std_20)

target_compile_definitions(StalkerOnline.GameClient PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

target_link_libraries(StalkerOnline.GameClient PRIVATE
    ws2_32
)

if (MSVC)
    target_compile_options(StalkerOnline.GameClient PRIVATE /W4 /permissive-)
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT StalkerOnline.GameClient)