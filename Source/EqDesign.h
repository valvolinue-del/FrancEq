#pragma once

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <cmath>

// Minimum-phase biquad design shared by the audio thread and the response curve.
// Character only retunes Q and, for Brit N bells, adds a wide shoulder. It does
// not change the underlying cookbook filters.
namespace franceq
{

inline constexpr int bandCount = 7;
inline constexpr int maxSections = 5;
inline constexpr float minFrequency = 20.0f;
inline constexpr float maxFrequency = 20000.0f;

enum class BandType
{
    bell = 0,
    lowShelf,
    highShelf,
    lowCut,
    highCut,
    notch
};

enum class Character
{
    digital = 0,
    britN,
    britS,
    usA
};

struct BiquadCoeff
{
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
};

struct BandCoeffs
{
    BiquadCoeff sections[maxSections] {};
    int count = 0;
    float notchDepth = 0.0f;
};

struct DesignRequest
{
    BandType type = BandType::bell;
    double sampleRate = 48000.0;
    float frequency = 1000.0f;
    float gainDb = 0.0f;
    float q = 0.71f;
    int slopeIndex = 1;
    float shoulder = 0.0f;
};

inline float dbToGain (float db) noexcept
{
    return std::pow (10.0f, db * 0.05f);
}

inline float defaultBandFrequency (int index) noexcept
{
    constexpr float frequencies[bandCount] = { 60.0f, 150.0f, 400.0f, 1000.0f, 2500.0f, 6000.0f, 12000.0f };
    return frequencies[static_cast<size_t> (juce::jlimit (0, bandCount - 1, index))];
}

inline float clampFrequency (float frequency, double sampleRate) noexcept
{
    const float nyquist = static_cast<float> (sampleRate * 0.49);
    const float upper = std::min (maxFrequency, std::max (minFrequency + 1.0f, nyquist));
    return juce::jlimit (minFrequency, upper, frequency);
}

// Digital: the Q the user asked for.
// Brit N: broader bells and gentler shelves, a little wider still as |gain| grows.
// Brit S: tighter bells and shelves.
// US A: proportional Q, so bells and notches narrow as |gain| grows and sit at the
//        user's Q when the gain is zero.
inline float shapedQ (Character character, BandType type, float q, float gainDb) noexcept
{
    q = juce::jlimit (0.1f, 10.0f, q);
    const float amount = std::abs (gainDb) / 24.0f;
    const bool bell = type == BandType::bell;
    const bool shelf = type == BandType::lowShelf || type == BandType::highShelf;
    const bool notch = type == BandType::notch;

    switch (character)
    {
        case Character::britN:
            if (bell)
                return juce::jlimit (0.15f, 4.0f, q * 0.55f / (1.0f + 0.40f * amount));
            if (shelf)
                return juce::jlimit (0.25f, 1.15f, q * 0.70f);
            if (notch)
                return juce::jlimit (0.20f, 6.0f, q * 0.80f);
            return juce::jlimit (0.20f, 8.0f, q * 0.85f);

        case Character::britS:
            if (bell)
                return juce::jlimit (0.25f, 16.0f, q * 1.45f);
            if (shelf)
                return juce::jlimit (0.35f, 2.40f, q * 1.15f);
            return juce::jlimit (0.25f, 14.0f, q * 1.12f);

        case Character::usA:
            if (bell || notch)
                return juce::jlimit (0.12f, 16.0f, q * (1.0f + 1.65f * amount));
            if (shelf)
                return juce::jlimit (0.30f, 1.80f, q * (1.0f + 0.35f * amount));
            return q;

        case Character::digital:
            break;
    }

    return q;
}

inline BiquadCoeff fromArray (const std::array<double, 6>& coeffs) noexcept
{
    const double a0 = std::abs (coeffs[3]) < 1.0e-18 ? 1.0 : coeffs[3];
    const double inv = 1.0 / a0;
    return BiquadCoeff {
        coeffs[0] * inv,
        coeffs[1] * inv,
        coeffs[2] * inv,
        coeffs[4] * inv,
        coeffs[5] * inv,
    };
}

inline void butterworthQs (int order, double* qs, int& count) noexcept
{
    count = order / 2;
    for (int section = 0; section < count; ++section)
    {
        const double theta = (2.0 * static_cast<double> (section) + 1.0)
                           * juce::MathConstants<double>::pi
                           / (2.0 * static_cast<double> (order));
        qs[section] = 1.0 / (2.0 * std::sin (theta));
    }
}

inline BandCoeffs designBand (const DesignRequest& request) noexcept
{
    BandCoeffs result;
    if (request.sampleRate <= 0.0)
        return result;

    const float frequency = clampFrequency (request.frequency, request.sampleRate);
    const float q = juce::jlimit (0.1f, 18.0f, request.q);
    const bool cut = request.type == BandType::lowCut || request.type == BandType::highCut;

    auto add = [&result] (const std::array<double, 6>& coeffs)
    {
        if (result.count < maxSections)
            result.sections[result.count++] = fromArray (coeffs);
    };

    using Coeffs = juce::dsp::IIR::ArrayCoefficients<double>;

    if (request.type == BandType::notch)
    {
        result.notchDepth = juce::jlimit (0.0f, 1.0f, -request.gainDb / 24.0f);
        if (result.notchDepth < 0.004f)
        {
            result.notchDepth = 0.0f;
            return result;
        }

        add (Coeffs::makeNotch (request.sampleRate, static_cast<double> (frequency), static_cast<double> (q)));
        return result;
    }

    if (! cut && std::abs (request.gainDb) < 0.02f)
        return result;

    if (request.type == BandType::bell)
    {
        const float shoulder = juce::jlimit (0.0f, 1.0f, request.shoulder);
        const float mainDb = request.gainDb * (1.0f - 0.28f * shoulder);
        const float shoulderDb = request.gainDb * 0.28f * shoulder;
        add (Coeffs::makePeakFilter (request.sampleRate,
                                     static_cast<double> (frequency),
                                     static_cast<double> (q),
                                     static_cast<double> (dbToGain (mainDb))));

        if (std::abs (shoulderDb) >= 0.02f)
        {
            add (Coeffs::makePeakFilter (request.sampleRate,
                                         static_cast<double> (frequency),
                                         0.38,
                                         static_cast<double> (dbToGain (shoulderDb))));
        }

        return result;
    }

    if (request.type == BandType::lowShelf || request.type == BandType::highShelf)
    {
        const double shelfQ = static_cast<double> (juce::jlimit (0.2f, 2.5f, q));
        const double gain = static_cast<double> (dbToGain (request.gainDb));
        if (request.type == BandType::lowShelf)
            add (Coeffs::makeLowShelf (request.sampleRate, static_cast<double> (frequency), shelfQ, gain));
        else
            add (Coeffs::makeHighShelf (request.sampleRate, static_cast<double> (frequency), shelfQ, gain));
        return result;
    }

    const int order = request.slopeIndex <= 0 ? 2 : (request.slopeIndex == 1 ? 4 : 8);
    double qs[4] = { 0.0, 0.0, 0.0, 0.0 };
    int sections = 0;
    butterworthQs (order, qs, sections);
    const double qScale = static_cast<double> (q) / 0.7071067811865476;

    for (int section = 0; section < sections; ++section)
    {
        const double sectionQ = juce::jlimit (0.15, 18.0, qs[section] * qScale);
        if (request.type == BandType::lowCut)
            add (Coeffs::makeHighPass (request.sampleRate, static_cast<double> (frequency), sectionQ));
        else
            add (Coeffs::makeLowPass (request.sampleRate, static_cast<double> (frequency), sectionQ));
    }

    return result;
}

inline float bandMagnitudeDb (const BandCoeffs& band, float frequency, double sampleRate) noexcept
{
    if (band.count <= 0 || sampleRate <= 0.0)
        return 0.0f;

    const double omega = juce::MathConstants<double>::twoPi * static_cast<double> (frequency) / sampleRate;
    const double cw = std::cos (omega);
    const double sw = std::sin (omega);
    const double cw2 = std::cos (2.0 * omega);
    const double sw2 = std::sin (2.0 * omega);

    auto response = [cw, sw, cw2, sw2] (const BiquadCoeff& coeff, double real, double imag, double& outReal, double& outImag)
    {
        const double numRe = coeff.b0 + coeff.b1 * cw + coeff.b2 * cw2;
        const double numIm = -(coeff.b1 * sw + coeff.b2 * sw2);
        const double denRe = 1.0 + coeff.a1 * cw + coeff.a2 * cw2;
        const double denIm = -(coeff.a1 * sw + coeff.a2 * sw2);
        const double den = std::max (1.0e-20, denRe * denRe + denIm * denIm);
        const double hRe = (numRe * denRe + numIm * denIm) / den;
        const double hIm = (numIm * denRe - numRe * denIm) / den;
        outReal = real * hRe - imag * hIm;
        outImag = real * hIm + imag * hRe;
    };

    double real = 1.0;
    double imag = 0.0;
    for (int section = 0; section < band.count; ++section)
    {
        double nextReal = 0.0;
        double nextImag = 0.0;
        response (band.sections[section], real, imag, nextReal, nextImag);
        real = nextReal;
        imag = nextImag;
    }

    if (band.notchDepth > 0.0f)
    {
        const double depth = static_cast<double> (band.notchDepth);
        real = (1.0 - depth) + depth * real;
        imag = depth * imag;
    }

    const double magnitude = std::sqrt (real * real + imag * imag);
    return static_cast<float> (20.0 * std::log10 (std::max (1.0e-12, magnitude)));
}

} // namespace franceq
