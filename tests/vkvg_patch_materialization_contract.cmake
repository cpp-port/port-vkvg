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
  src/vkvg_device_internal.c
  vkh/src/vkh_image.c)

find_package(Git REQUIRED)
file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")

# During migration, build the contract fixture from the pinned commit so the
# verified dirty sources can remain available for byte-for-byte comparison.
foreach(relative IN LISTS relative_sources)
  get_filename_component(relative_directory "${relative}" DIRECTORY)
  file(MAKE_DIRECTORY "${pristine_source_dir}/${relative_directory}")
  set(source_repository "${vendored_source_dir}")
  set(source_repository_relative "${relative}")
  if(relative MATCHES "^vkh/(.*)$")
    set(source_repository "${vendored_source_dir}/vkh")
    set(source_repository_relative "${CMAKE_MATCH_1}")
  endif()
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${source_repository}" show "HEAD:${source_repository_relative}"
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
file(READ "${staged_output_dir}/vkh/src/vkh_image.c" vkh_image_source)
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
string(FIND "${device_source}"
  "dev->vkhDev                    = vkh_device_create" device_create_position)
if(device_create_position EQUAL -1)
  message(FATAL_ERROR "Unable to locate staged standalone device creation")
endif()
string(SUBSTRING "${device_source}" ${device_create_position} -1 device_creation_source)
string(FIND "${device_creation_source}"
  "info->qFamIdx" queue_family_position)
string(FIND "${device_creation_source}"
  "vkh_app_free_phyinfos(phyCount, phys);" free_phyinfos_position)
string(FIND "${device_internal_source}"
  "VK_PIPELINE_STAGE_ALL_COMMANDS_BIT" all_commands_stage)
string(FIND "${device_internal_source}"
  "VK_ATTACHMENT_STORE_OP_STORE" multisample_store)
if(sample_guard EQUAL -1 OR
   queue_family_position EQUAL -1 OR
   free_phyinfos_position EQUAL -1 OR
   queue_family_position GREATER free_phyinfos_position OR
   all_commands_stage EQUAL -1 OR
   multisample_store EQUAL -1)
  message(FATAL_ERROR "Missing staged device/render-pass fixes")
endif()

