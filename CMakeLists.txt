cmake_minimum_required(VERSION 3.15)

project(SBShot VERSION 0.1.0 LANGUAGES CXX RC)

add_executable(SBShot WIN32
    src/main.cpp
    assets/resource.rc
)

target_compile_features(SBShot PRIVATE cxx_std_17)
target_compile_definitions(SBShot PRIVATE
    UNICODE
    _UNICODE
    WINVER=0x0601
    _WIN32_WINNT=0x0601
)

target_link_libraries(SBShot PRIVATE
    user32
    gdi32
    shell32
    msimg32
    comctl32
)

if(MSVC)
    set_property(TARGET SBShot PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(SBShot PRIVATE /W4 /permissive- /utf-8)
    target_link_options(SBShot PRIVATE
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
    )
else()
    target_compile_options(SBShot PRIVATE -Wall -Wextra -Wpedantic)
    target_link_options(SBShot PRIVATE
        -static-libgcc
        -static-libstdc++
        $<$<CONFIG:Release>:-s>
        $<$<CONFIG:Release>:-Wl,--gc-sections>
    )
endif()
