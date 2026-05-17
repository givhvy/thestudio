cmake_minimum_required(VERSION 3.22)
project(StratumVSTHost VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- JUCE ---------------------------------------------------------------
# Clone JUCE alongside this folder:
#   git clone https://github.com/juce-framework/JUCE.git vst-host/JUCE
# -------------------------------------------------------------------------
add_compile_definitions(
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
)
add_subdirectory(JUCE)

juce_add_gui_app(StratumVSTHost
    PRODUCT_NAME "Stratum DAW"
    VERSION 1.0.0
    ICON_BIG "src/app_icon.png"
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
)

target_sources(StratumVSTHost PRIVATE
    src/main.cpp
    src/JsonRpcServer.cpp
    src/AudioEngine.cpp
    src/PluginHost.cpp
    src/PluginWindow.cpp
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
    src/VideoPanel.cpp
)

target_compile_definitions(StratumVSTHost PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_AU=0
    _CRT_SECURE_NO_WARNINGS=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2=1
    JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(StratumVSTHost PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
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
