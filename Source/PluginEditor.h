#pragma once
#include "PluginProcessor.h"

namespace tvui
{
inline juce::Colour gold()   { return juce::Colour (0xFFD4AF37); }
inline juce::Colour orange() { return juce::Colour (0xFFFF7A00); }
inline juce::Colour panel()  { return juce::Colour (0xFF141414); }
inline juce::Colour light()  { return juce::Colour (0xFFE8E0CC); }
}

class TopvoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TopvoxLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, tvui::light());
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, tvui::light());
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xFF1E1A10));
        setColour (juce::ComboBox::textColourId, tvui::gold());
        setColour (juce::ComboBox::outlineColourId, tvui::gold());
        setColour (juce::ComboBox::arrowColourId, tvui::orange());
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xFF1A1710));
        setColour (juce::PopupMenu::textColourId, tvui::light());
        setColour (juce::PopupMenu::highlightedBackgroundColourId, tvui::orange());
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float angle = start + pos * (end - start);

        juce::Path track;
        track.addCentredArc (c.x, c.y, radius - 2.0f, radius - 2.0f, 0.0f, start, end, true);
        g.setColour (juce::Colour (0xFF2B2B2B));
        g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val;
        val.addCentredArc (c.x, c.y, radius - 2.0f, radius - 2.0f, 0.0f, start, angle, true);
        g.setColour (tvui::orange());
        g.strokePath (val, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float kr = radius * 0.72f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xFF4A4A4A), c.x - kr * 0.4f, c.y - kr * 0.6f,
                                                 juce::Colour (0xFF0D0D0D), c.x + kr * 0.5f, c.y + kr, true));
        g.fillEllipse (c.x - kr, c.y - kr, 2.0f * kr, 2.0f * kr);
        g.setColour (tvui::gold().withAlpha (0.8f));
        g.drawEllipse (c.x - kr, c.y - kr, 2.0f * kr, 2.0f * kr, 1.5f);

        juce::Path p;
        p.addRoundedRectangle (-1.8f, -kr + 3.0f, 3.6f, kr * 0.5f, 1.5f);
        g.setColour (tvui::gold());
        g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.5f);
        const bool big = b.getComponentID() == "big";
        const float corner = big ? 14.0f : 7.0f;
        if (b.getToggleState())
        {
            g.setGradientFill (juce::ColourGradient (tvui::gold(), r.getX(), r.getY(), tvui::orange(), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, corner);
            if (big) { g.setColour (tvui::orange().withAlpha (0.35f)); g.drawRoundedRectangle (r.expanded (2.0f), corner + 2.0f, 3.0f); }
        }
        else
        {
            g.setColour (juce::Colour (over ? 0xFF2A2418 : 0xFF181510));
            g.fillRoundedRectangle (r, corner);
            g.setColour (tvui::orange());
            g.drawRoundedRectangle (r, corner, 2.0f);
        }
        if (down) { g.setColour (juce::Colours::black.withAlpha (0.25f)); g.fillRoundedRectangle (r, corner); }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const bool big = b.getComponentID() == "big";
        g.setFont (juce::Font (juce::FontOptions (big ? 30.0f : 14.0f, juce::Font::bold)));
        g.setColour (b.getToggleState() ? juce::Colours::black : tvui::gold());
        g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
    }
};

class VuMeter : public juce::Component
{
public:
    explicit VuMeter (juce::String n) : name (std::move (n)) {}
    void setLevelDb (float d) { level = d; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xFF2E2616), b.getCentreX(), b.getY(),
                                                 juce::Colour (0xFF0C0A06), b.getCentreX(), b.getBottom(), false));
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (tvui::gold().withAlpha (0.7f));
        g.drawRoundedRectangle (b, 10.0f, 1.5f);

        const float px = b.getCentreX(), py = b.getBottom() - 24.0f;
        const float r = juce::jmin (b.getHeight() - 56.0f, (b.getWidth() * 0.5f - 16.0f) / 0.79f);
        auto angleFor = [] (float db) { return -0.9f + juce::jlimit (0.0f, 1.0f, (db + 40.0f) / 43.0f) * 1.8f; };

        juce::Path arc1, arc2;
        arc1.addCentredArc (px, py, r, r, 0.0f, angleFor (-40.0f), angleFor (0.0f), true);
        arc2.addCentredArc (px, py, r, r, 0.0f, angleFor (0.0f), angleFor (3.0f), true);
        g.setColour (tvui::gold().withAlpha (0.6f));  g.strokePath (arc1, juce::PathStrokeType (2.0f));
        g.setColour (tvui::orange());                 g.strokePath (arc2, juce::PathStrokeType (4.0f));

        const float ticks[] = { -40.0f, -30.0f, -20.0f, -10.0f, -5.0f, 0.0f, 3.0f };
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::plain)));
        for (float t : ticks)
        {
            const float a = angleFor (t), s = std::sin (a), c = std::cos (a);
            g.setColour (t >= 0.0f ? tvui::orange() : tvui::gold());
            g.drawLine (px + s * r * 0.86f, py - c * r * 0.86f, px + s * r, py - c * r, 2.0f);
            g.setColour (tvui::light().withAlpha (0.8f));
            g.drawText (juce::String ((int) t),
                        juce::Rectangle<float> (0.0f, 0.0f, 30.0f, 14.0f).withCentre ({ px + s * r * 0.72f, py - c * r * 0.72f }),
                        juce::Justification::centred);
        }

        const float a = angleFor (level), s = std::sin (a), c = std::cos (a);
        g.setColour (tvui::orange());
        g.drawLine (px, py, px + s * r * 0.95f, py - c * r * 0.95f, 2.5f);
        g.fillEllipse (px - 6.0f, py - 6.0f, 12.0f, 12.0f);

        g.setColour (tvui::gold());
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (name + " (dB)", b.withTrimmedTop (b.getHeight() - 22.0f), juce::Justification::centred);
    }

private:
    juce::String name;
    float level = -60.0f;
};

class TopvoxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopvoxEditor (TopvoxProcessor&);
    ~TopvoxEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    TopvoxProcessor& proc;
    TopvoxLookAndFeel lnf;
    VuMeter vuIn { "ENTREE" }, vuOut { "SORTIE" };
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::TextButton autoMixBtn { "AUTO MIX" }, autoEqBtn { "AUTO EQ" }, autoGainBtn { "AUTO GAIN" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aMixAtt, aEqAtt, aGainAtt;
    juce::ComboBox presetBox;
    juce::Label presetLabel;
    float inDb = -60.0f, outDb = -60.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopvoxEditor)
};
