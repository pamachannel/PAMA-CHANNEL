#include "PluginEditor.h"

namespace
{
struct KnobDef { const char* id; const char* text; const char* suffix; int decimals; };
const KnobDef kKnobs[16] = {
    { "inGain", "ENTREE", " dB", 1 },       { "gate", "NOISE GATE", " dB", 0 },
    { "eqLow", "EQ BAS", " dB", 1 },        { "eqMid", "EQ MEDIUM", " dB", 1 },
    { "eqAir", "EQ AIR", " dB", 1 },        { "reson", "RESONANCES", " %", 0 },
    { "comp", "COMPRESSEUR", " %", 0 },     { "deess", "DE-ESSER", " %", 0 },
    { "sat", "SATURATION", " %", 0 },       { "revMix", "REVERB", " %", 0 },
    { "revSize", "TAILLE REVERB", " %", 0 },{ "dlyMix", "DELAY", " %", 0 },
    { "dlyTime", "TEMPS DELAY", " ms", 0 }, { "dlyFb", "FEEDBACK", " %", 0 },
    { "limit", "LIMITEUR", " dB", 1 },      { "outGain", "SORTIE", " dB", 1 },
};
}

TopvoxEditor::TopvoxEditor (TopvoxProcessor& p) : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setSize (960, 600);

    addAndMakeVisible (vuIn);
    addAndMakeVisible (vuOut);

    for (const auto& d : kKnobs)
    {
        auto k = std::make_unique<Knob>();
        k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
        k->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, d.id, k->slider);
        k->slider.setTextValueSuffix (d.suffix);
        k->slider.setNumDecimalPlacesToDisplay (d.decimals);
        k->label.setText (d.text, juce::dontSendNotification);
        k->label.setJustificationType (juce::Justification::centred);
        k->label.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
        k->label.setColour (juce::Label::textColourId, tvui::gold());
        k->label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (k->slider);
        addAndMakeVisible (k->label);
        knobs.push_back (std::move (k));
    }

    autoMixBtn.setComponentID ("big");
    for (auto* b : { &autoMixBtn, &autoEqBtn, &autoGainBtn })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }
    aMixAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "autoMix", autoMixBtn);
    aEqAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "autoEq", autoEqBtn);
    aGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "autoGain", autoGainBtn);

    presetLabel.setText ("PRESET", juce::dontSendNotification);
    presetLabel.setColour (juce::Label::textColourId, tvui::gold());
    presetLabel.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    addAndMakeVisible (presetLabel);
    presetBox.addItemList (TopvoxProcessor::presetNames(), 1);
    presetBox.setSelectedItemIndex (proc.getCurrentPreset(), juce::dontSendNotification);
    presetBox.onChange = [this] { proc.applyPreset (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);

    startTimerHz (30);
}

TopvoxEditor::~TopvoxEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TopvoxEditor::timerCallback()
{
    auto toDb = [] (float peak) { return juce::Decibels::gainToDecibels (peak, -60.0f); };
    inDb = juce::jmax (toDb (proc.inPeak.exchange (0.0f)), inDb - 1.5f);
    outDb = juce::jmax (toDb (proc.outPeak.exchange (0.0f)), outDb - 1.5f);
    vuIn.setLevelDb (inDb);
    vuOut.setLevelDb (outDb);
}

void TopvoxEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF0A0A0A));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xFF241E10), 0.0f, 0.0f, juce::Colour (0xFF0A0A0A), 0.0f, 72.0f, false));
    g.fillRect (0, 0, getWidth(), 72);
    g.setColour (tvui::gold());
    g.fillRect (0, 71, getWidth(), 2);

    g.setColour (tvui::gold());
    g.setFont (juce::Font (juce::FontOptions (34.0f, juce::Font::bold)));
    g.drawText ("TOPVOX AI", 24, 4, 230, 42, juce::Justification::centredLeft);
    g.setColour (tvui::orange());
    g.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
    g.drawText ("STUDIO", 232, 10, 140, 34, juce::Justification::centredLeft);
    g.setColour (tvui::light().withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::plain)));
    g.drawText ("Mixage vocal automatique - 100% hors ligne", 26, 46, 360, 18, juce::Justification::centredLeft);

    for (int row = 0; row < 2; ++row)
    {
        const juce::Rectangle<float> r (16.0f, row == 0 ? 254.0f : 428.0f, 928.0f, 166.0f);
        g.setColour (tvui::panel());
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (tvui::gold().withAlpha (0.25f));
        g.drawRoundedRectangle (r, 10.0f, 1.0f);
    }
}

void TopvoxEditor::resized()
{
    presetLabel.setBounds (560, 20, 70, 32);
    presetBox.setBounds (630, 20, 300, 32);

    vuIn.setBounds (20, 84, 280, 162);
    vuOut.setBounds (660, 84, 280, 162);
    autoMixBtn.setBounds (330, 92, 300, 86);
    autoEqBtn.setBounds (330, 192, 146, 40);
    autoGainBtn.setBounds (484, 192, 146, 40);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int row = (int) i / 8, col = (int) i % 8;
        const int x = 20 + col * 115, y = row == 0 ? 258 : 432;
        knobs[i]->label.setBounds (x, y, 115, 18);
        knobs[i]->slider.setBounds (x, y + 18, 115, 140);
    }
}
