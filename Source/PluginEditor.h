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

private:
    void timerCallback() override;
    void configureKnob (juce::Slider& slider);
    void configureLabel (juce::Label& label, const juce::String& text);
    void configureChoiceButton (juce::TextButton& button);
    void selectBand (int index);
    void syncCharacterButtons();
    void rebuildBandAttachments();

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
    juce::Slider inputSlider;
    juce::Slider outputSlider;
    juce::Slider freqSlider;
    juce::Slider gainSlider;
    juce::Slider qSlider;
    juce::Slider thresholdSlider;
    juce::Slider ratioSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider kneeSlider;
    juce::Slider makeupSlider;
    juce::Slider mixSlider;
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

    juce::Rectangle<int> titleBounds;
    juce::Rectangle<int> railBounds;
    juce::Rectangle<int> bandBounds;
    juce::Rectangle<int> compBounds;
    int selectedBand = 3;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrancEqAudioProcessorEditor)
};
