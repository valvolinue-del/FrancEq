#pragma once

#include "EqDesign.h"

#include <array>
#include <atomic>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

namespace ParamIDs
{
inline constexpr auto inputGain = "inputGain";
inline constexpr auto outputGain = "outputGain";
inline constexpr auto bypass = "bypass";
inline constexpr auto character = "character";
inline constexpr auto compEnabled = "compEnabled";
inline constexpr auto compThreshold = "compThreshold";
inline constexpr auto compRatio = "compRatio";
inline constexpr auto compAttack = "compAttack";
inline constexpr auto compRelease = "compRelease";
inline constexpr auto compKnee = "compKnee";
inline constexpr auto compMakeup = "compMakeup";
inline constexpr auto compMix = "compMix";
inline constexpr auto compAutoMakeup = "compAutoMakeup";
inline constexpr auto compPosition = "compPosition";

inline juce::String bandEnabled (int index) { return "band" + juce::String (index + 1) + "Enabled"; }
inline juce::String bandType (int index) { return "band" + juce::String (index + 1) + "Type"; }
inline juce::String bandFreq (int index) { return "band" + juce::String (index + 1) + "Freq"; }
inline juce::String bandGain (int index) { return "band" + juce::String (index + 1) + "Gain"; }
inline juce::String bandQ (int index) { return "band" + juce::String (index + 1) + "Q"; }
inline juce::String bandSlope (int index) { return "band" + juce::String (index + 1) + "Slope"; }
} // namespace ParamIDs

