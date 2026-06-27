#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generate top-down illustration SVGs for the 5 V2V safety systems
(BSW, DNPW, EEBL, FCW, IMA), styled to match the V2X dashboard dark theme.

Each SVG is a self-contained scene you can drop into a slide or open in the
gallery (index.html). Re-run this script after tweaking to regenerate.

    python3 generate.py
"""

import os

OUT = os.path.dirname(os.path.abspath(__file__))

W, H = 1600, 1000

# ---- Palette (mirrors DashBoard/css/style.css :root tokens) ----
TEXT   = "#e8eaed"
MUTED  = "#8b93a0"
ACCENT = "#4aa6ff"   # host / V2V link
PURPLE = "#7c5cff"   # DSRC broadcast
SAFE   = "#2ecc71"
WARN   = "#f5a623"
CRIT   = "#ff4d4f"
ROAD   = "#1a1c22"

AR = "'Noto Sans Arabic','Vazirmatn','Segoe UI',sans-serif"
EN = "'Segoe UI','Noto Sans Arabic',sans-serif"


# ====================================================================
#  Reusable defs (gradients + glow filters + arrow markers)
# ====================================================================
def defs():
    return f"""
  <defs>
    <radialGradient id="gBg" cx="50%" cy="34%" r="85%">
      <stop offset="0%"  stop-color="#0e1016"/>
      <stop offset="55%" stop-color="#06070a"/>
      <stop offset="100%" stop-color="#000000"/>
    </radialGradient>
    <linearGradient id="gRoad" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%"  stop-color="#23262e"/>
      <stop offset="50%" stop-color="{ROAD}"/>
      <stop offset="100%" stop-color="#101218"/>
    </linearGradient>
    <linearGradient id="gHost" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%"  stop-color="#6fc0ff"/>
      <stop offset="45%" stop-color="#3f8fe6"/>
      <stop offset="100%" stop-color="#1f4f8f"/>
    </linearGradient>
    <linearGradient id="gThreat" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%"  stop-color="#2b2f38"/>
      <stop offset="45%" stop-color="#16181e"/>
      <stop offset="100%" stop-color="#08090c"/>
    </linearGradient>
    <linearGradient id="gNeutral" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%"  stop-color="#3a3f4a"/>
      <stop offset="50%" stop-color="#272b33"/>
      <stop offset="100%" stop-color="#15171c"/>
    </linearGradient>
    <linearGradient id="gTruck" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%"  stop-color="#5b6272"/>
      <stop offset="50%" stop-color="#454b57"/>
      <stop offset="100%" stop-color="#2b2f38"/>
    </linearGradient>
    <linearGradient id="gGlass" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0%"  stop-color="#0b1622"/>
      <stop offset="50%" stop-color="#13283d"/>
      <stop offset="100%" stop-color="#0b1622"/>
    </linearGradient>
    <filter id="glowB" x="-60%" y="-60%" width="220%" height="220%">
      <feGaussianBlur stdDeviation="7" result="b"/>
      <feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge>
    </filter>
    <filter id="glowR" x="-80%" y="-80%" width="260%" height="260%">
      <feGaussianBlur stdDeviation="6" result="b"/>
      <feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge>
    </filter>
    <filter id="soft" x="-40%" y="-40%" width="180%" height="180%">
      <feDropShadow dx="0" dy="6" stdDeviation="10" flood-color="#000" flood-opacity="0.55"/>
    </filter>
    <marker id="arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7"
            markerHeight="7" orient="auto-start-reverse">
      <path d="M0 0 L10 5 L0 10 z" fill="{ACCENT}"/>
    </marker>
    <marker id="arrowW" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7"
            markerHeight="7" orient="auto-start-reverse">
      <path d="M0 0 L10 5 L0 10 z" fill="{WARN}"/>
    </marker>
  </defs>"""


# ====================================================================
#  Building blocks
# ====================================================================
def car(cx, cy, angle=0, kind="threat", brake=False, scale=1.0, plate=None):
    """Top-down car centred at (cx,cy), nose pointing +x before rotation."""
    L, Wd = 150, 74
    if kind == "host":
        body, stroke, headl, glow = "url(#gHost)", "#bfe2ff", "#eaf6ff", "glowB"
    elif kind == "neutral":
        body, stroke, headl, glow = "url(#gNeutral)", "#566070", "#fff0c8", "soft"
    else:
        body, stroke, headl, glow = "url(#gThreat)", "#3c424e", "#ffe6a8", "soft"

    tail = CRIT
    tail_w = 5 if not brake else 9
    brake_bloom = ""
    if brake:
        brake_bloom = (f'<ellipse cx="{-L/2-26}" cy="0" rx="46" ry="40" '
                       f'fill="{CRIT}" opacity="0.30" filter="url(#glowR)"/>')

    hl = Wd/2 - 16  # lateral offset of lamps
    wheels = "".join(
        f'<rect x="{x-17}" y="{y-7}" width="34" height="14" rx="6" fill="#070809"/>'
        for x in (-44, 46) for y in (-(Wd/2+2), (Wd/2+2)))

    g = f"""
  <g transform="translate({cx},{cy}) rotate({angle}) scale({scale})" filter="url(#{glow})">
    {wheels}
    {brake_bloom}
    <rect x="{-L/2}" y="{-Wd/2}" width="{L}" height="{Wd}" rx="26" ry="30"
          fill="{body}" stroke="{stroke}" stroke-width="2"/>
    <rect x="-30" y="{-Wd/2+12}" width="74" height="{Wd-24}" rx="16"
          fill="url(#gGlass)" stroke="#0a141f" stroke-width="1.5"/>
    <line x1="44" y1="{-Wd/2+13}" x2="44" y2="{Wd/2-13}" stroke="#08121c" stroke-width="2"/>
    <rect x="{L/2-16}" y="{-hl-6}" width="14" height="12" rx="3" fill="{headl}"/>
    <rect x="{L/2-16}" y="{hl-6}"  width="14" height="12" rx="3" fill="{headl}"/>
    <rect x="{-L/2+4}" y="{-hl-7}" width="9" height="14" rx="3" fill="{tail}" opacity="0.95"/>
    <rect x="{-L/2+4}" y="{hl-7}"  width="9" height="14" rx="3" fill="{tail}" opacity="0.95"/>
    <line x1="{-L/2+5}" y1="0" x2="{-L/2+5}" y2="0" stroke="{tail}" stroke-width="{tail_w}"/>
  </g>"""
    return g


def truck(cx, cy, angle=0):
    L, Wd = 220, 92
    wheels = "".join(
        f'<rect x="{x-20}" y="{y-8}" width="40" height="16" rx="6" fill="#070809"/>'
        for x in (-70, 0, 72) for y in (-(Wd/2+2), (Wd/2+2)))
    return f"""
  <g transform="translate({cx},{cy}) rotate({angle})" filter="url(#soft)">
    {wheels}
    <rect x="{-L/2}" y="{-Wd/2}" width="{L}" height="{Wd}" rx="16"
          fill="url(#gTruck)" stroke="#7e879a" stroke-width="2.5"/>
    <line x1="{L/2-52}" y1="{-Wd/2}" x2="{L/2-52}" y2="{Wd/2}" stroke="#1a1d24" stroke-width="3"/>
    <rect x="{L/2-46}" y="{-Wd/2+8}" width="40" height="{Wd-16}" rx="10"
          fill="url(#gGlass)" stroke="#0a141f" stroke-width="1.5"/>
    <rect x="{L/2-8}" y="{-Wd/2+12}" width="8" height="14" rx="3" fill="#fff0c8"/>
    <rect x="{L/2-8}" y="{Wd/2-26}" width="8" height="14" rx="3" fill="#fff0c8"/>
    <rect x="{-L/2+3}" y="{-Wd/2+12}" width="7" height="14" rx="3" fill="{CRIT}"/>
    <rect x="{-L/2+3}" y="{Wd/2-26}" width="7" height="14" rx="3" fill="{CRIT}"/>
  </g>"""


def broadcast(cx, cy, color=PURPLE, rings=(58, 104, 154, 210, 272), label=True):
    out = []
    for i, r in enumerate(rings):
        op = max(0.08, 0.55 - i * 0.1)
        out.append(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="{color}" '
                   f'stroke-width="2.6" stroke-dasharray="2 12" opacity="{op:.2f}"/>')
    if label:
        out.append(f'<g transform="translate({cx},{cy-rings[-1]-6})">'
                   f'<rect x="-44" y="-26" width="88" height="26" rx="13" fill="{color}" opacity="0.16"/>'
                   f'<text x="0" y="-7" font-family="{EN}" font-size="15" font-weight="800" '
                   f'fill="{color}" text-anchor="middle">V2V · DSRC</text></g>')
    return "".join(out)


def link(x1, y1, x2, y2, color=ACCENT, dash="6 9"):
    return (f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{color}" '
            f'stroke-width="3" stroke-dasharray="{dash}" opacity="0.9" '
            f'marker-end="url(#arrow)"/>')


def burst(cx, cy, color=CRIT, r=34):
    pts = []
    import math
    for k in range(16):
        rr = r if k % 2 == 0 else r * 0.46
        a = math.pi * 2 * k / 16 - math.pi / 2
        pts.append(f"{cx+rr*math.cos(a):.1f},{cy+rr*math.sin(a):.1f}")
    return (f'<polygon points="{" ".join(pts)}" fill="{color}" opacity="0.9" '
            f'filter="url(#glowR)"/>'
            f'<text x="{cx}" y="{cy+6}" font-family="{EN}" font-size="22" '
            f'font-weight="900" fill="#fff" text-anchor="middle">!</text>')


def road_h(y0, y1):
    return (f'<rect x="-60" y="{y0}" width="{W+120}" height="{y1-y0}" fill="url(#gRoad)"/>'
            f'<line x1="-60" y1="{y0}" x2="{W+60}" y2="{y0}" stroke="#3a3f4a" stroke-width="4"/>'
            f'<line x1="-60" y1="{y1}" x2="{W+60}" y2="{y1}" stroke="#3a3f4a" stroke-width="4"/>')


def road_v(x0, x1):
    return (f'<rect x="{x0}" y="-60" width="{x1-x0}" height="{H+120}" fill="url(#gRoad)"/>'
            f'<line x1="{x0}" y1="-60" x2="{x0}" y2="{H+60}" stroke="#3a3f4a" stroke-width="4"/>'
            f'<line x1="{x1}" y1="-60" x2="{x1}" y2="{H+60}" stroke="#3a3f4a" stroke-width="4"/>')


def dash_h(y, x0=-60, x1=W+60, color="#525762"):
    return (f'<line x1="{x0}" y1="{y}" x2="{x1}" y2="{y}" stroke="{color}" stroke-width="5" '
            f'stroke-dasharray="46 38" stroke-linecap="round" opacity="0.85"/>')


def dash_v(x, y0=-60, y1=H+60, color="#525762"):
    return (f'<line x1="{x}" y1="{y0}" x2="{x}" y2="{y1}" stroke="{color}" stroke-width="5" '
            f'stroke-dasharray="46 38" stroke-linecap="round" opacity="0.85"/>')


def double_yellow_h(y):
    return (f'<line x1="-60" y1="{y-4}" x2="{W+60}" y2="{y-4}" stroke="#caa63c" stroke-width="4"/>'
            f'<line x1="-60" y1="{y+4}" x2="{W+60}" y2="{y+4}" stroke="#caa63c" stroke-width="4"/>')


def label(x, y, text, size=22, color=TEXT, anchor="middle", weight=700, font=AR, rtl=True):
    # NOTE: no direction="rtl" — with text-anchor="end" it shifts the anchor to the
    # wrong (left) edge and overflows. The HarfBuzz shaper lays Arabic out RTL anyway.
    return (f'<text x="{x}" y="{y}" font-family="{font}" font-size="{size}" '
            f'font-weight="{weight}" fill="{color}" text-anchor="{anchor}">{text}</text>')


def tag(cx, cy, text, color=TEXT, bg="rgba(0,0,0,0.55)"):
    w = 26 + len(text) * 11
    return (f'<g transform="translate({cx},{cy})">'
            f'<rect x="{-w/2}" y="-18" width="{w}" height="32" rx="16" fill="{bg}" '
            f'stroke="{color}" stroke-opacity="0.45" stroke-width="1.5"/>'
            f'<text x="0" y="4" font-family="{AR}" font-size="18" font-weight="700" '
            f'fill="{color}" text-anchor="middle">{text}</text></g>')


def chip(x, text, color):
    w = 30 + len(text) * 11
    return (w + 12, f'<g transform="translate({x},928)">'
            f'<rect x="0" y="0" width="{w}" height="40" rx="20" fill="{color}" fill-opacity="0.14" '
            f'stroke="{color}" stroke-opacity="0.55" stroke-width="1.5"/>'
            f'<circle cx="22" cy="20" r="6" fill="{color}"/>'
            f'<text x="38" y="26" font-family="{EN}" font-size="17" font-weight="700" '
            f'fill="{color}">{text}</text></g>')


# ====================================================================
#  Frame (header + scene + caption)
# ====================================================================
def frame(acro, name_en, title_ar, scene, desc_ar, chips):
    cx = 44
    chip_svg = []
    for txt, col in chips:
        adv, s = chip(cx, txt, col)
        chip_svg.append(s)
        cx += adv
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}"
     font-family="{EN}" width="{W}" height="{H}">
  {defs()}
  <rect x="0" y="0" width="{W}" height="{H}" fill="url(#gBg)"/>
  <rect x="14" y="14" width="{W-28}" height="{H-28}" rx="26" fill="none"
        stroke="#ffffff" stroke-opacity="0.06" stroke-width="1.5"/>

  <!-- header -->
  <text x="44" y="70" font-family="{EN}" font-size="50" font-weight="900" fill="{ACCENT}">{acro}</text>
  <text x="46" y="100" font-family="{EN}" font-size="19" font-weight="600" fill="{MUTED}">{name_en}</text>
  {label(W-44, 78, title_ar, size=44, color=TEXT, anchor="end", weight=800)}
  <line x1="44" y1="120" x2="{W-44}" y2="120" stroke="{ACCENT}" stroke-opacity="0.25" stroke-width="2"/>

  <!-- scene -->
  <g>{scene}</g>

  <!-- caption -->
  <rect x="14" y="862" width="{W-28}" height="{H-862-14}" rx="22"
        fill="rgba(6,8,12,0.85)" stroke="#ffffff" stroke-opacity="0.06" stroke-width="1.5"/>
  <text x="{W-44}" y="906" font-family="{AR}" font-size="25" font-weight="600"
        fill="{TEXT}" direction="rtl" text-anchor="start">{desc_ar}</text>
  {''.join(chip_svg)}
</svg>"""


