# Whisker Scope 🐈

A scope, spectrum and frequency analyzer that had too much catnip at lunch.

Free and open source analyser plugin (VST3 and Standalone) from Studio 87. v0.3.1 adds genre reference presets (chart-study based and rule-of-thumb ones, clearly labelled; see docs/genre-curves.md). v0.3.0 adds a key detector on the Spectrum and reference files with reference presets on Shapes. v0.2.0 has the real editor built from the approved mockup. It has been built and checked by CI (pluginval) but has not been tested by hand in a DAW or on macOS yet.

## What exists
- Scope, Spectrum, Stereo field and Shapes views, resizable window (fixed shape, all edges and corners, 25 % to 250 %)
- Every control is an automatable parameter; presets and saved shapes are plain files
- L/R and Mid/Side analysis, 60 bands at 1/6 octave for Shapes
- Reference matching with a real Mid/Side match EQ in the audio path (30 bells per channel, Strength, Max move, EQ on, auto level, safety limiter)
- Sidechain layer: the second input is drawn in yellow and the Spectrum marks where two tracks may mask each other
- Peak label with level, frequency and note; momentary loudness-style meters, correlation, width and mono check
- PDF manual with an interface tour made from screenshots of the real editor

## Not done yet
- Tests by hand in real DAWs, on macOS and on Linux hosts
- Installers not tested on real machines
- Match EQ and presets not tuned by ear

## Build
`cmake -B build -DWS_BUILD_PLUGIN=ON && cmake --build build`. Screenshots of the real editor (for the manual tour): add `-DWS_BUILD_SHOOT=ON`, run `xvfb-run -a ws_shoot <dir> 1360 0`, then `python3 tools/tour_gen.py <dir> docs/tour`. The engine tests run without JUCE: `g++ -std=c++17 -Iinclude tests/engine_test.cpp && ./a.out`.

## Try the design
Open [`docs/mockup/whisker-scope-mockup.html`](docs/mockup/whisker-scope-mockup.html) in a browser. It is a playable mockup of the plugin window.

## Docs
- [Roadmap](ROADMAP.md)
- [Manual text](docs/manual.txt)

## Support
Free to use.

<p align="center">
  <a href="https://ko-fi.com/studio87"><img src="https://img.shields.io/badge/Support_Studio_87-on_Ko--fi-FF5E5B?style=for-the-badge&logo=kofi&logoColor=white" alt="Support Studio 87 on Ko-fi"></a>
</p>

☕ Free and open source, made by one person. If it helps your music, a coffee on [Ko-fi](https://ko-fi.com/studio87) keeps it coming. 💜

## License
AGPL-3.0-or-later, see [LICENSE](LICENSE).

Original design. It contains no code, presets or artwork from any other product.
