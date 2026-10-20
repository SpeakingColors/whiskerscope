#!/usr/bin/env python3
"""Makes docs/tour from the screenshots of the real editor.
  1. build with -DWS_BUILD_SHOOT=ON, run:  xvfb-run -a ws_shoot <rawDir> 1360 0   (writes real_<page>.png and real_<page>.json)
  2. python3 tools/tour_gen.py <rawDir> docs/tour
Pink numbered badges mark controls, teal lettered badges mark displays. Needs Pillow."""
import json, os, sys
from PIL import Image, ImageDraw, ImageFont

raw, out = sys.argv[1], sys.argv[2]; os.makedirs(out, exist_ok=True)
here = os.path.dirname(os.path.abspath(__file__)); FONT = os.path.join(here, "..", "assets", "fonts", "BarlowCondensed-Bold.ttf")
font = ImageFont.truetype(FONT, 17)
pages = [("spec", "Spectrum", "A"), ("scope", "Scope", "B"), ("stereo", "Stereo field", "C"), ("shapes", "Shapes", "D")]
load = lambda k: json.load(open(os.path.join(raw, "real_%s.json" % k), encoding="utf-8"))
TABS = {"Spectrum", "Scope", "Stereo field", "Shapes"}
region = lambda it: "top" if it["box"][1] < 200 else ("meters" if it["box"][0] >= 1075 else "page")
order = lambda it: (it["box"][1] // 40, it["box"][0])
n = 0
def number(items):
    global n
    for it in sorted(items, key=order): n += 1; it["n"] = n
    return items

spec = load("spec")
top = number([i for i in spec if region(i) == "top" or (i["name"] in TABS)])
meters = number([i for i in spec if region(i) == "meters" and i["name"] not in TABS])
panels = []
for key, title, letter in pages:
    its = load(key); disp = [i for i in its if i["kind"] == "display"][0]
    ctl = number([i for i in its if region(i) == "page" and i["kind"] != "display" and i["name"] not in TABS])
    panels.append({"key": key, "title": title, "letter": letter, "disp": disp, "ctl": ctl})

def badge(d, x, y, text, teal):
    w = d.textlength(text, font=font) + 10; w = max(w, 22)
    box = (x, y, x + w, y + 22); d.rounded_rectangle(box, radius=11 if not teal else 4, fill=(0, 200, 170) if teal else (255, 45, 140), outline=(0, 0, 0), width=1)
    d.text((x + w / 2, y + 11), text, font=font, fill=(0, 0, 0) if teal else (255, 255, 255), anchor="mm")

def annotate(img, items, letters):
    d = ImageDraw.Draw(img)
    for it in items:
        x, y, w, h = it["box"]
        if it["kind"] == "knob": bx, by = x + w - 26, y + 2
        elif it["kind"] == "menu": bx, by = x + w - 24, y - 2
        else: bx, by = x + w - 20, y - 10
        badge(d, bx, by, str(it["n"]), False)
    for L, b in letters: badge(d, b[0], b[1], L, True)

def shot(k): return Image.open(os.path.join(raw, "real_%s.png" % k)).convert("RGB")
# full picture: Spectrum page with the header, rig, meters and the spectrum controls numbered, every display lettered
full = shot("spec"); letters = [(p["letter"], [p["disp"]["box"][0] + p["disp"]["box"][2] - 40, p["disp"]["box"][1] + p["disp"]["box"][3] - 56]) for p in panels[:1]] + [("E", [1150, 212])]
annotate(full, top + meters + panels[0]["ctl"], letters); full.save(os.path.join(out, "ws_full.png"))
res = {"key": "ws", "top": [{"n": i["n"], "name": i["name"], "tip": i["tip"]} for i in top if i["tip"]], "items": []}
for i, p in enumerate(panels, 1):
    img = shot(p["key"]); annotate(img, p["ctl"], [(p["letter"], [p["disp"]["box"][0] + p["disp"]["box"][2] - 40, p["disp"]["box"][1] + p["disp"]["box"][3] - 56])])
    img.crop((44, 240, 1072, 880)).save(os.path.join(out, "ws_p%d.png" % i))
    res["items"].append({"title": p["title"], "img": "ws_p%d.png" % i, "letter": p["letter"], "items": [{"n": c["n"], "kind": c["kind"], "name": c["name"], "tip": c["tip"], "value": c["value"]} for c in p["ctl"]]})
img = shot("spec"); annotate(img, meters, [("E", [1150, 212])]); img.crop((1075, 200, 1320, 880)).save(os.path.join(out, "ws_p5.png"))
res["items"].append({"title": "Meters", "img": "ws_p5.png", "letter": "E", "items": [{"n": c["n"], "kind": c["kind"], "name": c["name"], "tip": c["tip"], "value": c["value"]} for c in meters]})
json.dump(res, open(os.path.join(out, "ws.json"), "w", encoding="utf-8"), indent=1, ensure_ascii=False)
open(os.path.join(out, "key.txt"), "w").write("ws\n"); print("controls numbered:", n)
