#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
juce::String formatSignedDb (float value, int maximumLength)
{
    juce::String text = juce::String (value, 1) + " dB";
    if (value > 0.05f)
        text = "+" + text;

    return maximumLength > 0 ? text.substring (0, maximumLength) : text;
}

float parseDb (const juce::String& text)
{
    auto trimmed = text.trim().toLowerCase();
    trimmed = trimmed.replace ("db", {}).trim();
    if (trimmed.startsWithChar ('+'))
        trimmed = trimmed.substring (1);
    return trimmed.getFloatValue();
}

juce::String formatHz (float value, int maximumLength)
{
    juce::String text;
    if (value >= 10000.0f)
        text = juce::String (value / 1000.0f, 1) + " kHz";
    else if (value >= 1000.0f)
        text = juce::String (value / 1000.0f, 2) + " kHz";
    else
        text = juce::String (juce::roundToInt (value)) + " Hz";

    return maximumLength > 0 ? text.substring (0, maximumLength) : text;
}

float parseHz (const juce::String& text)
{
    const auto trimmed = text.trim().toLowerCase();
    const float number = trimmed.getFloatValue();
    return trimmed.containsChar ('k') ? number * 1000.0f : number;
}

juce::String formatPlain (float value, int maximumLength, int decimals)
{
    const auto text = juce::String (value, decimals);
    return maximumLength > 0 ? text.substring (0, maximumLength) : text;
}

juce::String formatPlainDb (float value, int maximumLength)
{
    const auto text = juce::String (value, 1) + " dB";
    return maximumLength > 0 ? text.substring (0, maximumLength) : text;
}

float parsePlain (const juce::String& text)
{
    return text.trim().getFloatValue();
}

juce::String formatMs (float value, int maximumLength)
{
    const int decimals = value < 10.0f ? 2 : (value < 100.0f ? 1 : 0);
    const auto text = juce::String (value, decimals) + " ms";
    return maximumLength > 0 ? text.substring (0, maximumLength) : text;
}

float parseMs (const juce::String& text)
{
    auto trimmed = text.trim().toLowerCase();
    if (trimmed.endsWith ("ms"))
        trimmed = trimmed.dropLastCharacters (2);
    return trimmed.trim().getFloatValue();
}

juce::String formatRatio (float value, int maximumLength)
{
    const auto text = juce::String (value, value < 10.0f ? 2 : 1) + " : 1";
    return maximumLength > 0 ? text.substring (0, maximumLength) : text;
}

float parseRatio (const juce::String& text)
{
    return text.trim().upToFirstOccurrenceOf (":", false, false).trim().getFloatValue();
}

juce::String formatPercent (float value, int maximumLength)
{
    const auto text = juce::String (juce::roundToInt (value * 100.0f)) + " %";
    return maximumLength > 0 ? text.substring (0, maximumLength) : text;
}

float parsePercent (const juce::String& text)
{
    const auto trimmed = text.trim().toLowerCase();
    const float number = trimmed.getFloatValue();
    if (trimmed.containsChar ('%') || number > 1.0f)
        return number / 100.0f;
    return number;
}

juce::String formatQ (float value, int maximumLength)
{
    return formatPlain (value, maximumLength, 2);
}

void addFloat (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
               const juce::String& id,
               const juce::String& name,
               juce::NormalisableRange<float> range,
               float value,
               const juce::String& label,
               juce::String (*toText) (float, int),
               float (*fromText) (const juce::String&))
{
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 },
        name,
        std::move (range),
        value,
        juce::AudioParameterFloatAttributes()
            .withLabel (label)
            .withStringFromValueFunction (toText)
            .withValueFromStringFunction (fromText)));
}

bool near (float a, float b, float tolerance) noexcept
{
    return std::abs (a - b) <= tolerance;
}

float shapeSample (float x, float pre, float even, float odd) noexcept
{
    if (std::abs (even) < 1.0e-6f && std::abs (odd) < 1.0e-6f && std::abs (pre - 1.0f) < 1.0e-5f)
        return x;

    const float safePre = std::max (0.25f, pre);
    const float u = safePre * x;
    const float y = std::tanh (u + even * u * u + odd * u * u * u);
    return y / safePre;
}

float shapeCompressor (float x, float even, float odd) noexcept
{
    if (even <= 0.0f && odd <= 0.0f)
        return x;

    const float x2 = x * x;
    return x + even * x2 * (1.0f - 0.65f * std::abs (x)) + odd * x * x2 * (1.0f - 0.45f * x2);
}

float gainComputerDb (float inputDb, float thresholdDb, float ratio, float kneeDb) noexcept
{
    const float over = inputDb - thresholdDb;
    const float reduction = (1.0f / std::max (1.0f, ratio)) - 1.0f;

    if (kneeDb <= 0.01f)
        return over <= 0.0f ? 0.0f : reduction * over;

    const float half = kneeDb * 0.5f;
    if (over <= -half)
        return 0.0f;
    if (over >= half)
        return reduction * over;

    const float x = over + half;
    return reduction * (x * x) / (2.0f * kneeDb);
}

