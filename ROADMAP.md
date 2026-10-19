# Roadmap

1. Design mockup (done, `docs/mockup`)
2. JUCE plugin: analysis engine and the four views (code written, engine unit tested, CI added; not tested in a DAW)
3. Real editor built from the mockup (first version written)
4. Safety limiter, smoothing, reported latency, pluginval in CI
5. Release workflow (one zip: Windows .exe, macOS .pkg, Linux Flatpak extension and .vst3, PDF manual)
6. Version comes from the release tag (for example v0.2.0), is built into the plugin and shown on the About page, so tag, installer and plugin always match
7. Sidechain layer (masking view), not started
8. Tests in real DAWs and on macOS before any release
