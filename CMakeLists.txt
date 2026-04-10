cmake_minimum_required(VERSION 3.21)

project(CapScriptPro
    VERSION 2.0.0
    DESCRIPTION "YouTube Transcript Search Tool - C++ Qt Edition"
    LANGUAGES CXX
)

if(NOT WIN32)
    message(FATAL_ERROR "CapScriptPro is now Windows-only.")
endif()


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Concurrent
    Svg
)


find_package(Qt6 QUIET COMPONENTS Multimedia MultimediaWidgets)
if(Qt6Multimedia_FOUND AND Qt6MultimediaWidgets_FOUND)
    message(STATUS "Qt6 Multimedia found: video playback enabled")
    add_compile_definitions(CAPSCRIPT_MULTIMEDIA)
else()
    message(STATUS "Qt6 Multimedia NOT found: video playback disabled")
endif()


find_package(Qt6 OPTIONAL_COMPONENTS Quick Qml QuickWidgets WebView WebViewQuick)


set(CAPSCRIPT_WEBVIEW2_AVAILABLE OFF)
set(CAPSCRIPT_WEBVIEW2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2/sdk/build/native/include")
if(WIN32 AND EXISTS "${CAPSCRIPT_WEBVIEW2_INCLUDE_DIR}/WebView2.h")
    set(CAPSCRIPT_WEBVIEW2_AVAILABLE ON)
    add_compile_definitions(CAPSCRIPT_WEBVIEW2)
endif()




set(CAPSCRIPT_WEBVIEW_BACKEND_AVAILABLE OFF)
if(QT_FEATURE_webview_webview2_plugin OR
    QT_FEATURE_webview_winrt_plugin OR
   CAPSCRIPT_WEBVIEW2_AVAILABLE)
    set(CAPSCRIPT_WEBVIEW_BACKEND_AVAILABLE ON)
endif()

if(Qt6WebView_FOUND)
    message(STATUS "Qt6 WebView found")
endif()

if(CAPSCRIPT_WEBVIEW_BACKEND_AVAILABLE)
    message(STATUS "WebView2 backend available")
    add_compile_definitions(CAPSCRIPT_WEBVIEW)
    message(STATUS "CAPSCRIPT_WEBVIEW enabled")
else()
    message(STATUS "WebView2 backend NOT available")
    message(STATUS "CAPSCRIPT_WEBVIEW disabled (browser fallback)")
endif()


find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
message(STATUS "Python3 include dirs: ${Python3_INCLUDE_DIRS}")
message(STATUS "Python3 libraries:    ${Python3_LIBRARIES}")
message(STATUS "Python3 version:      ${Python3_VERSION}")


set(SOURCES

    src/main.cpp


    src/app/Application.cpp


    src/core/PythonBridge.cpp
    src/core/Settings.cpp
    src/core/AutoUpdater.cpp
    src/core/UrlLauncher.cpp


    src/workers/SearchWorker.cpp
    src/workers/ClipWorker.cpp
    src/workers/RenderWorker.cpp


    src/ui/MainWindow.cpp
    src/ui/TitleBar.cpp
    src/ui/Sidebar.cpp


    src/ui/pages/SearchPage.cpp
    src/ui/pages/ViewerPage.cpp
    src/ui/pages/ClipDownloaderPage.cpp
    src/ui/pages/RendererPage.cpp
    src/ui/pages/ListCreatorPage.cpp
    src/ui/pages/AboutPage.cpp


    src/ui/widgets/DonateButton.cpp
    src/ui/widgets/GitHubButton.cpp
    src/ui/widgets/FeedbackWidget.cpp
    src/ui/widgets/SupportPopup.cpp


    src/ui/widgets/WebView2Widget.cpp


    src/ui/styles/ThemeManager.cpp
)

set(HEADERS
    src/app/Application.h
    src/core/PythonBridge.h
    src/core/Settings.h
    src/core/AutoUpdater.h
    src/core/ToolPaths.h
    src/core/UrlLauncher.h
    src/workers/SearchWorker.h
    src/workers/ClipWorker.h
    src/workers/RenderWorker.h
    src/ui/MainWindow.h
    src/ui/TitleBar.h
    src/ui/Sidebar.h
    src/ui/pages/SearchPage.h
    src/ui/pages/ViewerPage.h
    src/ui/pages/ClipDownloaderPage.h
    src/ui/pages/RendererPage.h
    src/ui/pages/ListCreatorPage.h
    src/ui/pages/AboutPage.h
    src/ui/widgets/DonateButton.h
    src/ui/widgets/GitHubButton.h
    src/ui/widgets/FeedbackWidget.h
    src/ui/widgets/WebView2Widget.h
    src/ui/styles/ThemeManager.h
)


set(RESOURCES
    assets/resources.qrc
)


set(WINDOWS_RC CapScriptPro.rc)
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${RESOURCES} ${WINDOWS_RC})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${Python3_INCLUDE_DIRS}
)

