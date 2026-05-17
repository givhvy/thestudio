#include "AppWindow.h"

#if JUCE_WINDOWS
 // Forward-declare Win32 bits to avoid <windows.h> macro pollution.
 using HWND_t = void*;
 extern "C" __declspec(dllimport) long __stdcall GetWindowLongW (HWND_t, int);
 extern "C" __declspec(dllimport) long __stdcall SetWindowLongW (HWND_t, int, long);
 #define GWL_STYLE_         (-16)
 #define WS_MINIMIZEBOX_    0x00020000L
 #define WS_MAXIMIZEBOX_    0x00010000L
 #define WS_SYSMENU_        0x00080000L
#endif

AppWindow::AppWindow (PluginHost& pluginHost, AudioEngine& audioEngine)
    : DocumentWindow ("Stratum DAW", juce::Colour(0xff0a0a0a), 0)
{
    mainComponent = std::make_unique<MainComponent>(pluginHost, audioEngine);
    setUsingNativeTitleBar (false);
    setTitleBarHeight (0);
    setContentNonOwned (mainComponent.get(), true);
    setResizable(true, true);

    // Try to restore the last saved window position/size; fall back to a
    // sensible default centred on screen. canPersist_ stays false during
    // construction so JUCE's own resize callbacks during setup don't trash
    // the saved bounds.
    restoreWindowState();
    if (getWidth() < 200 || getHeight() < 200)
        centreWithSize (1280, 800);

    setVisible (true);

    // From here on, every user-driven move/resize gets persisted.
    canPersist_ = true;

   #if JUCE_WINDOWS
    // Add WS_MINIMIZEBOX/WS_MAXIMIZEBOX/WS_SYSMENU to the borderless window so
    // Windows treats it as a normal taskbar app (click-to-minimize, Win+D, etc.).
    if (auto* peer = getPeer())
    {
        auto hwnd = (HWND_t) peer->getNativeHandle();
        long style = GetWindowLongW (hwnd, GWL_STYLE_);
        SetWindowLongW (hwnd, GWL_STYLE_,
                        style | WS_MINIMIZEBOX_ | WS_MAXIMIZEBOX_ | WS_SYSMENU_);
    }
   #endif
}

void AppWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

void AppWindow::minimiseButtonPressed()
{
    DocumentWindow::minimiseButtonPressed();
}

void AppWindow::maximiseButtonPressed()
{
    if (mainComponent)
        mainComponent->toggleMaximize();
}

juce::File AppWindow::windowStateFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
              .getChildFile ("Stratum DAW")
              .getChildFile ("window.state");
}

void AppWindow::saveWindowState()
{
    auto f = windowStateFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (getWindowStateAsString());
}

void AppWindow::restoreWindowState()
{
    auto f = windowStateFile();
    if (! f.existsAsFile()) return;
    auto s = f.loadFileAsString();
    if (s.isNotEmpty())
        restoreWindowStateFromString (s);
}
