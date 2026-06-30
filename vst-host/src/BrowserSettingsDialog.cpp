#include "BrowserSettingsDialog.h"

BrowserSettingsDialog::BrowserSettingsDialog (const Paths& current, juce::Component* parentForBounds)
    : paths_ (current), parentForBounds_ (parentForBounds)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
}

BrowserSettingsDialog::~BrowserSettingsDialog() = default;

void BrowserSettingsDialog::resized()
{
    // Layout — one row per library, each row 36px tall, plus header + footer.
    const int rowH    = 36;
    const int padX    = 18;
    const int padTop  = 56;
    const int labelW  = 110;
    const int fieldW  = juce::jmax(260, getWidth() - padX*2 - labelW - 32 - 32 - 12);
    const int btnW    = 32;

    for (int i = 0; i < kNumLibs; ++i)
    {
        const int y = padTop + i * rowH;
        rowRects_[i][0] = { padX, y, labelW, rowH - 8 };                                    // label
        rowRects_[i][1] = { padX + labelW, y, fieldW, rowH - 8 };                            // text field
        rowRects_[i][2] = { rowRects_[i][1].getRight() + 6, y, btnW, rowH - 8 };             // Browse
        rowRects_[i][3] = { rowRects_[i][2].getRight() + 4, y, btnW, rowH - 8 };             // Reset
    }

    // Footer buttons
    const int footerY = padTop + kNumLibs * rowH + 16;
    const int btnFW = 110;
    const int btnFH = 32;
    okRect_      = { getWidth() - padX - btnFW,                 footerY, btnFW, btnFH };
    cancelRect_ = { okRect_.getX()  - 8 - btnFW,                footerY, btnFW, btnFH };
    resetAllRect_ = { padX,                                 footerY, 130, btnFH };
}

void BrowserSettingsDialog::paint (juce::Graphics& g)
{
    // Dim background (drawn by caller behind the dialog). Fill ourselves.
    g.setColour (juce::Colour (0xf0101216));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 14.0f);
    g.setColour (juce::Colour (0x33000000));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 14.0f, 1.0f);

    // Title
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions().withName ("Segoe UI").withHeight (18.0f).withStyle ("Bold"));
    g.drawText ("Library Folders", 18, 18, getWidth() - 36, 28, juce::Justification::left);

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (juce::FontOptions().withName ("Segoe UI").withHeight (10.5f));
    g.drawText ("Choose where Stratum looks for drum kits, loops, acapellas, etc. Empty = default.",
               18, 36, getWidth() - 36, 16, juce::Justification::left);

    // Rows
    g.setFont (juce::FontOptions().withName ("Segoe UI").withHeight (12.0f));
    juce::String* pathPtrs[kNumLibs] = {
        &paths_.drums, &paths_.loops, &paths_.acapella,
        &paths_.tags, &paths_.marketplace, &paths_.all
    };
    for (int i = 0; i < kNumLibs; ++i)
    {
        const auto& r = rowRects_[i][0];
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawText (rowLabels_[i], r, juce::Justification::centredLeft);

        // Field background
        const auto fr = rowRects_[i][1];
        g.setColour (juce::Colour (0xff18181c));
        g.fillRoundedRectangle (fr.toFloat(), 5.0f);
        g.setColour (juce::Colour (0x33ffffff));
        g.drawRoundedRectangle (fr.toFloat().reduced (0.5f), 5.0f, 1.0f);

        // Field text
        g.setColour (pathPtrs[i]->isEmpty()
                        ? juce::Colours::white.withAlpha (0.35f)
                        : juce::Colours::white);
        g.setFont (juce::FontOptions().withName ("Menlo").withHeight (11.0f));
        const juce::String display = pathPtrs[i]->isEmpty() ? juce::String ("(default)") : *pathPtrs[i];
        g.drawText (display, fr.reduced (8, 0), juce::Justification::centredLeft);

        // Browse + Reset
        drawChrome (g, rowRects_[i][2], false);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions().withName ("Segoe UI").withHeight (11.0f).withStyle ("Bold"));
        g.drawText ("\u2026", rowRects_[i][2], juce::Justification::centred);

        drawChrome (g, rowRects_[i][3], false);
        g.setColour (juce::Colours::white);
        g.drawText ("\u21BA", rowRects_[i][3], juce::Justification::centred);
    }

    // Footer
    drawChrome (g, okRect_,     true);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions().withName ("Segoe UI").withHeight (12.0f).withStyle ("Bold"));
    g.drawText ("OK",      okRect_,     juce::Justification::centred);
    drawChrome (g, cancelRect_, false);
    g.drawText ("Cancel",  cancelRect_, juce::Justification::centred);
    drawChrome (g, resetAllRect_, false);
    g.setFont (juce::FontOptions().withName ("Segoe UI").withHeight (10.5f));
    g.drawText ("Reset all", resetAllRect_, juce::Justification::centred);
}

