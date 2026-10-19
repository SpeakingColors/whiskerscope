cmake_minimum_required(VERSION 3.22)
project(WhiskerScope VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Analysis engine tests (no JUCE needed): ctest, or build/ws_engine_test
enable_testing()
add_executable(ws_engine_test tests/engine_test.cpp)
target_include_directories(ws_engine_test PRIVATE include)
add_test(NAME engine COMMAND ws_engine_test)

# Plugin. Pass -DWS_JUCE_DIR=/path/to/JUCE to use a local JUCE, otherwise JUCE is fetched.
option(WS_BUILD_PLUGIN "Build the VST3 plugin and standalone app" ON)
if(WS_BUILD_PLUGIN)
  if(WS_JUCE_DIR)
    add_subdirectory(${WS_JUCE_DIR} juce)
  else()
    include(FetchContent)
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.16-security GIT_SHALLOW ON)
    FetchContent_MakeAvailable(juce)
  endif()

  juce_add_plugin(WhiskerScope
    COMPANY_NAME "Studio 87"
    PLUGIN_MANUFACTURER_CODE Spkc
    PLUGIN_CODE WhSc
    FORMATS VST3 Standalone
    PRODUCT_NAME "Whisker Scope"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Analyzer
    COPY_PLUGIN_AFTER_BUILD FALSE)

  target_sources(WhiskerScope PRIVATE src/PluginProcessor.cpp src/PluginEditor.cpp)
  target_include_directories(WhiskerScope SYSTEM PRIVATE include)
  target_include_directories(WhiskerScope PRIVATE src)
  target_compile_definitions(WhiskerScope PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
  target_link_libraries(WhiskerScope PRIVATE
    juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
