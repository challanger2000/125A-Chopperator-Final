cmake_minimum_required(VERSION 3.25)
project(Chopperator VERSION 1.0.0 LANGUAGES CXX)
include(CTest)
find_package(Threads REQUIRED)
add_executable(chopper_core_tests tests/CoreTests.cpp)
add_executable(chopper_extended_tests tests/ExtendedTests.cpp)
add_executable(chopper_host_tests tests/HostEngineTests.cpp)
add_executable(chopper_registry_tests tests/EditorPreferencesTests.cpp)
target_link_libraries(chopper_host_tests PRIVATE Threads::Threads)
foreach(target IN ITEMS chopper_core_tests chopper_extended_tests chopper_host_tests chopper_registry_tests)
  target_include_directories(${target} PRIVATE src)
  target_compile_features(${target} PRIVATE cxx_std_23)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endforeach()
add_test(NAME core_contract COMMAND chopper_core_tests)
add_test(NAME extended_contract COMMAND chopper_extended_tests)
add_test(NAME host_engine_contract COMMAND chopper_host_tests)
add_test(NAME editor_registry_reentrancy COMMAND chopper_registry_tests)
add_test(NAME release_negative_control COMMAND chopper_core_tests --negative)
set_tests_properties(release_negative_control PROPERTIES WILL_FAIL TRUE)

option(CHOPPER_BUILD_VST3 "Build native VST3 adapter" OFF)
if(CHOPPER_BUILD_VST3)
  if(NOT WIN32 AND NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64)$"))
    message(FATAL_ERROR "Supported targets: Windows and Linux x86-64")
  endif()
  set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES ON CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
  set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)
  set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
  include(FetchContent)
  FetchContent_Declare(vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG v3.8.1_build_84
    GIT_SUBMODULES_RECURSE TRUE)
  FetchContent_MakeAvailable(vst3sdk)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    execute_process(COMMAND ${CMAKE_COMMAND}
      "-DVSTGUI_ROOT=${vst3sdk_SOURCE_DIR}/vstgui4"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/patch_vstgui_linux.cmake"
      COMMAND_ERROR_IS_FATAL ANY)
  endif()
  smtg_add_vst3plugin(Chopperator PACKAGE_NAME "125A Chopperator" SOURCES_LIST src/Vst3.cpp)
  target_compile_features(Chopperator PRIVATE cxx_std_20)
  target_link_libraries(Chopperator PRIVATE sdk vstgui_support)
  set(CHOPPER_GUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gui-draft" CACHE PATH "Prepared verified GUI resources")
  set(CHOPPER_GUI_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/chopper.uidesc
    ${CHOPPER_GUI_DIR}/logo.png ${CHOPPER_GUI_DIR}/logo_3x.png
    ${CHOPPER_GUI_DIR}/knob_L_0.5x.png ${CHOPPER_GUI_DIR}/knob_L_0.75x.png ${CHOPPER_GUI_DIR}/knob_L_200.png ${CHOPPER_GUI_DIR}/knob_L_1.5x.png
    ${CHOPPER_GUI_DIR}/knob_S_0.5x.png ${CHOPPER_GUI_DIR}/knob_S_0.75x.png ${CHOPPER_GUI_DIR}/knob_S_200.png ${CHOPPER_GUI_DIR}/knob_S_1.5x.png)
  set(CHOPPER_NOTICE_FILES
    ${vst3sdk_SOURCE_DIR}/LICENSE.txt
    ${vst3sdk_SOURCE_DIR}/vstgui4/LICENSE)
  foreach(notice IN LISTS CHOPPER_NOTICE_FILES)
    if(NOT EXISTS "${notice}")
      message(FATAL_ERROR "Missing pinned SDK license: ${notice}")
    endif()
  endforeach()
  # SDK resource helper emits an unquoted VS macro for package paths with
  # spaces. Use the actual binary directory and VERBATIM commands instead.
  foreach(asset IN LISTS CHOPPER_GUI_FILES)
    if(NOT EXISTS "${asset}")
      message(FATAL_ERROR "Missing prepared GUI asset: ${asset}")
    endif()
  endforeach()
  add_custom_target(chopper_gui_resources ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Chopperator>/../Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CHOPPER_GUI_FILES} "$<TARGET_FILE_DIR:Chopperator>/../Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${vst3sdk_SOURCE_DIR}/LICENSE.txt" "$<TARGET_FILE_DIR:Chopperator>/../Resources/Steinberg-VST3-LICENSE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${vst3sdk_SOURCE_DIR}/vstgui4/LICENSE" "$<TARGET_FILE_DIR:Chopperator>/../Resources/VSTGUI-LICENSE.txt"
    DEPENDS Chopperator ${CHOPPER_GUI_FILES} ${CHOPPER_NOTICE_FILES}
    VERBATIM)
  smtg_target_configure_version_file(Chopperator)
  if(WIN32)
    target_sources(Chopperator PRIVATE ${vst3sdk_SOURCE_DIR}/public.sdk/source/main/dllmain.cpp)
    set(chopper_host_platform ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/module_win32.cpp)
    set(chopper_host_libs user32 ole32 gdi32)
  else()
    # smtg_add_vst3plugin may add linuxmain automatically when SDK scope is visible.
    get_target_property(chopper_sources Chopperator SOURCES)
    if(NOT "${chopper_sources}" MATCHES "linuxmain\\.cpp")
      target_sources(Chopperator PRIVATE ${vst3sdk_SOURCE_DIR}/public.sdk/source/main/linuxmain.cpp)
    endif()
    find_package(X11 REQUIRED)
    set(chopper_host_platform ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/module_linux.cpp)
    set(chopper_host_libs X11::X11 X11::Xtst ${CMAKE_DL_LIBS})
  endif()
  add_executable(chopper_native_tests tests/NativeVst3Tests.cpp
    ${chopper_host_platform}
    ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/plugprovider.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/source/common/memorystream.cpp)
  target_compile_features(chopper_native_tests PRIVATE cxx_std_17)
  target_link_libraries(chopper_native_tests PRIVATE sdk_hosting ${chopper_host_libs} Threads::Threads)
endif()
