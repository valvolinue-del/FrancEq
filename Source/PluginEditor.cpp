#include "PluginEditor.h"
#include "Wheel.h"

#include <algorithm>

namespace
{
void drawTrackedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                      const juce::Font& font, juce::Colour colour, float tracking)
{
    float width = 0.0f;
    for (int index = 0; index < text.length(); ++index)
        width += juce::GlyphArrangement::getStringWidth (font, text.substring (index, index + 1));
    if (text.length() > 1)
        width += tracking * static_cast<float> (text.length() - 1);

    float x = area.getCentreX() - width * 0.5f;
    g.setFont (font);
    g.setColour (colour);
    for (int index = 0; index < text.length(); ++index)
    {
        const auto glyph = text.substring (index, index + 1);
        const float glyphWidth = juce::GlyphArrangement::getStringWidth (font, glyph);
        g.drawText (glyph, juce::Rectangle<float> (x, area.getY(), glyphWidth + tracking + 2.0f, area.getHeight()),
                    juce::Justification::centredLeft, false);
        x += glyphWidth + tracking;
    }
}
} // namespace

FrancLookAndFeel::FrancLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, franc::ivory);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, franc::gold.withAlpha (0.28f));
    setColour (juce::Slider::rotarySliderFillColourId, franc::gold);
    setColour (juce::Slider::rotarySliderOutlineColourId, franc::track);
    setColour (juce::Slider::thumbColourId, franc::goldBright);
    setColour (juce::Label::textColourId, franc::gold);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ResizableWindow::backgroundColourId, franc::background);
    setColour (juce::TextEditor::textColourId, franc::ivory);
    setColour (juce::TextEditor::backgroundColourId, franc::panelRaised);
    setColour (juce::TextEditor::outlineColourId, franc::gold);
    setColour (juce::TextEditor::focusedOutlineColourId, franc::goldBright);
    setColour (juce::TextEditor::highlightColourId, franc::gold.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, franc::gold);
    setColour (juce::TooltipWindow::backgroundColourId, franc::panelRaised);
    setColour (juce::TooltipWindow::textColourId, franc::ivory);
    setColour (juce::TooltipWindow::outlineColourId, franc::gold);
    setColour (juce::ComboBox::backgroundColourId, franc::panelRaised);
    setColour (juce::ComboBox::textColourId, franc::ivory);
    setColour (juce::ComboBox::outlineColourId, franc::gold);
    setColour (juce::ComboBox::arrowColourId, franc::gold);
    setColour (juce::PopupMenu::backgroundColourId, franc::panelRaised);
    setColour (juce::PopupMenu::textColourId, franc::ivory);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, franc::gold);
    setColour (juce::PopupMenu::highlightedTextColourId, franc::background);
    setColour (juce::TextButton::buttonColourId, franc::panelRaised);
    setColour (juce::TextButton::buttonOnColourId, franc::gold);
    setColour (juce::TextButton::textColourOffId, franc::gold);
    setColour (juce::TextButton::textColourOnId, franc::background);
}

void FrancLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float rotaryStartAngle, float rotaryEndAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                          static_cast<float> (width), static_cast<float> (height))
                      .reduced (6.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const float arcRadius = radius - 5.0f;

    juce::Path trackPath;
    trackPath.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (franc::track);
    g.strokePath (trackPath, juce::PathStrokeType (3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valuePath;
    valuePath.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour (franc::gold.withAlpha (0.28f));
    g.strokePath (valuePath, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (franc::gold);
    g.strokePath (valuePath, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobRadius = radius * 0.68f;
    juce::ColourGradient body (juce::Colour::fromRGB (0x2c, 0x2a, 0x32), centre.x, centre.y - knobRadius * 0.6f,
                               juce::Colour::fromRGB (0x0c, 0x0c, 0x10), centre.x, centre.y + knobRadius, true);
    g.setGradientFill (body);
    g.fillEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillEllipse (centre.x - knobRadius * 0.55f, centre.y - knobRadius * 0.72f, knobRadius * 1.05f, knobRadius * 0.48f);

    g.setColour (franc::gold.withAlpha (0.85f));
    g.drawEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.15f);

    juce::Path pointer;
    const float pointerLength = knobRadius * 0.72f;
    pointer.addRoundedRectangle (-1.15f, -pointerLength, 2.3f, pointerLength * 0.58f, 1.0f);
    g.setColour (franc::goldBright);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void FrancLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (2.0f, 1.0f);
    auto pillRow = bounds.removeFromTop (std::min (34.0f, bounds.getHeight() * 0.70f));
    auto pill = juce::Rectangle<float> (48.0f, 24.0f).withCentre (pillRow.getCentre());
    const bool on = button.getToggleState();

    g.setColour (on ? franc::gold : franc::track);
    g.fillRoundedRectangle (pill, 12.0f);
    g.setColour (franc::gold.withAlpha (on || highlighted ? 1.0f : 0.45f));
    g.drawRoundedRectangle (pill, 12.0f, highlighted || down ? 1.8f : 1.15f);

    auto thumb = pill.reduced (3.0f).withWidth (18.0f);
    if (on)
        thumb.setX (pill.getRight() - thumb.getWidth() - 3.0f);
    g.setColour (on ? franc::background : franc::gold);
    g.fillEllipse (thumb);

    if (bounds.getHeight() > 8.0f)
    {
        g.setColour (on ? franc::goldBright : franc::muted);
        g.setFont (juce::Font (juce::FontOptions (11.0f).withKerningFactor (0.08f)));
        g.drawText (on ? "ON" : "OFF", bounds, juce::Justification::centred, false);
    }
}

void FrancLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                             bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = button.getToggleState();
    g.setColour (on ? franc::gold : franc::panelRaised);
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (franc::gold.withAlpha (on || highlighted ? 1.0f : 0.40f));
    g.drawRoundedRectangle (bounds, 7.0f, on || down ? 1.6f : 1.0f);
}

void FrancLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height)).reduced (1.0f);
    g.setColour (franc::panelRaised);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (franc::gold.withAlpha (box.isMouseOver() ? 0.95f : 0.45f));
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    juce::Path arrow;
    const float cx = bounds.getRight() - 14.0f;
    const float cy = bounds.getCentreY();
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
    arrow.lineTo (cx + 4.0f, cy - 2.0f);
    arrow.lineTo (cx, cy + 3.0f);
    arrow.closeSubPath();
    g.setColour (franc::gold);
    g.fillPath (arrow);
}

void FrancLookAndFeel::drawCornerResizer (juce::Graphics& g, int width, int height, bool, bool)
{
    g.setColour (franc::gold.withAlpha (0.55f));
    const float right = static_cast<float> (width);
    const float bottom = static_cast<float> (height);
    for (int index = 0; index < 3; ++index)
    {
        const float offset = static_cast<float> (index * 4 + 4);
        g.drawLine (right - offset - 8.0f, bottom - 3.0f, right - 3.0f, bottom - offset - 8.0f, 1.1f);
    }
}

juce::Label* FrancLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::Font (juce::FontOptions (12.5f)));
    label->setJustificationType (juce::Justification::centred);
    label->setColour (juce::Label::textColourId, franc::ivory);
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return label;
}

juce::Font FrancLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::Font (juce::FontOptions (12.5f));
}

juce::Font FrancLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    const float size = juce::jlimit (10.0f, 15.0f, buttonHeight * 0.46f);
    return juce::Font (juce::FontOptions (size).withKerningFactor (0.06f));
}

juce::Font FrancLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    const float size = juce::jlimit (11.0f, 16.0f, box.getHeight() * 0.46f);
    return juce::Font (juce::FontOptions (size));
}

void FrancKnob::applyDragSensitivity (const juce::ModifierKeys& mods)
{
    setMouseDragSensitivity (franceq::wheelIsFine (mods) ? 2200 : 250);
}

void FrancKnob::mouseDown (const juce::MouseEvent& event)
{
    applyDragSensitivity (event.mods);
    juce::Slider::mouseDown (event);
}