struct CompTiming
{
    float attackScale = 1.0f;
    float releaseScale = 1.0f;
    float kneeAdd = 0.0f;
    float even = 0.0f;
    float odd = 0.0f;
    bool feedback = false;
    bool programRelease = false;
};

struct SatTarget
{
    float pre = 1.0f;
    float even = 0.0f;
    float odd = 0.0f;
    float blend = 0.0f;
};

CompTiming timingFor (franceq::Character character) noexcept
{
    switch (character)
    {
        case franceq::Character::britN:
            return { 1.45f, 1.30f, 2.5f, 0.07f, 0.02f, false, false };
        case franceq::Character::britS:
            return { 0.55f, 1.00f, 0.0f, 0.00f, 0.09f, false, true };
        case franceq::Character::usA:
            return { 0.80f, 1.10f, 1.0f, 0.05f, 0.05f, true, false };
        case franceq::Character::digital:
            break;
    }

    return {};
}

SatTarget saturationFor (franceq::Character character) noexcept
{
    switch (character)
    {
        case franceq::Character::britN:
            return { 1.82f, 0.22f, 0.055f, 1.0f };
        case franceq::Character::britS:
            return { 1.84f, 0.008f, 0.21f, 1.0f };
        case franceq::Character::usA:
            return { 1.85f, 0.13f, 0.14f, 1.0f };
        case franceq::Character::digital:
            break;
    }

    return {};
}

franceq::Character characterFrom (float raw) noexcept
{
    return static_cast<franceq::Character> (juce::jlimit (0, 3, juce::roundToInt (raw)));
}

franceq::BandType typeFrom (float raw) noexcept
{
    return static_cast<franceq::BandType> (juce::jlimit (0, 5, juce::roundToInt (raw)));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout FrancEqAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto hzRange = juce::NormalisableRange<float> {
        franceq::minFrequency,
        franceq::maxFrequency,
        [] (float start, float end, float proportion)
        {
            return start * std::pow (end / start, proportion);
        },
        [] (float start, float end, float value)
        {
            const float clamped = juce::jlimit (start, end, value);
            return std::log (clamped / start) / std::log (end / start);
        },
    };

    addFloat (layout, ParamIDs::inputGain, "Input Gain", { -24.0f, 24.0f, 0.01f }, 0.0f, "dB", formatSignedDb, parseDb);
    addFloat (layout, ParamIDs::outputGain, "Output Gain", { -24.0f, 24.0f, 0.01f }, 0.0f, "dB", formatSignedDb, parseDb);

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::bypass, 1 },
        "Bypass",
        false,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction (
            [] (bool enabled, int) { return enabled ? juce::String ("On") : juce::String ("Off"); })));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::character, 1 },
        "Character",
        juce::StringArray { "Digital", "Brit N", "Brit S", "US A" },
        0));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::compEnabled, 1 },
        "Compressor",
        false,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction (
            [] (bool enabled, int) { return enabled ? juce::String ("On") : juce::String ("Off"); })));

    addFloat (layout, ParamIDs::compThreshold, "Threshold", { -60.0f, 0.0f, 0.01f }, -18.0f, "dB", formatSignedDb, parseDb);
    addFloat (layout, ParamIDs::compRatio, "Ratio", { 1.0f, 20.0f, 0.01f, 0.5f }, 4.0f, "", formatRatio, parseRatio);
    addFloat (layout, ParamIDs::compAttack, "Attack", { 0.1f, 200.0f, 0.01f, 0.35f }, 10.0f, "ms", formatMs, parseMs);
    addFloat (layout, ParamIDs::compRelease, "Release", { 5.0f, 2000.0f, 0.01f, 0.35f }, 120.0f, "ms", formatMs, parseMs);
    addFloat (layout, ParamIDs::compKnee, "Knee", { 0.0f, 24.0f, 0.01f }, 6.0f, "dB", formatPlainDb, parsePlain);
    addFloat (layout, ParamIDs::compMakeup, "Makeup", { -12.0f, 24.0f, 0.01f }, 0.0f, "dB", formatSignedDb, parseDb);
    addFloat (layout, ParamIDs::compMix, "Comp Mix", { 0.0f, 1.0f, 0.001f }, 1.0f, "%", formatPercent, parsePercent);

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::compAutoMakeup, 1 },
        "Auto Makeup",
        false,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction (
            [] (bool enabled, int) { return enabled ? juce::String ("On") : juce::String ("Off"); })));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::compPosition, 1 },
        "Compressor Position",
        juce::StringArray { "Post", "Pre" },
        0));

    for (int band = 0; band < franceq::bandCount; ++band)
    {
        const auto number = juce::String (band + 1);
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::bandEnabled (band), 1 },
            "Band " + number + " Enable",
            true));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ParamIDs::bandType (band), 1 },
            "Band " + number + " Type",
            juce::StringArray { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch" },
            0));

        addFloat (layout,
                  ParamIDs::bandFreq (band),
                  "Band " + number + " Frequency",
                  hzRange,
                  franceq::defaultBandFrequency (band),
                  "Hz",
                  formatHz,
                  parseHz);
        addFloat (layout,
                  ParamIDs::bandGain (band),
                  "Band " + number + " Gain",
                  { -24.0f, 24.0f, 0.01f },
                  0.0f,
                  "dB",
                  formatSignedDb,
                  parseDb);
        addFloat (layout,
                  ParamIDs::bandQ (band),
                  "Band " + number + " Q",
                  { 0.1f, 10.0f, 0.001f, 0.45f },
                  0.71f,
                  "",
                  formatQ,
                  parsePlain);

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ParamIDs::bandSlope (band), 1 },
            "Band " + number + " Slope",
            juce::StringArray { "12 dB/oct", "24 dB/oct", "48 dB/oct" },
            1));
    }

    return layout;
}

