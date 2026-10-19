#include "PluginEditor.h"

SolomonEditor::SolomonEditor (SolomonProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    title.setText ("Solomon", juce::dontSendNotification);
    title.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, juce::Colour (0xff3ddc97));
    addAndMakeVisible (title);

    gainSlider.setRange (0.0, 1.0, 0.01);
    gainSlider.setValue (proc.gain.load(), juce::dontSendNotification);
    gainSlider.setTooltip ("Output level");
    gainSlider.onValueChange = [this] { proc.gain.store ((float) gainSlider.getValue()); };
    addAndMakeVisible (gainSlider);

    modeBox.addItemList ({ "Mode A", "Mode B", "Mode C" }, 1);
    modeBox.setSelectedItemIndex (proc.state.mode, juce::dontSendNotification);
    modeBox.onChange = [this] { proc.state.mode = juce::jmax (0, modeBox.getSelectedItemIndex()); };
    addAndMakeVisible (modeBox);

    // resizable, with a fixed aspect ratio; the host-provided corner works in Live, Reaper, Logic
    setResizable (true, true);
    setResizeLimits (360, 200, 1440, 800);
    getConstrainer()->setFixedAspectRatio (720.0 / 400.0);
    setSize (720, 400);
    startTimerHz (10);
}

SolomonEditor::~SolomonEditor()
{
    stopTimer();
}

void SolomonEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff161a20));
}

void SolomonEditor::resized()
{
    auto r = getLocalBounds().reduced (16);
    title.setBounds (r.removeFromTop (40));
    r.removeFromTop (12);
    gainSlider.setBounds (r.removeFromTop (32));
    r.removeFromTop (12);
    modeBox.setBounds (r.removeFromTop (28).removeFromLeft (220));
}

void SolomonEditor::timerCallback()
{
    // the host may reload state while the editor is open
    const int rev = proc.stateRevision.load();
    if (rev != seenRevision)
    {
        seenRevision = rev;
        gainSlider.setValue (proc.gain.load(), juce::dontSendNotification);
        modeBox.setSelectedItemIndex (proc.state.mode, juce::dontSendNotification);
    }
}
