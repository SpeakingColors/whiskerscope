#!/usr/bin/env python3
"""Builds the short PDF manual from docs/manual.txt. Usage: python3 tools/make_manual.py <ignored> <docs/manual.txt> <out.pdf>
(The first argument is kept so the release workflow can call every Studio 87 plugin the same way.) Needs: pip install reportlab"""
import sys
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, ListFlowable, ListItem

txt, out = sys.argv[2], sys.argv[3]
esc = lambda s: s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
h1 = ParagraphStyle("h1", fontName="Helvetica-Bold", fontSize=22, spaceAfter=4, textColor=colors.HexColor("#4F009A"))
sub = ParagraphStyle("sub", fontName="Helvetica", fontSize=10, textColor=colors.HexColor("#777777"), spaceAfter=14)
h2 = ParagraphStyle("h2", fontName="Helvetica-Bold", fontSize=14, spaceBefore=12, spaceAfter=4, textColor=colors.HexColor("#4F009A"))
body = ParagraphStyle("b", fontName="Helvetica", fontSize=10.5, leading=14.5, spaceAfter=6)
doc = SimpleDocTemplate(out, pagesize=A4, leftMargin=20 * mm, rightMargin=20 * mm, topMargin=18 * mm, bottomMargin=18 * mm, title="Whisker Scope manual", author="Studio 87")
story = [Paragraph("Whisker Scope", h1), Paragraph("Manual. Free and open source by Studio 87. Support: ko-fi.com/studio87", sub)]
sections, cur = [], None
for line in open(txt, encoding="utf-8").read().splitlines():
    if line.startswith("# "): cur = [line[2:].strip(), []]; sections.append(cur)
    elif cur is not None: cur[1].append(line)
for title, lines in sections:
    story.append(Paragraph(esc(title), h2)); buf, items = [], []
    def flush_text():
        global buf
        if buf: story.append(Paragraph(esc(" ".join(buf)), body)); buf = []
    def flush_items():
        global items
        if items: story.append(ListFlowable([ListItem(Paragraph(esc(i), body)) for i in items], bulletType="bullet", leftIndent=14)); items = []
    for l in lines:
        if l.startswith("- "): flush_text(); items.append(l[2:].strip())
        elif l.strip() == "": flush_text(); flush_items()
        else: flush_items() if False else None; buf.append(l.strip())
    flush_text(); flush_items()
doc.build(story)
print("wrote", out)
