#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Wheel.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void expect (bool condition, const std::string& message)
{
    if (! condition)
    {
        std::cerr << "FAIL " << message << "\n";
        ++failures;
    }
    else
    {
        std::cout << "ok   " << message << "\n";
    }
}

void setRaw (FrancEqAudioProcessor& processor, const juce::String& id, float raw)
{
    auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (id));
    if (parameter == nullptr)
    {
        expect (false, "missing parameter " + id.toStdString());
        return;
    }

    parameter->setValueNotifyingHost (parameter->convertTo0to1 (raw));
}

float getRaw (FrancEqAudioProcessor& processor, const juce::String& id)
{
    return processor.apvts.getRawParameterValue (id)->load();
}

struct Rendered
{
    std::vector<float> left;
    int latency = 0;
};

Rendered render (FrancEqAudioProcessor& processor, const std::vector<float>& input, double sampleRate, int block, int channels)
{
    if (channels == 1)
    {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::mono());
        layout.outputBuses.add (juce::AudioChannelSet::mono());
        expect (processor.setBusesLayout (layout), "mono layout is accepted");
    }

    processor.prepareToPlay (sampleRate, block);

    Rendered rendered;
    rendered.latency = processor.getLatencySamples();
    rendered.left.assign (input.size(), 0.0f);

    juce::AudioBuffer<float> buffer (channels, block);
    juce::MidiBuffer midi;
    size_t offset = 0;
    while (offset < input.size())
    {
        const int count = static_cast<int> (std::min (static_cast<size_t> (block), input.size() - offset));
        buffer.clear();
        for (int channel = 0; channel < channels; ++channel)
            for (int sample = 0; sample < count; ++sample)
                buffer.setSample (channel, sample, input[offset + static_cast<size_t> (sample)]);

        juce::AudioBuffer<float> slice (buffer.getArrayOfWritePointers(), channels, count);
        processor.processBlock (slice, midi);

        for (int sample = 0; sample < count; ++sample)
            rendered.left[offset + static_cast<size_t> (sample)] = slice.getSample (0, sample);

        offset += static_cast<size_t> (count);
    }

    return rendered;
}

std::vector<float> sine (int count, double sampleRate, double frequency, float amplitude)
{
    std::vector<float> data (static_cast<size_t> (count));
    for (int sample = 0; sample < count; ++sample)
        data[static_cast<size_t> (sample)] = amplitude * std::sin (2.0 * juce::MathConstants<double>::pi * frequency
                                                                    * static_cast<double> (sample) / sampleRate);
    return data;
}

bool allFinite (const std::vector<float>& data)
{
    for (float value : data)
        if (! std::isfinite (value))
            return false;
    return true;
}

double rms (const std::vector<float>& data, int start, int count)
{
    double sum = 0.0;
    int used = 0;
    const int end = std::min (start + count, static_cast<int> (data.size()));
    for (int sample = std::max (0, start); sample < end; ++sample)
    {
        const double value = data[static_cast<size_t> (sample)];
        sum += value * value;
        ++used;
    }

    return used > 0 ? std::sqrt (sum / static_cast<double> (used)) : 0.0;
}

double dbOf (double amplitude)
{
    return 20.0 * std::log10 (std::max (1.0e-12, amplitude));
}

double goertzel (const std::vector<float>& data, int start, int count, double frequency, double sampleRate)
{
    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const double coefficient = 2.0 * std::cos (omega);
    double s0 = 0.0;
    double s1 = 0.0;
    double s2 = 0.0;
    for (int sample = 0; sample < count; ++sample)
    {
        s0 = static_cast<double> (data[static_cast<size_t> (start + sample)]) + coefficient * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - coefficient * s1 * s2));
}

int impulsePeak (const std::vector<float>& data)
{
    int index = 0;
    float peak = 0.0f;
    for (int sample = 0; sample < static_cast<int> (data.size()); ++sample)
    {
        const float value = std::abs (data[static_cast<size_t> (sample)]);
        if (value > peak)
        {
            peak = value;
            index = sample;
        }
    }
    return index;
}
} // namespace