void FrancKnob::mouseDrag (const juce::MouseEvent& event)
{
    applyDragSensitivity (event.mods);
    juce::Slider::mouseDrag (event);
}

void FrancKnob::mouseUp (const juce::MouseEvent& event)
{
    setMouseDragSensitivity (250);
    juce::Slider::mouseUp (event);
}

void FrancKnob::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (! frequencyWheel)
    {
        juce::Slider::mouseWheelMove (event, wheel);
        return;
    }

    const float notches = franceq::notchesFromWheel (wheel)
                        * (franceq::wheelIsFine (event.mods) ? franceq::wheelFineFactor : 1.0f);
    if (std::abs (notches) < 1.0e-6f)
        return;

    const float current = static_cast<float> (getValue());
    const float next = franceq::frequencyAfterNotches (current, notches);
    const double currentPos = valueToProportionOfLength (current);
    const double nextPos = valueToProportionOfLength (next);
    const double proportionDelta = nextPos - currentPos;
    if (std::abs (proportionDelta) < 1.0e-6)
        return;

    // Hand the step to the slider so the gesture, text box, and popup match a normal wheel.
    juce::MouseWheelDetails synthetic;
    synthetic.deltaY = static_cast<float> (proportionDelta / franceq::sliderWheelProportion);
    synthetic.isSmooth = true;
    juce::Slider::mouseWheelMove (event, synthetic);
}

FrancEqAudioProcessorEditor::InterfaceCanvas::InterfaceCanvas (FrancEqAudioProcessorEditor& ownerIn)
    : owner (ownerIn)
{
    setOpaque (true);
}

void FrancEqAudioProcessorEditor::InterfaceCanvas::paint (juce::Graphics& g)
{
    owner.paintInterface (g);
}

void FrancEqAudioProcessorEditor::InterfaceCanvas::resized()
{
    owner.layoutDesign();
}

GainReductionMeter::GainReductionMeter (FrancEqAudioProcessor& processorIn)
    : processor (processorIn)
{
    startTimerHz (60);
}

void GainReductionMeter::timerCallback()
{
    repaint();
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (4.0f, 2.0f);
    g.setColour (franc::panelRaised);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (franc::gold.withAlpha (0.40f));
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

    const float gr = juce::jlimit (-24.0f, 0.0f, processor.getGainReductionDb());
    const float amount = -gr / 24.0f;
    auto fill = bounds.reduced (3.0f);
    fill.setHeight (fill.getHeight() * amount);
    g.setColour (franc::gold.withAlpha (0.90f));
    g.fillRoundedRectangle (fill, 3.0f);

    g.setColour (franc::ivory);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText (juce::String (gr, 1), bounds.reduced (2.0f), juce::Justification::centredBottom, false);
}

