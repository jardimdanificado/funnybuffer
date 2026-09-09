cmake_minimum_required(VERSION 3.10)
project(Wagnostic C)

set(CMAKE_C_STANDARD 11)

# Find SDL2
find_package(SDL2 REQUIRED)

# Sources (shared wasm3 engine)
file(GLOB WASM3_SRC "emulators/wasm3/wasm3/source/*.c")

# ── wagnostic (wasm3 + SDL2 + headless / GIF native runner) ──────────────────
set(WAGNOSTIC_SOURCES
    emulators/wasm3/host.c
    emulators/wasm3/gif_encoder.c
    ${WASM3_SRC}
)

add_executable(wagnostic ${WAGNOSTIC_SOURCES})

target_include_directories(wagnostic PRIVATE
    include
    emulators/wasm3
    emulators/wasm3/wasm3/source
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(wagnostic PRIVATE
    ${SDL2_LIBRARIES}
)

if(WIN32)
    find_package(GLEW REQUIRED)
    target_link_libraries(wagnostic PRIVATE GLEW::GLEW opengl32)
else()
    target_link_libraries(wagnostic PRIVATE GL m)
endif()

if(MSVC)
    target_compile_options(wagnostic PRIVATE /std:c11)
endif()

