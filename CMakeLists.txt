cmake_minimum_required(VERSION 3.16)
PROJECT(vkvg VERSION 0.3.0 DESCRIPTION "Vulkan Vector Graphic")


ADD_COMPILE_DEFINITIONS(
   "VKVG_VERSION_MAJOR=${vkvg_VERSION_MAJOR}"
   "VKVG_VERSION_MINOR=${vkvg_VERSION_MINOR}"
   "VKVG_VERSION_REVISION=${vkvg_VERSION_PATCH}"
)


SET(LANG "C")
SET(CMAKE_${LANG}_STANDARD 11)
SET(CMAKE_${LANG}_EXTENSIONS on)

SET(CMAKE_CXX_STANDARD 11)


if (${LINUX})
   ADD_COMPILE_OPTIONS(-lm)
endif()

#list(APPEND library_references
#  acme
# )


#list(APPEND library_source
#   nanovg/src/nanovg.c
#   nanovg_gl3.c
#   throw_exception.cpp
#)
#
#
#if(WIN32)
#
#   #list(APPEND audio_source
#   #${PROJECT_NAME}/windows/tts_speaker.cpp
#   #${PROJECT_NAME}/windows/tts_speaker.h
#   #  )
#
#elseif(${LINUX} OR ${__BSD__} OR ${ANDROID})
#
#
#   list(APPEND library_source
#      #glad_egl.c
#   )
#
#endif()


FIND_PACKAGE(Vulkan REQUIRED)
FIND_PACKAGE(Threads REQUIRED)