FrancEqAudioProcessorEditor::FrancEqAudioProcessorEditor (FrancEqAudioProcessor& processorIn)
    : AudioProcessorEditor (&processorIn),
      processor (processorIn),
      display (processorIn),
      meter (processorIn),
      tooltipWindow (this, 700),
      canvas (*this)
{
    setLookAndFeel (&lookAndFeel);
    tooltipWindow.setColour (juce::TooltipWindow::backgroundColourId, franc::panelRaised);
    tooltipWindow.setColour (juce::TooltipWindow::textColourId, franc::ivory);
    tooltipWindow.setColour (juce::TooltipWindow::outlineColourId, franc::gold);

    addAndMakeVisible (canvas);
    canvas.addAndMakeVisible (display);
    display.onSelectBand = [this] (int index) { selectBand (index); };

    configureLabel (characterLabel, "CHARACTER");
    configureLabel (inputLabel, "INPUT");
    configureLabel (outputLabel, "OUTPUT");
    configureLabel (bypassLabel, "BYPASS");
    configureLabel (bandLabel, "BAND");
    configureLabel (enableLabel, "ENABLE");
    configureLabel (typeLabel, "TYPE");
    configureLabel (slopeLabel, "SLOPE");
    configureLabel (freqLabel, "FREQ");
    configureLabel (gainLabel, "GAIN");
    configureLabel (qLabel, "Q");
    configureLabel (compLabel, "COMP");
    configureLabel (autoLabel, "AUTO");
    configureLabel (positionLabel, "ORDER");
    configureLabel (thresholdLabel, "THRESH");
    configureLabel (ratioLabel, "RATIO");
    configureLabel (attackLabel, "ATTACK");
    configureLabel (releaseLabel, "RELEASE");
    configureLabel (kneeLabel, "KNEE");
    configureLabel (makeupLabel, "MAKEUP");
    configureLabel (mixLabel, "MIX");
    configureLabel (grLabel, "GR");

    const char* characterNames[] = { "DIGITAL", "BRIT N", "BRIT S", "US A" };
    const char* characterTips[] = {
        "Clean minimum-phase EQ. No added harmonics.",
        "Broader curves and a wide shoulder, with gentle transformer colour. Inspired by classic British discrete console EQ.",
        "Tighter curves, and a faster program-dependent compressor. Inspired by classic British bus EQ.",
        "Proportional Q: bells narrow as the gain grows. Inspired by classic American console EQ.",
    };

    for (int index = 0; index < 4; ++index)
    {
        characterButtons[index].setButtonText (characterNames[index]);
        characterButtons[index].setTooltip (characterTips[index]);
        characterButtons[index].setClickingTogglesState (false);
        configureChoiceButton (characterButtons[index]);
        characterButtons[index].onClick = [this, index]
        {
            if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (ParamIDs::character)))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (index)));
                parameter->endChangeGesture();
            }
            syncCharacterButtons();
        };
    }

    for (int band = 0; band < franceq::bandCount; ++band)
    {
        bandButtons[band].setButtonText (juce::String (band + 1));
        bandButtons[band].setTooltip ("Select band " + juce::String (band + 1) + ".");
        bandButtons[band].setClickingTogglesState (false);
        configureChoiceButton (bandButtons[band]);
        bandButtons[band].onClick = [this, band] { selectBand (band); };
    }

    configureKnob (inputSlider, false);
    configureKnob (outputSlider, false);
    configureKnob (freqSlider, true);
    configureKnob (gainSlider, false);
    configureKnob (qSlider, false);
    configureKnob (thresholdSlider, false);
    configureKnob (ratioSlider, false);
    configureKnob (attackSlider, false);
    configureKnob (releaseSlider, false);
    configureKnob (kneeSlider, false);
    configureKnob (makeupSlider, false);
    configureKnob (mixSlider, false);

    inputSlider.setTooltip ("Gain in front of the EQ.");
    outputSlider.setTooltip ("Gain after the EQ and compressor.");
    freqSlider.setTooltip ("Band frequency, from 20 Hz to 20 kHz. Each scroll step is about a sixth of an octave. Shift or Cmd is fine.");
    gainSlider.setTooltip ("Band gain. Cuts ignore gain. Notch depth follows negative gain. Double-click resets.");
    qSlider.setTooltip ("Bandwidth. The knob's scroll is unchanged. Scroll a node to move from wide to narrow in a few steps. Shift or Cmd is fine.");
    thresholdSlider.setTooltip ("Level where the compressor starts to work.");
    ratioSlider.setTooltip ("How hard the compressor turns the signal down.");
    attackSlider.setTooltip ("How quickly the compressor reacts.");
    releaseSlider.setTooltip ("How quickly the compressor lets go.");
    kneeSlider.setTooltip ("Softens the onset of compression.");
    makeupSlider.setTooltip ("Gain after compression. Auto adds a level estimate on top.");
    mixSlider.setTooltip ("Parallel mix. Zero is the uncompressed signal.");

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass the whole plug-in, latency-aligned.");
    enableButton.setClickingTogglesState (true);
    enableButton.setTooltip ("Enable the selected band. Double-click a node to toggle it, or to reset its gain.");
    compEnable.setClickingTogglesState (true);
    compEnable.setTooltip ("Enable the compressor.");
    autoButton.setClickingTogglesState (true);
    autoButton.setTooltip ("Add a rough makeup gain from the threshold and ratio.");
    canvas.addAndMakeVisible (bypassButton);
    canvas.addAndMakeVisible (enableButton);
    canvas.addAndMakeVisible (compEnable);
    canvas.addAndMakeVisible (autoButton);
    canvas.addAndMakeVisible (meter);

    typeBox.addItemList (juce::StringArray { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch" }, 1);
    slopeBox.addItemList (juce::StringArray { "12 dB/oct", "24 dB/oct", "48 dB/oct" }, 1);
    positionBox.addItemList (juce::StringArray { "Post", "Pre" }, 1);
    typeBox.setTooltip ("Bell, shelf, cut, or notch.");
    slopeBox.setTooltip ("Cut slope. Bells, shelves, and notches ignore it.");
    positionBox.setTooltip ("Run the compressor before or after the EQ.");
    for (auto* box : { &typeBox, &slopeBox, &positionBox })
    {
        box->setScrollWheelEnabled (false);
        canvas.addAndMakeVisible (box);
    }

    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, ParamIDs::inputGain, inputSlider);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, ParamIDs::outputGain, outputSlider);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::bypass, bypassButton);
    compEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::compEnabled, compEnable);
    autoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::compAutoMakeup, autoButton);
    positionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, ParamIDs::compPosition, positionBox);
    thresholdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, ParamIDs::compThreshold, thresholdSlider);
    ratioAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, ParamIDs::compRatio, ratioSlider);
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, ParamIDs::compAttack, attackSlider);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, ParamIDs::compRelease, releaseSlider);
    kneeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, ParamIDs::compKnee, kneeSlider);
    makeupAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, ParamIDs::compMakeup, makeupSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, ParamIDs::compMix, mixSlider);

    selectBand (3);
    syncCharacterButtons();
    startTimerHz (30);

    setResizable (true, true);
    setResizeLimits (FrancEqAudioProcessor::minEditorWidth,
                     FrancEqAudioProcessor::minEditorHeight,
                     FrancEqAudioProcessor::maxEditorWidth,
                     FrancEqAudioProcessor::maxEditorHeight);
    setSize (processor.getEditorWidth(), processor.getEditorHeight());
    editorReady = true;
}

