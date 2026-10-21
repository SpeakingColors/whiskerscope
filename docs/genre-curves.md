# Genre reference presets in Whisker Scope (v0.3.1)

Reference presets on the Shapes page are starting points made by us from formulas. Nothing is measured from a commercial song and no vendor data, preset or table was copied. Names tell you how much to trust them:

- **(study)**: shape follows a peer-reviewed study of chart recordings. Medium confidence, but it is an average of hit records 1950 to 2010, not of any club genre today.
- **(rule of thumb)**: our own construction from producer and engineer advice (frequency ranges of kick, sub, leads). Low confidence: the amounts of the boosts are our judgement.
- **(rough guess)**: no published data was found. Very low confidence.

Use them to get a first idea, then load a real reference song of your own (Load file) and trust that over any preset.

## Format

One curve is a tilt in dB per octave about 1 kHz (the band level is per Hz, so pink noise is -3 dB/oct), a few bells (centre Hz, width in octaves, gain dB), an optional high-pass corner (12 dB/oct below it) and an optional extra slope above a top corner. The plugin levels every shape to its own average, so only the balance matters. The Side curve is the Mid curve lowered by 18 dB below 150 Hz, 8 dB to 500 Hz and 3 dB above: that is our convention; the studies below are mono and say nothing about Side. The formulas live in `tools/gen_genre_curves.py`, which writes both the mockup and the plugin.

## Sources and what they actually say

| Source | Says | Quality |
|---|---|---|
| Pestana, Ma, Reiss, "Spectral characteristics of popular commercial recordings 1950-2010", AES Convention 135, 2013 (772 US and UK number-one singles, mono, 4096 FFT). https://ciencia.ucp.pt/en/publications/spectral-characteristics-of-popular-commercial-recordings-1950-20/ | About 5 dB per octave decay between 100 Hz and 4 kHz, steeper above; a severe low-cut near 60 Hz; electronic and hip hop have an "extremely extended low end"; the low end is the only region where genre intervals do not overlap; hip hop and electronic are the brightest. Genre slopes (log-log, 100 Hz to 10 kHz): pop -0.94, rock -0.98, electronic -0.75, hip hop -0.81, jazz -1.29. Our conversion to dB/oct multiplies by 6.02 (magnitude): pop -5.7, electronic -4.5, hip hop -4.9. | Peer reviewed. Numbers read from a secondary copy; the paper says the genre table has a centroid column that contradicts its text. |
| Voxengo SPAN manual, via Filimowicz, "Using Spectrum Analyzer Slopes" https://medium.com/ai-music/spectrum-analyzer-slopes-in-audio-mixing-d6df8892ea3 | SPAN shows spectra with a 4.5 dB/oct slope by default; 3 dB/oct is flat for pink noise. | Vendor choice and a blog; shows the convention, not a measurement. |
| TrackSensei, how to mix hardstyle https://tracksensei.com/blog/how-to-mix-hardstyle | Kick sub tail 40 to 60 Hz, tok 200 to 800 Hz, crunch 2 to 8 kHz, screech zone 500 Hz to 5 kHz (resonances 2 to 4 kHz); masters -4 to -6 LUFS; club genres -7 to -9. | Commercial site, opinion. |
| Hardwave Studios, frequency ranges in hardstyle https://hardwavestudios.com/learn/spectrum-analysis/frequency-ranges-in-hardstyle | Kick sub 30 to 80 Hz (fundamental 40 to 65), punch 80 to 200, click 1 to 4 kHz, screech 1 to 5 kHz, hats 4 to 16 kHz, problem zone 200 to 400 Hz. Says its numbers are heuristics. | Vendor heuristics. |
| Beat Kitchen, mixing electronic https://beatkitchen.io/guides/electronic-music/09-mixing-electronic/ | LUFS: house -8 to -10, hardstyle and DnB -6 to -8, deep house -10 to -12. Sub 20 to 65 Hz pure. No spectral targets per genre. | Guide, approximate. |
| Freshly Baked Studios, house mastering https://freshlybakedstudios.com/blog/house-music-mastering | Club master -7 to -10 LUFS; high-pass 25 to 30 Hz; kick and bass weight 60 to 150 Hz; mud 150 to 300 Hz; boxy 300 to 500 Hz. | Engineer opinion, no data. |
| Future Proof Music School, hard techno https://futureproofmusicschool.com/blog/making-hard-techno-a-path-to-unique-sound-design | Rumble kept below about 150-200 Hz; master low end mono below 150 Hz; sides cut 200 to 400 Hz; about -6 to -5 LUFS. | School blog, opinion. |
| richiezxy, mastering modern EDM bass music https://richiezxy.btw.so/mastering-modern-edm-bass-music-drum--bass-and-beyond | DnB kick 60 to 80 Hz, bassline 40 to 60, sub to 25; dubstep sub 30 to 50, wobble fundamental about 60, harmonics 200 to 500; high-pass 20 to 25 Hz. | Unsourced personal page. |
| MusicRadar, 24 happy hardcore tips https://www.musicradar.com/tuition/tech/24-happy-hardcore-tips-33701 | Kick punch around 100 Hz, sine sub an octave under the bass, hoover sounds nasty with a notch. No mastering data. | Magazine tips. |
| iZotope Tonal Balance Control docs https://downloads.izotope.com/docs/tonal-balance-control/meters-and-target-curves/index.html | States its factory targets come from analysing many tracks (Orchestral, Bass Heavy, Modern). | Vendor claim; no data used. |
| Spotify for Artists loudness page https://support.spotify.com/ao-en/artists/article/loudness-normalization/ | Normal level -14 LUFS (loud -11, quiet -19); keep true peak at or below -1 dBTP, -2 if louder than -14. | Official platform page. |

