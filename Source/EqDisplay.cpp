#include "EqDisplay.h"

#include "Palette.h"

#include <algorithm>
#include <cmath>

namespace
{
juce::Colour bandColour (int index)
{
    static const juce::Colour colours[] = {
        juce::Colour::fromRGB (0xc9, 0xa8, 0x4c),
        juce::Colour::fromRGB (0xe4, 0xc5, 0x6b),
        juce::Colour::fromRGB (0xf3, 0xe6, 0xc0),
        juce::Colour::fromRGB (0xb8, 0x8a, 0x3e),
        juce::Colour::fromRGB (0xd4, 0xb4, 0x6a),
        juce::Colour::fromRGB (0xa8, 0x90, 0x58),
        juce::Colour::fromRGB (0xf4, 0xef, 0xe2),
    };

    return colours[static_cast<size_t> (juce::jlimit (0, franceq::bandCount - 1, index))];
}

const char* typeName (franceq::BandType type)
{
    switch (type)
    {
        case franceq::BandType::bell: return "BELL";
        case franceq::BandType::lowShelf: return "LOW SHELF";
        case franceq::BandType::highShelf: return "HIGH SHELF";
        case franceq::BandType::lowCut: return "LOW CUT";
        case franceq::BandType::highCut: return "HIGH CUT";
        case franceq::BandType::notch: return "NOTCH";
    }

    return "BELL";
}

const char* characterName (franceq::Character character)
{
    switch (character)
    {
        case franceq::Character::digital: return "DIGITAL";
        case franceq::Character::britN: return "BRIT N";
        case franceq::Character::britS: return "BRIT S";
        case franceq::Character::usA: return "US A";
    }

    return "DIGITAL";
}

juce::String formatReadoutFrequency (float frequency)
{
    if (frequency >= 1000.0f)
        return juce::String (frequency / 1000.0f, frequency >= 10000.0f ? 1 : 2) + " kHz";
    return juce::String (juce::roundToInt (frequency)) + " Hz";
}
} // namespace

EqDisplay::EqDisplay (FrancEqAudioProcessor& processorToUse)
    : processor (processorToUse),
      fft (FrancEqAudioProcessor::analyserOrder),
      window (static_cast<size_t> (FrancEqAudioProcessor::analyserSize), juce::dsp::WindowingFunction<float>::hann)
{
    frequencies.resize (static_cast<size_t> (curvePoints));
    combined.resize (static_cast<size_t> (curvePoints));
    fftData.resize (static_cast<size_t> (FrancEqAudioProcessor::analyserSize * 2));
    framePre.resize (static_cast<size_t> (FrancEqAudioProcessor::analyserSize));
    framePost.resize (static_cast<size_t> (FrancEqAudioProcessor::analyserSize));
    preColumn.fill (-96.0f);
    postColumn.fill (-96.0f);

    for (int point = 0; point < curvePoints; ++point)
    {
        const float proportion = static_cast<float> (point) / static_cast<float> (curvePoints - 1);
        frequencies[static_cast<size_t> (point)] = franceq::minFrequency
                                                 * std::pow (franceq::maxFrequency / franceq::minFrequency, proportion);
    }

    for (auto& curve : bandCurve)
        curve.resize (static_cast<size_t> (curvePoints), 0.0f);

    setOpaque (true);
    refresh();
    startTimerHz (60);
}

void EqDisplay::setSelectedBand (int index)
{
    selected = juce::jlimit (0, franceq::bandCount - 1, index);
    repaint();
}

void EqDisplay::timerCallback()
{
    refresh();
}

juce::Rectangle<float> EqDisplay::plotArea() const
{
    return getLocalBounds().toFloat().reduced (48.0f, 22.0f).withTrimmedBottom (8.0f);
}

float EqDisplay::frequencyToX (float frequency, juce::Rectangle<float> area) const
{
    const float clamped = juce::jlimit (franceq::minFrequency, franceq::maxFrequency, frequency);
    const float proportion = std::log (clamped / franceq::minFrequency) / std::log (franceq::maxFrequency / franceq::minFrequency);
    return area.getX() + proportion * area.getWidth();
}

float EqDisplay::xToFrequency (float x, juce::Rectangle<float> area) const
{
    const float proportion = juce::jlimit (0.0f, 1.0f, (x - area.getX()) / std::max (1.0f, area.getWidth()));
    return franceq::minFrequency * std::pow (franceq::maxFrequency / franceq::minFrequency, proportion);
}