FrancEqAudioProcessorEditor::~FrancEqAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void FrancEqAudioProcessorEditor::configureKnob (FrancKnob& slider, bool frequencyWheel)
{
    slider.setFrequencyWheel (frequencyWheel);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 18);
    slider.setPopupDisplayEnabled (true, false, this);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                juce::MathConstants<float>::pi * 2.8f,
                                true);
    slider.setColour (juce::Slider::textBoxTextColourId, franc::ivory);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    canvas.addAndMakeVisible (slider);
}

void FrancEqAudioProcessorEditor::fitSliderText (FrancKnob& slider)
{
    const int width = juce::jlimit (44, 92, slider.getWidth() - 4);
    const int height = juce::jlimit (14, 20, slider.getHeight() / 5);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, width, height);
}

void FrancEqAudioProcessorEditor::configureLabel (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (11.0f).withKerningFactor (0.12f)));
    label.setColour (juce::Label::textColourId, franc::gold);
    label.setInterceptsMouseClicks (false, false);
    canvas.addAndMakeVisible (label);
}

void FrancEqAudioProcessorEditor::configureChoiceButton (juce::TextButton& button)
{
    button.setColour (juce::TextButton::buttonColourId, franc::panelRaised);
    button.setColour (juce::TextButton::buttonOnColourId, franc::gold);
    button.setColour (juce::TextButton::textColourOffId, franc::gold);
    button.setColour (juce::TextButton::textColourOnId, franc::background);
    canvas.addAndMakeVisible (button);
}

