#pragma once

#include "PluginProcessor.h"

#include <array>
#include <functional>
#include <vector>

// Message-thread frequency response, band nodes, and spectrum. It reads parameter
// atomics and the analyser ring; it never locks and never touches the audio thread.
class EqDisplay : public juce::Component,
                  private juce::Timer
{
public:
    explicit EqDisplay (FrancEqAudioProcessor& processorToUse);

    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    void setSelectedBand (int index);
    int getSelectedBand() const noexcept { return selected; }
    void refresh();

    std::function<void (int)> onSelectBand;

private:
    void timerCallback() override;
    juce::Rectangle<float> plotArea() const;
    float frequencyToX (float frequency, juce::Rectangle<float> area) const;
    float xToFrequency (float x, juce::Rectangle<float> area) const;
    float dbToY (float db, juce::Rectangle<float> area) const;
    float yToDb (float y, juce::Rectangle<float> area) const;
    float yToQ (float y, juce::Rectangle<float> area) const;
    int bandAt (juce::Point<float> position) const;
    juce::Point<float> nodePosition (int band, juce::Rectangle<float> area) const;
    void rebuildCurves();
    void rebuildSpectrum();
    juce::RangedAudioParameter* parameter (const juce::String& id) const;
    void setRaw (juce::RangedAudioParameter* parameterToSet, float raw) const;
    void beginDrag (int band);
    void endDrag();
    float rawValue (const juce::String& id) const;

    static constexpr int curvePoints = 420;
    static constexpr int spectrumColumns = 280;
    static constexpr float dbRange = 30.0f;

    FrancEqAudioProcessor& processor;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    std::vector<float> frequencies;
    std::vector<float> combined;
    std::array<std::vector<float>, franceq::bandCount> bandCurve;
    std::vector<float> fftData;
    std::vector<float> framePre;
    std::vector<float> framePost;
    std::array<float, spectrumColumns> preColumn {};
    std::array<float, spectrumColumns> postColumn {};

    int selected = 3;
    int dragBand = -1;
    bool dragArmed = false;
    bool dragChangesQ = false;
    bool gestureOpen = false;
    juce::RangedAudioParameter* dragFrequency = nullptr;
    juce::RangedAudioParameter* dragGain = nullptr;
    juce::RangedAudioParameter* dragQ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqDisplay)
};
