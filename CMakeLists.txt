cmake_minimum_required(VERSION 3.20)

project(T8ReplayUnlimitedTakeoverTimer
    VERSION     1.0.0
    LANGUAGES   CXX
    DESCRIPTION "Tekken 8: remove the 10 s limit on the replay Takeover")

if(NOT WIN32)
    message(FATAL_ERROR "Tekken 8 runs on Windows only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Configure for x64:  cmake -B build -A x64")
endif()

set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# Static CRT: the plugin drops into the game folder with no VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
endif()

# ---------------------------------------------------------------------------
# Two builds of the same patch, for the two ways of getting a DLL into T8:
#
#   T8ReplayUnlimitedTakeoverTimer.asi  - for anyone already running the
#                                         Ultimate ASI Loader. Exports nothing.
#   dinput8.dll                         - standalone, no loader needed. Adds
#                                         dinput8_proxy.cpp, which forwards the
#                                         six real DirectInput exports on to
#                                         System32 so the game still gets them.
#
# They are MUTUALLY EXCLUSIVE and only one thing can own the name dinput8.dll:
# the ASI loader itself installs under it, as do opendojo and Enable Debug.
# Install the .asi if any of those is present.
# ---------------------------------------------------------------------------
add_library(takeover SHARED takeover.cpp)
set_target_properties(takeover PROPERTIES
    OUTPUT_NAME "T8ReplayUnlimitedTakeoverTimer"
    SUFFIX      ".asi")

add_library(takeover_dinput8 SHARED takeover.cpp dinput8_proxy.cpp)
target_compile_definitions(takeover_dinput8 PRIVATE T8T_DINPUT8_PROXY)
set_target_properties(takeover_dinput8 PROPERTIES
    OUTPUT_NAME "dinput8"
    PREFIX      ""
    SUFFIX      ".dll")

# ---------------------------------------------------------------------------
# Deploy. The .asi ONLY — copying dinput8.dll into the game folder would
# overwrite whatever owns that slot (on this machine, the ASI loader), so that
# one is left in build/<config>/ for you to install by hand.
#
# EVERY BUILD OVERWRITES THE INSTALLED .asi — a plain compile check replaces
# what the game loads. Configure with -DT8T_DEPLOY_DIR="" to skip it.
# ---------------------------------------------------------------------------
set(T8T_DEPLOY_DIR
    "C:/Program Files (x86)/Steam/steamapps/common/TEKKEN 8/Polaris/Binaries/Win64/plugins"
    CACHE PATH "Ultimate ASI Loader plugins dir to copy the built .asi into after each build")

if(T8T_DEPLOY_DIR)
    get_filename_component(T8T_DEPLOY_PARENT "${T8T_DEPLOY_DIR}" DIRECTORY)
    if(EXISTS "${T8T_DEPLOY_PARENT}")
        add_custom_command(TARGET takeover POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${T8T_DEPLOY_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:takeover>"
                    "${T8T_DEPLOY_DIR}/T8ReplayUnlimitedTakeoverTimer.asi"
            COMMENT "Deploying T8ReplayUnlimitedTakeoverTimer.asi to ${T8T_DEPLOY_DIR}")
    else()
        message(WARNING "T8T_DEPLOY_DIR parent does not exist, skipping deploy: ${T8T_DEPLOY_DIR}")
    endif()
endif()

# ---------------------------------------------------------------------------
# Self-check for the pattern decoder and scanner (not built by default):
#   cmake --build build --config Release --target selftest
#   ./build/Release/selftest.exe
# ---------------------------------------------------------------------------
add_executable(selftest EXCLUDE_FROM_ALL selftest.cpp)
target_compile_definitions(selftest PRIVATE _CRT_SECURE_NO_WARNINGS)