void FrancEqAudioProcessorEditor::selectBand (int index)
{
    selectedBand = juce::jlimit (0, franceq::bandCount - 1, index);
    display.setSelectedBand (selectedBand);

    for (int band = 0; band < franceq::bandCount; ++band)
        bandButtons[band].setToggleState (band == selectedBand, juce::dontSendNotification);

    rebuildBandAttachments();
}

void FrancEqAudioProcessorEditor::rebuildBandAttachments()
{
    enableAttachment.reset();
    typeAttachment.reset();
    slopeAttachment.reset();
    freqAttachment.reset();
    gainAttachment.reset();
    qAttachment.reset();

    enableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, ParamIDs::bandEnabled (selectedBand), enableButton);
    typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, ParamIDs::bandType (selectedBand), typeBox);
    slopeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, ParamIDs::bandSlope (selectedBand), slopeBox);
    freqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, ParamIDs::bandFreq (selectedBand), freqSlider);
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, ParamIDs::bandGain (selectedBand), gainSlider);
    qAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, ParamIDs::bandQ (selectedBand), qSlider);
}

void FrancEqAudioProcessorEditor::syncCharacterButtons()
{
    const int index = juce::jlimit (0, 3, juce::roundToInt (processor.apvts.getRawParameterValue (ParamIDs::character)->load()));
    for (int button = 0; button < 4; ++button)
        characterButtons[button].setToggleState (button == index, juce::dontSendNotification);
}

void FrancEqAudioProcessorEditor::timerCallback()
{
    syncCharacterButtons();
}

void FrancEqAudioProcessorEditor::refreshForSnapshot()
{
    syncCharacterButtons();
    display.refresh();
}

void FrancEqAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (franc::background);
}

void FrancEqAudioProcessorEditor::paintInterface (juce::Graphics& g)
{
    g.fillAll (franc::background);

    juce::ColourGradient glow (franc::gold.withAlpha (0.09f), static_cast<float> (canvas.getWidth()) * 0.5f, 28.0f,
                               franc::background, static_cast<float> (canvas.getWidth()) * 0.5f, 260.0f, true);
    g.setGradientFill (glow);
    g.fillRect (canvas.getLocalBounds());

    if (! titleBounds.isEmpty())
    {
        auto title = titleBounds.toFloat();
        auto rule = title.removeFromTop (14.0f);
        const float centreX = rule.getCentreX();
        g.setColour (franc::gold.withAlpha (0.75f));
        g.drawLine (rule.getX() + 40.0f, rule.getCentreY(), centreX - 14.0f, rule.getCentreY(), 1.0f);
        g.drawLine (centreX + 14.0f, rule.getCentreY(), rule.getRight() - 40.0f, rule.getCentreY(), 1.0f);

        juce::Path diamond;
        const float y = rule.getCentreY();
        diamond.startNewSubPath (centreX, y - 4.0f);
        diamond.lineTo (centreX + 4.0f, y);
        diamond.lineTo (centreX, y + 4.0f);
        diamond.lineTo (centreX - 4.0f, y);
        diamond.closeSubPath();
        g.setColour (franc::gold);
        g.fillPath (diamond);

        const juce::Font titleFont (juce::FontOptions (32.0f, juce::Font::bold).withKerningFactor (0.045f));
        drawTrackedText (g, "FrancEq", title.removeFromTop (36.0f), titleFont, franc::gold, 1.4f);

        const juce::Font subtitleFont (juce::FontOptions (11.0f).withKerningFactor (0.18f));
        drawTrackedText (g, "PARAMETRIC EQ", title, subtitleFont, franc::muted, 2.2f);
    }

    auto paintPanel = [&g] (juce::Rectangle<int> bounds)
    {
        if (bounds.isEmpty())
            return;
        auto panelBounds = bounds.toFloat();
        g.setColour (franc::panel);
        g.fillRoundedRectangle (panelBounds, 16.0f);
        g.setColour (franc::gold.withAlpha (0.20f));
        g.drawRoundedRectangle (panelBounds.reduced (0.5f), 16.0f, 1.0f);
    };

    paintPanel (railBounds);
    paintPanel (bandBounds);
    paintPanel (compBounds);
}

