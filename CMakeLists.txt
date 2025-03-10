cmake_minimum_required(VERSION 3.21)

project(FFmpegCodecPlugin)

if(FFmpegCodecPlugin_IS_TOP_LEVEL)
    include(BuildOnRoot.cmake)

    set(NVI_PATH "" CACHE STRING "The NVI repository or include path.")
    find_path(NVI_INCLUDE_DIR "NVI/Codec.h" HINTS ${NVI_PATH} ${NVI_PATH}/src/interface REQUIRED)
    message(STATUS "NVI interface: ${NVI_INCLUDE_DIR}")
endif()

find_package(ffmpeg CONFIG QUIET)
if (ffmpeg_FOUND)
    message(STATUS "Found ffmpeg from ${ffmpeg_DIR}.")
else()
    find_path(FFMPEG_INCLUDE_DIR "libavcodec/avcodec.h" REQUIRED)
    find_library(AVCODEC_LIBRARY_D "avcodec" HINTS ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib REQUIRED NO_DEFAULT_PATH NO_CMAKE_PATH)
    find_library(AVCODEC_LIBRARY_R "avcodec" HINTS ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib REQUIRED NO_DEFAULT_PATH NO_CMAKE_PATH)
    find_library(AVUTIL_LIBRARY_D "avutil" HINTS ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib REQUIRED NO_DEFAULT_PATH NO_CMAKE_PATH)
    find_library(AVUTIL_LIBRARY_R "avutil" HINTS ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib REQUIRED NO_DEFAULT_PATH NO_CMAKE_PATH)

    add_library(libavutil INTERFACE)    
    target_include_directories(libavutil INTERFACE ${FFMPEG_INCLUDE_DIR})
    target_link_libraries(libavutil INTERFACE debug ${AVUTIL_LIBRARY_D} optimized ${AVUTIL_LIBRARY_R})
    if(WIN32)
        target_link_libraries(libavutil INTERFACE Bcrypt)
    endif()

    add_library(libavcodec INTERFACE)
    target_include_directories(libavcodec INTERFACE ${FFMPEG_INCLUDE_DIR})
    target_link_libraries(libavcodec INTERFACE debug ${AVCODEC_LIBRARY_D} optimized ${AVCODEC_LIBRARY_R})
    target_link_libraries(libavcodec INTERFACE libavutil)
    if (WIN32)
        target_link_libraries(libavcodec INTERFACE "mfuuid;strmiids")
    endif()
    if (APPLE)
        target_link_libraries(libavcodec INTERFACE "m;-Wl,-weak_framework,VideoToolbox;-Wl,-weak_framework,CoreMedia;-Wl,-weak_framework,CoreVideo;-Wl,-weak_framework,CoreFoundation;-Wl,-weak_framework,CoreServices;")
    endif()
    add_library(ffmpeg::avcodec ALIAS libavcodec)
endif()

if (TARGET ffmpeg::avcodec)
    find_package(fmt CONFIG QUIET)
    file(GLOB_RECURSE SRC_FILES "*.h" "*.hpp" "*.cpp")
    add_library(${PROJECT_NAME} SHARED  ${SRC_FILES})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC_FILES})
    set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "Plugin")
    target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if (NVI_INCLUDE_DIR)
        target_include_directories(${PROJECT_NAME} PRIVATE ${NVI_INCLUDE_DIR})
    endif()
    target_link_libraries(${PROJECT_NAME} PRIVATE ffmpeg::avcodec)
    if (TARGET fmt::fmt-header-only)
        target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt-header-only)
        target_compile_definitions(${PROJECT_NAME} PRIVATE _HAS_FMT)
    endif()
    if (ENABLE_NVCODEC)
        find_path(FFNVCODEC_INCLUDE_DIRS "ffnvcodec/dynlink_cuda.h")
        if (FFNVCODEC_INCLUDE_DIRS)
            target_include_directories(${PROJECT_NAME} PRIVATE ${FFNVCODEC_INCLUDE_DIRS}/ffnvcodec)
        else()
            find_package(nvcodec CONFIG REQUIRED)
            target_link_libraries(${PROJECT_NAME} PRIVATE nvcodec-header-only)
        endif()
        target_compile_definitions(${PROJECT_NAME} PRIVATE _NVCODEC)
    endif()
    if(WIN32)
        install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION pdb OPTIONAL)
    elseif(APPLE)
        install(FILES $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_NAME:${PROJECT_NAME}>.dSYM DESTINATION dsym)
    else()
        if (DEFINED CMAKE_OBJCOPY)
            install(CODE "execute_process(
                COMMAND ${CMAKE_OBJCOPY} --only-keep-debug libFFmpegCodecPlugin.so libFFmpegCodecPlugin.so.dbg
                WORKING_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})"
            )
            install(CODE "execute_process(
                COMMAND ${CMAKE_OBJCOPY} --strip-debug --strip-unneeded libFFmpegCodecPlugin.so
                WORKING_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})"
            )
            install(FILES ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libFFmpegCodecPlugin.so.dbg DESTINATION dbg)
        endif()
    endif()
    install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
    )
else ()
    message(STATUS "Not config FFmpegCodecPlugin.")
endif()
