cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─────────────────────────────────────────────────────────
#  Project declaration
# ─────────────────────────────────────────────────────────
project(GD-AI-Solver-Extreme VERSION 1.0.0)

# ─────────────────────────────────────────────────────────
#  Locate Geode SDK
#  The CI workflow sets GEODE_SDK via the geode-sdk/build-geode-mod action.
#  Locally, export GEODE_SDK=/path/to/geode before running cmake.
# ─────────────────────────────────────────────────────────
if(NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR
        "[GD-AI] GEODE_SDK environment variable is not set.\n"
        "Run: export GEODE_SDK=/path/to/geode  (Linux/macOS)\n"
        "  or set GEODE_SDK=C:\\path\\to\\geode  (Windows)"
    )
endif()
set(GEODE_SDK "$ENV{GEODE_SDK}" CACHE PATH "Path to the Geode SDK root")

# ─────────────────────────────────────────────────────────
#  Collect source files
# ─────────────────────────────────────────────────────────
file(GLOB_RECURSE MOD_SOURCES
    src/*.cpp
    src/hooks/*.cpp
    src/utils/*.cpp
)

# ─────────────────────────────────────────────────────────
#  Shared library target (Geode mods are .dll / .so)
# ─────────────────────────────────────────────────────────
add_library(${PROJECT_NAME} SHARED ${MOD_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# ─────────────────────────────────────────────────────────
#  Platform-specific compile flags
# ─────────────────────────────────────────────────────────
if(ANDROID)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -O2
        -fvisibility=hidden      # minimise symbol leakage
        -fno-exceptions          # GD Android strips C++ EH
        -fno-rtti
        -Wno-unused-result
    )
    # Android NDK thread support
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
elseif(WIN32)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /O2 /W3
        /wd4996   # suppress deprecation warnings for MSVC
    )
    # Ensure Unicode on Windows
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
endif()

# ─────────────────────────────────────────────────────────
#  Geode SDK integration  (must come after target creation)
# ─────────────────────────────────────────────────────────
add_subdirectory(${GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode-sdk)
target_link_libraries(${PROJECT_NAME} geode-sdk)

# setup_geode_mod finishes packaging: copies mod.json, resources, etc.
setup_geode_mod(${PROJECT_NAME})