void FrancEqAudioProcessorEditor::resized()
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    const float scale = juce::jmin (static_cast<float> (getWidth()) / static_cast<float> (designWidth),
                                     static_cast<float> (getHeight()) / static_cast<float> (designHeight));
    const float scaledW = static_cast<float> (designWidth) * scale;
    const float scaledH = static_cast<float> (designHeight) * scale;
    const float offsetX = (static_cast<float> (getWidth()) - scaledW) * 0.5f;
    const float offsetY = (static_cast<float> (getHeight()) - scaledH) * 0.5f;

    canvas.setTransform (juce::AffineTransform::scale (scale).translated (offsetX, offsetY));
    canvas.setBounds (0, 0, designWidth, designHeight);

    // setResizeLimits can clamp a not-yet-sized editor to the minimum before the
    // saved size is applied. Ignore that pass so it does not overwrite plugin state.
    if (editorReady)
        processor.setEditorSize (getWidth(), getHeight());
}

void FrancEqAudioProcessorEditor::layoutDesign()
{
    if (canvas.getWidth() <= 0 || canvas.getHeight() <= 0)
        return;

    auto area = canvas.getLocalBounds().reduced (16, 10);
    titleBounds = area.removeFromTop (76);
    area.removeFromTop (6);

    auto rail = area.removeFromRight (176);
    area.removeFromRight (8);
    railBounds = rail;

    compBounds = area.removeFromBottom (128);
    area.removeFromBottom (8);
    bandBounds = area.removeFromBottom (140);
    area.removeFromBottom (8);
    display.setBounds (area);

    auto column = rail.reduced (8, 6);
    characterLabel.setBounds (column.removeFromTop (16));
    for (auto& button : characterButtons)
    {
        button.setBounds (column.removeFromTop (32).reduced (2, 2));
        column.removeFromTop (3);
    }

    column.removeFromTop (8);
    const int knobHeight = juce::jmax (90, (column.getHeight() - 8) / 3);
    auto inputArea = column.removeFromTop (knobHeight);
    inputLabel.setBounds (inputArea.removeFromTop (16));
    inputSlider.setBounds (inputArea);
    auto outputArea = column.removeFromTop (knobHeight);
    outputLabel.setBounds (outputArea.removeFromTop (16));
    outputSlider.setBounds (outputArea);
    bypassLabel.setBounds (column.removeFromTop (16));
    bypassButton.setBounds (column);

    auto band = bandBounds.reduced (12, 8);
    auto header = band.removeFromTop (34);
    bandLabel.setBounds (header.removeFromLeft (52));
    enableLabel.setBounds (header.removeFromRight (64));
    enableButton.setBounds (header.removeFromRight (78).reduced (0, 1));
    const int buttons = franceq::bandCount;
    for (int index = 0; index < buttons; ++index)
    {
        const int width = header.getWidth() / (buttons - index);
        bandButtons[index].setBounds (header.removeFromLeft (width).reduced (3, 3));
    }

    auto typeArea = band.removeFromLeft (150);
    auto slopeArea = band.removeFromLeft (142);
    typeLabel.setBounds (typeArea.removeFromTop (16));
    typeBox.setBounds (typeArea.removeFromTop (30).reduced (4, 2));
    slopeLabel.setBounds (slopeArea.removeFromTop (16));
    slopeBox.setBounds (slopeArea.removeFromTop (30).reduced (4, 2));

    juce::Slider* sliders[] = { &freqSlider, &gainSlider, &qSlider };
    juce::Label* labels[] = { &freqLabel, &gainLabel, &qLabel };
    for (int index = 0; index < 3; ++index)
    {
        auto knob = band.removeFromLeft (band.getWidth() / (3 - index));
        labels[index]->setBounds (knob.removeFromTop (16));
        sliders[index]->setBounds (knob);
    }

    auto comp = compBounds.reduced (12, 8);
    auto meterArea = comp.removeFromRight (68);
    grLabel.setBounds (meterArea.removeFromTop (16));
    meter.setBounds (meterArea.reduced (2, 0));

    auto side = comp.removeFromLeft (86);
    compLabel.setBounds (side.removeFromTop (14));
    compEnable.setBounds (side.removeFromTop (juce::jmin (52, side.getHeight() / 2)));
    autoLabel.setBounds (side.removeFromTop (14));
    autoButton.setBounds (side);

    auto positionArea = comp.removeFromLeft (108);
    positionLabel.setBounds (positionArea.removeFromTop (16));
    positionBox.setBounds (positionArea.removeFromTop (30).reduced (4, 2));

    juce::Slider* compSliders[] = { &thresholdSlider, &ratioSlider, &attackSlider, &releaseSlider,
                                    &kneeSlider, &makeupSlider, &mixSlider };
    juce::Label* compLabels[] = { &thresholdLabel, &ratioLabel, &attackLabel, &releaseLabel,
                                  &kneeLabel, &makeupLabel, &mixLabel };
    for (int index = 0; index < 7; ++index)
    {
        auto knob = comp.removeFromLeft (comp.getWidth() / (7 - index));
        compLabels[index]->setBounds (knob.removeFromTop (16));
        compSliders[index]->setBounds (knob);
    }

    for (auto* slider : { &inputSlider, &outputSlider, &freqSlider, &gainSlider, &qSlider,
                          &thresholdSlider, &ratioSlider, &attackSlider, &releaseSlider,
                          &kneeSlider, &makeupSlider, &mixSlider })
        fitSliderText (*slider);
}

