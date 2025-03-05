
if (WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

if (APPLE)
    set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
endif()

option(MSVC_STATIC_RT "Use Static VC Run-Time Library." ON)
if(MSVC)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} /MP")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
    if (MSVC_STATIC_RT)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    if(CMAKE_GENERATOR STREQUAL "Ninja")
        add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/Zc:__cplusplus>)
        add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/std:c++17>)
    else()
        add_compile_options("/Zc:__cplusplus" "/std:c++17")
    endif()
endif(MSVC)

if (CMAKE_COMPILER_IS_GNUCXX)
    add_link_options("-Wl,--exclude-libs,ALL")
endif()

set(CMAKE_CXX_STANDARD 17) #-stdc++17
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) #-fPIC