Where sources disagree: TrackSensei puts hardstyle at -4 to -6 LUFS, Beat Kitchen at -6 to -8, and the project recipes at -6.5 to -7.5. Hardwave says the kick sub reaches 80 Hz, TrackSensei stops at 60 Hz. The low-mid trouble zone is 200 to 400 Hz in one, 150 to 300 Hz in another. We kept loudness out of the presets for this reason: "Match reference" would otherwise push your output to a loudness nobody agrees on.

## What is solid and what is not

- Solid: the overall slope near -4.5 to -5 dB/oct from 100 Hz to a few kHz, the roll-off below about 60 Hz in chart music, and that electronic and hip hop carry more low end and top than pop or rock.
- Thin: every club genre. No peer-reviewed average spectrum of house, techno, trance, DnB, dubstep or hardstyle was found, and no published "standard" curve for UK hardcore or happy hardcore exists that we could find. Those presets are the base slope plus bells placed where the sources say the kick, sub and leads live.
- Not covered: the Side channel, the true loudness norm of a genre, and anything about today's releases (the study ends in 2010).

## The curves

| Preset | Tilt dB/oct | High-pass | Bells (Hz, width oct, dB) | Top |
|---|---|---|---|---|
| Master slope (-4.5 dB/oct) | -4.5 | none | none | none |
| Chart pop average (study) | -5 | 45 Hz | 85, 0.7, +1.5 | -2 dB/oct above 4 kHz |
| Chart electronic (study) | -4.5 | 30 Hz | 55, 0.8, +3 | -1.5 dB/oct above 5 kHz |
| Chart hip hop (study) | -4.9 | 28 Hz | 50, 0.7, +5 | none |
| House club (rule of thumb) | -4.5 | 28 Hz | 80, 0.8, +3; 220, 1, -1.5 | none |
| Techno club (rule of thumb) | -4.5 | 28 Hz | 55, 0.7, +3; 220, 1, -1.5 | none |
| Hard techno (rule of thumb) | -4 | 28 Hz | 50, 0.6, +4; 300, 1, -2.5 | none |
| Trance club (rule of thumb) | -4 | 30 Hz | 55, 0.7, +3; 1200, 1.5, +1; 5000, 1.3, +1.5 | none |
| DnB (rule of thumb) | -4.5 | 25 Hz | 50, 0.8, +4; 70, 0.4, +2; 350, 1, -1 | none |
| Dubstep (rule of thumb) | -4.5 | 22 Hz | 40, 0.7, +5; 350, 1.3, +2 | none |
| Hardstyle (rule of thumb) | -4 | 30 Hz | 50, 0.55, +6; 500, 0.8, +1.5; 3000, 1.2, +3 | -3 dB/oct above 8 kHz |
| UK hardcore (rough guess) | -3.5 | 35 Hz | 70, 0.8, +3; 100, 0.5, +2; 2500, 1.5, +2.5 | none |
