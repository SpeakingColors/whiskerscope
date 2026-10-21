# Roadmap

1. Design mockup (done, `docs/mockup`)
2. JUCE plugin: analysis engine and the four views (code written, engine unit tested, CI added; not tested in a DAW)
3. Real editor built from the mockup (done in v0.2.0: layout, wood cheeks, info bubbles, resizable, automatable parameters)
4. Safety limiter, smoothing, reported latency, pluginval in CI
5. Release workflow (one zip: Windows .exe, macOS .pkg, Linux Flatpak extension and .vst3, PDF manual)
6. Version comes from the release tag (for example v0.2.0), is built into the plugin and shown on the About page, so tag, installer and plugin always match
7. Sidechain layer (masking view) and the real match EQ stage (done in v0.2.0)
8. Key/scale detector on the Spectrum; reference audio files and a reference preset library on Shapes (done in v0.3.0)
9. Tests by hand in real DAWs and on macOS (not possible from the build sandbox)