// Parametric EQ, character saturation, and a compressor. Meter and analyser
// atomics are written on the audio thread and read by the editor without a lock.
class FrancEqAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int analyserOrder = 11;
    static constexpr int analyserSize = 1 << analyserOrder;
    static constexpr int analyserRing = analyserSize * 2;
    static constexpr uint32_t analyserMask = static_cast<uint32_t> (analyserRing - 1);

    FrancEqAudioProcessor();
    ~FrancEqAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "FrancEq"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    bool supportsDoublePrecisionProcessing() const override { return true; }
    double getTailLengthSeconds() const override { return 0.2; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    float getGainReductionDb() const noexcept
    {
        return gainReductionDb.load (std::memory_order_relaxed);
    }

    void copyAnalyser (float* pre, float* post, int numSamples) const noexcept;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    struct CompFrame
    {
        float thresholdDb = -18.0f;
        float ratio = 4.0f;
        float kneeDb = 6.0f;
        float attackMs = 10.0f;
        float releaseMs = 120.0f;
        float makeupDb = 0.0f;
        float mix = 1.0f;
        float enable = 0.0f;
        bool autoMakeup = false;
    };

    struct DelayLine
    {
        void prepare (int maxDelaySamples, int numChannelsToUse);
        void setDelay (int samples) noexcept;
        void reset() noexcept;
        float processSample (int channel, float input) noexcept;
        void advance() noexcept;

        std::vector<float> buffer;
        int capacity = 1;
        int writePos = 0;
        int delaySamples = 0;
    };

    struct Section
    {
        double b0 = 1.0;
        double b1 = 0.0;
        double b2 = 0.0;
        double a1 = 0.0;
        double a2 = 0.0;
        double z1 = 0.0;
        double z2 = 0.0;

        void reset() noexcept { z1 = 0.0; z2 = 0.0; }

        void set (const franceq::BiquadCoeff& coeff) noexcept
        {
            b0 = coeff.b0;
            b1 = coeff.b1;
            b2 = coeff.b2;
            a1 = coeff.a1;
            a2 = coeff.a2;
        }

        double process (double x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    struct BandRuntime
    {
        Section sections[franceq::maxSections];
        int count = 0;
        int type = -1;
        float notchDepth = 0.0f;

        void reset() noexcept
        {
            for (auto& section : sections)
                section.reset();
        }
    };

    struct BandCache
    {
        float frequency = -1.0f;
        float gainDb = 999.0f;
        float q = -1.0f;
        float shoulder = -1.0f;
        int type = -1;
        int slope = -1;
    };

    struct BandPointers
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* frequency = nullptr;
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* q = nullptr;
        std::atomic<float>* slope = nullptr;
    };

    struct DcBlocker
    {
        float radius = 0.995f;
        float x1 = 0.0f;
        float y1 = 0.0f;

        void prepare (double sampleRate) noexcept
        {
            radius = std::exp (-juce::MathConstants<float>::twoPi * 12.0f / static_cast<float> (sampleRate));
            x1 = 0.0f;
            y1 = 0.0f;
        }

        float process (float x) noexcept
        {
            const float y = x - x1 + radius * y1;
            x1 = x;
            y1 = y;
            return y;
        }
    };

    void processChunk (juce::AudioBuffer<float>& buffer);
    void updateBand (int band, const franceq::DesignRequest& request);
    float applyBand (int channel, int band, float input, float enable) noexcept;
    void applyCompressor (float* samples, int numChannels, const CompFrame& frame) noexcept;
    void saturateBlock (int numChannels, int numSamples) noexcept;
    void pushAnalyser (float pre, float post) noexcept;

    BandPointers bandPtr[franceq::bandCount] {};
    std::atomic<float>* inputGainPtr = nullptr;
    std::atomic<float>* outputGainPtr = nullptr;
    std::atomic<float>* bypassPtr = nullptr;
    std::atomic<float>* characterPtr = nullptr;
    std::atomic<float>* compEnabledPtr = nullptr;
    std::atomic<float>* compThresholdPtr = nullptr;
    std::atomic<float>* compRatioPtr = nullptr;
    std::atomic<float>* compAttackPtr = nullptr;
    std::atomic<float>* compReleasePtr = nullptr;
    std::atomic<float>* compKneePtr = nullptr;
    std::atomic<float>* compMakeupPtr = nullptr;
    std::atomic<float>* compMixPtr = nullptr;
    std::atomic<float>* compAutoMakeupPtr = nullptr;
    std::atomic<float>* compPositionPtr = nullptr;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> freqSmooth[franceq::bandCount];
    juce::SmoothedValue<float> gainSmooth[franceq::bandCount];
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> qSmooth[franceq::bandCount];
    juce::SmoothedValue<float> enableSmooth[franceq::bandCount];
    juce::SmoothedValue<float> shoulderSmooth;
    juce::SmoothedValue<float> inputSmooth;
    juce::SmoothedValue<float> outputSmooth;
    juce::SmoothedValue<float> bypassSmooth;
    juce::SmoothedValue<float> satPreSmooth;
    juce::SmoothedValue<float> satEvenSmooth;
    juce::SmoothedValue<float> satOddSmooth;
    juce::SmoothedValue<float> satBlendSmooth;
    juce::SmoothedValue<float> compEnableSmooth;
    juce::SmoothedValue<float> compThresholdSmooth;
    juce::SmoothedValue<float> compRatioSmooth;
    juce::SmoothedValue<float> compAttackSmooth;
    juce::SmoothedValue<float> compReleaseSmooth;
    juce::SmoothedValue<float> compKneeSmooth;
    juce::SmoothedValue<float> compMakeupSmooth;
    juce::SmoothedValue<float> compMixSmooth;

    BandRuntime runtime[2][franceq::bandCount];
    BandCache cache[franceq::bandCount];

    DelayLine dryDelay;
    DelayLine wetDelay;
    DcBlocker dcBlock[2];

    juce::dsp::Oversampling<float> oversampling {
        2,
        2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        false,
        true,
    };

    juce::AudioBuffer<float> eqBuffer;
    juce::AudioBuffer<float> digBuffer;
    juce::AudioBuffer<float> floatScratch;
    std::vector<float> satPre;
    std::vector<float> satEven;
    std::vector<float> satOdd;
    std::vector<float> blendEnv;
    std::vector<float> outGainEnv;
    std::vector<float> bypassEnv;
    std::vector<float> dryEnvL;
    std::vector<float> dryEnvR;
    std::vector<float> preMono;
    std::vector<CompFrame> compFrames;

    std::array<float, analyserRing> preRing {};
    std::array<float, analyserRing> postRing {};
    std::atomic<uint32_t> analyserWrite { 0 };
    std::atomic<float> gainReductionDb { 0.0f };

    float compEnv = 0.0f;
    float compGrDb = 0.0f;
    float compLastOut = 0.0f;
    franceq::Character chunkCharacter = franceq::Character::digital;

    int maxBlock = 0;
    int latencySamples = 0;
    double currentSampleRate = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrancEqAudioProcessor)
};
