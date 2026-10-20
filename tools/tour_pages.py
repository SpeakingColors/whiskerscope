"""Interface tour pages for the Whisker Scope manual. Reads docs/tour/<key>.json, extra.json and the PNGs made by tools/tour_gen.py
from screenshots of the real editor (tools/shoot.cpp). Used by tools/make_manual.py."""
import json, os
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.lib.utils import ImageReader
from reportlab.platypus import Paragraph, Spacer, Table, TableStyle, Image, PageBreak, CondPageBreak

PINK, TEAL, INK = colors.HexColor("#C2185B"), colors.HexColor("#007A66"), colors.HexColor("#222222")
CELL = ParagraphStyle("tc", fontName="Helvetica", fontSize=9, leading=12, textColor=INK)
CELLB = ParagraphStyle("tcb", fontName="Helvetica-Bold", fontSize=9, leading=12, textColor=PINK)

def _img(path, width, maxh=None):
    w, h = ImageReader(path).getSize()
    if maxh and width * h / w > maxh: width = maxh * w / h
    return Image(path, width=width, height=width * h / w)

def _table(rows):
    t = Table(rows, colWidths=[10 * mm, 34 * mm, 126 * mm])
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("BACKGROUND", (0, 0), (-1, -1), colors.HexColor("#F3F3F7")),
                           ("LINEBELOW", (0, 0), (-1, -1), 0.5, colors.white), ("TOPPADDING", (0, 0), (-1, -1), 4), ("BOTTOMPADDING", (0, 0), (-1, -1), 4)]))
    return t

def tour_story(tdir, key, h2, body, esc):
    p = os.path.join(tdir, key + ".json")
    if not os.path.exists(p): return []
    d = json.load(open(p, encoding="utf-8")); X = json.load(open(os.path.join(tdir, "extra.json"), encoding="utf-8"))[key]
    sub = ParagraphStyle("tsub", parent=body, fontName="Helvetica-Bold", fontSize=11.5, leading=15, textColor=TEAL, spaceBefore=6, spaceAfter=3)
    out = [PageBreak(), Paragraph("Interface tour", h2),
           Paragraph(esc("Pink numbers mark controls, teal letters mark displays. Every number is explained in the tables on the next pages. The pictures are screenshots of the real plugin window (rendered offscreen from the plugin code with a made-up test signal), not a mockup."), body),
           Spacer(1, 4), _img(os.path.join(tdir, key + "_full.png"), 170 * mm), Spacer(1, 6),
           Paragraph("Signal flow", sub), Paragraph(esc(X["flow"]), body)]
    out += [Paragraph("Header and tabs", sub), _table([[Paragraph(str(t["n"]), CELLB), Paragraph(esc(t["name"]), CELLB), Paragraph(esc(t["tip"]), CELL)] for t in d["top"]])]
    for P in d["items"]:
        L = P["letter"]
        out += [CondPageBreak(150 * mm), Paragraph(esc("%s. %s" % (L, P["title"])), h2), _img(os.path.join(tdir, P["img"]), 130 * mm, 100 * mm), Spacer(1, 4),
                Paragraph("<b>Display %s:</b> %s" % (L, esc(X["displays"][L])), body), Spacer(1, 4)]
        rows = []
        for it in P["items"]:
            val = (" (shown: %s)" % it["value"]) if it["value"] and it["kind"] == "menu" else ""
            rows.append([Paragraph(str(it["n"]), CELLB), Paragraph(esc(it["name"]), CELLB), Paragraph(esc(it["tip"]) + "<font color='#777777'>" + esc(val) + "</font>", CELL)])
        if rows: out.append(_table(rows))
    out += [PageBreak(), Paragraph("Under the hood", h2)]
    for t in X["deep"]: out.append(Paragraph("&bull; " + esc(t), body))
    return out