float EqDisplay::dbToY (float db, juce::Rectangle<float> area) const
{
    const float clamped = juce::jlimit (-dbRange, dbRange, db);
    const float proportion = (clamped + dbRange) / (dbRange * 2.0f);
    return area.getBottom() - proportion * area.getHeight();
}

float EqDisplay::yToDb (float y, juce::Rectangle<float> area) const
{
    const float proportion = juce::jlimit (0.0f, 1.0f, (area.getBottom() - y) / std::max (1.0f, area.getHeight()));
    return juce::jlimit (-24.0f, 24.0f, -dbRange + proportion * dbRange * 2.0f);
}

float EqDisplay::yToQ (float y, juce::Rectangle<float> area) const
{
    const float proportion = juce::jlimit (0.0f, 1.0f, (area.getBottom() - y) / std::max (1.0f, area.getHeight()));
    return juce::jlimit (0.1f, 10.0f, 0.1f * std::pow (100.0f, proportion));
}

float EqDisplay::rawValue (const juce::String& id) const
{
    if (const auto* value = processor.apvts.getRawParameterValue (id))
        return value->load();
    return 0.0f;
}

juce::RangedAudioParameter* EqDisplay::parameter (const juce::String& id) const
{
    return dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (id));
}

void EqDisplay::setRaw (juce::RangedAudioParameter* parameterToSet, float raw) const
{
    if (parameterToSet != nullptr)
        parameterToSet->setValueNotifyingHost (parameterToSet->convertTo0to1 (raw));
}

void EqDisplay::rebuildCurves()
{
    const double sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    const auto character = static_cast<franceq::Character> (juce::jlimit (0, 3, juce::roundToInt (rawValue (ParamIDs::character))));
    const float shoulder = character == franceq::Character::britN ? 1.0f : 0.0f;

    franceq::BandCoeffs designed[franceq::bandCount];
    bool enabled[franceq::bandCount] = {};

    for (int band = 0; band < franceq::bandCount; ++band)
    {
        enabled[band] = rawValue (ParamIDs::bandEnabled (band)) >= 0.5f;
        const auto type = static_cast<franceq::BandType> (juce::jlimit (0, 5, juce::roundToInt (rawValue (ParamIDs::bandType (band)))));
        const float gain = rawValue (ParamIDs::bandGain (band));
        franceq::DesignRequest request;
        request.type = type;
        request.sampleRate = sampleRate;
        request.frequency = rawValue (ParamIDs::bandFreq (band));
        request.gainDb = gain;
        request.q = franceq::shapedQ (character, type, rawValue (ParamIDs::bandQ (band)), gain);
        request.slopeIndex = juce::jlimit (0, 2, juce::roundToInt (rawValue (ParamIDs::bandSlope (band))));
        request.shoulder = type == franceq::BandType::bell ? shoulder : 0.0f;
        designed[band] = enabled[band] ? franceq::designBand (request) : franceq::BandCoeffs {};
    }

    for (int point = 0; point < curvePoints; ++point)
    {
        const float frequency = frequencies[static_cast<size_t> (point)];
        float sum = 0.0f;
        for (int band = 0; band < franceq::bandCount; ++band)
        {
            const float db = enabled[band] ? franceq::bandMagnitudeDb (designed[band], frequency, sampleRate) : 0.0f;
            bandCurve[static_cast<size_t> (band)][static_cast<size_t> (point)] = db;
            sum += db;
        }
        combined[static_cast<size_t> (point)] = sum;
    }
}

void EqDisplay::rebuildSpectrum()
{
    processor.copyAnalyser (framePre.data(), framePost.data(), FrancEqAudioProcessor::analyserSize);

    const double sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    const int fftSize = FrancEqAudioProcessor::analyserSize;
    const int half = fftSize / 2;

    auto consume = [&] (const std::vector<float>& frame, std::array<float, spectrumColumns>& columns)
    {
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        std::copy (frame.begin(), frame.end(), fftData.begin());
        window.multiplyWithWindowingTable (fftData.data(), static_cast<size_t> (fftSize));
        fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

        for (int column = 0; column < spectrumColumns; ++column)
        {
            const float startFrequency = frequencies[static_cast<size_t> ((column * (curvePoints - 1)) / spectrumColumns)];
            const float endFrequency = frequencies[static_cast<size_t> (std::min (curvePoints - 1, ((column + 1) * (curvePoints - 1)) / spectrumColumns))];
            int startBin = static_cast<int> (std::floor (startFrequency / sampleRate * static_cast<double> (fftSize)));
            int endBin = static_cast<int> (std::ceil (endFrequency / sampleRate * static_cast<double> (fftSize)));
            startBin = juce::jlimit (1, half, startBin);
            endBin = juce::jlimit (startBin, half, endBin);

            float peak = 0.0f;
            for (int bin = startBin; bin <= endBin; ++bin)
                peak = std::max (peak, fftData[static_cast<size_t> (bin)]);

            const float db = 20.0f * std::log10 (peak * (4.0f / static_cast<float> (fftSize)) + 1.0e-12f);
            auto& shown = columns[static_cast<size_t> (column)];
            shown = db > shown ? shown * 0.45f + db * 0.55f : shown * 0.84f + db * 0.16f;
        }
    };

    consume (framePre, preColumn);
    consume (framePost, postColumn);
}

