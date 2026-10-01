cmake_minimum_required(VERSION 3.22)

project(TransientLockComp VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()

# ---------------------------------------------------------------- JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

option(TLC_COPY_AFTER_BUILD "Copiar el plugin a la carpeta del sistema tras compilar" ON)

# ------------------------------------------------------------- Formatos
set(TLC_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND TLC_FORMATS AU)
endif()

# AAX: define -DAAX_SDK_PATH=/ruta/al/AAX_SDK (SDK de Avid, requiere cuenta)
if(DEFINED AAX_SDK_PATH AND EXISTS "${AAX_SDK_PATH}")
    juce_set_aax_sdk_path("${AAX_SDK_PATH}")
    list(APPEND TLC_FORMATS AAX)
    message(STATUS "AAX habilitado: ${AAX_SDK_PATH}")
endif()

juce_add_plugin(TransientLockComp
    COMPANY_NAME                 "YourCompany"
    BUNDLE_ID                    com.yourcompany.transientlockcomp
    PLUGIN_MANUFACTURER_CODE     Ycmp
    PLUGIN_CODE                  Tlkc
    FORMATS                      ${TLC_FORMATS}
    PRODUCT_NAME                 "TransientLock Comp"
    IS_SYNTH                     FALSE
    NEEDS_MIDI_INPUT             FALSE
    NEEDS_MIDI_OUTPUT            FALSE
    IS_MIDI_EFFECT               FALSE
    COPY_PLUGIN_AFTER_BUILD      ${TLC_COPY_AFTER_BUILD}
    VST3_CATEGORIES              Fx Dynamics
    AU_MAIN_TYPE                 kAudioUnitType_Effect)

target_sources(TransientLockComp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/TransientLockDSP.h)

target_compile_definitions(TransientLockComp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(TransientLockComp
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
