cmake_minimum_required(VERSION 3.16)

project(FFmpegCodecPlugin)

if(${CMAKE_SOURCE_DIR} EQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    include(BuildOnRoot.cmake)

    find_package(NVI CONFIG REQUIRED)
    find_path(NVI_INCLUDE_DIRS "NVI/Codec.h" REQUIRED)
endif()

find_package(ffmpeg CONFIG QUIET)
find_package(fmt CONFIG QUIET)

if (TARGET ffmpeg::avcodec)
    file(GLOB_RECURSE SRC_FILES "*.h" "*.hpp" "*.cpp")
    add_library(${PROJECT_NAME} SHARED  ${SRC_FILES})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC_FILES})
    set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "Plugin")
    target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if (NVI_INCLUDE_DIRS)
        target_include_directories(${PROJECT_NAME} PRIVATE ${NVI_INCLUDE_DIRS})
    endif()
    target_link_libraries(${PROJECT_NAME} PRIVATE ffmpeg::avcodec)
    if (TARGET fmt::fmt-header-only)
        target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt-header-only)
        target_compile_definitions(${PROJECT_NAME} PRIVATE _HAS_FMT)
    endif()
    if (TARGET nvcodec-header-only)
        target_link_libraries(${PROJECT_NAME} PUBLIC nvcodec-header-only)
        target_compile_definitions(${PROJECT_NAME} PRIVATE _NVCODEC)
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