void EqDisplay::refresh()
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    rebuildCurves();
    rebuildSpectrum();
    repaint();
}

int EqDisplay::bandAt (juce::Point<float> position) const
{
    const auto area = plotArea();
    int found = -1;
    float best = 18.0f;

    for (int band = 0; band < franceq::bandCount; ++band)
    {
        const float distance = position.getDistanceFrom (nodePosition (band, area));
        const bool preferSelected = band == selected && distance < best + 4.0f;
        if (distance < best || (preferSelected && distance <= 18.0f))
        {
            best = distance;
            found = band;
        }
    }

    return found;
}

juce::Point<float> EqDisplay::nodePosition (int band, juce::Rectangle<float> area) const
{
    const auto type = static_cast<franceq::BandType> (juce::jlimit (0, 5, juce::roundToInt (rawValue (ParamIDs::bandType (band)))));
    const float frequency = rawValue (ParamIDs::bandFreq (band));
    float gain = rawValue (ParamIDs::bandGain (band));
    if (type == franceq::BandType::lowCut || type == franceq::BandType::highCut)
        gain = 0.0f;

    return { frequencyToX (frequency, area), dbToY (gain, area) };
}

void EqDisplay::beginDrag (int band)
{
    endDrag();
    dragBand = band;
    const auto type = static_cast<franceq::BandType> (juce::jlimit (0, 5, juce::roundToInt (rawValue (ParamIDs::bandType (band)))));
    dragChangesQ = type == franceq::BandType::lowCut || type == franceq::BandType::highCut;
    dragFrequency = parameter (ParamIDs::bandFreq (band));
    dragGain = parameter (ParamIDs::bandGain (band));
    dragQ = parameter (ParamIDs::bandQ (band));

    if (dragFrequency != nullptr)
        dragFrequency->beginChangeGesture();
    if (dragChangesQ)
    {
        if (dragQ != nullptr)
            dragQ->beginChangeGesture();
    }
    else if (dragGain != nullptr)
    {
        dragGain->beginChangeGesture();
    }

    gestureOpen = true;
}

void EqDisplay::endDrag()
{
    if (! gestureOpen)
        return;

    if (dragFrequency != nullptr)
        dragFrequency->endChangeGesture();
    if (dragChangesQ)
    {
        if (dragQ != nullptr)
            dragQ->endChangeGesture();
    }
    else if (dragGain != nullptr)
    {
        dragGain->endChangeGesture();
    }

    gestureOpen = false;
    dragBand = -1;
    dragFrequency = nullptr;
    dragGain = nullptr;
    dragQ = nullptr;
}

void EqDisplay::mouseMove (const juce::MouseEvent& event)
{
    setMouseCursor (bandAt (event.position) >= 0 ? juce::MouseCursor::DraggingHandCursor
                                                 : juce::MouseCursor::NormalCursor);
}

void EqDisplay::mouseDown (const juce::MouseEvent& event)
{
    const int band = bandAt (event.position);
    dragArmed = band >= 0;
    if (band < 0)
        return;

    setSelectedBand (band);
    if (onSelectBand != nullptr)
        onSelectBand (band);
}

void EqDisplay::mouseDrag (const juce::MouseEvent& event)
{
    if (! dragArmed || selected < 0)
        return;

    if (! gestureOpen)
    {
        if (rawValue (ParamIDs::bandEnabled (selected)) < 0.5f)
            setRaw (parameter (ParamIDs::bandEnabled (selected)), 1.0f);
        beginDrag (selected);
    }

    const auto area = plotArea();
    setRaw (dragFrequency, xToFrequency (event.position.x, area));

    if (dragChangesQ)
        setRaw (dragQ, yToQ (event.position.y, area));
    else
        setRaw (dragGain, yToDb (event.position.y, area));
}

void EqDisplay::mouseUp (const juce::MouseEvent&)
{
    dragArmed = false;
    endDrag();
}

