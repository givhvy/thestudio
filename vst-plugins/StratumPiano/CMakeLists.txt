juce_add_plugin(StratumPiano
    COMPANY_NAME "Stratum"
    COMPANY_WEBSITE "https://stratum.local"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_COPY_DIR "${STRATUM_PLUGIN_INSTALL_DIR}"
    PLUGIN_MANUFACTURER_CODE Stra
    PLUGIN_CODE Pia1
    FORMATS AU VST3
    PRODUCT_NAME "Stratum Piano")

target_sources(StratumPiano
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp)

target_compile_definitions(StratumPiano
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(StratumPiano
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
