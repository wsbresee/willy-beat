cmake_minimum_required(VERSION 3.22)
project(Drumwright VERSION 2.0.1)

set(CMAKE_CXX_STANDARD 17)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Drumwright
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
    PRODUCT_NAME                "Drumwright"
    ICON_BIG                    "${CMAKE_SOURCE_DIR}/Resources/logo.png"
    ICON_SMALL                  "${CMAKE_SOURCE_DIR}/Resources/logo.png"
)

juce_generate_juce_header(Drumwright)

target_sources(Drumwright
    PRIVATE
        Source/PatternLibrary.cpp
        Source/VariationGenerator.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DrumwrightLookAndFeel.cpp
        Source/StockDrums.cpp
        Source/TagVectorIndex.mm
)

target_compile_definitions(Drumwright
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Drumwright
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
    target_link_libraries(Drumwright PRIVATE ${NATURAL_LANGUAGE_FRAMEWORK})

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

    set(PRESET_BUNDLE "${BAKED_DIR}/PresetBundle.bin")

    add_custom_command(
        OUTPUT  "${PRESET_BUNDLE}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/Tools/pack_presets.py"
                "${PRESETS_DIR}" "${PRESET_BUNDLE}"
        DEPENDS ${BAKE_PRESET_SOURCES}
        COMMENT "pack_presets: bundling starter preset bank"
        VERBATIM)

    juce_add_binary_data(DrumwrightBakedData
        HEADER_NAME BinaryData.h
        NAMESPACE   BinaryData
        SOURCES     "${BAKED_BIN}"
                    "${PRESET_BUNDLE}"
                    "${CMAKE_SOURCE_DIR}/Resources/logo_ui.png")

    target_link_libraries(Drumwright PRIVATE DrumwrightBakedData)
    target_compile_definitions(Drumwright PRIVATE
        JUCE_INCLUDED_BAKED_EMBEDDINGS=1
        JUCE_INCLUDED_PRESET_BUNDLE=1)
endif()
