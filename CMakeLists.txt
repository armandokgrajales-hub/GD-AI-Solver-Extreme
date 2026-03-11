cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─────────────────────────────────────────────────────────
#  CMP0175 — suppress Geode SDK's internal add_custom_command
#  warning that fires on CMake 3.31+ runners (ubuntu-latest).
#  Must be set BEFORE any add_subdirectory call.
# ─────────────────────────────────────────────────────────
if(POLICY CMP0175)
    cmake_policy(SET CMP0175 OLD)
endif()

# ─────────────────────────────────────────────────────────
#  Project declaration
#  Note: hyphens are valid in project names but we expose an
#  internal alias without them for target properties below.
# ─────────────────────────────────────────────────────────
project(GD-AI-Solver-Extreme VERSION 1.0.0)

# ─────────────────────────────────────────────────────────
#  Locate Geode SDK
#
#  Priority (highest → lowest):
#    1. CMake cache variable  -DGEODE_SDK=<path>  (set by build-geode-mod action)
#    2. Environment variable  GEODE_SDK            (local dev / manual CI)
#
#  The geode-sdk/build-geode-mod GitHub Action passes the SDK path as a
#  CMake variable (-DGEODE_SDK), NOT as an environment variable, so we
#  must check the cmake variable FIRST and avoid overwriting it with CACHE.
# ─────────────────────────────────────────────────────────
if(NOT DEFINED GEODE_SDK)
    if(DEFINED ENV{GEODE_SDK})
        set(GEODE_SDK "$ENV{GEODE_SDK}")
        message(STATUS "[GD-AI] GEODE_SDK from environment: ${GEODE_SDK}")
    else()
        message(FATAL_ERROR
            "[GD-AI] GEODE_SDK is not set.\n"
            "  • CI:    handled automatically by geode-sdk/build-geode-mod action.\n"
            "  • Local: export GEODE_SDK=/path/to/geode   (Linux/macOS)\n"
            "           set    GEODE_SDK=C:\\path\\to\\geode (Windows)\n"
            "           or pass -DGEODE_SDK=<path> to cmake."
        )
    endif()
else()
    message(STATUS "[GD-AI] GEODE_SDK from CMake variable: ${GEODE_SDK}")
endif()

# ─────────────────────────────────────────────────────────
#  Collect source files
#  GLOB_RECURSE is fine here; CI always does a clean configure.
# ─────────────────────────────────────────────────────────
file(GLOB_RECURSE MOD_SOURCES
    src/*.cpp
    src/hooks/*.cpp
    src/utils/*.cpp
)

# ─────────────────────────────────────────────────────────
#  Shared library target
# ─────────────────────────────────────────────────────────
add_library(${PROJECT_NAME} SHARED ${MOD_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# ─────────────────────────────────────────────────────────
#  Geode SDK — MUST be added before platform flags so that
#  Geode's toolchain definitions are already in scope.
# ─────────────────────────────────────────────────────────
add_subdirectory(${GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode-sdk)

# ─────────────────────────────────────────────────────────
#  Link against Geode (provides cocos2d, fmt, etc.)
# ─────────────────────────────────────────────────────────
target_link_libraries(${PROJECT_NAME} geode-sdk)

# ─────────────────────────────────────────────────────────
#  Platform-specific compile flags
#  Applied AFTER add_subdirectory so Geode's toolchain vars
#  (ANDROID, WIN32, etc.) are correctly populated.
# ─────────────────────────────────────────────────────────
if(ANDROID)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -O2
        -fvisibility=hidden      # minimise symbol leakage on Android .so
        -Wno-unused-result
    )
    # NOTE: -fno-exceptions / -fno-rtti removed — Geode itself uses exceptions
    # and RTTI internally; disabling them causes link errors with geode-sdk.
    target_link_libraries(${PROJECT_NAME} PRIVATE log)  # Android system log

elseif(WIN32)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /O2
        /W3
        /wd4996   # deprecation warnings
        /wd4244   # narrowing conversion (common in GD bindings)
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        UNICODE _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX               # avoid Windows.h min/max macro collisions
    )
endif()

# ─────────────────────────────────────────────────────────
#  Geode packaging — copies mod.json, resources, signs the .geode
# ─────────────────────────────────────────────────────────
setup_geode_mod(${PROJECT_NAME})

