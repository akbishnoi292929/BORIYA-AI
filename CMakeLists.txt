cmake_minimum_required(VERSION 3.22)
project(BORIYA_AI VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enforce static C++ runtime
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT TARGET juce::juce_audio_utils)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        add_subdirectory(JUCE)
    else()
        include(FetchContent)
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 7.0.12
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(JUCE)
    endif()
endif()

juce_add_plugin(BORIYA_AI
    COMPANY_NAME "OG RJ HOOD"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Orjh
    PLUGIN_CODE BoAI
    FORMATS VST3
    PRODUCT_NAME "BORIYA AI"
    VST3_CATEGORIES "Instrument" "Synth"
)

set_target_properties(BORIYA_AI PROPERTIES 
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    JUCE_VST3_GENERATE_MODULE_INFO OFF
)

file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h"
)

target_sources(BORIYA_AI PRIVATE ${SourceFiles})

# Pure Windows 7 safe target definitions
target_compile_definitions(BORIYA_AI PUBLIC
    WINVER=0x0601
    _WIN32_WINNT=0x0601
    NTDDI_VERSION=0x06010000
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_DIRECTWRITE=0
    JUCE_DIRECT2D=0
    JUCE_WINGDI=1
    JUCE_DISABLE_VST3_MANIFEST_HELPER=1
)

if(MSVC)
    # SSE2 instruction set lock taaki puraane CPU ya Win7 par AVX crash na ho
    target_compile_options(BORIYA_AI PRIVATE /MT /bigobj /EHsc /arch:SSE2)
    target_link_options(BORIYA_AI PRIVATE /SUBSYSTEM:WINDOWS,6.01)
endif()

target_link_libraries(BORIYA_AI PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
)

juce_generate_juce_header(BORIYA_AI)