FrancEqAudioProcessor::FrancEqAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FrancEq", createParameterLayout())
{
    inputGainPtr = apvts.getRawParameterValue (ParamIDs::inputGain);
    outputGainPtr = apvts.getRawParameterValue (ParamIDs::outputGain);
    bypassPtr = apvts.getRawParameterValue (ParamIDs::bypass);
    characterPtr = apvts.getRawParameterValue (ParamIDs::character);
    compEnabledPtr = apvts.getRawParameterValue (ParamIDs::compEnabled);
    compThresholdPtr = apvts.getRawParameterValue (ParamIDs::compThreshold);
    compRatioPtr = apvts.getRawParameterValue (ParamIDs::compRatio);
    compAttackPtr = apvts.getRawParameterValue (ParamIDs::compAttack);
    compReleasePtr = apvts.getRawParameterValue (ParamIDs::compRelease);
    compKneePtr = apvts.getRawParameterValue (ParamIDs::compKnee);
    compMakeupPtr = apvts.getRawParameterValue (ParamIDs::compMakeup);
    compMixPtr = apvts.getRawParameterValue (ParamIDs::compMix);
    compAutoMakeupPtr = apvts.getRawParameterValue (ParamIDs::compAutoMakeup);
    compPositionPtr = apvts.getRawParameterValue (ParamIDs::compPosition);

    for (int band = 0; band < franceq::bandCount; ++band)
    {
        bandPtr[band].enabled = apvts.getRawParameterValue (ParamIDs::bandEnabled (band));
        bandPtr[band].type = apvts.getRawParameterValue (ParamIDs::bandType (band));
        bandPtr[band].frequency = apvts.getRawParameterValue (ParamIDs::bandFreq (band));
        bandPtr[band].gain = apvts.getRawParameterValue (ParamIDs::bandGain (band));
        bandPtr[band].q = apvts.getRawParameterValue (ParamIDs::bandQ (band));
        bandPtr[band].slope = apvts.getRawParameterValue (ParamIDs::bandSlope (band));
    }
}

void FrancEqAudioProcessor::DelayLine::prepare (int maxDelaySamples, int numChannelsToUse)
{
    capacity = std::max (1, maxDelaySamples) + 1;
    const auto channels = static_cast<size_t> (std::max (1, numChannelsToUse));
    buffer.assign (static_cast<size_t> (capacity) * channels, 0.0f);
    writePos = 0;
    delaySamples = 0;
}

void FrancEqAudioProcessor::DelayLine::setDelay (int samples) noexcept
{
    delaySamples = juce::jlimit (0, std::max (0, capacity - 1), samples);
}

void FrancEqAudioProcessor::DelayLine::reset() noexcept
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    writePos = 0;
}

float FrancEqAudioProcessor::DelayLine::processSample (int channel, float input) noexcept
{
    if (delaySamples <= 0 || buffer.empty())
        return input;

    float* data = buffer.data() + static_cast<size_t> (channel) * static_cast<size_t> (capacity);
    int read = writePos - delaySamples;
    if (read < 0)
        read += capacity;

    const float output = data[read];
    data[writePos] = input;
    return output;
}

void FrancEqAudioProcessor::DelayLine::advance() noexcept
{
    if (delaySamples <= 0 || capacity <= 1)
        return;

    ++writePos;
    if (writePos >= capacity)
        writePos = 0;
}

void FrancEqAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    if (sampleRate <= 0.0)
        return;

    currentSampleRate = sampleRate;
    maxBlock = juce::jlimit (1, 8192, samplesPerBlock);

    oversampling.reset();
    oversampling.initProcessing (static_cast<size_t> (maxBlock));
    latencySamples = static_cast<int> (std::lround (static_cast<double> (oversampling.getLatencyInSamples())));
    latencySamples = std::max (0, latencySamples);
    setLatencySamples (latencySamples);

    const int delayCapacity = std::max (8, latencySamples + 8);
    dryDelay.prepare (delayCapacity, 2);
    wetDelay.prepare (delayCapacity, 2);
    dryDelay.setDelay (latencySamples);
    wetDelay.setDelay (latencySamples);

    eqBuffer.setSize (2, maxBlock, false, false, true);
    digBuffer.setSize (2, maxBlock, false, false, true);
    floatScratch.setSize (2, maxBlock, false, false, true);

    const auto preparedSamples = static_cast<size_t> (maxBlock);
    satPre.assign (preparedSamples, 1.0f);
    satEven.assign (preparedSamples, 0.0f);
    satOdd.assign (preparedSamples, 0.0f);
    blendEnv.assign (preparedSamples, 0.0f);
    outGainEnv.assign (preparedSamples, 1.0f);
    bypassEnv.assign (preparedSamples, 0.0f);
    dryEnvL.assign (preparedSamples, 0.0f);
    dryEnvR.assign (preparedSamples, 0.0f);
    preMono.assign (preparedSamples, 0.0f);
    compFrames.assign (preparedSamples, CompFrame {});

    chunkCharacter = characterFrom (characterPtr->load());
    const auto saturation = saturationFor (chunkCharacter);

    const auto resetLinear = [sampleRate] (juce::SmoothedValue<float>& smoother, float seconds, float value)
    {
        smoother.reset (sampleRate, seconds);
        smoother.setCurrentAndTargetValue (value);
    };

    const auto resetMulti = [sampleRate] (auto& smoother, float seconds, float value)
    {
        smoother.reset (sampleRate, seconds);
        smoother.setCurrentAndTargetValue (std::max (1.0e-3f, value));
    };

    resetLinear (shoulderSmooth, 0.04f, chunkCharacter == franceq::Character::britN ? 1.0f : 0.0f);
    resetLinear (inputSmooth, 0.02f, franceq::dbToGain (inputGainPtr->load()));
    resetLinear (outputSmooth, 0.02f, franceq::dbToGain (outputGainPtr->load()));
    resetLinear (bypassSmooth, 0.02f, bypassPtr->load() >= 0.5f ? 1.0f : 0.0f);
    resetLinear (satPreSmooth, 0.04f, saturation.pre);
    resetLinear (satEvenSmooth, 0.04f, saturation.even);
    resetLinear (satOddSmooth, 0.04f, saturation.odd);
    resetLinear (satBlendSmooth, 0.03f, saturation.blend);
    resetLinear (compEnableSmooth, 0.015f, compEnabledPtr->load() >= 0.5f ? 1.0f : 0.0f);
    resetLinear (compThresholdSmooth, 0.02f, compThresholdPtr->load());
    resetLinear (compRatioSmooth, 0.02f, compRatioPtr->load());
    resetLinear (compAttackSmooth, 0.02f, compAttackPtr->load());
    resetLinear (compReleaseSmooth, 0.02f, compReleasePtr->load());
    resetLinear (compKneeSmooth, 0.02f, compKneePtr->load());
    resetLinear (compMakeupSmooth, 0.02f, compMakeupPtr->load());
    resetLinear (compMixSmooth, 0.02f, compMixPtr->load());

    for (int band = 0; band < franceq::bandCount; ++band)
    {
        cache[band] = {};
        cache[band].frequency = -1.0f;
        cache[band].type = -1;

        for (int channel = 0; channel < 2; ++channel)
        {
            runtime[channel][band].reset();
            runtime[channel][band].count = 0;
            runtime[channel][band].type = -1;
            runtime[channel][band].notchDepth = 0.0f;
        }

        const float frequency = std::max (franceq::minFrequency, bandPtr[band].frequency->load());
        const float gain = bandPtr[band].gain->load();
        const auto type = typeFrom (bandPtr[band].type->load());
        resetMulti (freqSmooth[band], 0.04f, frequency);
        resetLinear (gainSmooth[band], 0.03f, gain);
        resetMulti (qSmooth[band], 0.03f, franceq::shapedQ (chunkCharacter, type, bandPtr[band].q->load(), gain));
        resetLinear (enableSmooth[band], 0.015f, bandPtr[band].enabled->load() >= 0.5f ? 1.0f : 0.0f);
    }

    dcBlock[0].prepare (sampleRate);
    dcBlock[1].prepare (sampleRate);
    compEnv = 0.0f;
    compGrDb = 0.0f;
    compLastOut = 0.0f;
    gainReductionDb.store (0.0f, std::memory_order_relaxed);
    preRing.fill (0.0f);
    postRing.fill (0.0f);
    analyserWrite.store (0, std::memory_order_relaxed);
}

