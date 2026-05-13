#include "AppWindow.h"

AppWindow::AppWindow (PluginHost& pluginHost, AudioEngine& audioEngine)
    : DocumentWindow ("Stratum DAW", juce::Colour(0xff0a0a0a), 0)
{
    mainComponent = std::make_unique<MainComponent>(pluginHost, audioEngine);
    setUsingNativeTitleBar (false);
    setTitleBarHeight (0);
    setContentNonOwned (mainComponent.get(), true);
    centreWithSize (1280, 800);
    setResizable(true, true);
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
    if (mainComponent)
        mainComponent->toggleMaximize();
}
