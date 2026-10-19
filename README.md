# Whisker Scope 🐈

A scope, spectrum and frequency analyzer that had too much catnip at lunch.

Free and open source analyser plugin (VST3) from Studio 87. Early stage (v0.1.0 code, no release yet): the analysis engine and all four views are written and unit tested. It has not been tested in a DAW yet.

## What exists
- Scope, Spectrum, Stereo field and Shapes views
- L/R and Mid/Side analysis, 60 bands at 1/6 octave for Shapes
- Peak label with level, frequency and note
- Reference matching: capture a reference, see the EQ difference, save and share shapes as text
- Momentary loudness (K weighted, a monitor, not certified) and stereo meters
- The audio passes through untouched

## Not done yet
- Sidechain layer (masking view)
- Tests inside real DAWs, on macOS, and on Linux hosts
- No release published, no installers tested

## Build
`cmake -B build -DWS_BUILD_PLUGIN=ON && cmake --build build`. The engine tests run without JUCE: `g++ -std=c++17 -Iinclude tests/engine_test.cpp && ./a.out`.

## Try the design
Open [`docs/mockup/whisker-scope-mockup.html`](docs/mockup/whisker-scope-mockup.html) in a browser. It is a playable mockup, not the plugin.

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
