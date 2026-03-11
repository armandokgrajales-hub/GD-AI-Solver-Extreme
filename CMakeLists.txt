cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─────────────────────────────────────────────────────────
#  CMP0175 — must be set BEFORE add_subdirectory.
#  Geode's GeodeFile.cmake uses add_custom_command(TARGET)
#  with DEPENDS, which CMake 3.31+ flags as an error without
#  this policy set to OLD.
# ─────────────────────────────────────────────────────────
if(POLICY CMP0175)
    cmake_policy(SET CMP0175 OLD)
endif()

project(GD-AI-Solver-Extreme VERSION 1.0.0)

# ─────────────────────────────────────────────────────────
#  Locate Geode SDK
#
#  geode-sdk/build-geode-mod injects -DGEODE_SDK=<path> as a
#  plain CMake variable, NOT an env var, so we check that first.
# ─────────────────────────────────────────────────────────
if(NOT DEFINED GEODE_SDK)
    if(DEFINED ENV{GEODE_SDK})
        set(GEODE_SDK "$ENV{GEODE_SDK}")
        message(STATUS "[GD-AI] GEODE_SDK from env: ${GEODE_SDK}")
    else()
        message(FATAL_ERROR
            "[GD-AI] GEODE_SDK is not set.\n"
            "  CI   : handled automatically by geode-sdk/build-geode-mod.\n"
            "  Local: export GEODE_SDK=/path/to/geode  or  -DGEODE_SDK=<path>"
        )
    endif()
else()
    message(STATUS "[GD-AI] GEODE_SDK: ${GEODE_SDK}")
endif()

# ─────────────────────────────────────────────────────────
#  Sources
# ─────────────────────────────────────────────────────────
file(GLOB_RECURSE MOD_SOURCES
    src/*.cpp
    src/hooks/*.cpp
    src/utils/*.cpp
)

# ─────────────────────────────────────────────────────────
#  Target
# ─────────────────────────────────────────────────────────
add_library(${PROJECT_NAME} SHARED ${MOD_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# ─────────────────────────────────────────────────────────
#  Geode SDK  (add_subdirectory before any linking)
# ─────────────────────────────────────────────────────────
add_subdirectory(${GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode-sdk)

# ─────────────────────────────────────────────────────────
#  Linking — ALL calls use the PLAIN (no-keyword) signature.
#
#  CRITICAL: Geode's GeodeFile.cmake calls
#      target_link_libraries(<target> geode-sdk)
#  with the plain signature. Mixing keyword (PRIVATE/PUBLIC)
#  with plain on the same target causes a CMake fatal error:
#    "keyword signature has already been used … all-keyword or all-plain"
#
#  Solution: consolidate ALL libraries into ONE plain call here
#  so Geode's internal call is consistent with ours.
# ─────────────────────────────────────────────────────────
if(ANDROID)
    target_link_libraries(${PROJECT_NAME} geode-sdk log)
else()
    target_link_libraries(${PROJECT_NAME} geode-sdk)
endif()

# ─────────────────────────────────────────────────────────
#  Compile options  (PRIVATE is safe here — the signature
#  restriction only applies to target_link_libraries)
# ─────────────────────────────────────────────────────────
if(ANDROID)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -O2
        -fvisibility=hidden
        -Wno-unused-result
    )
elseif(WIN32)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /O2 /W3
        /wd4996
        /wd4244
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        UNICODE _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

# ─────────────────────────────────────────────────────────
#  Geode packaging
# ─────────────────────────────────────────────────────────
setup_geode_mod(${PROJECT_NAME})