#FILE(GLOB VKH_SRC vkvg/vkh/src/*.c vkvg/vkh/src/deps/*.c)
FILE(GLOB VKH_SRC vkvg/vkh/src/*.c)


FILE(GLOB VKVG_SRC vkvg/src/*.c vkvg/src/deps/*.c)
include("${CMAKE_CURRENT_SOURCE_DIR}/patches/vkvg_patch_manifest.cmake")
set(VKVG_PATCH_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/vkvg-patched")
set(VKVG_PATCHED_SOURCES)
set(VKVG_PRISTINE_PATCHED_SOURCES)
foreach(relative IN LISTS VKVG_PATCHED_RELATIVE_SOURCES)
   list(APPEND VKVG_PRISTINE_PATCHED_SOURCES
      "${CMAKE_CURRENT_SOURCE_DIR}/vkvg/${relative}")
   list(APPEND VKVG_PATCHED_SOURCES
      "${VKVG_PATCH_OUTPUT_DIR}/${relative}")
endforeach()

set(VKVG_PATCH_DEPENDENCIES)
foreach(patch_name IN LISTS VKVG_PATCH_FILES)
   list(APPEND VKVG_PATCH_DEPENDENCIES
      "${CMAKE_CURRENT_SOURCE_DIR}/patches/${patch_name}")
endforeach()

add_custom_command(
   OUTPUT ${VKVG_PATCHED_SOURCES} "${VKVG_PATCH_OUTPUT_DIR}/vkvg-patches.stamp"
   COMMAND "${CMAKE_COMMAND}"
      "-DVKVG_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/vkvg"
      "-DVKVG_OUTPUT_DIR=${VKVG_PATCH_OUTPUT_DIR}"
      "-DVKVG_PATCH_DIR=${CMAKE_CURRENT_SOURCE_DIR}/patches"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/patches/materialize_vkvg_sources.cmake"
   DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/patches/materialize_vkvg_sources.cmake"
      "${CMAKE_CURRENT_SOURCE_DIR}/patches/vkvg_patch_manifest.cmake"
      ${VKVG_PATCH_DEPENDENCIES}
      ${VKVG_PRISTINE_PATCHED_SOURCES}
   VERBATIM)
set_source_files_properties(${VKVG_PATCHED_SOURCES} PROPERTIES GENERATED TRUE)
add_custom_target(vkvg_materialize_patches
   DEPENDS ${VKVG_PATCHED_SOURCES} "${VKVG_PATCH_OUTPUT_DIR}/vkvg-patches.stamp")

FILE(GLOB VKVG_HEADERS vkvg/src/*.h)
SET (VKVG_PUBLIC_HEADERS "include/vkvg.h;include/vkvg-svg.h")

IF (VKVG_SVG)
   ADD_DEFINITIONS (-DVKVG_SVG)
   FILE(GLOB SVG_SRC vkvg/external/vkvg-svg/src/*.c)
ELSE()
   FILE(GLOB SVG_SRC vkvg/src/nsvg/*.c)
ENDIF()

LIST (APPEND VKVG_SRC ${SVG_SRC} ${VKH_SRC})
list(REMOVE_ITEM VKVG_SRC ${VKVG_PRISTINE_PATCHED_SOURCES})
list(APPEND VKVG_SRC ${VKVG_PATCHED_SOURCES})

ADD_LIBRARY(${PROJECT_NAME} SHARED ${VKVG_SRC} ${VKVG_HEADERS})
ADD_LIBRARY(static_${PROJECT_NAME} STATIC ${VKVG_SRC} ${VKVG_HEADERS})
add_dependencies(${PROJECT_NAME} vkvg_materialize_patches)
add_dependencies(static_${PROJECT_NAME} vkvg_materialize_patches)
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Vulkan)
target_link_libraries(static_${PROJECT_NAME} PUBLIC Vulkan::Vulkan)

#add_library(${PROJECT_NAME} SHARED ${library_source})
#add_library(static_${PROJECT_NAME} STATIC ${library_source})


#target_precompile_headers(${PROJECT_NAME} PRIVATE platform.h)
#target_precompile_headers(static_${PROJECT_NAME} PRIVATE platform.h)


set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_CONFIG_H)
target_compile_definitions(static_${PROJECT_NAME} PRIVATE HAVE_CONFIG_H)
target_compile_definitions(${PROJECT_NAME} PRIVATE VKVG_ENABLE_VK_SCALAR_BLOCK_LAYOUT)
target_compile_definitions(static_${PROJECT_NAME} PRIVATE VKVG_ENABLE_VK_SCALAR_BLOCK_LAYOUT)


if (MSVC)
   target_compile_options(${PROJECT_NAME} PRIVATE "/MD$<$<CONFIG:Debug>:d>")
   target_link_options(${PROJECT_NAME} PRIVATE "/DEFAULTLIB:msvcrt$<$<CONFIG:Debug>:d>")
   target_link_options(${PROJECT_NAME} PRIVATE "/SUBSYSTEM:WINDOWS")
   target_link_libraries(${PROJECT_NAME} PRIVATE ${library_references})
   set_target_properties(${PROJECT_NAME} PROPERTIES
      WINDOWS_EXPORT_ALL_SYMBOLS ON
   )
elseif(${LINUX} OR ${__BSD__})


   #target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fpermissive  ${EXTRA_CXX_TARGET_COMPILER_OPTIONS}>)
   #target_compile_options(static_${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fpermissive  ${EXTRA_CXX_TARGET_COMPILER_OPTIONS}>)
   target_compile_definitions(static_${PROJECT_NAME} PRIVATE CUBE)


   #target_link_options(${PROJECT_NAME} PRIVATE -Wl,-rpath=$ORIGIN)
   target_link_libraries(${PROJECT_NAME} PRIVATE vulkan)
   #target_link_libraries(${PROJECT_NAME} PRIVATE glad ${library_references})
   target_link_options(${PROJECT_NAME} PRIVATE -Wl,-z,defs)
   target_link_options(static_${PROJECT_NAME} PRIVATE --whole-archive)


endif()




target_include_directories(${PROJECT_NAME} PRIVATE .)
target_include_directories(${PROJECT_NAME} PRIVATE $ENV{HOME}/sensitive/sensitive/include)
target_include_directories(${PROJECT_NAME} PRIVATE $ENV{__sensitive}/sensitive/include)
list(APPEND library_include_directories ${CMAKE_CURRENT_SOURCE_DIR}/../..)
list(APPEND library_include_directories ${CMAKE_CURRENT_SOURCE_DIR}/..)
list(APPEND library_include_directories ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND library_include_directories ${CMAKE_CURRENT_SOURCE_DIR}/vkvg/src)
list(APPEND library_include_directories ${CMAKE_CURRENT_SOURCE_DIR}/vkvg/vkh/src)
list(APPEND library_include_directories ${CMAKE_CURRENT_SOURCE_DIR}/include)
list(APPEND library_include_directories ${CMAKE_CURRENT_SOURCE_DIR}/../../_/${PROJECT_NAME})



target_include_directories(${PROJECT_NAME} PRIVATE ${library_include_directories})
target_include_directories(static_${PROJECT_NAME} PRIVATE ${library_include_directories})


target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)


target_compile_definitions(${PROJECT_NAME} PRIVATE _${PROJECT_NAME}_project)


option(VKVG_WRAPPER_BUILD_TESTS "Build SceneFoundry VKVG wrapper regression tests" OFF)
if(VKVG_WRAPPER_BUILD_TESTS)
   enable_testing()
   add_test(
      NAME vkvg_patch_materialization_contract
      COMMAND "${CMAKE_COMMAND}"
         "-DWRAPPER_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
         "-DTEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/contract-vkvg-patched"
         -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/vkvg_patch_materialization_contract.cmake")
   add_executable(vkvg_offscreen_regression tests/offscreen_regression.c)
   target_include_directories(vkvg_offscreen_regression PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/include")
   target_link_libraries(vkvg_offscreen_regression PRIVATE ${PROJECT_NAME})
   add_test(NAME vkvg_offscreen_regression COMMAND vkvg_offscreen_regression)
endif()
