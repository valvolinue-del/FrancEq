# FrancEq

FrancEq is a parametric EQ and compressor (VST3 and Standalone) built with JUCE 8 and C++20.

Seven bands cover bell, low shelf, high shelf, low cut, high cut, and notch. Cuts can be 12, 24, or 48 dB/oct. Frequency, gain, and Q are smoothed, and the biquad coefficients are rebuilt from those smoothed values, so moving a control does not zipper. A character control changes the curve shape and adds a gently oversampled saturation stage. The compressor follows the EQ by default and can be moved in front of it.

The editor draws the combined response in gold, a faint curve for each band, and a real-time spectrum behind them. Drag a node to change frequency and gain, scroll to change Q, and double-click to reset a moved gain or to toggle the band. The display reads a lock-free ring written by the audio thread and repaints at 60 Hz.

BRIT N, BRIT S, and US A are inspired by classic Neve, SSL, and API console EQ character. Neve, SSL, and API are trademarks of their owners. FrancEq is not affiliated with, endorsed by, or associated with those companies.

## Character

| UI | What it does |
| --- | --- |
| DIGITAL | Minimum-phase biquads at the Q you set. No saturation. This is the default. |
| BRIT N | Broader bells (and a little broader as the gain grows), plus a wide low-Q shoulder on each bell so neighbouring bands interact. The saturator leans on even harmonics, in the manner of a transformer. The compressor is slower and softer. |
| BRIT S | Tighter bells and shelves. The saturator leans on odd harmonics. The compressor attacks faster and shortens its release when only a little gain reduction is happening. |
| US A | Proportional Q: bells and notches get narrower as \|gain\| grows, and return to the knob's Q at 0 dB. The saturator mixes even and odd harmonics. The compressor uses a feedback-style detector. |

All four paths report the same latency. Digital bypasses the saturator and uses a matching delay. The other three run the waveshaper at 4x oversampling (`juce::dsp::Oversampling`) so the added harmonics fold back less. Small-signal gain of the waveshaper is unity, so a moderate sine stays close to the same level in every mode. Loud peaks are softened in the coloured modes.

## Compressor

Threshold, ratio, attack, release, knee, makeup, parallel mix, and an optional auto-makeup estimate. Stereo channels share one detector. The gain-reduction meter shows the current reduction in dB. Auto makeup adds about `0.35 * -threshold * (1 - 1/ratio)` dB, capped at 18 dB.

## Parameters

Global and compressor:

| ID | Name | Range | Default |
| --- | --- | --- | --- |
| `inputGain` | Input Gain | -24 to +24 dB | 0 dB |
| `outputGain` | Output Gain | -24 to +24 dB | 0 dB |
| `bypass` | Bypass | off / on | off |
| `character` | Character | Digital, Brit N, Brit S, US A | Digital |
| `compEnabled` | Compressor | off / on | off |
| `compPosition` | Position | Post, Pre | Post |
| `compThreshold` | Threshold | -60 to 0 dB | -18 dB |
| `compRatio` | Ratio | 1:1 to 20:1 | 4:1 |
| `compAttack` | Attack | 0.1–200 ms | 10 ms |
| `compRelease` | Release | 5–2000 ms | 120 ms |
| `compKnee` | Knee | 0–24 dB | 6 dB |
| `compMakeup` | Makeup | -12 to +24 dB | 0 dB |
| `compAutoMakeup` | Auto Makeup | off / on | off |
| `compMix` | Comp Mix | 0–100% | 100% |

Each band `N` from 1 to 7 has `bandNEnabled`, `bandNType`, `bandNFreq` (20 Hz–20 kHz, logarithmic), `bandNGain` (±24 dB), `bandNQ` (0.1–10), and `bandNSlope` (12 / 24 / 48 dB/oct). Defaults are enabled bells at 0 dB, spread from 60 Hz to 12 kHz, so Digital starts flat. State is saved and restored through the parameter tree.

## Build

