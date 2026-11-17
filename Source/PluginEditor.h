#pragma once

#include "EqDisplay.h"
#include "Palette.h"
#include "PluginProcessor.h"

#include <memory>

class FrancLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FrancLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
};

class FrancKnob : public juce::Slider
{
public:
    void setFrequencyWheel (bool shouldUseFrequencyWheel) { frequencyWheel = shouldUseFrequencyWheel; }

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void applyDragSensitivity (const juce::ModifierKeys& mods);
    bool frequencyWheel = false;
};

class GainReductionMeter : public juce::Component,
                           private juce::Timer
{
public:
    explicit GainReductionMeter (FrancEqAudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    FrancEqAudioProcessor& processor;
};

class FrancEqAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit FrancEqAudioProcessorEditor (FrancEqAudioProcessor&);
    ~FrancEqAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshForSnapshot();
    juce::String layoutProblems() const;

private:
    class InterfaceCanvas : public juce::Component
    {
    public:
        explicit InterfaceCanvas (FrancEqAudioProcessorEditor& ownerIn);
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        FrancEqAudioProcessorEditor& owner;
    };

    void timerCallback() override;
    void paintInterface (juce::Graphics&);
    void layoutDesign();
    void configureKnob (FrancKnob& slider, bool frequencyWheel);
    void configureLabel (juce::Label& label, const juce::String& text);
    void configureChoiceButton (juce::TextButton& button);
    void selectBand (int index);
    void syncCharacterButtons();
    void rebuildBandAttachments();
    void fitSliderText (FrancKnob& slider);

    FrancEqAudioProcessor& processor;
    FrancLookAndFeel lookAndFeel;
    EqDisplay display;
    GainReductionMeter meter;
    juce::TooltipWindow tooltipWindow;

    juce::Label characterLabel;
    juce::Label inputLabel;
    juce::Label outputLabel;
    juce::Label bypassLabel;
    juce::Label bandLabel;
    juce::Label enableLabel;
    juce::Label typeLabel;
    juce::Label slopeLabel;
    juce::Label freqLabel;
    juce::Label gainLabel;
    juce::Label qLabel;
    juce::Label compLabel;
    juce::Label autoLabel;
    juce::Label positionLabel;
    juce::Label thresholdLabel;
    juce::Label ratioLabel;
    juce::Label attackLabel;
    juce::Label releaseLabel;
    juce::Label kneeLabel;
    juce::Label makeupLabel;
    juce::Label mixLabel;
    juce::Label grLabel;

    juce::TextButton characterButtons[4];
    juce::TextButton bandButtons[franceq::bandCount];
    FrancKnob inputSlider;
    FrancKnob outputSlider;
    FrancKnob freqSlider;
    FrancKnob gainSlider;
    FrancKnob qSlider;
    FrancKnob thresholdSlider;
    FrancKnob ratioSlider;
    FrancKnob attackSlider;
    FrancKnob releaseSlider;
    FrancKnob kneeSlider;
    FrancKnob makeupSlider;
    FrancKnob mixSlider;
    juce::ToggleButton bypassButton;
    juce::ToggleButton enableButton;
    juce::ToggleButton compEnable;
    juce::ToggleButton autoButton;
    juce::ComboBox typeBox;
    juce::ComboBox slopeBox;
    juce::ComboBox positionBox;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> slopeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> compEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> positionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thresholdAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ratioAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> kneeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> makeupAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;

    InterfaceCanvas canvas;
    juce::Rectangle<int> titleBounds;
    juce::Rectangle<int> railBounds;
    juce::Rectangle<int> bandBounds;
    juce::Rectangle<int> compBounds;
    int selectedBand = 3;
    bool editorReady = false;

    static constexpr int designWidth = FrancEqAudioProcessor::defaultEditorWidth;
    static constexpr int designHeight = FrancEqAudioProcessor::defaultEditorHeight;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrancEqAudioProcessorEditor)
};
