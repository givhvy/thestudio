#include "PluginWindow.h"

PluginWindow::PluginWindow(const juce::String& title, juce::AudioProcessorEditor* editor)
    : title_(title), editor_(editor)
{
    setOpaque(true);

    closeBtn_.setButtonText("X");
    closeBtn_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
    closeBtn_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff97316));
    closeBtn_.onClick = [this]() { if (onClose) onClose(); };
    addAndMakeVisible(closeBtn_);

    if (editor_ != nullptr)
    {
        addAndMakeVisible(editor_);
        // Size the window to the editor's natural size + title bar height.
        const int ew = juce::jmax(320, editor_->getWidth());
        const int eh = juce::jmax(180, editor_->getHeight());
        setSize(ew, eh + TITLE_H);
    }
    else
    {
        setSize(420, 280);
    }
}

PluginWindow::~PluginWindow()
{
    // Detach the editor child BEFORE PluginHost deletes it. JUCE will not
    // delete it for us because addAndMakeVisible doesn't transfer ownership.
    if (editor_ != nullptr)
        removeChildComponent(editor_);
}

void PluginWindow::paint(juce::Graphics& g)
{
    auto b = getLocalBounds();
    auto title = b.removeFromTop(TITLE_H);

    // Body background (helps if the editor doesn't fully repaint on resize)
    g.fillAll(juce::Colour(0xff0d0d10));

    // Title bar
    juce::ColourGradient tg(juce::Colour(0xff27272a), 0.0f, (float)title.getY(),
                            juce::Colour(0xff18181b), 0.0f, (float)title.getBottom(), false);
    g.setGradientFill(tg);
    g.fillRect(title);

    g.setColour(juce::Colour(0xff3f3f46));
    g.drawHorizontalLine(title.getBottom() - 1, 0.0f, (float)getWidth());

    // Title text
    g.setColour(juce::Colour(0xffe4e4e7));
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
    g.drawText(title_, title.reduced(8, 0), juce::Justification::centredLeft);

    // Outer frame
    g.setColour(juce::Colours::black);
    g.drawRect(getLocalBounds(), 1);
}

void PluginWindow::resized()
{
    auto b = getLocalBounds();
    auto title = b.removeFromTop(TITLE_H);

    // Close button on the right side of the title bar
    closeBtn_.setBounds(title.removeFromRight(TITLE_H).reduced(3));

    if (editor_ != nullptr)
        editor_->setBounds(b);
}

void PluginWindow::mouseDown(const juce::MouseEvent& e)
{
    isDraggingTitle_ = (e.y < TITLE_H);
    if (isDraggingTitle_)
        dragger_.startDraggingComponent(this, e);
}

void PluginWindow::mouseDrag(const juce::MouseEvent& e)
{
    if (isDraggingTitle_)
        dragger_.dragComponent(this, e, nullptr);
}
