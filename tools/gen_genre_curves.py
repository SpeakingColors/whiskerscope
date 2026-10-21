#!/usr/bin/env python3
"""Single source of truth for the genre reference presets (starting points).
Run: python3 tools/gen_genre_curves.py   (rewrites the marked blocks in the mockup and in src/PluginProcessor.cpp)
Format of one curve: name, tilt dB/oct about 1 kHz (band level is per-Hz density, so pink noise = -3),
bumps (centre Hz, width in octaves, gain dB), hp corner Hz (12 dB/oct below it, 0 = none),
top corner Hz and extra dB/oct above it (0 = none). Levels are relative: the plugin levels every shape to its own average.
Nothing here is measured from a song. Sources and confidence are in docs/genre-curves.md."""
import re, sys, pathlib
ROOT = pathlib.Path(__file__).resolve().parents[1]
CURVES = [
 # name, tilt, bumps, hp, topHz, topSlope
 ("Gentle tilt (-3 dB/oct)", -3, [], 0, 0, 0),
 ("Bright lead", -2, [(3000,1.2,5),(9000,1,4),(60,1,-6)], 0, 0, 0),
 ("Dark warm pad", -5, [(250,1,3),(8000,1,-6)], 0, 0, 0),
 ("Kick and bass focus", -4.5, [(55,0.7,8),(110,0.8,3),(2500,1.2,-4)], 0, 0, 0),
 ("Trance supersaw", -3.5, [(1200,1.2,3),(5000,1.3,3),(40,1,-8)], 0, 0, 0),
 ("Vocal presence", -4, [(200,1,2),(3000,1,4),(100,0.8,-6)], 0, 0, 0),
 # medium confidence: published study of chart records (Pestana, Ma, Reiss 2013) and the 4.5 dB/oct analyser convention
 ("Master slope (-4.5 dB/oct)", -4.5, [], 0, 0, 0),
 ("Chart pop average (study)", -5, [(85,0.7,1.5)], 45, 4000, -2),
 ("Chart electronic (study)", -4.5, [(55,0.8,3)], 30, 5000, -1.5),
 ("Chart hip hop (study)", -4.9, [(50,0.7,5)], 28, 0, 0),
 # low confidence: our own constructions from practitioner ranges, amounts are rules of thumb
 ("House club (rule of thumb)", -4.5, [(80,0.8,3),(220,1,-1.5)], 28, 0, 0),
 ("Techno club (rule of thumb)", -4.5, [(55,0.7,3),(220,1,-1.5)], 28, 0, 0),
 ("Hard techno (rule of thumb)", -4, [(50,0.6,4),(300,1,-2.5)], 28, 0, 0),
 ("Trance club (rule of thumb)", -4, [(55,0.7,3),(1200,1.5,1),(5000,1.3,1.5)], 30, 0, 0),
 ("DnB (rule of thumb)", -4.5, [(50,0.8,4),(70,0.4,2),(350,1,-1)], 25, 0, 0),
 ("Dubstep (rule of thumb)", -4.5, [(40,0.7,5),(350,1.3,2)], 22, 0, 0),
 ("Hardstyle (rule of thumb)", -4, [(50,0.55,6),(500,0.8,1.5),(3000,1.2,3)], 30, 8000, -3),
 ("UK hardcore (rough guess)", -3.5, [(70,0.8,3),(100,0.5,2),(2500,1.5,2.5)], 35, 0, 0),
]
def fmt(x): return ("%g" % x)
def js():
    rows = []
    for n,t,b,hp,tf,ts in CURVES:
        bs = ",".join("[%s,%s,%s]" % (fmt(a),fmt(w),fmt(g)) for a,w,g in b)
        rows.append('  ["%s", %s, [%s], %s, %s, %s]' % (n, fmt(t), bs, fmt(hp), fmt(tf), fmt(ts)))
    return "var FACT = [\n" + ",\n".join(rows) + "\n];"
def cpp():
    rows = []
    for n,t,b,hp,tf,ts in CURVES:
        bs = ", ".join("{ %s, %s, %s }" % (fmt(a),fmt(w),fmt(g)) for a,w,g in b)
        rows.append('        { "%s", %s, { %s }, %s, %s, %s }' % (n, fmt(t), bs, fmt(hp), fmt(tf), fmt(ts)))
    return ",\n".join(rows)
def patch(path, begin, end, body):
    s = path.read_text(); pat = re.compile(re.escape(begin) + r".*?" + re.escape(end), re.S)
    if not pat.search(s): sys.exit("markers missing in " + str(path))
    path.write_text(pat.sub(lambda m: begin + "\n" + body + "\n" + end, s, count=1))
if __name__ == "__main__":
    assert all(len(c[0]) <= 40 for c in CURVES) and len({c[0] for c in CURVES}) == len(CURVES)
    patch(ROOT/"docs/mockup/whisker-scope-mockup.html", "/* GENRE-CURVES-BEGIN */", "/* GENRE-CURVES-END */", js())
    patch(ROOT/"src/PluginProcessor.cpp", "// GENRE-CURVES-BEGIN", "// GENRE-CURVES-END", cpp())
    print("ok", len(CURVES))