juce::String FrancEqAudioProcessorEditor::layoutProblems() const
{
    juce::String problems;
    const auto window = getLocalBounds().toFloat();
    const auto visual = canvas.getBounds().toFloat().transformedBy (canvas.getTransform());

    if (visual.getWidth() < 8.0f || visual.getHeight() < 8.0f)
        problems << "scaled interface has no area; ";
    if (visual.getX() < -1.5f || visual.getY() < -1.5f
        || visual.getRight() > window.getRight() + 1.5f
        || visual.getBottom() > window.getBottom() + 1.5f)
        problems << "scaled interface sits outside the window; ";

    const auto canvasBounds = canvas.getLocalBounds();
    const juce::Rectangle<int> panels[] = { titleBounds, railBounds, bandBounds, compBounds, display.getBounds() };
    for (int index = 0; index < 5; ++index)
    {
        if (panels[index].getWidth() < 40 || panels[index].getHeight() < 24)
            problems << "panel " << index << " is too small; ";
        if (! canvasBounds.contains (panels[index]))
            problems << "panel " << index << " leaves the interface; ";
        for (int other = index + 1; other < 5; ++other)
            if (panels[index].intersects (panels[other]))
                problems << "panels " << index << " and " << other << " overlap; ";
    }

    juce::Array<const juce::Component*> controls;
    for (auto& button : characterButtons)
        controls.add (&button);
    for (auto& button : bandButtons)
        controls.add (&button);
    const juce::Component* more[] = {
        &display, &meter, &inputSlider, &outputSlider, &freqSlider, &gainSlider, &qSlider,
        &thresholdSlider, &ratioSlider, &attackSlider, &releaseSlider, &kneeSlider,
        &makeupSlider, &mixSlider, &bypassButton, &enableButton, &compEnable, &autoButton,
        &typeBox, &slopeBox, &positionBox,
    };
    for (auto* control : more)
        controls.add (control);

    for (int index = 0; index < controls.size(); ++index)
    {
        const auto bounds = controls[index]->getBounds();
        if (bounds.getWidth() < 16 || bounds.getHeight() < 16)
            problems << "control " << index << " is clipped (" << bounds.getWidth() << "x" << bounds.getHeight() << "); ";
        if (! canvasBounds.contains (bounds))
            problems << "control " << index << " leaves the interface; ";

        for (int other = index + 1; other < controls.size(); ++other)
            if (bounds.intersects (controls[other]->getBounds()))
                problems << "controls " << index << " and " << other << " overlap; ";
    }

    return problems;
}