int runTests()
{
    constexpr double sampleRate = 48000.0;
    constexpr int block = 256;

    {
        FrancEqAudioProcessor processor;
        expect (processor.getName() == "FrancEq", "plug-in name is FrancEq");
        expect (processor.getLatencySamples() == 0, "latency is unset before prepare");
    }

    {
        FrancEqAudioProcessor processor;
        std::vector<float> impulse (block * 8, 0.0f);
        impulse[0] = 1.0f;
        const auto rendered = render (processor, impulse, sampleRate, block, 2);
        expect (rendered.latency > 0, "oversampled saturation reports latency (" + std::to_string (rendered.latency) + ")");
        expect (allFinite (rendered.left), "digital impulse is finite");
        const int peak = impulsePeak (rendered.left);
        expect (peak == rendered.latency, "digital flat impulse lands on the reported latency (peak " + std::to_string (peak) + ")");
        expect (std::abs (rendered.left[static_cast<size_t> (peak)] - 1.0f) < 1.0e-4f, "digital flat impulse peak is unity");
    }

    {
        FrancEqAudioProcessor processor;
        const auto input = sine (sampleRate, sampleRate, 1000.0, 0.25f);
        const auto rendered = render (processor, input, sampleRate, block, 2);
        expect (allFinite (rendered.left), "digital sine is finite");

        double maxError = 0.0;
        for (int sample = rendered.latency + 8; sample < static_cast<int> (input.size()); ++sample)
        {
            const double error = std::abs (static_cast<double> (rendered.left[static_cast<size_t> (sample)])
                                          - static_cast<double> (input[static_cast<size_t> (sample - rendered.latency)]));
            maxError = std::max (maxError, error);
        }

        const double inputRms = rms (input, 4000, 8000);
        const double outputRms = rms (rendered.left, 4000, 8000);
        std::cout << "     digital alignment error " << maxError
                  << "  level " << (dbOf (outputRms) - dbOf (inputRms)) << " dB\n";
        expect (maxError < 1.0e-4, "digital flat EQ matches the delayed input");
        expect (std::abs (dbOf (outputRms) - dbOf (inputRms)) < 0.05, "digital flat EQ is transparent in level");
    }

    {
        FrancEqAudioProcessor processor;
        setRaw (processor, ParamIDs::bandGain (3), 12.0f);
        setRaw (processor, ParamIDs::bandFreq (3), 1000.0f);
        setRaw (processor, ParamIDs::bandQ (3), 2.0f);
        const auto input = sine (static_cast<int> (sampleRate), sampleRate, 1000.0, 0.1f);
        const auto rendered = render (processor, input, sampleRate, block, 2);
        const double delta = dbOf (rms (rendered.left, 8000, 16000)) - dbOf (rms (input, 8000, 16000));
        std::cout << "     bell boost " << delta << " dB\n";
        expect (allFinite (rendered.left), "boosted bell is finite");
        expect (delta > 10.5 && delta < 13.5, "1 kHz bell boosts a 1 kHz sine by about 12 dB");
    }

    {
        FrancEqAudioProcessor processor;
        setRaw (processor, ParamIDs::bandType (0), 3.0f);
        setRaw (processor, ParamIDs::bandFreq (0), 300.0f);
        setRaw (processor, ParamIDs::bandSlope (0), 2.0f);
        setRaw (processor, ParamIDs::bandQ (0), 0.71f);
        const auto low = render (processor, sine (static_cast<int> (sampleRate), sampleRate, 50.0, 0.2f), sampleRate, block, 2);
        FrancEqAudioProcessor highProcessor;
        setRaw (highProcessor, ParamIDs::bandType (0), 3.0f);
        setRaw (highProcessor, ParamIDs::bandFreq (0), 300.0f);
        setRaw (highProcessor, ParamIDs::bandSlope (0), 2.0f);
        setRaw (highProcessor, ParamIDs::bandQ (0), 0.71f);
        const auto high = render (highProcessor, sine (static_cast<int> (sampleRate), sampleRate, 2000.0, 0.2f), sampleRate, block, 2);
        const double lowDrop = dbOf (rms (low.left, 8000, 16000)) - dbOf (0.2 / std::sqrt (2.0));
        const double highDrop = dbOf (rms (high.left, 8000, 16000)) - dbOf (0.2 / std::sqrt (2.0));
        std::cout << "     low cut 50 Hz " << lowDrop << " dB, 2 kHz " << highDrop << " dB\n";
        expect (lowDrop < -30.0, "48 dB/oct low cut attenuates 50 Hz");
        expect (std::abs (highDrop) < 2.0, "the same low cut leaves 2 kHz near unity");
    }

    {
        FrancEqAudioProcessor processor;
        setRaw (processor, ParamIDs::compEnabled, 1.0f);
        setRaw (processor, ParamIDs::compThreshold, -24.0f);
        setRaw (processor, ParamIDs::compRatio, 20.0f);
        setRaw (processor, ParamIDs::compKnee, 0.0f);
        setRaw (processor, ParamIDs::compAttack, 0.5f);
        setRaw (processor, ParamIDs::compRelease, 80.0f);
        setRaw (processor, ParamIDs::compMakeup, 0.0f);
        setRaw (processor, ParamIDs::compMix, 1.0f);
        setRaw (processor, ParamIDs::compAutoMakeup, 0.0f);
        setRaw (processor, ParamIDs::compPosition, 0.0f);
        const auto input = sine (static_cast<int> (sampleRate), sampleRate, 220.0, 0.5f);
        const auto rendered = render (processor, input, sampleRate, block, 2);
        const double delta = dbOf (rms (rendered.left, 24000, 12000)) - dbOf (rms (input, 24000, 12000));
        std::cout << "     compressor delta " << delta << " dB, GR " << processor.getGainReductionDb() << " dB\n";
        expect (allFinite (rendered.left), "compressor output is finite");
        expect (delta < -8.0, "compressor reduces a sine above threshold");
        expect (processor.getGainReductionDb() < -3.0f, "gain-reduction meter reads reduction");
    }

    {
        const auto input = sine (static_cast<int> (sampleRate), sampleRate, 1000.0, 0.25f);
        const double inputDb = dbOf (rms (input, 8000, 8000));
        for (int mode = 1; mode <= 3; ++mode)
        {
            FrancEqAudioProcessor processor;
            setRaw (processor, ParamIDs::character, static_cast<float> (mode));
            const auto rendered = render (processor, input, sampleRate, block, 2);
            const double delta = dbOf (rms (rendered.left, 8000, 8000)) - inputDb;
            float peak = 0.0f;
            for (float sample : rendered.left)
                peak = std::max (peak, std::abs (sample));
            std::cout << "     character " << mode << " level " << delta << " dB, peak " << peak << "\n";
            expect (allFinite (rendered.left), "character mode " + std::to_string (mode) + " is finite");
            expect (std::abs (delta) < 2.0, "character mode " + std::to_string (mode) + " stays near unity on a moderate sine");
            expect (peak < 2.0f, "character mode " + std::to_string (mode) + " does not explode");
        }
    }

    {
        const auto input = sine (static_cast<int> (sampleRate), sampleRate, 1000.0, 0.5f);
        auto ratio = [] (FrancEqAudioProcessor& processor, int harmonic)
        {
            const auto rendered = render (processor, sine (static_cast<int> (sampleRate), sampleRate, 1000.0, 0.5f), sampleRate, block, 2);
            const int start = static_cast<int> (sampleRate) - 4800;
            const double fundamental = goertzel (rendered.left, start, 4800, 1000.0, sampleRate);
            const double overtone = goertzel (rendered.left, start, 4800, 1000.0 * harmonic, sampleRate);
            return overtone / std::max (1.0e-12, fundamental);
        };

        FrancEqAudioProcessor digital;
        FrancEqAudioProcessor britN;
        setRaw (britN, ParamIDs::character, 1.0f);
        FrancEqAudioProcessor britS;
        setRaw (britS, ParamIDs::character, 2.0f);
        const double digitalSecond = ratio (digital, 2);
        const double neveSecond = ratio (britN, 2);
        const double digitalThird = ratio (digital, 3);
        const double sslThird = ratio (britS, 3);
        std::cout << "     H2 digital " << digitalSecond << " brit N " << neveSecond
                  << "  H3 digital " << digitalThird << " brit S " << sslThird << "\n";
        expect (neveSecond > digitalSecond * 4.0 && neveSecond > 1.0e-4, "Brit N adds even harmonics");
        expect (sslThird > digitalThird * 4.0 && sslThird > 1.0e-4, "Brit S adds odd harmonics");
        juce::ignoreUnused (input);
    }

    {
        franceq::DesignRequest digital;
        digital.type = franceq::BandType::bell;
        digital.sampleRate = sampleRate;
        digital.frequency = 1000.0f;
        digital.gainDb = 18.0f;
        digital.q = franceq::shapedQ (franceq::Character::digital, franceq::BandType::bell, 1.0f, 18.0f);
        digital.shoulder = 0.0f;
        auto api = digital;
        api.q = franceq::shapedQ (franceq::Character::usA, franceq::BandType::bell, 1.0f, 18.0f);
        const auto digitalCoeffs = franceq::designBand (digital);
        const auto apiCoeffs = franceq::designBand (api);
        const float digitalSide = franceq::bandMagnitudeDb (digitalCoeffs, 1800.0f, sampleRate);
        const float apiSide = franceq::bandMagnitudeDb (apiCoeffs, 1800.0f, sampleRate);
        std::cout << "     proportional Q side digital " << digitalSide << " dB, US A " << apiSide << " dB\n";
        expect (api.q > digital.q + 0.5f, "US A raises Q as gain grows");
        expect (apiSide < digitalSide - 1.0f, "US A bell is narrower than digital at the same gain");
    }

    {
        FrancEqAudioProcessor processor;
        const auto rendered = render (processor, sine (4000, sampleRate, 440.0, 0.2f), sampleRate, 128, 1);
        expect (allFinite (rendered.left), "mono processing is finite");
    }

    {
        FrancEqAudioProcessor processor;
        processor.prepareToPlay (sampleRate, 128);
        juce::AudioBuffer<double> buffer (2, 128);
        juce::MidiBuffer midi;
        for (int blockIndex = 0; blockIndex < 20; ++blockIndex)
        {
            for (int sample = 0; sample < 128; ++sample)
            {
                const float value = 0.2f * std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f
                                                     * static_cast<float> (blockIndex * 128 + sample) / static_cast<float> (sampleRate));
                buffer.setSample (0, sample, value);
                buffer.setSample (1, sample, value);
            }
            processor.processBlock (buffer, midi);
        }

        bool finite = true;
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < 128; ++sample)
                finite = finite && std::isfinite (buffer.getSample (channel, sample));
        expect (finite, "double-precision processing is finite");
    }

    {
        FrancEqAudioProcessor processor;
        setRaw (processor, ParamIDs::bandGain (4), 6.5f);
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        setRaw (processor, ParamIDs::bandGain (4), 0.0f);
        processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        expect (std::abs (getRaw (processor, ParamIDs::bandGain (4)) - 6.5f) < 0.05f, "state save and restore keeps band gain");
    }

    {
        FrancEqAudioProcessor processor;
        setRaw (processor, ParamIDs::bandGain (3), 12.0f);
        setRaw (processor, ParamIDs::bandFreq (3), 1000.0f);
        setRaw (processor, ParamIDs::bypass, 1.0f);
        const auto input = sine (static_cast<int> (sampleRate) / 2, sampleRate, 1000.0, 0.2f);
        const auto rendered = render (processor, input, sampleRate, block, 2);
        const double delta = dbOf (rms (rendered.left, 4000, 8000)) - dbOf (rms (input, 4000, 8000));
        expect (std::abs (delta) < 0.05, "bypass ignores the EQ boost");
    }

    {
        for (int oddBlock : { 1, 7, 1024 })
        {
            FrancEqAudioProcessor processor;
            const auto rendered = render (processor, sine (3000, 96000.0, 1000.0, 0.3f), 96000.0, oddBlock, 2);
            expect (allFinite (rendered.left), "finite at 96 kHz, block " + std::to_string (oddBlock));
        }
    }

    {
        expect (std::abs (franceq::frequencyAfterNotches (440.0f, 6.0f) - 880.0f) < 0.05f,
                "six frequency notches are one octave");
        expect (std::abs (franceq::frequencyAfterNotches (1000.0f, 1.0f) / 1000.0f - std::pow (2.0f, 1.0f / 6.0f)) < 1.0e-4f,
                "one frequency notch is a sixth of an octave");
        expect (std::abs (franceq::frequencyAfterNotches (1000.0f, 0.1f) / 1000.0f - std::pow (2.0f, 0.1f / 6.0f)) < 1.0e-4f,
                "fine frequency notches are a tenth of a coarse step");

        float proportion = 0.0f;
        for (int notch = 0; notch < 6; ++notch)
            proportion = franceq::proportionAfterNotches (proportion, 1.0f, franceq::bandWheelNotches);
        expect (std::abs (proportion - 1.0f) < 1.0e-4f, "six bandwidth notches cover the Q range");

        juce::MouseWheelDetails discrete {};
        discrete.deltaY = 0.039f;
        discrete.isSmooth = false;
        expect (std::abs (franceq::notchesFromWheel (discrete) - 1.0f) < 1.0e-4f, "a small mouse click is one notch");

        juce::MouseWheelDetails smooth {};
        smooth.deltaY = 0.05f;
        smooth.isSmooth = true;
        expect (std::abs (franceq::notchesFromWheel (smooth) - 0.25f) < 1.0e-4f, "trackpad deltas add up in notch units");

        FrancEqAudioProcessor processor;
        auto* q = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (ParamIDs::bandQ (3)));
        const float start = 0.71f;
        const float next = q->convertFrom0to1 (franceq::proportionAfterNotches (q->convertTo0to1 (start), 1.0f, franceq::bandWheelNotches));
        const float previous = start * std::pow (1.07f, 1.0f);
        std::cout << "     bandwidth wheel " << start << " -> " << next << " (old step " << previous << ")\n";
        expect (next > previous * 1.4f, "a node wheel notch changes bandwidth much faster than the old 7 percent step");
        expect (next < 4.0f, "one bandwidth notch does not jump the whole Q range");
    }

    {
        FrancEqAudioProcessor processor;
        processor.setEditorSize (1500, 980);
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        processor.setEditorSize (FrancEqAudioProcessor::defaultEditorWidth, FrancEqAudioProcessor::defaultEditorHeight);
        processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        expect (processor.getEditorWidth() == 1500 && processor.getEditorHeight() == 980, "plugin state keeps the editor size");
        processor.setEditorSize (20, 20);
        expect (processor.getEditorWidth() == FrancEqAudioProcessor::minEditorWidth
                    && processor.getEditorHeight() == FrancEqAudioProcessor::minEditorHeight,
                "editor size clamps to the minimum");
        processor.setEditorSize (4000, 4000);
        expect (processor.getEditorWidth() == FrancEqAudioProcessor::maxEditorWidth
                    && processor.getEditorHeight() == FrancEqAudioProcessor::maxEditorHeight,
                "editor size clamps to the maximum");
    }

    if (failures == 0)
        std::cout << "All sanity checks passed.\n";
    else
        std::cout << failures << " sanity check(s) failed.\n";

    return failures == 0 ? 0 : 1;
}