void BrowserSettingsDialog::drawChrome (juce::Graphics& g, juce::Rectangle<int> r, bool isOk)
{
    const auto f = r.toFloat().reduced (2.0f);
    g.setColour (isOk ? juce::Colour (0xff2a7be0) : juce::Colour (0xff2a2a30));
    g.fillRoundedRectangle (f, 5.0f);
    g.setColour (isOk ? juce::Colour (0x66ffffff) : juce::Colour (0x33ffffff));
    g.drawRoundedRectangle (f.reduced (0.5f), 5.0f, 1.0f);
}

void BrowserSettingsDialog::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < kNumLibs; ++i)
    {
        if (rowRects_[i][2].contains (e.x, e.y)) { openFolderPicker (i); return; }
        if (rowRects_[i][3].contains (e.x, e.y)) { doReset (i); return; }
    }
    if (okRect_.contains (e.x, e.y))     { doOk();     return; }
    if (cancelRect_.contains (e.x, e.y)) { doCancel(); return; }
    if (resetAllRect_.contains (e.x, e.y))
    {
        paths_ = Paths{};
        repaint();
    }
}

bool BrowserSettingsDialog::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)  { doCancel(); return true; }
    if (key == juce::KeyPress::returnKey) { doOk();     return true; }
    return false;
}

void BrowserSettingsDialog::openFolderPicker (int row)
{
    juce::String startPath;
    juce::String* pathPtrs[kNumLibs] = {
        &paths_.drums, &paths_.loops, &paths_.acapella,
        &paths_.tags,  &paths_.marketplace, &paths_.all
    };
    if (pathPtrs[row]->isNotEmpty() && juce::File (*pathPtrs[row]).isDirectory())
        startPath = *pathPtrs[row];
    else
        startPath = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getFullPathName();

    auto chooser = std::make_unique<juce::FileChooser> (
        "Choose " + rowLabels_[row] + " folder", juce::File (startPath), "");
    juce::Component::SafePointer<BrowserSettingsDialog> safe (this);
    int capturedRow = row;
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                          | juce::FileBrowserComponent::canSelectDirectories,
        [safe, capturedRow, chooser = chooser.get()] (const juce::FileChooser& fc) mutable
    {
        if (auto* dlg = safe.getComponent())
        {
            if (fc.getResult().exists())
            {
                juce::String* pathPtrs[] = {
                    &dlg->paths_.drums, &dlg->paths_.loops, &dlg->paths_.acapella,
                    &dlg->paths_.tags,  &dlg->paths_.marketplace, &dlg->paths_.all
                };
                *pathPtrs[capturedRow] = fc.getResult().getFullPathName();
                dlg->repaint();
            }
        }
    });
}

void BrowserSettingsDialog::doReset (int row)
{
    juce::String* pathPtrs[kNumLibs] = {
        &paths_.drums, &paths_.loops, &paths_.acapella,
        &paths_.tags,  &paths_.marketplace, &paths_.all
    };
    *pathPtrs[row] = juce::String();
    repaint();
}

void BrowserSettingsDialog::doOk()     { if (onResult_) onResult_ (paths_); if (auto* p = getParentComponent()) p->removeChildComponent (this); }
void BrowserSettingsDialog::doCancel() { if (auto* p = getParentComponent()) p->removeChildComponent (this); }

void BrowserSettingsDialog::runModal (OnResult onResult)
{
    onResult_ = onResult;

    // Centre on parent
    juce::Rectangle<int> b (560, 360);
    if (parentForBounds_ != nullptr)
    {
        const auto pr = parentForBounds_->getBoundsInParent();
        b.setCentre (pr.getCentreX(), pr.getCentreY());
    }
    else
    {
        const auto d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
        b.setCentre (d.getCentreX(), d.getCentreY());
    }
    setBounds (b);
    setVisible (true);
    grabKeyboardFocus();
}
