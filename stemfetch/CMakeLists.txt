cmake_minimum_required(VERSION 3.22)
project(StemFetch VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(StemFetch
    COMPANY_NAME "StemFetch"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Stmf
    PLUGIN_CODE Sfch
    FORMATS VST3
    PRODUCT_NAME "StemFetch"
    VST3_CATEGORIES Instrument)

target_sources(StemFetch PRIVATE plugin/PluginMain.cpp)
target_include_directories(StemFetch PRIVATE plugin)

target_compile_definitions(StemFetch PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(StemFetch
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