foreach(required_text
  "vkMapMemory(img->pDev->dev, img->memory, 0, VK_WHOLE_SIZE, 0, &data)"
  "vkUnmapMemory(img->pDev->dev, img->memory)")
  string(FIND "${vkh_image_source}" "${required_text}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "Missing staged non-VMA image readback fix: ${required_text}")
  endif()
endforeach()

file(READ "${WRAPPER_DIR}/vkvg.vcxproj" vcxproj)
foreach(required_text
  "Name=\"MaterializeVkvgPatchedSources\""
  "BeforeTargets=\"ClCompile\""
  "<VkvgPatchOutputDir>$(IntDir)vkvg-patched</VkvgPatchOutputDir>"
  "$(IntDir)vkvg-patched\\src\\vkvg_context_internal.c"
  "$(IntDir)vkvg-patched\\src\\vkvg_device.c"
  "$(IntDir)vkvg-patched\\src\\vkvg_device_internal.c"
  "$(IntDir)vkvg-patched\\vkh\\src\\vkh_image.c"
  "<ObjectFileName>$(IntDir)vkvg_context_internal.obj</ObjectFileName>"
  "<ObjectFileName>$(IntDir)vkvg_device.obj</ObjectFileName>"
  "<ObjectFileName>$(IntDir)vkvg_device_internal.obj</ObjectFileName>"
  "<ObjectFileName>$(IntDir)vkh_image.obj</ObjectFileName>"
  "0005-preserve-queue-family-before-free.patch"
  "0006-implement-non-vma-image-mapping.patch"
  "<AdditionalIncludeDirectories>$(ProjectDir)vkvg\\vkh\\src;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>"
  "materialize_vkvg_sources.cmake")
  string(FIND "${vcxproj}" "${required_text}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "Visual Studio wrapper is missing: ${required_text}")
  endif()
endforeach()

set(generated_source_include
  "<AdditionalIncludeDirectories>$(ProjectDir)vkvg\\src;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>")
set(generated_source_include_count 0)
set(remaining_vcxproj "${vcxproj}")
while(TRUE)
  string(FIND "${remaining_vcxproj}" "${generated_source_include}" position)
  if(position EQUAL -1)
    break()
  endif()
  math(EXPR next_position "${position} + 1")
  string(SUBSTRING "${remaining_vcxproj}" ${next_position} -1 remaining_vcxproj)
  math(EXPR generated_source_include_count "${generated_source_include_count} + 1")
endwhile()
if(NOT generated_source_include_count EQUAL 3)
  message(FATAL_ERROR
    "Each generated Visual Studio source needs the pristine vkvg/src include path")
endif()

foreach(pristine_source
  "vkvg\\src\\vkvg_context_internal.c"
  "vkvg\\src\\vkvg_device.c"
  "vkvg\\src\\vkvg_device_internal.c"
  "vkvg\\vkh\\src\\vkh_image.c")
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

string(FIND "${wrapper_cmake}"
  "LIST (APPEND VKVG_SRC \${SVG_SRC} \${VKH_SRC})" append_vkh_position)
string(FIND "${wrapper_cmake}"
  "list(REMOVE_ITEM VKVG_SRC \${VKVG_PRISTINE_PATCHED_SOURCES})" remove_pristine_position)
if(append_vkh_position EQUAL -1 OR
   remove_pristine_position EQUAL -1 OR
   remove_pristine_position LESS append_vkh_position)
  message(FATAL_ERROR "CMake must remove pristine patched sources after appending VKH sources")
endif()

foreach(vulkan_link
  "target_link_libraries(\${PROJECT_NAME} PUBLIC Vulkan::Vulkan)"
  "target_link_libraries(static_\${PROJECT_NAME} PUBLIC Vulkan::Vulkan)")
  string(FIND "${wrapper_cmake}" "${vulkan_link}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "CMake target is missing Vulkan usage requirements: ${vulkan_link}")
  endif()
endforeach()

foreach(scalar_layout_definition
  "target_compile_definitions(\${PROJECT_NAME} PRIVATE VKVG_ENABLE_VK_SCALAR_BLOCK_LAYOUT)"
  "target_compile_definitions(static_\${PROJECT_NAME} PRIVATE VKVG_ENABLE_VK_SCALAR_BLOCK_LAYOUT)")
  string(FIND "${wrapper_cmake}" "${scalar_layout_definition}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "CMake target is missing VKVG scalar layout support: ${scalar_layout_definition}")
  endif()
endforeach()

# MSBuild places generated files below the enclosing SceneFoundry repository.
# Verify that parent Git discovery cannot turn patch application into a no-op
# for ignored intermediate files.
get_filename_component(in_repository_output
  "${WRAPPER_DIR}/../../../time-windows/vkvg-patch-contract"
  ABSOLUTE)
file(REMOVE_RECURSE "${in_repository_output}")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DVKVG_SOURCE_DIR=${pristine_source_dir}"
    "-DVKVG_OUTPUT_DIR=${in_repository_output}"
    "-DVKVG_PATCH_DIR=${patch_dir}"
    -P "${materializer}"
  RESULT_VARIABLE in_repository_result
  OUTPUT_VARIABLE in_repository_stdout
  ERROR_VARIABLE in_repository_stderr)
if(NOT in_repository_result EQUAL 0)
  message(FATAL_ERROR
    "In-repository materialization failed (${in_repository_result})\n"
    "${in_repository_stdout}\n${in_repository_stderr}")
endif()
file(READ "${in_repository_output}/src/vkvg_context_internal.c" in_repository_context)
string(FIND "${in_repository_context}"
  "leaked push-constant value" in_repository_clear)
file(REMOVE_RECURSE "${in_repository_output}")
if(in_repository_clear EQUAL -1)
  message(FATAL_ERROR "In-repository materialization left generated sources pristine")
endif()
