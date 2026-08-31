cmake_minimum_required(VERSION 3.21)
project(VideoPlayerApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# app.rc carries the executable icon and version info; RC is not part of the
# project() language list, so it has to be turned on explicitly.
if(WIN32)
    enable_language(RC)
endif()

# --- Locate Qt6 -------------------------------------------------------------
# Helps CMAKE_PREFIX_PATH along by looking in the usual Qt online-installer
# location so a plain `cmake -S . -B build` works out of the box; an
# explicitly supplied -DCMAKE_PREFIX_PATH always takes precedence.
if(WIN32)
    file(GLOB _qt_auto_prefix_candidates
        "C:/Qt/*/mingw_64"
        "C:/Qt/*/msvc*_64"
    )
    if(_qt_auto_prefix_candidates)
        list(APPEND CMAKE_PREFIX_PATH ${_qt_auto_prefix_candidates})
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL Network)

# --- Locate libmpv -----------------------------------------------------------
# Expects the "mpv-dev" package (headers + import lib + runtime DLL) as
# distributed by https://github.com/shinchiro/mpv-winbuild-cmake/releases,
# vendored under third_party/mpv. Override MPV_ROOT to point elsewhere.
set(MPV_ROOT "${CMAKE_SOURCE_DIR}/third_party/mpv" CACHE PATH "Root of the libmpv dev package (headers + import lib + DLL)")

set(MPV_INCLUDE_DIR "${MPV_ROOT}/include")
find_library(MPV_LIBRARY
    NAMES mpv libmpv libmpv.dll.a
    PATHS "${MPV_ROOT}"
    NO_DEFAULT_PATH
)
file(GLOB MPV_RUNTIME_DLL "${MPV_ROOT}/libmpv-2.dll" "${MPV_ROOT}/mpv-2.dll" "${MPV_ROOT}/mpv-1.dll")

if(NOT EXISTS "${MPV_INCLUDE_DIR}/mpv/client.h")
    message(FATAL_ERROR "libmpv headers not found at ${MPV_INCLUDE_DIR}/mpv. Set MPV_ROOT to the mpv-dev package root.")
endif()
if(NOT MPV_LIBRARY)
    message(FATAL_ERROR "libmpv import library not found under ${MPV_ROOT}. Set MPV_ROOT to the mpv-dev package root.")
endif()
if(NOT MPV_RUNTIME_DLL)
    message(WARNING "libmpv runtime DLL not found under ${MPV_ROOT}; the built executable will fail to launch until it is placed next to it.")
endif()

# --- Target ------------------------------------------------------------------
add_executable(VideoPlayerApp WIN32
    main.cpp
    Theme.h
    Theme.cpp
    Translation.h
    Translation.cpp
    MainWindow.h
    MainWindow.cpp
    MpvPlayer.h
    MpvPlayer.cpp
    VideoWidget.h
    VideoWidget.cpp
    TimelineSlider.h
    TimelineSlider.cpp
    VolumeSlider.h
    VolumeSlider.cpp
    IconButton.h
    IconButton.cpp
    TitleBar.h
    TitleBar.cpp
    SettingsDialog.h
    SettingsDialog.cpp
    SettingsKeys.h
    PlaylistWidget.h
    PlaylistWidget.cpp
    TaskbarProgress.h
    TaskbarProgress.cpp
    Downloader.h
    Downloader.cpp
    AboutDialog.h
    AboutDialog.cpp
    SingleInstance.h
    SingleInstance.cpp
    MediaInfoDialog.h
    MediaInfoDialog.cpp
    Equalizer.h
    Equalizer.cpp
    VideoSettings.h
    VideoSettings.cpp
    FileMemory.h
    FileMemory.cpp
    EqualizerDialog.h
    EqualizerDialog.cpp
    SubtitleStyle.h
    SubtitleStyle.cpp
    app.qrc
    $<$<BOOL:${WIN32}>:${CMAKE_CURRENT_SOURCE_DIR}/app.rc>
)

# Produced binary is PearPlayer.exe; the CMake target itself keeps its
# original name so every other reference in this file stays unchanged.
set_target_properties(VideoPlayerApp PROPERTIES OUTPUT_NAME "PearPlayer")

# The source dir is on the include path so app.rc resolves "resources/..."
# under both windres and rc.exe, which take their search paths from here.
target_include_directories(VideoPlayerApp PRIVATE
    "${MPV_INCLUDE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(VideoPlayerApp PRIVATE
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::OpenGL
    Qt6::Network
    "${MPV_LIBRARY}"
    opengl32
    ole32
    uuid
)

# Language files live beside the executable, where Translation.cpp looks for
# them, and where a user can drop another one in without touching the install.
#
# Its own always-run target rather than a POST_BUILD step on the app: a
# POST_BUILD only fires when the executable is relinked, so editing a .json
# alone left a stale copy next to the exe -- the app then ran against the old
# file and the new strings looked untranslated.
add_custom_target(copy_translations ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/translations" "$<TARGET_FILE_DIR:VideoPlayerApp>/translations"
    COMMENT "Copying translations"
)
add_dependencies(VideoPlayerApp copy_translations)

# Ship the mpv runtime DLL next to the executable so the app runs without
# extra PATH setup.
if(MPV_RUNTIME_DLL)
    add_custom_command(TARGET VideoPlayerApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MPV_RUNTIME_DLL}" "$<TARGET_FILE_DIR:VideoPlayerApp>"
    )
endif()

# Qt6 needs its own DLLs (Core/Gui/Widgets/OpenGLWidgets...) and platform
# plugin next to the executable too; windeployqt handles that in one shot.
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
if(WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET VideoPlayerApp POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations "$<TARGET_FILE:VideoPlayerApp>"
    )
endif()
