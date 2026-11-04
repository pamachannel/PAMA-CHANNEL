#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const char* kPresetIds[15] = { "inGain", "gate", "eqLow", "eqMid", "eqAir", "reson", "comp", "deess",
                               "sat", "revMix", "revSize", "dlyMix", "dlyTime", "dlyFb", "limit" };
//                          in   gate  low   mid   air  res  comp des  sat  rvM rvS dlM dlT  dlF lim
const float kPresets[6][15] = {
    { 0, -55,  0.0f, 0.0f, 0.0f, 30, 40, 40, 10, 10, 40,  0, 250, 25, -1 },   // Neutre
    { 0, -50,  1.0f, 1.5f, 2.0f, 35, 70, 50, 25,  8, 30, 10, 180, 20, -1 },   // Rap
    { 0, -58,  0.0f, 1.0f, 2.5f, 25, 45, 45, 12, 22, 55,  8, 300, 25, -1 },   // Chant
    { 0, -55, -1.5f, 1.0f, 1.5f, 40, 55, 35, 15, 10, 35,  0, 250, 25, -1 },   // Voix masculine
    { 0, -55, -2.0f, 0.5f, 2.0f, 25, 45, 60,  8, 14, 45,  0, 250, 25, -1 },   // Voix feminine
    { 0, -50,  1.5f, 2.0f, 3.0f, 35, 75, 50, 35, 12, 35, 18, 220, 30, -1 },   // Coupe-decale
};
}

juce::StringArray TopvoxProcessor::presetNames()
{
    juce::StringArray a;
    a.add ("Neutre");
    a.add ("Rap");
    a.add ("Chant");
    a.add ("Voix masculine");
    a.add (juce::String::fromUTF8 ("Voix f\xc3\xa9minine"));
    a.add (juce::String::fromUTF8 ("Coup\xc3\xa9-d\xc3\xa9" "cal\xc3\xa9"));
    return a;
}

juce::AudioProcessorValueTreeState::ParameterLayout TopvoxProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto f = [&l] (const char* id, const char* name, float lo, float hi, float step, float def)
    {
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                                                            juce::NormalisableRange<float> (lo, hi, step), def));
    };
    auto b = [&l] (const char* id, const char* name, bool def)
    {
        l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    };
    b ("autoMix", "Auto Mix", true);
    b ("autoEq", "Auto EQ", true);
    b ("autoGain", "Auto Gain", true);
    f ("inGain", "Entree", -12.0f, 12.0f, 0.1f, 0.0f);
    f ("gate", "Noise Gate", -80.0f, -20.0f, 0.5f, -55.0f);
    f ("eqLow", "EQ Bas", -6.0f, 6.0f, 0.1f, 0.0f);
    f ("eqMid", "EQ Medium", -6.0f, 6.0f, 0.1f, 0.0f);
    f ("eqAir", "EQ Air", -6.0f, 6.0f, 0.1f, 0.0f);
    f ("reson", "Resonances", 0.0f, 100.0f, 1.0f, 30.0f);
    f ("comp", "Compresseur", 0.0f, 100.0f, 1.0f, 40.0f);
    f ("deess", "De-Esser", 0.0f, 100.0f, 1.0f, 40.0f);
    f ("sat", "Saturation", 0.0f, 100.0f, 1.0f, 10.0f);
    f ("revMix", "Reverb", 0.0f, 100.0f, 1.0f, 10.0f);
    f ("revSize", "Taille Reverb", 0.0f, 100.0f, 1.0f, 40.0f);
    f ("dlyMix", "Delay", 0.0f, 100.0f, 1.0f, 0.0f);
    f ("dlyTime", "Temps Delay", 50.0f, 800.0f, 1.0f, 250.0f);
    f ("dlyFb", "Feedback Delay", 0.0f, 80.0f, 1.0f, 25.0f);
    f ("limit", "Limiteur", -6.0f, 0.0f, 0.1f, -1.0f);
    f ("outGain", "Sortie", -12.0f, 12.0f, 0.1f, 0.0f);
    return l;
}

TopvoxProcessor::TopvoxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TOPVOX", createLayout())
{
    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pr.autoMix = g ("autoMix"); pr.autoEq = g ("autoEq"); pr.autoGain = g ("autoGain");
    pr.inGain = g ("inGain"); pr.gate = g ("gate"); pr.eqLow = g ("eqLow"); pr.eqMid = g ("eqMid");
    pr.eqAir = g ("eqAir"); pr.reson = g ("reson"); pr.comp = g ("comp"); pr.deess = g ("deess");
    pr.sat = g ("sat"); pr.revMix = g ("revMix"); pr.revSize = g ("revSize"); pr.dlyMix = g ("dlyMix");
    pr.dlyTime = g ("dlyTime"); pr.dlyFb = g ("dlyFb"); pr.limit = g ("limit"); pr.outGain = g ("outGain");
}

bool TopvoxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == out;
}

void TopvoxProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    engine.prepare (sr);
    reverb.setSampleRate (sr);
    reverb.reset();
}

void TopvoxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = juce::jmin (2, buffer.getNumChannels());
    if (n <= 0 || nch <= 0) return;
    for (int c = nch; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    float* l = buffer.getWritePointer (0);
    float* r = nch > 1 ? buffer.getWritePointer (1) : nullptr;

    tv::VoiceSettings s;
    s.autoMix = pr.autoMix->load() > 0.5f;
    s.autoEq = pr.autoEq->load() > 0.5f;
    s.autoGain = pr.autoGain->load() > 0.5f;
    s.inGain = pr.inGain->load(); s.gate = pr.gate->load();
    s.eqLow = pr.eqLow->load(); s.eqMid = pr.eqMid->load(); s.eqAir = pr.eqAir->load();
    s.reson = pr.reson->load(); s.comp = pr.comp->load(); s.deess = pr.deess->load(); s.sat = pr.sat->load();
    s.dlyMix = pr.dlyMix->load(); s.dlyTime = pr.dlyTime->load(); s.dlyFb = pr.dlyFb->load();
    s.limit = pr.limit->load(); s.outGain = pr.outGain->load();

    float inPk = 0.0f, outPk = 0.0f;
    engine.processChain (l, r, n, s, inPk);

    const float rv = pr.revMix->load() / 100.0f;
    if (rv > 0.002f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.25f + 0.70f * (pr.revSize->load() / 100.0f);
        rp.damping = 0.55f;
        rp.wetLevel = rv * 0.12f;
        rp.dryLevel = 0.5f;
        rp.width = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
        if (r != nullptr) reverb.processStereo (l, r, n); else reverb.processMono (l, n);
    }

    engine.processOutput (l, r, n, s, outPk);

    inPeak.store (juce::jmax (inPeak.load(), inPk));
    outPeak.store (juce::jmax (outPeak.load(), outPk));
}

void TopvoxProcessor::applyPreset (int index)
{
    if (index < 0 || index > 5) return;
    currentPreset.store (index);
    for (int i = 0; i < 15; ++i)
        if (auto* p = apvts.getParameter (kPresetIds[i]))
            p->setValueNotifyingHost (p->convertTo0to1 (kPresets[index][i]));
}

juce::AudioProcessorEditor* TopvoxProcessor::createEditor() { return new TopvoxEditor (*this); }

void TopvoxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void TopvoxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentPreset.store ((int) apvts.state.getProperty ("preset", 0));
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TopvoxProcessor(); }