void FrancEqAudioProcessor::releaseResources()
{
    currentSampleRate = 0.0;
    oversampling.reset();
    dryDelay.reset();
    wetDelay.reset();
    compEnv = 0.0f;
    compGrDb = 0.0f;
    compLastOut = 0.0f;
    gainReductionDb.store (0.0f, std::memory_order_relaxed);

    for (int channel = 0; channel < 2; ++channel)
        for (int band = 0; band < franceq::bandCount; ++band)
            runtime[channel][band].reset();
}

bool FrancEqAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    if (input != output)
        return false;

    return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
}

void FrancEqAudioProcessor::updateBand (int band, const franceq::DesignRequest& request)
{
    auto& slot = cache[static_cast<size_t> (band)];
    if (slot.type == static_cast<int> (request.type)
        && slot.slope == request.slopeIndex
        && near (slot.frequency, request.frequency, 0.002f)
        && near (slot.gainDb, request.gainDb, 0.002f)
        && near (slot.q, request.q, 0.0005f)
        && near (slot.shoulder, request.shoulder, 0.0005f))
        return;

    const int previousType = slot.type;
    const auto designed = franceq::designBand (request);

    for (int channel = 0; channel < 2; ++channel)
    {
        auto& runtimeBand = runtime[channel][band];
        if (previousType != static_cast<int> (request.type))
            runtimeBand.reset();

        for (int section = 0; section < designed.count; ++section)
        {
            if (previousType == static_cast<int> (request.type) && section >= runtimeBand.count)
                runtimeBand.sections[section].reset();
            runtimeBand.sections[section].set (designed.sections[section]);
        }

        for (int section = designed.count; section < runtimeBand.count; ++section)
            runtimeBand.sections[section].reset();

        runtimeBand.count = designed.count;
        runtimeBand.type = static_cast<int> (request.type);
        runtimeBand.notchDepth = designed.notchDepth;
    }

    slot.type = static_cast<int> (request.type);
    slot.slope = request.slopeIndex;
    slot.frequency = request.frequency;
    slot.gainDb = request.gainDb;
    slot.q = request.q;
    slot.shoulder = request.shoulder;
}

float FrancEqAudioProcessor::applyBand (int channel, int band, float input, float enable) noexcept
{
    auto& runtimeBand = runtime[channel][band];
    if (runtimeBand.count <= 0)
        return input;

    double y = static_cast<double> (input);
    for (int section = 0; section < runtimeBand.count; ++section)
        y = runtimeBand.sections[section].process (y);

    if (! std::isfinite (y))
    {
        runtimeBand.reset();
        return input;
    }

    float wet = static_cast<float> (y);
    if (runtimeBand.notchDepth > 0.0f)
        wet = input + (wet - input) * runtimeBand.notchDepth;

    if (enable >= 0.9995f)
        return wet;
    if (enable <= 0.0005f)
        return input;

    return input + (wet - input) * enable;
}

void FrancEqAudioProcessor::applyCompressor (float* samples, int numChannels, const CompFrame& frame) noexcept
{
    const auto timing = timingFor (chunkCharacter);
    const float ratio = std::max (1.0f, frame.ratio);
    const float knee = juce::jlimit (0.0f, 36.0f, frame.kneeDb + timing.kneeAdd);
    const float attackMs = juce::jlimit (0.05f, 200.0f, frame.attackMs * timing.attackScale);

    float releaseMs = juce::jlimit (5.0f, 2000.0f, frame.releaseMs * timing.releaseScale);
    if (timing.programRelease)
    {
        const float depth = juce::jlimit (0.0f, 1.0f, -compGrDb / 18.0f);
        releaseMs = juce::jlimit (5.0f, 2000.0f, releaseMs * (0.35f + 1.20f * depth));
    }

    const float sampleRate = static_cast<float> (currentSampleRate);
    const float attackCoeff = std::exp (-1.0f / (attackMs * 0.001f * sampleRate));
    const float releaseCoeff = std::exp (-1.0f / (releaseMs * 0.001f * sampleRate));

    float detector = std::abs (samples[0]);
    if (numChannels > 1)
        detector = std::max (detector, std::abs (samples[1]));
    if (timing.feedback)
        detector = compLastOut;

    const float coeff = detector > compEnv ? attackCoeff : releaseCoeff;
    compEnv = detector + coeff * (compEnv - detector);
    if (! std::isfinite (compEnv) || compEnv < 1.0e-8f)
        compEnv = 0.0f;

    const float inputDb = 20.0f * std::log10 (std::max (compEnv, 1.0e-8f));
    compGrDb = gainComputerDb (inputDb, frame.thresholdDb, ratio, knee);

    if (frame.enable <= 0.0005f)
    {
        compLastOut = std::abs (samples[0]);
        if (numChannels > 1)
            compLastOut = std::max (compLastOut, std::abs (samples[1]));
        gainReductionDb.store (0.0f, std::memory_order_relaxed);
        return;
    }

    float makeupDb = frame.makeupDb;
    if (frame.autoMakeup)
    {
        const float nominal = -frame.thresholdDb * (1.0f - 1.0f / ratio) * 0.35f;
        makeupDb += juce::jlimit (0.0f, 18.0f, nominal);
    }

    const float reduction = franceq::dbToGain (compGrDb);
    const float makeup = franceq::dbToGain (makeupDb);
    const float reduced0 = samples[0] * reduction;
    const float reduced1 = numChannels > 1 ? samples[1] * reduction : reduced0;
    compLastOut = std::max (std::abs (reduced0), std::abs (reduced1));

    const float wet0 = shapeCompressor (reduced0 * makeup, timing.even, timing.odd);
    const float wet1 = numChannels > 1 ? shapeCompressor (reduced1 * makeup, timing.even, timing.odd) : wet0;
    const float mix = juce::jlimit (0.0f, 1.0f, frame.mix) * juce::jlimit (0.0f, 1.0f, frame.enable);

    samples[0] = samples[0] + (wet0 - samples[0]) * mix;
    if (numChannels > 1)
        samples[1] = samples[1] + (wet1 - samples[1]) * mix;

    gainReductionDb.store (compGrDb * juce::jlimit (0.0f, 1.0f, frame.enable), std::memory_order_relaxed);
}