# ====================================================================
#  Scenes
# ====================================================================
def scene_fcw():
    y = 460
    s = []
    s.append(road_h(300, 640))
    s.append(dash_h(470))
    host_x, lead_x = 380, 1010
    # red collision corridor between host and lead
    s.append(f'<rect x="{host_x+80}" y="{y-46}" width="{lead_x-host_x-160}" height="92" rx="14" '
             f'fill="{CRIT}" opacity="0.16"/>')
    s.append(f'<rect x="{host_x+80}" y="{y-46}" width="{lead_x-host_x-160}" height="92" rx="14" '
             f'fill="none" stroke="{CRIT}" stroke-opacity="0.5" stroke-dasharray="8 8" stroke-width="2"/>')
    s.append(broadcast(lead_x, y))
    s.append(car(host_x, y, 0, "host"))
    s.append(car(lead_x, y, 0, "threat"))
    s.append(burst((host_x+lead_x)//2, y-90, CRIT))
    # TTC distance arrow
    s.append(f'<line x1="{host_x+80}" y1="{y+78}" x2="{lead_x-80}" y2="{y+78}" stroke="{WARN}" '
             f'stroke-width="3" marker-start="url(#arrowW)" marker-end="url(#arrowW)"/>')
    s.append(label((host_x+lead_x)//2, y+108, "زمن الاصطدام TTC", size=20, color=WARN))
    s.append(tag(host_x, y+150, "سيارتك", ACCENT))
    s.append(tag(lead_x, y+150, "السيارة الأمامية", CRIT))
    return frame(
        "FCW", "Forward Collision Warning", "تحذير الاصطدام الأمامي",
        "".join(s),
        "بيحسب زمن الاصطدام (TTC) مع السيارة اللي قدامك في نفس الحارة، ولو قرب من الحد بينذرك صوت وضوء قبل الاصطدام.",
        [("TTC ≤ 3s  Warning", WARN), ("TTC ≤ 2s  Critical", CRIT),
         ("same direction", ACCENT), ("V2V · DSRC", PURPLE)])


def scene_eebl():
    y = 470
    s = []
    s.append(road_h(320, 620))
    s.append(dash_h(470))
    host_x, truck_x, lead_x = 300, 760, 1230
    s.append(broadcast(lead_x, y, rings=(60, 120, 190, 270, 360, 450), label=False))
    # message reaches host even though truck blocks the view (NLOS)
    s.append(car(host_x, y, 0, "host"))
    s.append(truck(truck_x, y, 0))
    s.append(car(lead_x, y, 0, "threat", brake=True))
    s.append(label(truck_x, y+8, "رؤية محجوبة", size=18, color=MUTED))
    s.append(burst(lead_x-118, y-86, CRIT, r=28))
    s.append(label(lead_x, y-150, "فرملة مفاجئة", size=22, color=CRIT))
    s.append(tag(host_x, y+150, "سيارتك", ACCENT))
    s.append(tag(truck_x, y+170, "شاحنة بتحجب الرؤية", MUTED))
    s.append(tag(lead_x, y+150, "بتفرمل بقوة", CRIT))
    return frame(
        "EEBL", "Emergency Electronic Brake Light", "إضاءة الفرامل الإلكترونية الطارئة",
        "".join(s),
        "السيارة اللي قدام لما تفرمل فجأة (تباطؤ أكبر من الحد) بتبثّ تحذير عبر V2V، فتوصلك حتى لو الرؤية محجوبة بشاحنة.",
        [("decel > 4 m/s²", WARN), ("TTC ≤ 3s / 2s", CRIT),
         ("works without line-of-sight", ACCENT), ("V2V · DSRC", PURPLE)])


def scene_bsw():
    s = []
    s.append(road_h(280, 700))
    s.append(dash_h(490))
    host_x, host_y = 900, 600          # host in lower lane (heading right)
    other_x, other_y = 720, 380        # neighbour in upper lane, rear-left blind spot
    # blind-spot zone (rear-left of host)
    s.append(f'<polygon points="{host_x-30},{host_y-40} {host_x-300},{host_y-40} '
             f'{host_x-300},{host_y-220} {host_x-30},{host_y-150}" '
             f'fill="{WARN}" opacity="0.16"/>')
    s.append(f'<polygon points="{host_x-30},{host_y-40} {host_x-300},{host_y-40} '
             f'{host_x-300},{host_y-220} {host_x-30},{host_y-150}" '
             f'fill="none" stroke="{WARN}" stroke-opacity="0.55" stroke-dasharray="8 8" stroke-width="2"/>')
    s.append(label(host_x-165, host_y-130, "النقطة العمياء", size=22, color=WARN))
    s.append(broadcast(other_x, other_y, rings=(54, 96, 142)))
    s.append(car(other_x, other_y, 0, "threat"))
    s.append(car(host_x, host_y, 0, "host"))
    # side-distance marker
    s.append(f'<line x1="{host_x-60}" y1="{host_y-40}" x2="{host_x-60}" y2="{other_y+40}" '
             f'stroke="{WARN}" stroke-width="3" marker-start="url(#arrowW)" marker-end="url(#arrowW)"/>')
    s.append(label(host_x-50, (host_y+other_y)//2, "< 150 سم", size=18, color=WARN, anchor="start"))
    s.append(tag(host_x+30, host_y+140, "سيارتك", ACCENT))
    s.append(tag(other_x, other_y-180, "سيارة في النقطة العمياء", CRIT))
    return frame(
        "BSW", "Blind Spot Warning", "تحذير النقطة العمياء",
        "".join(s),
        "بيكتشف سيارة ماشية بنفس اتجاهك وموجودة في النقطة العمياء جنبك (أقل من ١٥٠ سم) وينبهك قبل ما تغيّر الحارة.",
        [("side < 150 cm", WARN), ("same direction", ACCENT),
         ("left / right side", SAFE), ("V2V · DSRC", PURPLE)])


def scene_dnpw():
    s = []
    s.append(road_h(280, 700))
    s.append(double_yellow_h(490))
    host_x, host_y = 360, 600           # your lane (heading right →)
    slow_x, slow_y = 820, 600
    onc_x, onc_y = 1230, 380            # oncoming lane (heading left ←)
    s.append(label(120, 250, "← الاتجاه المعاكس", size=20, color=MUTED, anchor="start"))
    s.append(label(120, 745, "اتجاهك →", size=20, color=MUTED, anchor="start"))
    s.append(broadcast(onc_x, onc_y, rings=(56, 104, 158, 214)))
    # intended overtake path swinging into the oncoming lane
    s.append(f'<path d="M {host_x+86} {host_y-30} C {slow_x-120} {host_y-60}, '
             f'{slow_x-150} {slow_y-180}, {slow_x+10} {onc_y+50} '
             f'S {slow_x+200} {slow_y-160}, {slow_x+220} {host_y-30}" '
             f'fill="none" stroke="{ACCENT}" stroke-width="4" stroke-dasharray="10 9" '
             f'opacity="0.9" marker-end="url(#arrow)"/>')
    s.append(car(host_x, host_y, 0, "host"))
    s.append(car(slow_x, slow_y, 0, "neutral"))
    s.append(car(onc_x, onc_y, 180, "threat"))
    s.append(burst((slow_x+onc_x)//2, 470, CRIT, r=30))
    s.append(label((slow_x+onc_x)//2, 415, "ممنوع التجاوز", size=24, color=CRIT))
    s.append(tag(host_x, host_y+140, "سيارتك", ACCENT))
    s.append(tag(slow_x, slow_y+140, "سيارة بطيئة قدامك", MUTED))
    s.append(tag(onc_x, onc_y-160, "سيارة قادمة عكسك", CRIT))
    return frame(
        "DNPW", "Do Not Pass Warning", "تحذير عدم التجاوز",
        "".join(s),
        "وانت بتفكر تتجاوز السيارة اللي قدامك، بيكتشف سيارة قادمة في الاتجاه المعاكس ويحسب TTC وينذرك متعدّيش.",
        [("TTC ≤ 6s  Warning", WARN), ("TTC ≤ 4s  Critical", CRIT),
         ("opposite direction", ACCENT), ("V2V · DSRC", PURPLE)])


def scene_ima():
    s = []
    ix0, ix1 = 660, 940      # vertical road x-band
    iy0, iy1 = 350, 630      # horizontal road y-band
    cxv = (ix0+ix1)//2
    cyh = (iy0+iy1)//2
    s.append(road_h(iy0, iy1))
    s.append(road_v(ix0, ix1))
    s.append(dash_h(cyh, x0=-60, x1=ix0))
    s.append(dash_h(cyh, x0=ix1, x1=W+60))
    s.append(dash_v(cxv, y0=-60, y1=iy0))
    s.append(dash_v(cxv, y0=iy1, y1=H+60))
    # 20 m proximity gate
    s.append(f'<circle cx="{cxv}" cy="{cyh}" r="250" fill="none" stroke="{WARN}" '
             f'stroke-opacity="0.4" stroke-dasharray="4 12" stroke-width="3"/>')
    s.append(label(cxv, iy0-22, "نطاق ٢٠ متر من التقاطع", size=18, color=WARN))
    host_x, host_y = cxv, 820          # approaching from bottom, heading up
    cross_x, cross_y = 300, cyh        # crossing from left, heading right
    s.append(broadcast(cross_x, cross_y, rings=(54, 100, 150, 206, 270)))
    s.append(car(cross_x, cross_y, 0, "threat"))
    s.append(car(host_x, host_y, -90, "host"))
    s.append(burst(cxv, cyh, CRIT, r=38))
    s.append(tag(host_x, host_y+135, "سيارتك", ACCENT))
    s.append(tag(cross_x, cross_y-150, "سيارة من اتجاه عرضي", CRIT))
    return frame(
        "IMA", "Intersection Movement Assist", "مساعد الحركة عند التقاطعات",
        "".join(s),
        "عند الاقتراب من تقاطع (خلال ٢٠ متر) بيكتشف سيارة قادمة من اتجاه عرضي ويحسب زمن الوصول وينذرك بخطر التصادم.",
        [("within 20 m", WARN), ("delay ≤ 4s / 2s", CRIT),
         ("crossing direction", ACCENT), ("V2V · DSRC", PURPLE)])


SCENES = {
    "fcw":  scene_fcw,
    "eebl": scene_eebl,
    "bsw":  scene_bsw,
    "dnpw": scene_dnpw,
    "ima":  scene_ima,
}

if __name__ == "__main__":
    for name, fn in SCENES.items():
        path = os.path.join(OUT, f"{name}.svg")
        with open(path, "w", encoding="utf-8") as f:
            f.write(fn())
        print("wrote", path)