if(CAPSCRIPT_WEBVIEW2_AVAILABLE)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CAPSCRIPT_WEBVIEW2_INCLUDE_DIR}
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    Qt6::Svg
    ${Python3_LIBRARIES}
)

if(Qt6Multimedia_FOUND AND Qt6MultimediaWidgets_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Multimedia
        Qt6::MultimediaWidgets
    )
endif()

if(Qt6WebView_FOUND AND Qt6QuickWidgets_FOUND AND Qt6WebViewQuick_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickWidgets
        Qt6::WebView
        Qt6::WebViewQuick
    )
endif()


target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    APP_NAME="${PROJECT_NAME}"
    ORG_NAME="serptail"
    UPDATE_REPO_OWNER="serptail"
    UPDATE_REPO_NAME="CapScript-Youtube-Subtitle-Search-Tool"
)

add_executable(CapScriptUpdater WIN32
    src/updater/main.cpp
    CapScriptUpdater.rc
)

target_link_libraries(CapScriptUpdater PRIVATE
    Qt6::Core
    Qt6::Gui
)

set_target_properties(CapScriptUpdater PROPERTIES
    OUTPUT_NAME "CapScriptUpdater"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:CapScriptUpdater>"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_NAME:CapScriptUpdater>"
    COMMENT "Copying updater bootstrapper next to app binary"
)


target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)
target_link_libraries(${PROJECT_NAME} PRIVATE dwmapi)
if(CAPSCRIPT_WEBVIEW2_AVAILABLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE ole32)
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /utf-8)
endif()

if(WIN32 AND CAPSCRIPT_WEBVIEW2_AVAILABLE)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_WV2_ARCH x64)
    else()
        set(_WV2_ARCH x86)
    endif()

    set(_WV2_LOADER_DLL "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2/sdk/build/native/${_WV2_ARCH}/WebView2Loader.dll")
    if(EXISTS "${_WV2_LOADER_DLL}")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_WV2_LOADER_DLL}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/WebView2Loader.dll"
            COMMENT "Copying WebView2Loader.dll"
        )
    endif()
endif()


add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/python"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/python"
    COMMENT "Copying Python engine scripts to output directory"
)


add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/scripts/bundle_python.py" "${Python3_VERSION}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/python" "${CMAKE_CURRENT_SOURCE_DIR}/python/requirements.txt"
    COMMENT "Bundling Python ${Python3_VERSION} environment into output/python..."
)




if(WIN32 AND Qt6WebView_FOUND AND Qt6QuickWidgets_FOUND AND Qt6WebViewQuick_FOUND)

    get_filename_component(_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
    set(_qt_bin_dir "${_qt_prefix}/bin")
    set(_qml_import_path "${_qt_prefix}/qml")

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${_qml_import_path}/QtWebView"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/qml/QtWebView"
        COMMENT "Copying QtWebView QML module"
    )


    find_program(WINDEPLOYQT_EXECUTABLE windeployqt PATHS "${_qt_bin_dir}")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/assets/qml" "$<TARGET_FILE:${PROJECT_NAME}>"
            COMMENT "Running windeployqt to copy required Qt plugins..."
        )
    endif()
endif()