void FrancEqAudioProcessor::saturateBlock (int numChannels, int numSamples) noexcept
{
    juce::dsp::AudioBlock<float> full (eqBuffer);
    auto block = full.getSubsetChannelBlock (0, static_cast<size_t> (numChannels))
                     .getSubBlock (0, static_cast<size_t> (numSamples));

    auto oversampled = oversampling.processSamplesUp (block);
    const int upSamples = static_cast<int> (oversampled.getNumSamples());
    const auto factor = static_cast<int> (std::max<size_t> (1, oversampling.getOversamplingFactor()));

    if (upSamples > 0)
    {
        const int upChannels = static_cast<int> (std::min (oversampled.getNumChannels(), static_cast<size_t> (numChannels)));
        for (int channel = 0; channel < upChannels; ++channel)
        {
            float* data = oversampled.getChannelPointer (static_cast<size_t> (channel));
            for (int sample = 0; sample < upSamples; ++sample)
            {
                const int source = std::min (numSamples - 1, sample / std::max (1, factor));
                data[sample] = shapeSample (data[sample], satPre[static_cast<size_t> (source)],
                                            satEven[static_cast<size_t> (source)], satOdd[static_cast<size_t> (source)]);
            }
        }
    }

    oversampling.processSamplesDown (block);
}

void FrancEqAudioProcessor::pushAnalyser (float pre, float post) noexcept
{
    const auto index = analyserWrite.load (std::memory_order_relaxed);
    const auto slot = index & analyserMask;
    preRing[slot] = pre;
    postRing[slot] = post;
    analyserWrite.store (index + 1, std::memory_order_release);
}

