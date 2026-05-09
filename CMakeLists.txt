cmake_minimum_required(VERSION 3.22)
project(WillyBeat VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(WillyBeat
    COMPANY_NAME                "WillyBresee"
    PLUGIN_MANUFACTURER_CODE    Wbrs
    PLUGIN_CODE                 Wbdt
    FORMATS                     VST3 AU Standalone
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    VST3_CATEGORIES             Instrument Drum
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    PRODUCT_NAME                "WillyBeat"
)

juce_generate_juce_header(WillyBeat)

target_sources(WillyBeat
    PRIVATE
        Source/PatternLibrary.cpp
        Source/VariationGenerator.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/WillyBeatLookAndFeel.cpp
        Source/StockDrums.cpp
)

target_compile_definitions(WillyBeat
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(WillyBeat
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
