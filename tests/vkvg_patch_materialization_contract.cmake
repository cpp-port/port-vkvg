cmake_minimum_required(VERSION 3.16)

foreach(required WRAPPER_DIR TEST_OUTPUT_DIR)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

get_filename_component(WRAPPER_DIR "${WRAPPER_DIR}" ABSOLUTE)
get_filename_component(TEST_OUTPUT_DIR "${TEST_OUTPUT_DIR}" ABSOLUTE)
set(vendored_source_dir "${WRAPPER_DIR}/vkvg")
set(pristine_source_dir "${TEST_OUTPUT_DIR}/pristine")
set(staged_output_dir "${TEST_OUTPUT_DIR}/staged")
set(patch_dir "${WRAPPER_DIR}/patches")
set(materializer "${patch_dir}/materialize_vkvg_sources.cmake")
set(relative_sources
  src/vkvg_context_internal.c
  src/vkvg_device.c
  src/vkvg_device_internal.c)

find_package(Git REQUIRED)
file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")

# During migration, build the contract fixture from the pinned commit so the
# verified dirty sources can remain available for byte-for-byte comparison.
foreach(relative IN LISTS relative_sources)
  get_filename_component(relative_directory "${relative}" DIRECTORY)
  file(MAKE_DIRECTORY "${pristine_source_dir}/${relative_directory}")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${vendored_source_dir}" show "HEAD:${relative}"
    OUTPUT_FILE "${pristine_source_dir}/${relative}"
    RESULT_VARIABLE show_result
    ERROR_VARIABLE show_error)
  if(NOT show_result EQUAL 0)
    message(FATAL_ERROR "Could not read pinned VKVG source ${relative}: ${show_error}")
  endif()
  string(MAKE_C_IDENTIFIER "${relative}" source_key)
  file(SHA256 "${vendored_source_dir}/${relative}" "before_${source_key}")
endforeach()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DVKVG_SOURCE_DIR=${pristine_source_dir}"
    "-DVKVG_OUTPUT_DIR=${staged_output_dir}"
    "-DVKVG_PATCH_DIR=${patch_dir}"
    -P "${materializer}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Materialization failed (${result})\n${output}\n${error}")
endif()

foreach(relative IN LISTS relative_sources)
  string(MAKE_C_IDENTIFIER "${relative}" source_key)
  file(SHA256 "${vendored_source_dir}/${relative}" "after_${source_key}")
  if(NOT "${before_${source_key}}" STREQUAL "${after_${source_key}}")
    message(FATAL_ERROR "Materialization modified vendored ${relative}")
  endif()
endforeach()

file(READ "${WRAPPER_DIR}/.gitmodules" gitmodules)
if(gitmodules MATCHES "(^|[\r\n])[ \t]*branch[ \t]*=")
  message(FATAL_ERROR "VKVG submodule must be pinned only by its gitlink")
endif()

execute_process(
  COMMAND "${GIT_EXECUTABLE}" -C "${WRAPPER_DIR}" ls-files -s vkvg
  RESULT_VARIABLE gitlink_result
  OUTPUT_VARIABLE gitlink
  ERROR_VARIABLE gitlink_error
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT gitlink_result EQUAL 0 OR
   NOT gitlink MATCHES "^160000 65d1eecacbfeb0f7a288896cccb8e1e871ecf6cf 0[\t ]+vkvg$")
  message(FATAL_ERROR "Unexpected VKVG gitlink: ${gitlink}\n${gitlink_error}")
endif()

file(READ "${staged_output_dir}/src/vkvg_context_internal.c" context_source)
file(READ "${staged_output_dir}/src/vkvg_device.c" device_source)
file(READ "${staged_output_dir}/src/vkvg_device_internal.c" device_internal_source)
string(FIND "${context_source}"
  "ctx->pushConsts.fsq_patternType &= ~FULLSCREEN_BIT;" clear_position)
string(FIND "${context_source}"
  "CmdDrawIndexed(ctx->cmd" draw_position)
if(clear_position EQUAL -1 OR draw_position EQUAL -1 OR clear_position GREATER draw_position)
  message(FATAL_ERROR "Ordinary indexed draws do not restore FULLSCREEN_BIT state")
endif()

foreach(required_text
  "float signedArea = 0.0f;"
  "ecp_zcross(v0, v2, v1) * signedArea < 0")
  string(FIND "${context_source}" "${required_text}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "Missing staged winding fix: ${required_text}")
  endif()
endforeach()

string(FIND "${device_source}"
  "if (!(counts & info->samples))" sample_guard)
string(FIND "${device_internal_source}"
  "VK_PIPELINE_STAGE_ALL_COMMANDS_BIT" all_commands_stage)
string(FIND "${device_internal_source}"
  "VK_ATTACHMENT_STORE_OP_STORE" multisample_store)
if(sample_guard EQUAL -1 OR all_commands_stage EQUAL -1 OR multisample_store EQUAL -1)
  message(FATAL_ERROR "Missing staged device/render-pass fixes")
endif()

file(READ "${WRAPPER_DIR}/vkvg.vcxproj" vcxproj)
foreach(required_text
  "Name=\"MaterializeVkvgPatchedSources\""
  "BeforeTargets=\"ClCompile\""
  "$(IntDir)vkvg-patched\\src\\vkvg_context_internal.c"
  "$(IntDir)vkvg-patched\\src\\vkvg_device.c"
  "$(IntDir)vkvg-patched\\src\\vkvg_device_internal.c"
  "materialize_vkvg_sources.cmake")
  string(FIND "${vcxproj}" "${required_text}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "Visual Studio wrapper is missing: ${required_text}")
  endif()
endforeach()

foreach(pristine_source
  "vkvg\\src\\vkvg_context_internal.c"
  "vkvg\\src\\vkvg_device.c"
  "vkvg\\src\\vkvg_device_internal.c")
  string(FIND "${vcxproj}" "ClCompile Include=\"${pristine_source}\"" position)
  if(NOT position EQUAL -1)
    message(FATAL_ERROR "Visual Studio compiles pristine patched source: ${pristine_source}")
  endif()
endforeach()

file(READ "${WRAPPER_DIR}/CMakeLists.txt" wrapper_cmake)
foreach(required_text
  "include(\"\${CMAKE_CURRENT_SOURCE_DIR}/patches/vkvg_patch_manifest.cmake\")"
  "add_custom_command("
  "set_source_files_properties(\${VKVG_PATCHED_SOURCES} PROPERTIES GENERATED TRUE)"
  "list(REMOVE_ITEM VKVG_SRC \${VKVG_PRISTINE_PATCHED_SOURCES})"
  "list(APPEND VKVG_SRC \${VKVG_PATCHED_SOURCES})"
  "option(VKVG_WRAPPER_BUILD_TESTS"
  "tests/offscreen_regression.c")
  string(FIND "${wrapper_cmake}" "${required_text}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "CMake wrapper is missing: ${required_text}")
  endif()
endforeach()

foreach(target_declaration
  "ADD_LIBRARY(\${PROJECT_NAME} SHARED \${VKVG_SRC} \${VKVG_HEADERS})"
  "ADD_LIBRARY(static_\${PROJECT_NAME} STATIC \${VKVG_SRC} \${VKVG_HEADERS})")
  string(FIND "${wrapper_cmake}" "${target_declaration}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "CMake target does not consume the shared VKVG source list: ${target_declaration}")
  endif()
endforeach()