void EqDisplay::mouseDoubleClick (const juce::MouseEvent& event)
{
    endDrag();
    const int band = bandAt (event.position);
    if (band < 0)
        return;

    setSelectedBand (band);
    if (onSelectBand != nullptr)
        onSelectBand (band);

    const auto type = static_cast<franceq::BandType> (juce::jlimit (0, 5, juce::roundToInt (rawValue (ParamIDs::bandType (band)))));
    const bool cut = type == franceq::BandType::lowCut || type == franceq::BandType::highCut;
    const bool enabled = rawValue (ParamIDs::bandEnabled (band)) >= 0.5f;
    const float gain = rawValue (ParamIDs::bandGain (band));
    auto* enable = parameter (ParamIDs::bandEnabled (band));
    auto* gainParameter = parameter (ParamIDs::bandGain (band));

    if (! cut && enabled && std::abs (gain) > 0.4f)
    {
        if (gainParameter != nullptr)
        {
            gainParameter->beginChangeGesture();
            setRaw (gainParameter, 0.0f);
            gainParameter->endChangeGesture();
        }
        return;
    }

    if (enable != nullptr)
    {
        enable->beginChangeGesture();
        setRaw (enable, enabled ? 0.0f : 1.0f);
        enable->endChangeGesture();
    }
}

void EqDisplay::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    int band = bandAt (event.position);
    if (band < 0 && plotArea().contains (event.position))
        band = selected;
    if (band < 0)
        return;

    float delta = std::abs (wheel.deltaY) > std::abs (wheel.deltaX) ? wheel.deltaY : wheel.deltaX;
    if (wheel.isReversed)
        delta = -delta;
    if (std::abs (delta) < 1.0e-4f)
        return;

    delta = juce::jlimit (-3.0f, 3.0f, delta);
    const float q = juce::jlimit (0.1f, 10.0f, rawValue (ParamIDs::bandQ (band)) * std::pow (1.07f, delta));
    auto* qParameter = parameter (ParamIDs::bandQ (band));
    if (qParameter == nullptr)
        return;

    setSelectedBand (band);
    if (onSelectBand != nullptr)
        onSelectBand (band);

    qParameter->beginChangeGesture();
    setRaw (qParameter, q);
    qParameter->endChangeGesture();
}

void EqDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll (franc::background);
    g.setColour (franc::panel);
    g.fillRoundedRectangle (bounds.reduced (1.0f), 16.0f);
    g.setColour (franc::gold.withAlpha (0.32f));
    g.drawRoundedRectangle (bounds.reduced (1.5f), 16.0f, 1.0f);

    const auto area = plotArea();
    juce::Path clip;
    clip.addRoundedRectangle (bounds.reduced (2.0f), 15.0f);
    g.reduceClipRegion (clip);

    auto spectrumY = [area] (float db)
    {
        const float clamped = juce::jlimit (-96.0f, 6.0f, db);
        const float proportion = (clamped + 96.0f) / 102.0f;
        return area.getBottom() - proportion * area.getHeight();
    };

    juce::Path postFill;
    postFill.startNewSubPath (area.getX(), area.getBottom());
    for (int column = 0; column < spectrumColumns; ++column)
    {
        const float x = area.getX() + (static_cast<float> (column) + 0.5f) / static_cast<float> (spectrumColumns) * area.getWidth();
        postFill.lineTo (x, spectrumY (postColumn[static_cast<size_t> (column)]));
    }
    postFill.lineTo (area.getRight(), area.getBottom());
    postFill.closeSubPath();
    g.setColour (franc::gold.withAlpha (0.16f));
    g.fillPath (postFill);

    juce::Path preStroke;
    for (int column = 0; column < spectrumColumns; ++column)
    {
        const float x = area.getX() + (static_cast<float> (column) + 0.5f) / static_cast<float> (spectrumColumns) * area.getWidth();
        const float y = spectrumY (preColumn[static_cast<size_t> (column)]);
        if (column == 0)
            preStroke.startNewSubPath (x, y);
        else
            preStroke.lineTo (x, y);
    }
    g.setColour (franc::champagne.withAlpha (0.28f));
    g.strokePath (preStroke, juce::PathStrokeType (1.0f));

    const float gridDb[] = { -24.0f, -18.0f, -12.0f, -6.0f, 0.0f, 6.0f, 12.0f, 18.0f, 24.0f };
    const float gridHz[] = { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
    g.setFont (juce::Font (juce::FontOptions (10.0f)));

    for (float db : gridDb)
    {
        const float y = dbToY (db, area);
        g.setColour (std::abs (db) < 0.1f ? franc::gold.withAlpha (0.45f) : franc::gold.withAlpha (0.12f));
        g.drawHorizontalLine (juce::roundToInt (y), area.getX(), area.getRight());
        g.setColour (franc::muted.withAlpha (0.85f));
        g.drawText (juce::String (juce::roundToInt (db)),
                    juce::Rectangle<float> (4.0f, y - 7.0f, 36.0f, 14.0f),
                    juce::Justification::centredRight,
                    false);
    }

    for (float hz : gridHz)
    {
        const float x = frequencyToX (hz, area);
        g.setColour (franc::gold.withAlpha (0.10f));
        g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());
        g.setColour (franc::muted.withAlpha (0.85f));
        const auto label = hz >= 1000.0f ? juce::String (juce::roundToInt (hz / 1000.0f)) + "k" : juce::String (juce::roundToInt (hz));
        g.drawText (label,
                    juce::Rectangle<float> (x - 18.0f, area.getBottom() + 2.0f, 36.0f, 14.0f),
                    juce::Justification::centred,
                    false);
    }

    auto strokeCurve = [this, &g, area] (const std::vector<float>& curve, juce::Colour colour, float thickness)
    {
        juce::Path path;
        for (int point = 0; point < curvePoints; ++point)
        {
            const float x = frequencyToX (frequencies[static_cast<size_t> (point)], area);
            const float y = dbToY (curve[static_cast<size_t> (point)], area);
            if (point == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    for (int band = 0; band < franceq::bandCount; ++band)
    {
        if (rawValue (ParamIDs::bandEnabled (band)) < 0.5f)
            continue;
        strokeCurve (bandCurve[static_cast<size_t> (band)], bandColour (band).withAlpha (band == selected ? 0.45f : 0.22f), 1.15f);
    }

    strokeCurve (combined, franc::gold.withAlpha (0.22f), 6.0f);
    strokeCurve (combined, franc::gold.withAlpha (0.90f), 1.7f);

    for (int band = 0; band < franceq::bandCount; ++band)
    {
        const bool enabled = rawValue (ParamIDs::bandEnabled (band)) >= 0.5f;
        const auto point = nodePosition (band, area);
        const bool isSelected = band == selected;
        const float radius = isSelected ? 8.0f : 5.5f;
        const auto colour = bandColour (band).withAlpha (enabled ? 1.0f : 0.35f);

        if (isSelected)
        {
            g.setColour (franc::goldBright.withAlpha (0.85f));
            g.drawEllipse (point.x - radius - 3.0f, point.y - radius - 3.0f, (radius + 3.0f) * 2.0f, (radius + 3.0f) * 2.0f, 1.3f);
        }

        g.setColour (franc::background.withAlpha (0.85f));
        g.fillEllipse (point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour (colour);
        g.drawEllipse (point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f, isSelected ? 2.0f : 1.3f);
        g.setColour (enabled ? franc::ivory : franc::muted);
        g.setFont (juce::Font (juce::FontOptions (isSelected ? 11.0f : 9.5f, juce::Font::bold)));
        g.drawText (juce::String (band + 1),
                    juce::Rectangle<float> (point.x - 10.0f, point.y - 8.0f, 20.0f, 16.0f),
                    juce::Justification::centred,
                    false);
    }

    const auto character = static_cast<franceq::Character> (juce::jlimit (0, 3, juce::roundToInt (rawValue (ParamIDs::character))));
    const auto selectedType = static_cast<franceq::BandType> (juce::jlimit (0, 5, juce::roundToInt (rawValue (ParamIDs::bandType (selected)))));
    const float selectedGain = rawValue (ParamIDs::bandGain (selected));
    juce::String gainText = (selectedGain > 0.05f ? "+" : "") + juce::String (selectedGain, 1) + " dB";
    const auto readout = "B" + juce::String (selected + 1) + "  " + typeName (selectedType) + "  "
                       + formatReadoutFrequency (rawValue (ParamIDs::bandFreq (selected))) + "  "
                       + gainText + "  Q " + juce::String (rawValue (ParamIDs::bandQ (selected)), 2);

    g.setColour (franc::ivory.withAlpha (0.88f));
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.drawText (readout, bounds.reduced (16.0f, 10.0f).removeFromTop (16.0f), juce::Justification::centredLeft, false);
    g.setColour (franc::gold.withAlpha (0.80f));
    g.setFont (juce::Font (juce::FontOptions (11.0f).withKerningFactor (0.14f)));
    g.drawText (characterName (character), bounds.reduced (16.0f, 10.0f).removeFromTop (16.0f), juce::Justification::centredRight, false);
}