Requirements: CMake 3.22 or newer, a C++20 compiler, and a network connection the first time you configure (CMake fetches JUCE 8.0.15). To build against a JUCE checkout you already have, pass `-DFRANCEQ_JUCE_PATH=/path/to/JUCE` instead.

Web browser and curl support in JUCE are turned off, which avoids the Linux webkit dependency.

### Linux

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config \
  libasound2-dev libfreetype6-dev libfontconfig1-dev libx11-dev libxext-dev \
  libxrandr-dev libxinerama-dev libxcursor-dev libxrender-dev libxcomposite-dev \
  libgl1-mesa-dev libglu1-mesa-dev

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

`libfreetype6-dev` supplies the `libfreetype-dev` dependency JUCE links against, and `libfontconfig1-dev` is required alongside it. If `c++` on your machine is not GCC and cannot link `libstdc++`, configure with `-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++`.

### macOS

Install Xcode (or the Command Line Tools) and CMake, then:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release
```

The Standalone app can be launched directly. Copy `FrancEq.vst3` to `~/Library/Audio/Plug-Ins/VST3` when you want it in a host.

### Windows

From a Visual Studio x64 developer prompt, with the Desktop development with C++ workload installed:

```bat
cmake -B build -A x64
cmake --build build --config Release
```

Copy the `FrancEq.vst3` bundle to `C:\Program Files\Common Files\VST3`. The Standalone target is `FrancEq.exe`.

### Artefacts

After a Release build:

- VST3: `build/FrancEq_artefacts/Release/VST3/FrancEq.vst3`
- Standalone: `build/FrancEq_artefacts/Release/Standalone/FrancEq` (`.exe` on Windows, `.app` on macOS)

The plug-in is not copied into the system plug-in folders automatically.

### Sanity test

`FrancEqSanity` processes audio offline: a flat Digital EQ must match a delayed copy of the input, a bell must boost, a low cut must attenuate, the compressor must reduce gain, and the coloured modes must stay finite and add harmonics.

```bash
cmake --build build --config Release --target FrancEqSanity
./build/FrancEqSanity_artefacts/Release/FrancEqSanity
./build/FrancEqSanity_artefacts/Release/FrancEqSanity --snapshot /tmp/franceq-ui.png
```

Pass `-DFRANCEQ_BUILD_TESTS=OFF` to skip that target. The snapshot flag needs a display (or Xvfb) because it opens the editor offscreen.

## Continuous integration

`.github/workflows/ci.yml` builds a Windows MSVC x64 package and a macOS universal package (arm64 + x86_64, deployment target 11.0) and uploads `FrancEq-windows-x64` and `FrancEq-macos-universal`. A tag named `v*` also publishes those zip files as a GitHub Release.

Developer ID signing and notarization run only when all six secrets are set:

- `APPLE_CERTIFICATE_P12_BASE64`
- `APPLE_CERTIFICATE_PASSWORD`
- `APPLE_SIGNING_IDENTITY`
- `APPLE_ID`
- `APPLE_APP_SPECIFIC_PASSWORD`
- `APPLE_TEAM_ID`

If none of them are set, the macOS artifact is unsigned. If only some are set, the job fails rather than shipping a half-signed build.

## Layout

```
CMakeLists.txt
Source/PluginProcessor.h
Source/PluginProcessor.cpp
Source/PluginEditor.h
Source/PluginEditor.cpp
Source/EqDisplay.h
Source/EqDisplay.cpp
Source/EqDesign.h
Source/Palette.h
tests/Sanity.cpp
```

## Limitations

Coefficient updates follow the smoothed parameters every sample, which is smooth, but switching filter type resets that band's filter memory and can tick once. The coloured saturators are musical approximations of console behaviour, not circuit models. Auto makeup is a fixed estimate, not a loudness measurement. The spectrum is a 2048-point Hann FFT of the mono sum, with a separate pre-EQ and post-output trace.

JUCE itself is covered by the [JUCE licence](https://juce.com/legal/juce-8-licence/) (or AGPLv3). This plug-in's manufacturer code is `Frnc` and its plug-in code is `FrEq`.