void FrancEqAudioProcessor::processChunk (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || maxBlock <= 0 || currentSampleRate <= 0.0)
        return;

    const int numChannels = juce::jmin (2, buffer.getNumChannels(), juce::jmax (1, getTotalNumOutputChannels()));
    const int inputChannels = juce::jmin (numChannels, juce::jmax (0, getTotalNumInputChannels()));
    if (numChannels <= 0 || static_cast<int> (satPre.size()) < numSamples)
        return;

    chunkCharacter = characterFrom (characterPtr->load());
    const auto saturation = saturationFor (chunkCharacter);
    const bool compPre = compPositionPtr->load() >= 0.5f;
    const bool compAuto = compAutoMakeupPtr->load() >= 0.5f;
    const float nyquist = static_cast<float> (currentSampleRate * 0.49);

    shoulderSmooth.setTargetValue (chunkCharacter == franceq::Character::britN ? 1.0f : 0.0f);
    inputSmooth.setTargetValue (franceq::dbToGain (inputGainPtr->load()));
    outputSmooth.setTargetValue (franceq::dbToGain (outputGainPtr->load()));
    bypassSmooth.setTargetValue (bypassPtr->load() >= 0.5f ? 1.0f : 0.0f);
    satPreSmooth.setTargetValue (saturation.pre);
    satEvenSmooth.setTargetValue (saturation.even);
    satOddSmooth.setTargetValue (saturation.odd);
    satBlendSmooth.setTargetValue (saturation.blend);
    compEnableSmooth.setTargetValue (compEnabledPtr->load() >= 0.5f ? 1.0f : 0.0f);
    compThresholdSmooth.setTargetValue (compThresholdPtr->load());
    compRatioSmooth.setTargetValue (compRatioPtr->load());
    compAttackSmooth.setTargetValue (compAttackPtr->load());
    compReleaseSmooth.setTargetValue (compReleasePtr->load());
    compKneeSmooth.setTargetValue (compKneePtr->load());
    compMakeupSmooth.setTargetValue (compMakeupPtr->load());
    compMixSmooth.setTargetValue (compMixPtr->load());

    for (int band = 0; band < franceq::bandCount; ++band)
    {
        const float frequency = franceq::clampFrequency (bandPtr[band].frequency->load(), currentSampleRate);
        freqSmooth[band].setTargetValue (std::min (frequency, nyquist));
        gainSmooth[band].setTargetValue (bandPtr[band].gain->load());
        enableSmooth[band].setTargetValue (bandPtr[band].enabled->load() >= 0.5f ? 1.0f : 0.0f);
    }

    float* eqL = eqBuffer.getWritePointer (0);
    float* eqR = eqBuffer.getWritePointer (1);
    float* digL = digBuffer.getWritePointer (0);
    float* digR = digBuffer.getWritePointer (1);
    float* out[2] = {
        buffer.getWritePointer (0),
        numChannels > 1 ? buffer.getWritePointer (1) : nullptr,
    };

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float shoulder = shoulderSmooth.getNextValue();
        const float inputGain = inputSmooth.getNextValue();
        satPre[static_cast<size_t> (sample)] = satPreSmooth.getNextValue();
        satEven[static_cast<size_t> (sample)] = satEvenSmooth.getNextValue();
        satOdd[static_cast<size_t> (sample)] = satOddSmooth.getNextValue();
        blendEnv[static_cast<size_t> (sample)] = satBlendSmooth.getNextValue();
        outGainEnv[static_cast<size_t> (sample)] = outputSmooth.getNextValue();
        bypassEnv[static_cast<size_t> (sample)] = bypassSmooth.getNextValue();

        float in0 = inputChannels > 0 ? out[0][sample] : 0.0f;
        float in1 = inputChannels > 1 ? out[1][sample] : in0;
        dryEnvL[static_cast<size_t> (sample)] = dryDelay.processSample (0, in0);
        if (numChannels > 1)
            dryEnvR[static_cast<size_t> (sample)] = dryDelay.processSample (1, in1);
        dryDelay.advance();

        float x0 = in0 * inputGain;
        float x1 = (numChannels > 1 ? in1 : in0) * inputGain;

        CompFrame& frame = compFrames[static_cast<size_t> (sample)];
        frame.thresholdDb = compThresholdSmooth.getNextValue();
        frame.ratio = compRatioSmooth.getNextValue();
        frame.kneeDb = compKneeSmooth.getNextValue();
        frame.attackMs = compAttackSmooth.getNextValue();
        frame.releaseMs = compReleaseSmooth.getNextValue();
        frame.makeupDb = compMakeupSmooth.getNextValue();
        frame.mix = compMixSmooth.getNextValue();
        frame.enable = compEnableSmooth.getNextValue();
        frame.autoMakeup = compAuto;

        if (compPre)
        {
            float pair[2] = { x0, x1 };
            applyCompressor (pair, numChannels, frame);
            x0 = pair[0];
            x1 = pair[1];
        }

        preMono[static_cast<size_t> (sample)] = numChannels > 1 ? 0.5f * (x0 + x1) : x0;

        for (int band = 0; band < franceq::bandCount; ++band)
        {
            const float frequency = freqSmooth[band].getNextValue();
            const float gain = gainSmooth[band].getNextValue();
            const auto type = typeFrom (bandPtr[band].type->load());
            const int slope = juce::jlimit (0, 2, juce::roundToInt (bandPtr[band].slope->load()));
            qSmooth[band].setTargetValue (franceq::shapedQ (chunkCharacter, type, bandPtr[band].q->load(), gain));
            const float q = qSmooth[band].getNextValue();
            const float enable = enableSmooth[band].getNextValue();

            franceq::DesignRequest request;
            request.type = type;
            request.sampleRate = currentSampleRate;
            request.frequency = frequency;
            request.gainDb = gain;
            request.q = q;
            request.slopeIndex = slope;
            request.shoulder = type == franceq::BandType::bell ? shoulder : 0.0f;
            updateBand (band, request);

            x0 = applyBand (0, band, x0, enable);
            if (numChannels > 1)
                x1 = applyBand (1, band, x1, enable);
        }

        eqL[sample] = x0;
        eqR[sample] = numChannels > 1 ? x1 : x0;
        digL[sample] = wetDelay.processSample (0, x0);
        digR[sample] = wetDelay.processSample (1, numChannels > 1 ? x1 : x0);
        wetDelay.advance();
    }

    saturateBlock (numChannels, numSamples);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        eqL[sample] = dcBlock[0].process (eqL[sample]);
        if (numChannels > 1)
            eqR[sample] = dcBlock[1].process (eqR[sample]);

        const float blend = juce::jlimit (0.0f, 1.0f, blendEnv[static_cast<size_t> (sample)]);
        float y0 = digL[sample] + (eqL[sample] - digL[sample]) * blend;
        float y1 = digR[sample] + (eqR[sample] - digR[sample]) * blend;

        if (! compPre)
        {
            float pair[2] = { y0, y1 };
            applyCompressor (pair, numChannels, compFrames[static_cast<size_t> (sample)]);
            y0 = pair[0];
            y1 = pair[1];
        }

        const float outputGain = outGainEnv[static_cast<size_t> (sample)];
        y0 *= outputGain;
        y1 *= outputGain;

        const float bypass = juce::jlimit (0.0f, 1.0f, bypassEnv[static_cast<size_t> (sample)]);
        const float dry0 = dryEnvL[static_cast<size_t> (sample)];
        const float dry1 = numChannels > 1 ? dryEnvR[static_cast<size_t> (sample)] : dry0;
        y0 = dry0 + (y0 - dry0) * (1.0f - bypass);
        y1 = dry1 + (y1 - dry1) * (1.0f - bypass);

        if (! std::isfinite (y0))
            y0 = 0.0f;
        if (! std::isfinite (y1))
            y1 = 0.0f;

        out[0][sample] = y0;
        if (out[1] != nullptr)
            out[1][sample] = y1;

        const float post = numChannels > 1 ? 0.5f * (y0 + y1) : y0;
        pushAnalyser (preMono[static_cast<size_t> (sample)], post);
    }
}

void FrancEqAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (currentSampleRate <= 0.0 || numSamples <= 0 || maxBlock <= 0)
    {
        buffer.clear();
        return;
    }

    for (int channel = getTotalNumOutputChannels(); channel < buffer.getNumChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);

    if (numSamples <= maxBlock)
    {
        processChunk (buffer);
        return;
    }

    const int numChannels = buffer.getNumChannels();
    for (int offset = 0; offset < numSamples; offset += maxBlock)
    {
        const int count = std::min (maxBlock, numSamples - offset);
        juce::AudioBuffer<float> slice (buffer.getArrayOfWritePointers(), numChannels, offset, count);
        processChunk (slice);
    }
}

void FrancEqAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min (buffer.getNumChannels(), floatScratch.getNumChannels());
    if (currentSampleRate <= 0.0 || numSamples <= 0 || numChannels <= 0 || maxBlock <= 0)
    {
        buffer.clear();
        return;
    }

    int offset = 0;
    while (offset < numSamples)
    {
        const int count = std::min (maxBlock, numSamples - offset);
        for (int channel = 0; channel < numChannels; ++channel)
        {
            const double* source = buffer.getReadPointer (channel) + offset;
            float* destination = floatScratch.getWritePointer (channel);
            for (int sample = 0; sample < count; ++sample)
                destination[sample] = static_cast<float> (source[sample]);
        }

        juce::AudioBuffer<float> view (floatScratch.getArrayOfWritePointers(), numChannels, count);
        {
            juce::ScopedNoDenormals noDenormals;
            processChunk (view);
        }

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float* source = floatScratch.getReadPointer (channel);
            double* destination = buffer.getWritePointer (channel) + offset;
            for (int sample = 0; sample < count; ++sample)
                destination[sample] = static_cast<double> (source[sample]);
        }

        for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
            buffer.clear (channel, offset, count);

        offset += count;
    }
}

void FrancEqAudioProcessor::copyAnalyser (float* pre, float* post, int numSamples) const noexcept
{
    if (pre == nullptr || post == nullptr || numSamples <= 0)
        return;

    const auto count = static_cast<uint32_t> (std::min (numSamples, analyserSize));
    const auto end = analyserWrite.load (std::memory_order_acquire);
    for (uint32_t index = 0; index < count; ++index)
    {
        const auto slot = (end - count + index) & analyserMask;
        pre[index] = preRing[slot];
        post[index] = postRing[slot];
    }
}

void FrancEqAudioProcessor::setEditorSize (int width, int height)
{
    editorWidth.store (juce::jlimit (minEditorWidth, maxEditorWidth, width));
    editorHeight.store (juce::jlimit (minEditorHeight, maxEditorHeight, height));
}

void FrancEqAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("editorWidth", editorWidth.load(), nullptr);
    state.setProperty ("editorHeight", editorHeight.load(), nullptr);

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void FrancEqAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            setEditorSize ((int) state.getProperty ("editorWidth", editorWidth.load()),
                           (int) state.getProperty ("editorHeight", editorHeight.load()));
            apvts.replaceState (state);

            if (auto* editor = getActiveEditor())
            {
                const int width = getEditorWidth();
                const int height = getEditorHeight();
                auto apply = [safe = juce::Component::SafePointer<juce::AudioProcessorEditor> (editor), width, height]
                {
                    if (safe != nullptr)
                        safe->setSize (width, height);
                };

                if (auto* manager = juce::MessageManager::getInstanceWithoutCreating())
                {
                    if (manager->isThisTheMessageThread())
                        apply();
                    else
                        juce::MessageManager::callAsync (std::move (apply));
                }
            }
        }
    }
}

juce::AudioProcessorEditor* FrancEqAudioProcessor::createEditor()
{
    return new FrancEqAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FrancEqAudioProcessor();
}