int renderSnapshot (const char* path, int width, int height)
{
    juce::ScopedJuceInitialiser_GUI gui;
    FrancEqAudioProcessor processor;

    setRaw (processor, ParamIDs::character, 1.0f);
    setRaw (processor, ParamIDs::bandEnabled (0), 1.0f);
    setRaw (processor, ParamIDs::bandType (0), 3.0f);
    setRaw (processor, ParamIDs::bandFreq (0), 35.0f);
    setRaw (processor, ParamIDs::bandSlope (0), 1.0f);
    setRaw (processor, ParamIDs::bandType (1), 1.0f);
    setRaw (processor, ParamIDs::bandFreq (1), 140.0f);
    setRaw (processor, ParamIDs::bandGain (1), 3.5f);
    setRaw (processor, ParamIDs::bandType (2), 0.0f);
    setRaw (processor, ParamIDs::bandFreq (2), 420.0f);
    setRaw (processor, ParamIDs::bandGain (2), 5.0f);
    setRaw (processor, ParamIDs::bandQ (2), 0.9f);
    setRaw (processor, ParamIDs::bandFreq (3), 900.0f);
    setRaw (processor, ParamIDs::bandGain (3), -3.5f);
    setRaw (processor, ParamIDs::bandQ (3), 1.6f);
    setRaw (processor, ParamIDs::bandFreq (4), 3200.0f);
    setRaw (processor, ParamIDs::bandGain (4), 4.0f);
    setRaw (processor, ParamIDs::bandQ (4), 1.1f);
    setRaw (processor, ParamIDs::bandType (5), 2.0f);
    setRaw (processor, ParamIDs::bandFreq (5), 8000.0f);
    setRaw (processor, ParamIDs::bandGain (5), 3.0f);
    setRaw (processor, ParamIDs::bandType (6), 4.0f);
    setRaw (processor, ParamIDs::bandFreq (6), 16000.0f);
    setRaw (processor, ParamIDs::bandSlope (6), 0.0f);

    setRaw (processor, ParamIDs::compEnabled, 1.0f);
    setRaw (processor, ParamIDs::compThreshold, -16.0f);
    setRaw (processor, ParamIDs::compRatio, 4.0f);
    setRaw (processor, ParamIDs::compAttack, 8.0f);
    setRaw (processor, ParamIDs::compRelease, 140.0f);
    setRaw (processor, ParamIDs::compKnee, 6.0f);
    setRaw (processor, ParamIDs::compMix, 1.0f);

    std::vector<float> input (static_cast<size_t> (48000));
    for (size_t sample = 0; sample < input.size(); ++sample)
    {
        const double time = static_cast<double> (sample) / 48000.0;
        const float tone = 0.18f * std::sin (2.0 * juce::MathConstants<double>::pi * 140.0 * time)
                         + 0.14f * std::sin (2.0 * juce::MathConstants<double>::pi * 420.0 * time)
                         + 0.16f * std::sin (2.0 * juce::MathConstants<double>::pi * 1200.0 * time)
                         + 0.10f * std::sin (2.0 * juce::MathConstants<double>::pi * 3200.0 * time)
                         + 0.08f * std::sin (2.0 * juce::MathConstants<double>::pi * 8000.0 * time);
        const float noise = 0.04f * std::sin (2.0 * juce::MathConstants<double>::pi * (1800.0 + 40.0 * std::sin (time * 3.0)) * time);
        input[sample] = tone + noise;
    }

    render (processor, input, 48000.0, 256, 2);

    if (width == FrancEqAudioProcessor::minEditorWidth && height == FrancEqAudioProcessor::minEditorHeight)
    {
        FrancEqAudioProcessor restored;
        auto* open = restored.createEditorAndMakeActive();
        expect (open != nullptr
                    && open->getWidth() == FrancEqAudioProcessor::defaultEditorWidth
                    && open->getHeight() == FrancEqAudioProcessor::defaultEditorHeight,
                "a new editor opens at the default size");

        restored.setEditorSize (1500, 960);
        juce::MemoryBlock block;
        restored.getStateInformation (block);
        open->setSize (FrancEqAudioProcessor::defaultEditorWidth, FrancEqAudioProcessor::defaultEditorHeight);
        restored.setEditorSize (FrancEqAudioProcessor::defaultEditorWidth, FrancEqAudioProcessor::defaultEditorHeight);
        restored.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
        expect (open->getWidth() == 1500 && open->getHeight() == 960,
                "an open editor follows the size stored in the plug-in state");
        restored.editorBeingDeleted (open);
        delete open;
        if (failures != 0)
            return 4;
    }

    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditor());
    editor->setSize (width, height);
    if (auto* france = dynamic_cast<FrancEqAudioProcessorEditor*> (editor.get()))
    {
        const auto problems = france->layoutProblems();
        if (problems.isNotEmpty())
        {
            std::cerr << "Layout at " << width << "x" << height << ": " << problems << "\n";
            return 3;
        }

        france->refreshForSnapshot();
        france->resized();
    }

    juce::Image image (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
    {
        juce::Graphics graphics (image);
        editor->paintEntireComponent (graphics, true);
    }

    juce::File output (path);
    output.getParentDirectory().createDirectory();
    if (auto stream = output.createOutputStream())
    {
        juce::PNGImageFormat png;
        if (png.writeImageToStream (image, *stream))
        {
            std::cout << "Wrote " << path << "\n";
            return 0;
        }
    }

    std::cerr << "Could not write " << path << "\n";
    return 2;
}

