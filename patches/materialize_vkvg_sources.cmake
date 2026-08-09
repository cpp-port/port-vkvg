cmake_minimum_required(VERSION 3.16)

foreach(required VKVG_SOURCE_DIR VKVG_OUTPUT_DIR VKVG_PATCH_DIR)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

get_filename_component(VKVG_SOURCE_DIR "${VKVG_SOURCE_DIR}" ABSOLUTE)
get_filename_component(VKVG_OUTPUT_DIR "${VKVG_OUTPUT_DIR}" ABSOLUTE)
get_filename_component(VKVG_PATCH_DIR "${VKVG_PATCH_DIR}" ABSOLUTE)
file(RELATIVE_PATH output_from_source "${VKVG_SOURCE_DIR}" "${VKVG_OUTPUT_DIR}")
if(VKVG_OUTPUT_DIR STREQUAL VKVG_SOURCE_DIR OR
   NOT output_from_source MATCHES "^\.\./")
  message(FATAL_ERROR "VKVG_OUTPUT_DIR must be outside the pristine source tree")
endif()

include("${VKVG_PATCH_DIR}/vkvg_patch_manifest.cmake")
find_package(Git REQUIRED)
file(REMOVE_RECURSE "${VKVG_OUTPUT_DIR}")

foreach(relative IN LISTS VKVG_PATCHED_RELATIVE_SOURCES)
  if(NOT EXISTS "${VKVG_SOURCE_DIR}/${relative}")
    message(FATAL_ERROR "Missing pristine VKVG source: ${relative}")
  endif()
  get_filename_component(relative_directory "${relative}" DIRECTORY)
  file(MAKE_DIRECTORY "${VKVG_OUTPUT_DIR}/${relative_directory}")
  configure_file(
    "${VKVG_SOURCE_DIR}/${relative}"
    "${VKVG_OUTPUT_DIR}/${relative}"
    COPYONLY)
endforeach()

foreach(patch_name IN LISTS VKVG_PATCH_FILES)
  set(patch "${VKVG_PATCH_DIR}/${patch_name}")
  if(NOT EXISTS "${patch}")
    message(FATAL_ERROR "Missing VKVG wrapper patch: ${patch_name}")
  endif()
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --check --whitespace=nowarn "${patch}"
    WORKING_DIRECTORY "${VKVG_OUTPUT_DIR}"
    RESULT_VARIABLE check_result
    OUTPUT_VARIABLE check_output
    ERROR_VARIABLE check_error)
  if(NOT check_result EQUAL 0)
    message(FATAL_ERROR
      "VKVG patch check failed: ${patch_name}\n${check_output}\n${check_error}")
  endif()
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --whitespace=nowarn "${patch}"
    WORKING_DIRECTORY "${VKVG_OUTPUT_DIR}"
    RESULT_VARIABLE apply_result
    OUTPUT_VARIABLE apply_output
    ERROR_VARIABLE apply_error)
  if(NOT apply_result EQUAL 0)
    message(FATAL_ERROR
      "VKVG patch apply failed: ${patch_name}\n${apply_output}\n${apply_error}")
  endif()
endforeach()

file(WRITE "${VKVG_OUTPUT_DIR}/vkvg-patches.stamp" "materialized\n")
