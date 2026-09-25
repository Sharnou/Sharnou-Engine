cmake_minimum_required(VERSION 3.25)

project(SharnouEngine VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Sharnou Engine is Windows 10-only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Sharnou Engine requires a 64-bit Windows target.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

find_package(libavif CONFIG REQUIRED)

add_executable(SharnouEngine WIN32
    src/SharnouEngine.cpp
)

target_compile_definitions(SharnouEngine PRIVATE
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

if(MSVC)
    target_compile_options(SharnouEngine PRIVATE
        /std:c++latest
        /W4
        /WX-
        /permissive-
        /EHsc
        /utf-8
        /O2
        /GL
    )
    target_link_options(SharnouEngine PRIVATE
        /LTCG
        /OPT:REF
        /OPT:ICF
    )
endif()

target_link_libraries(SharnouEngine PRIVATE
    avif
    d3d11
    dxgi
    d3dcompiler
    user32
)

set_target_properties(SharnouEngine PROPERTIES
    OUTPUT_NAME "SharnouEngine"
)
