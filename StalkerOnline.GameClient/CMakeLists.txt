cmake_minimum_required(VERSION 3.17)

project(StalkerOnlineGameClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
set(SPEEDTREE_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SpeedTreeSDK" CACHE PATH "Path to SpeedTree SDK 7.x")
set(STALKERONLINE_HAS_SPEEDTREE_SDK OFF)

if (EXISTS "${SPEEDTREE_SDK_DIR}/Include/Core/Core.h")
    set(STALKERONLINE_HAS_SPEEDTREE_SDK ON)
endif()

set(STALKERONLINE_BUILD_SPEEDTREE_DX11_DEFAULT ${STALKERONLINE_HAS_SPEEDTREE_SDK})
option(STALKERONLINE_BUILD_SPEEDTREE_DX11 "Build the SpeedTree DirectX 11 renderer sources." ${STALKERONLINE_BUILD_SPEEDTREE_DX11_DEFAULT})

add_library(StalkerOnline.SpeedTreeIntegration STATIC
    src/SpeedTree/SpeedTreeIntegration.h
    src/SpeedTree/SpeedTreeIntegration.cpp
)

target_include_directories(StalkerOnline.SpeedTreeIntegration PUBLIC
    src
)

if (STALKERONLINE_HAS_SPEEDTREE_SDK)
    add_library(SpeedTreeCore STATIC
        "${SPEEDTREE_SDK_DIR}/Source/Core/Source/AllocatorStats.cpp"
        "${SPEEDTREE_SDK_DIR}/Source/Core/Source/CoordSys.cpp"
        "${SPEEDTREE_SDK_DIR}/Source/Core/Source/Core.cpp"
        "${SPEEDTREE_SDK_DIR}/Source/Core/Source/ErrorHandler.cpp"
        "${SPEEDTREE_SDK_DIR}/Source/Core/Source/FileSystem.cpp"
        "${SPEEDTREE_SDK_DIR}/Source/Core/Source/Fixed.cpp"
        "${SPEEDTREE_SDK_DIR}/Source/Core/Source/Memory.cpp"
        "${SPEEDTREE_SDK_DIR}/Source/Core/Source/Parser.cpp"
        "${SPEEDTREE_SDK_DIR}/Source/Core/Source/ScopeTrace.cpp"
        "${SPEEDTREE_SDK_DIR}/Source/Core/Source/Wind.cpp"
    )

    target_include_directories(SpeedTreeCore PUBLIC
        "${SPEEDTREE_SDK_DIR}/Include"
    )

    target_include_directories(SpeedTreeCore PRIVATE
        "${SPEEDTREE_SDK_DIR}/Source/Core/Source"
    )

    target_compile_definitions(SpeedTreeCore PUBLIC
        WIN32
        _WINDOWS
    )

    target_compile_definitions(SpeedTreeCore PRIVATE
        ST_EXPOSE_REGISTRY_FUNCTIONS
        ST_EVALUATION_BUILD
    )

    add_library(SpeedTreeForest STATIC
        "${SPEEDTREE_SDK_DIR}/Source/Forest/Source/Culling.cpp"
        "${SPEEDTREE_SDK_DIR}/Source/Forest/Source/Forest.cpp"
        "${SPEEDTREE_SDK_DIR}/Source/Forest/Source/Terrain.cpp"
    )

    target_link_libraries(SpeedTreeForest PUBLIC
        SpeedTreeCore
    )

    target_include_directories(SpeedTreeForest PUBLIC
        "${SPEEDTREE_SDK_DIR}/Include"
    )

    target_compile_definitions(SpeedTreeForest PUBLIC
        WIN32
        _WINDOWS
    )

    target_link_libraries(StalkerOnline.SpeedTreeIntegration PUBLIC
        SpeedTreeCore
        SpeedTreeForest
    )

    target_compile_definitions(StalkerOnline.SpeedTreeIntegration PUBLIC
        STALKERONLINE_WITH_SPEEDTREE=1
    )

    if (STALKERONLINE_BUILD_SPEEDTREE_DX11)
        add_library(SpeedTreeRenderInterface STATIC
            "${SPEEDTREE_SDK_DIR}/Source/Render Interface/Source/RenderInterface.cpp"
        )

        target_include_directories(SpeedTreeRenderInterface PUBLIC
            "${SPEEDTREE_SDK_DIR}/Include"
        )

        target_link_libraries(SpeedTreeRenderInterface PUBLIC
            SpeedTreeCore
            SpeedTreeForest
        )

        target_compile_definitions(SpeedTreeRenderInterface PUBLIC
            WIN32
            _WINDOWS
        )

        add_library(SpeedTreeRendererDX11 STATIC
            "${SPEEDTREE_SDK_DIR}/Source/Renderers/DirectX11/Source/DDSTextureLoader.cpp"
            "${SPEEDTREE_SDK_DIR}/Source/Renderers/DirectX11/Source/DirectX11Renderer.cpp"
            "${SPEEDTREE_SDK_DIR}/Source/Renderers/DirectX11/Source/GeometryBuffer.cpp"
            "${SPEEDTREE_SDK_DIR}/Source/Renderers/DirectX11/Source/RenderTarget.cpp"
            "${SPEEDTREE_SDK_DIR}/Source/Renderers/DirectX11/Source/Shaders.cpp"
            "${SPEEDTREE_SDK_DIR}/Source/Renderers/DirectX11/Source/StateBlock.cpp"
            "${SPEEDTREE_SDK_DIR}/Source/Renderers/DirectX11/Source/Texture.cpp"
        )

        target_include_directories(SpeedTreeRendererDX11 PUBLIC
            "${SPEEDTREE_SDK_DIR}/Include"
            "${SPEEDTREE_SDK_DIR}/Source/Renderers/DirectX11/Source"
        )

        target_link_libraries(SpeedTreeRendererDX11 PUBLIC
            SpeedTreeRenderInterface
            d3d11
            d3dcompiler
            dxguid
        )

        target_compile_definitions(SpeedTreeRendererDX11 PUBLIC
            WIN32
            _WINDOWS
            ST_DIRECTX11
        )

        target_link_libraries(StalkerOnline.SpeedTreeIntegration PUBLIC
            SpeedTreeRendererDX11
        )

        target_compile_definitions(StalkerOnline.SpeedTreeIntegration PUBLIC
            STALKERONLINE_WITH_SPEEDTREE_DX11=1
        )
    endif()

    if (MSVC)
        target_compile_options(StalkerOnline.SpeedTreeIntegration PRIVATE /W0 /wd4996 /wd4005 /Zc:twoPhase-)
        set_target_properties(StalkerOnline.SpeedTreeIntegration PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
        )

        foreach(SPEEDTREE_TARGET SpeedTreeCore SpeedTreeForest)
            target_compile_options(${SPEEDTREE_TARGET} PRIVATE /W0 /wd4996 /wd4005)
            set_target_properties(${SPEEDTREE_TARGET} PROPERTIES
                CXX_STANDARD 14
                CXX_STANDARD_REQUIRED ON
                CXX_EXTENSIONS OFF
            )
        endforeach()

        if (TARGET SpeedTreeRenderInterface)
            target_compile_options(SpeedTreeRenderInterface PRIVATE /W0 /wd4996 /wd4005)
            set_target_properties(SpeedTreeRenderInterface PROPERTIES
                CXX_STANDARD 14
                CXX_STANDARD_REQUIRED ON
                CXX_EXTENSIONS OFF
            )
        endif()

        if (TARGET SpeedTreeRendererDX11)
            target_compile_options(SpeedTreeRendererDX11 PRIVATE /W0 /wd4996 /wd4005)
            set_target_properties(SpeedTreeRendererDX11 PROPERTIES
                CXX_STANDARD 14
                CXX_STANDARD_REQUIRED ON
                CXX_EXTENSIONS OFF
            )
        endif()
    endif()
endif()

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
    src/Engine/Renderer/Dx11WorldRenderer.h
    src/Engine/Renderer/Dx11WorldRenderer.cpp

    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp

    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

add_executable(StalkerOnline.LevelEditor WIN32
    src/Editor/LevelEditor.cpp
    src/Editor/Heightmap.h
    src/Editor/Heightmap.cpp
    src/Editor/Dx11LevelEditorRenderer.h
    src/Editor/Dx11LevelEditorRenderer.cpp

    src/Editor/EditorScene.h
    src/Editor/EditorScene.cpp

    src/Engine/Renderer/Dx11Renderer.h
    src/Engine/Renderer/Dx11Renderer.cpp

    src/UI/UiStyle.h
    src/UI/UiStyle.cpp

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

target_include_directories(StalkerOnline.LevelEditor PRIVATE
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

target_compile_definitions(StalkerOnline.LevelEditor PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
)

target_link_libraries(StalkerOnline.GameClient PRIVATE
    StalkerOnline.SpeedTreeIntegration
    ws2_32
    d3d11
    dxgi
    d3dcompiler
)

target_link_libraries(StalkerOnline.LevelEditor PRIVATE
    StalkerOnline.SpeedTreeIntegration
    d3d11
    dxgi
    d3dcompiler
    comdlg32
)

target_compile_features(StalkerOnline.GameClient PRIVATE cxx_std_20)
target_compile_features(StalkerOnline.LevelEditor PRIVATE cxx_std_20)

if (MSVC)
    target_compile_options(StalkerOnline.GameClient PRIVATE /W4 /permissive-)
    target_compile_options(StalkerOnline.LevelEditor PRIVATE /W4 /permissive-)
endif()
