cmake_minimum_required(VERSION 3.22)
project(StratumVSTHost VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- JUCE ---------------------------------------------------------------
# Clone JUCE alongside this folder:
#   git clone https://github.com/juce-framework/JUCE.git vst-host/JUCE
# -------------------------------------------------------------------------
add_compile_definitions(JUCE_WEB_BROWSER=1)
if(WIN32)
    add_compile_definitions(
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    )
endif()
add_subdirectory(JUCE)

set(STRATUM_APP_OPTIONS
    PRODUCT_NAME "Stratum DAW"
    VERSION 1.0.0
    ICON_BIG "src/app_icon.png"
    NEEDS_WEB_BROWSER TRUE
)

if(WIN32)
    list(APPEND STRATUM_APP_OPTIONS NEEDS_WEBVIEW2 TRUE)
endif()

if(APPLE)
    list(APPEND STRATUM_APP_OPTIONS
        BUNDLE_ID "com.stratum.daw"
        ICON_FILE "src/app_icon.png"
    )
endif()

juce_add_gui_app(StratumVSTHost
    ${STRATUM_APP_OPTIONS}
)

target_sources(StratumVSTHost PRIVATE
    src/main.cpp
    src/JsonRpcServer.cpp
    src/AudioEngine.cpp
    src/PluginHost.cpp
    src/PluginWindow.cpp
    src/NativeEffectEditor.cpp
    src/NativeBridge.cpp
    src/WebBrowserHost.cpp
    src/AppWindow.cpp
    src/ReverbEffect.cpp
    src/MainComponent.cpp
    src/TransportBar.cpp
    src/ChannelRack.cpp
    src/Mixer.cpp
    src/Browser.cpp
    src/Playlist.cpp
    src/BottomDock.cpp
    src/PianoRoll.cpp
    src/AIPanel.cpp
    src/PatternsPanel.cpp
    src/MarketplacePanel.cpp
    src/DrumMidiParser.cpp
    src/VideoPanel.cpp
    src/ChordAnalysisEngine.cpp
    src/ChordifyMidiImporter.cpp
    src/ChordifyAutomationEngine.cpp
    src/ConsistencyPanel.cpp
    src/Midi808ImportSettings.cpp
    src/AgentRegistry.cpp
    src/OrgChartPanel.cpp
    src/YouTubePanel.cpp
)

target_compile_definitions(StratumVSTHost PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=0
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=1
    JUCE_MODAL_LOOPS_PERMITTED=1
)

if(WIN32)
    target_compile_definitions(StratumVSTHost PRIVATE
        _CRT_SECURE_NO_WARNINGS=1
        JUCE_PLUGINHOST_AU=0
        JUCE_USE_WIN_WEBVIEW2=1
    )
elseif(APPLE)
    target_compile_definitions(StratumVSTHost PRIVATE
        JUCE_PLUGINHOST_AU=1
    )
else()
    target_compile_definitions(StratumVSTHost PRIVATE
        JUCE_PLUGINHOST_AU=0
    )
endif()

target_link_libraries(StratumVSTHost PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_processors_headless
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_dsp
    juce::juce_events
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
    PUBLIC
        juce::juce_recommended_lto_flags
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/popular-song-midi")
    add_custom_command(TARGET StratumVSTHost POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/data/popular-song-midi"
            "$<TARGET_FILE_DIR:StratumVSTHost>/popular-song-midi"
        COMMENT "Bundling popular-song-midi drum library"
    )
endif()

if(APPLE)
    set(STRATUM_FRONTEND_DIST "${CMAKE_CURRENT_SOURCE_DIR}/../frontend/dist")
    if(EXISTS "${STRATUM_FRONTEND_DIST}/index.html")
        add_custom_command(TARGET StratumVSTHost POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:StratumVSTHost>/Contents/Resources/frontend"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${STRATUM_FRONTEND_DIST}"
                "$<TARGET_BUNDLE_DIR:StratumVSTHost>/Contents/Resources/frontend/dist"
            COMMENT "Bundling frontend/dist into Stratum DAW.app"
        )
    else()
        message(WARNING "frontend/dist was not found. Run the frontend build before packaging macOS.")
    endif()
endif()
