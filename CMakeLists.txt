cmake_minimum_required(VERSION 3.22)
project(WillyBeat VERSION 2.0.0)

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
        Source/TagVectorIndex.mm
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

if(APPLE)
    find_library(NATURAL_LANGUAGE_FRAMEWORK NaturalLanguage)
    find_library(FOUNDATION_FRAMEWORK       Foundation)
    target_link_libraries(WillyBeat PRIVATE ${NATURAL_LANGUAGE_FRAMEWORK})

    # ── BakeTagEmbeddings: pre-compute NLEmbedding vectors for every tag in
    # Presets/ so the plugin doesn't have to call the framework at startup.
    # Critical for Cubase's sandboxed VST3 host, which kills plugins that
    # block the main thread during init.
    add_executable(BakeTagEmbeddings Tools/BakeTagEmbeddings.mm)
    set_target_properties(BakeTagEmbeddings PROPERTIES CXX_STANDARD 17)
    target_link_libraries(BakeTagEmbeddings PRIVATE
        ${NATURAL_LANGUAGE_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK})

    set(BAKED_DIR "${CMAKE_BINARY_DIR}/baked")
    set(BAKED_BIN "${BAKED_DIR}/BakedEmbeddings.bin")
    set(PRESETS_DIR "${CMAKE_SOURCE_DIR}/Presets")

    file(GLOB BAKE_PRESET_SOURCES "${PRESETS_DIR}/*.beat")
    add_custom_command(
        OUTPUT  "${BAKED_BIN}"
        COMMAND "$<TARGET_FILE:BakeTagEmbeddings>" "${PRESETS_DIR}" "${BAKED_BIN}"
        DEPENDS BakeTagEmbeddings ${BAKE_PRESET_SOURCES}
        COMMENT "BakeTagEmbeddings: precomputing NLEmbedding vectors"
        VERBATIM)

    juce_add_binary_data(WillyBeatBakedData
        HEADER_NAME BinaryData.h
        NAMESPACE   BinaryData
        SOURCES     "${BAKED_BIN}")

    target_link_libraries(WillyBeat PRIVATE WillyBeatBakedData)
    target_compile_definitions(WillyBeat PRIVATE JUCE_INCLUDED_BAKED_EMBEDDINGS=1)
endif()