int main (int argc, char** argv)
{
    const int result = runTests();
    const char* snapshot = nullptr;
    const char* gallery = nullptr;
    int width = FrancEqAudioProcessor::defaultEditorWidth;
    int height = FrancEqAudioProcessor::defaultEditorHeight;

    for (int index = 1; index < argc; ++index)
    {
        if (std::strcmp (argv[index], "--snapshot") == 0 && index + 1 < argc)
            snapshot = argv[++index];
        else if (std::strcmp (argv[index], "--gallery") == 0 && index + 1 < argc)
            gallery = argv[++index];
        else if (std::strcmp (argv[index], "--width") == 0 && index + 1 < argc)
            width = std::atoi (argv[++index]);
        else if (std::strcmp (argv[index], "--height") == 0 && index + 1 < argc)
            height = std::atoi (argv[++index]);
    }

    if (gallery != nullptr)
    {
        const int sizes[][2] = { { 640, 400 }, { 1120, 700 }, { 1800, 1150 } };
        const char* names[] = { "franceq-640x400.png", "franceq-1120x700.png", "franceq-1800x1150.png" };
        for (int index = 0; index < 3; ++index)
        {
            const auto file = juce::File (gallery).getChildFile (names[index]);
            if (renderSnapshot (file.getFullPathName().toRawUTF8(), sizes[index][0], sizes[index][1]) != 0)
                return 2;
        }
    }

    if (snapshot != nullptr)
        return renderSnapshot (snapshot, width, height) == 0 ? result : 2;

    return result;
}
