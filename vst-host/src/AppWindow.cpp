#include "AppWindow.h"

AppWindow::AppWindow (PluginHost& pluginHost, AudioEngine& audioEngine)
    : DocumentWindow ("Stratum DAW", juce::Colours::darkgrey, 0)
{
    webHost = std::make_unique<WebBrowserHost> (pluginHost, audioEngine);
    setUsingNativeTitleBar (false);
    setTitleBarHeight (0);
    setContentNonOwned (webHost.get(), true);
    centreWithSize (1280, 800);
    setVisible (true);
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
    DocumentWindow::maximiseButtonPressed();
}
