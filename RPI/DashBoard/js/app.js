// ============================================================
//  V2X Dashboard - front-end
//  Reads ALL vehicle values from data.json (single source of
//  truth). data.json is updated by server.py (simulation now,
//  STM-over-UART later). The UI subscribes to /events (Server-Sent
//  Events): the server pushes new data the instant the file changes,
//  so updates appear with near-zero latency (no polling delay).
//  Weather is fetched live from Open-Meteo (falls back to data.json).
//
//  ADAS flag meaning:
//    flag 0 -> safe   |  flag 1 -> WARNING  |  flag 2 -> CRITICAL
// ============================================================

const WARNINGS = [
  ["eebl", "EEBL", "Emergency Electronic Brake Light"],
  ["fcw",  "FCW",  "Forward Collision Warning"],
  ["bsw",  "BSW",  "Blind Spot Warning"],
  ["dnpw", "DNPW", "Do Not Pass Warning"],
  ["ima",  "IMA",  "Intersection Movement Assist"],
];

// Per-module wording, matched to what each V2V module actually detects on this
// car (see V2V-STM32 SafetyEngine):
//   • FCW  = a vehicle ahead in the SAME direction (forward collision).
//   • DNPW = an ONCOMING vehicle (opposite direction) — overtaking risk.
//   • EEBL = a vehicle ahead braking hard (V2X brake-light broadcast).
//   • BSW  = a vehicle in this car's blind spot (side resolved at runtime).
//   • IMA  = crossing traffic at an intersection.
// `cause` is the short "why" line shown under the critical alert; `desc` is the
// one-liner on the warning toast. BSW is handled dynamically (depends on side).
const ALERT_INFO = {
  fcw:  { cause: "Vehicle ahead in your lane",
          desc:  "Vehicle ahead in your lane — keep your distance",
          crit:  "Vehicle ahead in your lane!\nBrake now — collision risk!" },
  dnpw: { cause: "Oncoming vehicle ahead",
          desc:  "Oncoming vehicle — do not overtake",
          crit:  "Oncoming vehicle ahead!\nDo NOT overtake — stay in your lane!" },
  eebl: { cause: "Vehicle ahead braking hard",
          desc:  "Vehicle ahead braking — be ready to slow",
          crit:  "Vehicle ahead is braking hard!\nReduce speed immediately!" },
  ima:  { cause: "Crossing traffic at intersection",
          desc:  "Crossing traffic ahead — approach with care",
          crit:  "Crossing vehicle at the intersection!\nYield and stop!" },
  // bsw filled in per-side by bswInfo()
};

// Map a blind-spot side ("left" | "right" | "both" | null) to BSW wording.
function bswInfo(side) {
  const where = side === "both" ? "BOTH blind spots"
              : side === "right" ? "your RIGHT blind spot"
              : side === "left"  ? "your LEFT blind spot"
              : "your blind spot";
  const noTurn = side === "both" ? "do not change lane either way"
               : side === "right" ? "do not move right"
               : side === "left"  ? "do not move left"
               : "do not change lane";
  return {
    cause: `Vehicle in ${where}`,
    desc:  `Vehicle in ${where} — ${noTurn}`,
    crit:  `Vehicle in ${where}!\n${noTurn.charAt(0).toUpperCase()}${noTurn.slice(1)}!`,
  };
}

// Resolve the wording for a module given the current adas snapshot (only BSW
// needs the snapshot, for its side).
function alertInfo(key, adas) {
  return key === "bsw" ? bswInfo(adas && adas.bswSide) : ALERT_INFO[key];
}

// ==================== Smart systems (V2N / V2P / AI) ====================
// Flags written into data.json by the gateway / AI-camera bridge:
//   v2n.trafficLight        0 no signal | 1 GO (green / emergency) | 2 STOP (red)
//   v2p.pedestrian          0 clear | 1 nearby (warning toast) | 2 crossing (critical)
//   v2p.position            0 none  | 1 hazard on the RIGHT | 2 hazard on the LEFT
//   v2p.motorcycleCollision 0 clear | 1 motorcycle collision risk (warning toast)
//   ai.leadCarCollision     0 clear | 1 stopped ahead (warning toast)
//                           | 2 stopped very close (critical)
// Warning toasts for these systems stack on the LEFT edge of the screen; the
// V2V/ADAS toasts keep the RIGHT edge. These systems do NOT use the full-screen
// popup (that stays exclusive to the V2V modules) — a critical here shows the
// red card + hazard marker + risk gauge and LOOPS THE ALARM SOUND instead.
const SMART_ORDER = ["tl", "ped", "mcy", "lca"];
const SMART_NAMES = {
  tl:  ["T-LIGHT",    "Traffic Light"],
  ped: ["PEDESTRIAN", "Pedestrian Detection"],
  mcy: ["MOTORCYCLE", "Motorcycle Collision"],
  lca: ["LEAD CAR",   "AI Lead Car Watch"],
};
const SMART_INFO = {
  lca: { desc: "Lead car stopped ahead — keep your distance" },
  mcy: { desc: "Motorcycle collision risk — keep clear" },
};

// Normalize the smart-system flags out of a data.json snapshot (all optional —
// missing sections / nulls read as 0 so an old bridge can't break the render).
function smartState(d) {
  const v2n = d.v2n || {}, v2p = d.v2p || {}, ai = d.ai || {};
  return {
    tl:      v2n.trafficLight || 0,
    ped:     v2p.pedestrian || 0,
    pedSide: v2p.position || 0,                        // 0 none | 1 right | 2 left
    mcy:     v2p.motorcycleCollision || 0,
    lca:     (ai.leadCarCollision != null ? ai.leadCarCollision
              : v2p.leadCarCollision) || 0,            // accept either home
  };
}

// Side wording for v2p.position.
function pedSideName(pos) {
  return pos === 1 ? "right" : pos === 2 ? "left" : null;
}

// Ultrasonic sensors: [key in data.ultrasonic, short label]
const SENSORS = [
  ["front",      "Front"],
  ["frontLeft",  "Front-L"],
  ["frontRight", "Front-R"],
  ["rear",       "Rear"],
  ["rearLeft",   "Rear-L"],
  ["rearRight",  "Rear-R"],
];
// Per-sensor distance bands (cm), mirrored from the STM32 firmware so the beam
// colours light up at the same distance the ADAS modules actually trigger:
//   • front      -> FCW   (FCW_FRONT_THRESHOLD 40 / DNPW_FRONT_THRESHOLD 20)
//   • rear       -> EEBL  (SafetyEngine MIN_SAFE_DISTANCE 15 / *CRITICAL_RATIO 10)
//   • 4 corners  -> BSW   (BSW_SIDE_THRESHOLD 30 / BSW_SIDE_CRITICAL 20)
// near = WARNING gate (yellow), close = CRITICAL gate (red), range = max shown.
const US_BANDS = {
  front:      { near: 40, close: 20, range: 40 },
  rear:       { near: 15, close: 10, range: 20 },
  frontLeft:  { near: 30, close: 20, range: 30 },
  frontRight: { near: 30, close: 20, range: 30 },
  rearLeft:   { near: 30, close: 20, range: 30 },
  rearRight:  { near: 30, close: 20, range: 30 },
};

// Default weather location (used if data.json has no weather.lat/lon)
const WX_LAT = 30.0444, WX_LON = 31.2357, WX_CITY = "Cairo";

const COMPASS_DIRS = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"];
const $ = (id) => document.getElementById(id);
const ARC_LEN  = 396;   // speedometer arc length
const RISK_ARC = 232;   // risk gauge arc length

// ==================== Inline SVG icons ====================
function svg(p) {
  return `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round">${p}</svg>`;
}
const ADAS_ICONS = {
  fcw:  svg(`<rect x="5" y="12" width="14" height="7" rx="2"/><path d="M7 12l1.6-4h6.8L17 12"/><path d="M12 2v3M9 3.5l1.4 2.2M15 3.5l-1.4 2.2"/>`),
  eebl: svg(`<circle cx="12" cy="12" r="9"/><path d="M12 7v6"/><circle cx="12" cy="16.4" r="0.7" fill="currentColor" stroke="none"/>`),
  bsw:  svg(`<rect x="8" y="4" width="8" height="16" rx="2.5"/><path class="bsw-wave bsw-left" d="M5 9c-1.1 1-1.1 5 0 6"/><path class="bsw-wave bsw-right" d="M19 9c1.1 1 1.1 5 0 6"/>`),
  dnpw: svg(`<rect x="3.5" y="6" width="6" height="12" rx="1.5"/><rect x="14.5" y="6" width="6" height="12" rx="1.5"/><path d="M12 3v18" stroke-dasharray="2.5 2.5"/>`),
  ima:  svg(`<path d="M12 3v18M3 12h18"/><path d="M9.5 8.5L12 6l2.5 2.5M9.5 15.5L12 18l2.5-2.5"/>`),
  // smart systems (V2N / V2P / AI) — same registry so toasts & popups find them
  tl:   svg(`<rect x="8.5" y="2.5" width="7" height="19" rx="3.5"/><circle cx="12" cy="7" r="1.5"/><circle cx="12" cy="12" r="1.5"/><circle cx="12" cy="17" r="1.5"/>`),
  ped:  svg(`<circle cx="12" cy="4.5" r="2"/><path d="M12 6.5v6M12 12.5l-3 6.5M12 12.5l3 6.5M12 8.5l-4 2.5M12 8.5l4 2.5"/>`),
  mcy:  svg(`<circle cx="5" cy="17" r="2.8"/><circle cx="19" cy="17" r="2.8"/><path d="M5 17l2.8-5.5h4.4l2.8 5.5M12.2 11.5l3.6-2.8h2M7.8 11.5H5.2"/>`),
  lca:  svg(`<rect x="5" y="5" width="14" height="7" rx="2"/><path d="M7 5l1.6-3h6.8L17 5"/><path d="M12 22v-3M9 20.5l1.4-2.2M15 20.5l-1.4-2.2"/>`),
};

// engine-temp state icons
const TEMP_SVG = {
  cold:   { color: "#4aa6ff", svg: svg(`<path d="M12 2v20"/><path d="M4.5 7l15 10M19.5 7l-15 10"/><path d="M9.5 3.5L12 6l2.5-2.5M9.5 20.5L12 18l2.5 2.5"/><path d="M3.2 11.3l3.2-.9-.9-3.2M20.8 11.3l-3.2-.9.9-3.2M3.2 12.7l3.2.9-.9 3.2M20.8 12.7l-3.2.9.9 3.2"/>`) },
  normal: { color: "#2ecc71", svg: svg(`<path d="M10 14.8V5a2 2 0 1 1 4 0v9.8a4 4 0 1 1-4 0Z"/><circle cx="12" cy="17" r="1.6" fill="currentColor" stroke="none"/>`) },
  hot:    { color: "#ff4d4f", svg: svg(`<path d="M12 2c1 3.5 4.5 5 4.5 9a4.5 4.5 0 1 1-9 0c0-1.7.7-2.9 1.5-3.9.3 1.7 1.5 2.2 1.5 2.2.7-2.5-1-4.7.5-7.3Z"/>`) },
};

// weather icons (by simple key)
const WX_SVG = {
  clear:   { color: "#f5a623", svg: svg(`<circle cx="12" cy="12" r="4.5"/><path d="M12 2v2.5M12 19.5V22M2 12h2.5M19.5 12H22M4.6 4.6l1.8 1.8M17.6 17.6l1.8 1.8M19.4 4.6l-1.8 1.8M6.4 17.6l-1.8 1.8"/>`) },
  cloud:   { color: "#9aa3b2", svg: svg(`<path d="M7 18a4 4 0 0 1 0-8 5 5 0 0 1 9.6-1.3A3.5 3.5 0 0 1 17.5 18Z"/>`) },
  rain:    { color: "#4aa6ff", svg: svg(`<path d="M7 15a4 4 0 0 1 0-8 5 5 0 0 1 9.6-1.3A3.5 3.5 0 0 1 17.5 15Z"/><path d="M8 18l-1 2.5M12 18l-1 2.5M16 18l-1 2.5"/>`) },
  snow:    { color: "#9fd2ff", svg: svg(`<path d="M7 15a4 4 0 0 1 0-8 5 5 0 0 1 9.6-1.3A3.5 3.5 0 0 1 17.5 15Z"/><path d="M9 19v.01M12 20.5v.01M15 19v.01"/>`) },
  thunder: { color: "#7c5cff", svg: svg(`<path d="M7 14a4 4 0 0 1 0-8 5 5 0 0 1 9.6-1.3A3.5 3.5 0 0 1 17.5 14"/><path d="M12 12l-2 4h3l-2 4"/>`) },
};
function wmoToKey(code) {
  if (code === 0) return { key: "clear",   label: "Clear" };
  if (code <= 3)  return { key: "cloud",   label: "Cloudy" };
  if (code <= 48) return { key: "cloud",   label: "Fog" };
  if (code <= 67) return { key: "rain",    label: "Rain" };
  if (code <= 77) return { key: "snow",    label: "Snow" };
  if (code <= 82) return { key: "rain",    label: "Showers" };
  if (code <= 86) return { key: "snow",    label: "Snow" };
  return { key: "thunder", label: "Storm" };
}
function condToKey(cond) {
  const c = (cond || "").toLowerCase();
  if (c.includes("rain") || c.includes("drizzle")) return "rain";
  if (c.includes("thunder") || c.includes("storm")) return "thunder";
  if (c.includes("snow")) return "snow";
  if (c.includes("cloud") || c.includes("fog") || c.includes("mist") || c.includes("overcast")) return "cloud";
  return "clear";
}

// ==================== Audio (Web Audio API) ====================
let audioCtx = null;
function getAudioCtx() {
  if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
  return audioCtx;
}
function playWarningBeep() {
  try {
    const ctx = getAudioCtx();
    const osc = ctx.createOscillator(), gain = ctx.createGain();
    osc.connect(gain); gain.connect(ctx.destination);
    osc.type = "triangle";
    osc.frequency.setValueAtTime(880, ctx.currentTime);
    osc.frequency.setValueAtTime(660, ctx.currentTime + 0.1);
    osc.frequency.setValueAtTime(880, ctx.currentTime + 0.2);
    gain.gain.setValueAtTime(0.3, ctx.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.01, ctx.currentTime + 0.35);
    osc.start(ctx.currentTime); osc.stop(ctx.currentTime + 0.35);
  } catch (_) {}
}
function playErrorAlarm() {
  try {
    const ctx = getAudioCtx();
    for (let i = 0; i < 4; i++) {
      const osc = ctx.createOscillator(), gain = ctx.createGain();
      osc.connect(gain); gain.connect(ctx.destination);
      osc.type = "square";
      const t = ctx.currentTime + i * 0.25;
      osc.frequency.setValueAtTime(i % 2 === 0 ? 800 : 600, t);
      gain.gain.setValueAtTime(0.2, t);
      gain.gain.exponentialRampToValueAtTime(0.01, t + 0.22);
      osc.start(t); osc.stop(t + 0.22);
    }
  } catch (_) {}
}

// ==================== Ultrasonic proximity beep ====================
let usSoundInterval = null;
let usBeepState = "safe";

function playUSBeep(type) {
  try {
    const ctx = getAudioCtx();
    const osc = ctx.createOscillator(), gain = ctx.createGain();
    osc.connect(gain); gain.connect(ctx.destination);
    osc.type = "sine";
    const dur = type === "close" ? 0.11 : 0.09;
    const freq = type === "close" ? 1200 : 880;
    const vol  = type === "close" ? 0.28 : 0.16;
    osc.frequency.setValueAtTime(freq, ctx.currentTime);
    gain.gain.setValueAtTime(vol, ctx.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.01, ctx.currentTime + dur);
    osc.start(ctx.currentTime); osc.stop(ctx.currentTime + dur);
  } catch (_) {}
}

function setUSBeepState(state) {
  if (state === usBeepState) return;
  usBeepState = state;
  if (usSoundInterval) { clearInterval(usSoundInterval); usSoundInterval = null; }
  if (state === "close") {
    playUSBeep("close");
    usSoundInterval = setInterval(() => playUSBeep("close"), 220);
  } else if (state === "near") {
    playUSBeep("near");
    usSoundInterval = setInterval(() => playUSBeep("near"), 700);
  }
}

// ==================== Toast Notifications ====================
// A toast lives only while its warning keeps being re-asserted. It clears
// itself automatically — no manual close needed — in two ways:
//   1. The instant a fresh update reports the warning gone, processAlerts
//      calls removeToast() and it slides out in 300ms.
//   2. A watchdog timer: every time the warning is re-asserted we re-arm a
//      timeout. If the data simply stops being pushed (the producer goes
//      silent while the flag is stuck at 1, so no new render ever arrives),
//      the timer still fires on its own and the toast clears instead of
//      lingering forever.
const activeToasts = new Map();   // key -> { el, timer }
const TOAST_AUTO_MS = 4000;       // self-clear if the cause isn't re-asserted within this

// side: "right" (default) = V2V/ADAS stack · "left" = V2N/V2P/AI stack, so the
// two families of warnings never pile up in the same corner.
function showToast(key, name, desc, side) {
  const existing = activeToasts.get(key);
  if (existing) {                 // already up: keep the watchdog alive + refresh text
    clearTimeout(existing.timer);
    existing.timer = setTimeout(() => removeToast(key), TOAST_AUTO_MS);
    const d = existing.el.querySelector(".toast-desc");
    if (d && desc) d.textContent = desc;   // BSW / V2P side may have changed
    return;
  }
  const el = document.createElement("div");
  el.className = "toast";
  el.innerHTML = `
    <span class="toast-icon">${ADAS_ICONS[key] || "⚠️"}</span>
    <div class="toast-body">
      <div class="toast-title">${name}</div>
      <div class="toast-desc"></div>
    </div>
    <button class="toast-close" onclick="removeToast('${key}')">✕</button>`;
  el.querySelector(".toast-desc").textContent = desc || "Warning detected — monitor situation";
  $(side === "left" ? "toastContainerLeft" : "toastContainer").appendChild(el);
  activeToasts.set(key, { el, timer: setTimeout(() => removeToast(key), TOAST_AUTO_MS) });
  playWarningBeep();
}
function removeToast(key) {
  const entry = activeToasts.get(key);
  if (!entry) return;
  clearTimeout(entry.timer);
  activeToasts.delete(key);
  entry.el.classList.add("toast-out");
  setTimeout(() => entry.el.remove(), 300);
}

// ==================== Critical Popup ====================
// Same self-clearing watchdog as the toasts: the overlay stays only while the
// critical alert keeps being re-asserted. If a fresh update reports it gone,
// processAlerts calls hideCritical() immediately; if the data simply stops
// arriving, the watchdog timer below hides it instead of leaving it stuck.
let activeCritical = null, criticalSoundInterval = null, criticalTimer = null;
const CRITICAL_AUTO_MS = 4000;    // self-clear if the danger isn't re-asserted within this
function showCritical(key, name, info, adas) {
  if (criticalTimer) clearTimeout(criticalTimer);
  criticalTimer = setTimeout(hideCritical, CRITICAL_AUTO_MS);   // re-arm watchdog
  // Always refresh the text (the BSW side can change while the alert is held);
  // only the one-time setup (icon swap, alarm) is gated on a new key.
  const abbr = (WARNINGS.find(([k]) => k === key) || [])[1] || key.toUpperCase();
  $("criticalTitle").textContent = `${abbr} — ${name}`;
  $("criticalMsg").textContent = (info && info.crit) || `${name} — Critical danger!`;
  $("criticalSystem").textContent = (info && info.cause) || name;
  const iconEl = $("criticalIcon");
  if (iconEl) {
    iconEl.innerHTML = ADAS_ICONS[key] || "⚠";
    // BSW: highlight the wave on the threat side, same as the ADAS grid.
    if (key === "bsw") iconEl.setAttribute("data-side", (adas && adas.bswSide) || "both");
    else iconEl.removeAttribute("data-side");
  }
  if (activeCritical === key) return;
  activeCritical = key;
  $("criticalOverlay").style.display = "flex";
  playErrorAlarm();
  if (criticalSoundInterval) clearInterval(criticalSoundInterval);
  criticalSoundInterval = setInterval(playErrorAlarm, 2000);
}
function hideCritical() {
  if (criticalTimer) { clearTimeout(criticalTimer); criticalTimer = null; }
  if (!activeCritical) return;
  activeCritical = null;
  $("criticalOverlay").style.display = "none";
  if (criticalSoundInterval) { clearInterval(criticalSoundInterval); criticalSoundInterval = null; }
}

// ==================== Event Log ====================
const MAX_EVENTS = 40;
const nowTime = () => new Date().toLocaleTimeString("en-GB", { hour12: false });
function addEvent(msg, level) {
  const list = $("eventList");
  const li = document.createElement("li");
  li.className = `event-item ${level}`;
  li.innerHTML = `<i class="ev-dot"></i><span class="ev-time">${nowTime()}</span><span class="ev-msg">${msg}</span>`;
  list.insertBefore(li, list.firstChild);
  while (list.children.length > MAX_EVENTS) list.removeChild(list.lastChild);
}

// ==================== ADAS status grid ====================
const ADAS_ORDER = ["fcw", "eebl", "bsw", "dnpw", "ima"];
function buildAdasGrid() {
  const grid = $("adasGrid");
  grid.innerHTML = "";
  for (const key of ADAS_ORDER) {
    const abbr = WARNINGS.find(([k]) => k === key)[1];
    const card = document.createElement("div");
    card.className = "adas-card safe";
    card.id = `adas-${key}`;
    card.innerHTML = `<div class="adas-name">${abbr}</div><div class="adas-ic">${ADAS_ICONS[key]}</div><div class="adas-status">SAFE</div>`;
    grid.appendChild(card);
  }
}
function updateAdasGrid(adas) {
  for (const key of ADAS_ORDER) {
    const flag = adas[key] || 0;
    const card = $(`adas-${key}`);
    if (!card) continue;
    const status = card.querySelector(".adas-status");
    // flag: 0 SAFE | 1 WARNING | 2 CRITICAL | 3 INVALID (bad/no reading).
    // INVALID must NOT look like SAFE — surface it in the warn style instead.
    if (flag === 2)      { card.className = "adas-card crit"; status.textContent = "ACTIVE"; }
    else if (flag === 1) { card.className = "adas-card warn"; status.textContent = "WARNING"; }
    else if (flag === 3) { card.className = "adas-card warn"; status.textContent = "INVALID"; }
    else                 { card.className = "adas-card safe"; status.textContent = "SAFE"; }
    // BSW: highlight the wave on the side the threat is on, so left vs right is
    // readable at a glance. side-* drives the wave colour via CSS.
    if (key === "bsw") {
      const active = flag === 1 || flag === 2;
      const side = active ? (adas.bswSide || "both") : "none";
      card.setAttribute("data-side", side);
    }
  }
}

// ==================== Smart systems grid (V2N / V2P / AI) ====================
function buildSmartGrid() {
  const grid = $("smartGrid");
  if (!grid) return;
  grid.innerHTML = "";
  for (const key of SMART_ORDER) {
    const card = document.createElement("div");
    card.className = "adas-card off";
    card.id = `smart-${key}`;
    card.innerHTML = `<div class="adas-name">${SMART_NAMES[key][0]}</div><div class="adas-ic">${ADAS_ICONS[key]}</div><div class="adas-status">--</div>`;
    grid.appendChild(card);
  }
}

// Per-card visual state. Only touches the DOM when a card actually changes so
// the 10Hz SSE stream never causes needless style recalcs (keeps the UI lag-free).
const _smartPrev = {};
function setSmartCard(key, cls, statusText) {
  const stamp = `${cls}|${statusText}`;
  if (_smartPrev[key] === stamp) return;
  _smartPrev[key] = stamp;
  const card = $(`smart-${key}`);
  if (!card) return;
  card.className = `adas-card ${cls}`;
  card.querySelector(".adas-status").textContent = statusText;
}

function updateSmartGrid(sys) {
  // V2N traffic light: not a hazard scale — GO is green, STOP is steady red.
  if (sys.tl === 1)      setSmartCard("tl", "go",   "GO");
  else if (sys.tl === 2) setSmartCard("tl", "stop", "STOP");
  else                   setSmartCard("tl", "off",  "NO SIGNAL");

  // V2P pedestrians (+ which side the hazard is on, from v2p.position):
  // nearby = warning (orange, like the toast), crossing = critical (red)
  const side = pedSideName(sys.pedSide);
  const sideTag = side ? ` · ${side === "right" ? "R" : "L"}` : "";
  if (sys.ped === 2)      setSmartCard("ped", "crit", `CROSSING${sideTag}`);
  else if (sys.ped === 1) setSmartCard("ped", "warn", `NEARBY${sideTag}`);
  else                    setSmartCard("ped", "safe", "CLEAR");

  // V2P motorcycle collision risk (binary)
  setSmartCard("mcy", sys.mcy ? "warn" : "safe", sys.mcy ? "RISK" : "SAFE");

  // AI lead-car watch (front camera): 1 warning, 2 imminent collision
  if (sys.lca === 2)      setSmartCard("lca", "crit", "DANGER");
  else if (sys.lca === 1) setSmartCard("lca", "warn", "WARNING");
  else                    setSmartCard("lca", "safe", "SAFE");
}

// ==================== AI-detected objects around the car ====================
// Visual markers on the center car view: a pedestrian standing on the road
// side / crossing in front, a motorcycle ahead, and the lead car the AI camera
// is watching. Orange = warning, red = danger; the side follows v2p.position.
function buildHazards() {
  $("hzPed").innerHTML = ADAS_ICONS.ped;
  $("hzMcy").innerHTML = ADAS_ICONS.mcy;
  $("hzLca").innerHTML = ADAS_ICONS.lca;
}

// Class-guarded setter, same idea as setSmartCard: the 10Hz stream only touches
// the DOM when a marker's state actually changes.
const _hazardPrev = {};
function setHazard(id, cls) {
  if (_hazardPrev[id] === cls) return;
  _hazardPrev[id] = cls;
  const el = $(id);
  if (el) el.className = cls;
}

function updateHazards(sys) {
  const side = pedSideName(sys.pedSide);

  // pedestrian, always standing still: nearby → beside the white edge line on
  // the flagged side; crossing → mid-road shifted toward the flagged side
  if (sys.ped === 2) {
    const spot = side === "right" ? "mid-right" : side === "left" ? "mid-left" : "mid";
    setHazard("hzPed", `hazard hz-ped on crit ${spot}`);
  } else if (sys.ped === 1) {
    setHazard("hzPed", `hazard hz-ped on warn ${side === "left" ? "pos-left" : "pos-right"}`);
  } else {
    setHazard("hzPed", "hazard hz-ped");
  }

  // motorcycle ahead (binary risk flag)
  setHazard("hzMcy", sys.mcy ? "hazard hz-mcy on warn" : "hazard hz-mcy");

  // lead car: warning far ahead, danger slides in close to the car
  if (sys.lca === 2)      setHazard("hzLca", "hazard hz-lca on crit close");
  else if (sys.lca === 1) setHazard("hzLca", "hazard hz-lca on warn");
  else                    setHazard("hzLca", "hazard hz-lca");
}

// ==================== Alert + event logic ====================
const prevStates = {}, prevSensor = {};

// Smart-system critical SOUND: these systems intentionally have no full-screen
// popup (the red card + hazard marker + risk gauge carry the visuals), but a
// critical must still be heard — loop the alarm while lca/ped report level 2.
// Same self-clearing watchdog pattern as the popup: if the data feed goes
// silent while the flag is stuck, the sound stops on its own.
let smartCritOn = false, smartCritInterval = null, smartCritTimer = null;
const SMART_CRIT_AUTO_MS = 4000;
function setSmartCritSound(active) {
  if (active) {
    clearTimeout(smartCritTimer);
    smartCritTimer = setTimeout(() => setSmartCritSound(false), SMART_CRIT_AUTO_MS);
    if (!smartCritOn) {
      smartCritOn = true;
      playErrorAlarm();
      smartCritInterval = setInterval(playErrorAlarm, 2000);
    }
  } else {
    if (smartCritTimer) { clearTimeout(smartCritTimer); smartCritTimer = null; }
    if (smartCritInterval) { clearInterval(smartCritInterval); smartCritInterval = null; }
    smartCritOn = false;
  }
}

// ==================== Traffic Light warning sound ====================
// Plays a two-tone descending beep that loops every 1.2 s while tl === 2 (STOP/red).
// Stops automatically when the light clears or the feed goes silent.
let tlWarnOn = false, tlWarnInterval = null, tlWarnTimer = null;
const TL_WARN_AUTO_MS = 4000;
function playTLWarnBeep() {
  try {
    const ctx = getAudioCtx();
    // First tone: high pitch
    const osc1 = ctx.createOscillator(), g1 = ctx.createGain();
    osc1.connect(g1); g1.connect(ctx.destination);
    osc1.type = "triangle";
    osc1.frequency.setValueAtTime(1050, ctx.currentTime);
    g1.gain.setValueAtTime(0.28, ctx.currentTime);
    g1.gain.exponentialRampToValueAtTime(0.01, ctx.currentTime + 0.18);
    osc1.start(ctx.currentTime); osc1.stop(ctx.currentTime + 0.18);
    // Second tone: low pitch (descending — "stop" feel)
    const osc2 = ctx.createOscillator(), g2 = ctx.createGain();
    osc2.connect(g2); g2.connect(ctx.destination);
    osc2.type = "triangle";
    osc2.frequency.setValueAtTime(700, ctx.currentTime + 0.22);
    g2.gain.setValueAtTime(0.22, ctx.currentTime + 0.22);
    g2.gain.exponentialRampToValueAtTime(0.01, ctx.currentTime + 0.42);
    osc2.start(ctx.currentTime + 0.22); osc2.stop(ctx.currentTime + 0.42);
  } catch (_) {}
}
function setTLWarnSound(active) {
  if (active) {
    clearTimeout(tlWarnTimer);
    tlWarnTimer = setTimeout(() => setTLWarnSound(false), TL_WARN_AUTO_MS);
    if (!tlWarnOn) {
      tlWarnOn = true;
      playTLWarnBeep();
      tlWarnInterval = setInterval(playTLWarnBeep, 1200);
    }
  } else {
    if (tlWarnTimer)    { clearTimeout(tlWarnTimer);      tlWarnTimer    = null; }
    if (tlWarnInterval) { clearInterval(tlWarnInterval);  tlWarnInterval = null; }
    tlWarnOn = false;
  }
}

// Toasts + event-log entries + sounds for the smart systems. Same ladder as
// the V2V modules — warning (1) → toast (with its beep), critical (2) → alarm
// loop — except the toasts stack on the LEFT edge so they never mix with the
// V2V ones on the right, and criticals never open the popup.
function processSmartAlerts(sys) {
  const side = pedSideName(sys.pedSide);

  // AI lead car: 1 = warning toast, 2 = critical (alarm below)
  if (sys.lca === 1) showToast("lca", "AI — Lead Car Watch", SMART_INFO.lca.desc, "left");
  else removeToast("lca");

  // Pedestrians: 1 nearby = warning toast, 2 crossing = critical (alarm below)
  if (sys.ped === 1) {
    showToast("ped", "V2P — Pedestrians",
              `Pedestrians near the road${side ? ` (${side})` : ""} — stay alert`, "left");
  } else removeToast("ped");

  if (sys.mcy === 1) showToast("mcy", "V2P — Motorcycle", SMART_INFO.mcy.desc, "left");
  else removeToast("mcy");

  // Traffic light STOP sound — looping warning beep while red
  setTLWarnSound(sys.tl === 2);

  // critical alarm sound (no popup for these systems)
  setSmartCritSound(sys.lca === 2 || sys.ped === 2);

  // Event log — one entry per state change, mirroring the ADAS wording
  const changed = (key, flag, msgs) => {
    if ((prevStates[key] || 0) !== flag && msgs[flag]) addEvent(...msgs[flag]);
    prevStates[key] = flag;
  };
  changed("tl", sys.tl, {
    0: ["Traffic Light — No Signal", "safe"],
    1: ["Traffic Light — GO (road open)", "safe"],
    2: ["Traffic Light — STOP (red)", "crit"],
  });
  changed("ped", sys.ped, {
    0: ["Pedestrians Cleared", "safe"],
    1: [`Pedestrians Nearby${side ? ` (${side})` : ""}`, "warn"],
    2: [`Pedestrians Crossing${side ? ` (${side})` : ""}`, "crit"],
  });
  changed("mcy", sys.mcy, {
    0: ["Motorcycle Risk Cleared", "safe"],
    1: ["Motorcycle Collision Risk", "warn"],
  });
  changed("lca", sys.lca, {
    0: ["Lead Car Cleared", "safe"],
    1: ["Lead Car Stopped Ahead", "warn"],
    2: ["Lead Car DANGER — Very Close", "crit"],
  });
}

function processAlerts(adas, sys) {
  let highestCritKey = null, highestCritName = null, highestCritInfo = null;
  for (const [key, abbr, name] of WARNINGS) {
    const flag = adas[key] || 0;
    const prev = prevStates[key] || 0;
    const info = alertInfo(key, adas);   // per-module wording (BSW resolves side)
    if (flag === 1) showToast(key, `${abbr} — ${name}`, info && info.desc);
    else removeToast(key);   // any non-warning state clears it right away (no-op if not shown)
    if (flag !== prev) {
      if (flag === 2)      addEvent(`${abbr} Triggered`, "crit");
      else if (flag === 1) addEvent(`${abbr} Warning`, "warn");
      else if (flag === 3) addEvent(`${abbr} Invalid Reading`, "warn");
      else                 addEvent(`${abbr} Cleared`, "safe");
    }
    if (flag === 2 && !highestCritKey) {
      highestCritKey = key; highestCritName = name; highestCritInfo = info;
    }
    prevStates[key] = flag;
  }
  // Smart systems: toasts + events + their own sounds. They never open the
  // popup — the full-screen overlay stays exclusive to the V2V modules.
  processSmartAlerts(sys);
  if (highestCritKey) { removeToast(highestCritKey); showCritical(highestCritKey, highestCritName, highestCritInfo, adas); }
  else hideCritical();
}

// drive the ultrasonic spotlight beams; returns worst state seen
function processUltrasonic(ultra) {
  let worst = "clear";
  for (const [key, label] of SENSORS) {
    const band = US_BANDS[key];
    const cm = ultra && ultra[key] != null ? ultra[key] : band.range + 1;
    let state = "off";
    if (cm < band.close)       state = "close";
    else if (cm < band.near)   state = "near";
    else if (cm <= band.range) state = "clear";

    const g = $(`sn-${key}`);
    if (g) {
      g.classList.remove("clear", "near", "close");
      if (state !== "off") g.classList.add(state);
    }

    if (state === "close" && prevSensor[key] !== "close") addEvent(`${label} Obstacle`, "crit");
    prevSensor[key] = state;

    if (state === "close") worst = "close";
    else if (state === "near" && worst !== "close") worst = "near";
  }
  setUSBeepState(worst === "close" ? "close" : worst === "near" ? "near" : "safe");
  return worst;
}

// ==================== Overall risk ====================
function updateRisk(adas, ultraWorst, sys) {
  const anyCrit = WARNINGS.some(([k]) => (adas[k] || 0) === 2) || ultraWorst === "close"
    || sys.lca === 2 || sys.ped === 2;
  const anyWarn = WARNINGS.some(([k]) => (adas[k] || 0) === 1) || ultraWorst === "near"
    || sys.lca === 1 || sys.ped === 1 || sys.mcy === 1;
  let label, sub, color, frac;
  if (anyCrit)      { label = "DANGER";  sub = "High Risk";   color = "var(--crit)"; frac = 0.92; }
  else if (anyWarn) { label = "WARNING"; sub = "Medium Risk"; color = "var(--warn)"; frac = 0.55; }
  else              { label = "SECURE";  sub = "Low Risk";    color = "var(--safe)"; frac = 0.16; }
  const arc = $("riskGauge");
  arc.style.strokeDasharray = `${(frac * RISK_ARC).toFixed(1)} ${RISK_ARC}`;
  arc.style.stroke = color;
  const lvl = $("riskLevel");
  lvl.textContent = label; lvl.style.color = color;
  $("riskSub").textContent = sub;
}

// ==================== Weather (live Open-Meteo + fallback) ====================
let wxLive = null, wxLoc = null;

function setWxIcon(el, key) {
  const ic = WX_SVG[key] || WX_SVG.clear;
  el.innerHTML = ic.svg;
  el.style.color = ic.color;
}

async function fetchWeather(lat, lon, city) {
  try {
    const url = `https://api.open-meteo.com/v1/forecast?latitude=${lat}&longitude=${lon}`
      + `&current=temperature_2m,relative_humidity_2m,weather_code`
      + `&daily=weather_code,temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=4`;
    const r = await fetch(url, { cache: "no-store" });
    if (!r.ok) throw new Error(r.status);
    wxLive = await r.json();
    renderWeatherLive(city);
  } catch (_) { /* keep data.json fallback already shown */ }
}

function renderWeatherLive(city) {
  if (!wxLive) return;
  const cur = wxLive.current;
  const w = wmoToKey(cur.weather_code);
  $("wxCity").textContent = city || WX_CITY;
  $("weatherTemp").textContent = Math.round(cur.temperature_2m);
  $("weatherCond").textContent = w.label;
  $("weatherHum").textContent = Math.round(cur.relative_humidity_2m);
  setWxIcon($("weatherIcon"), w.key);

  const d = wxLive.daily;
  const out = [];
  for (let i = 0; i < d.time.length && i < 4; i++) {
    const dd = wmoToKey(d.weather_code[i]);
    const name = i === 0 ? "Today" : new Date(d.time[i]).toLocaleDateString("en-US", { weekday: "short" });
    const hi = Math.round(d.temperature_2m_max[i]);
    const lo = Math.round(d.temperature_2m_min[i]);
    const ic = WX_SVG[dd.key] || WX_SVG.clear;
    out.push(`<div class="wx-day"><div class="d-name">${name}</div>`
      + `<div class="d-ic" style="color:${ic.color}">${ic.svg}</div>`
      + `<div class="d-temp">${hi}°<span class="lo"> / ${lo}°</span></div></div>`);
  }
  $("forecast").innerHTML = out.join("");
}

function renderWeatherFallback(w) {
  if (wxLive || !w) return;   // live data wins once available
  $("wxCity").textContent = w.city || WX_CITY;
  if (w.tempC != null)    $("weatherTemp").textContent = Math.round(w.tempC);
  if (w.humidity != null) $("weatherHum").textContent = Math.round(w.humidity);
  $("weatherCond").textContent = w.condition || "--";
  setWxIcon($("weatherIcon"), condToKey(w.condition));
}

function maybeFetchWeather(w) {
  const lat = w && w.lat != null ? w.lat : WX_LAT;
  const lon = w && w.lon != null ? w.lon : WX_LON;
  const city = w && w.city ? w.city : WX_CITY;
  const loc = `${lat},${lon}`;
  if (loc !== wxLoc) { wxLoc = loc; fetchWeather(lat, lon, city); }
}

// ==================== Render ====================
function render(d) {
  $("vehicleTag").textContent = `Vehicle #${d.meta.vehicleId}`;

  const sys = smartState(d);           // V2N / V2P / AI flags (missing → 0)
  processAlerts(d.adas, sys);
  updateAdasGrid(d.adas);
  updateSmartGrid(sys);
  updateHazards(sys);
  const ultraWorst = processUltrasonic(d.ultrasonic || {});
  updateRisk(d.adas, ultraWorst, sys);

  // Speedometer
  const speed = d.drive.speedKmh;
  const max = d.drive.maxGaugeKmh || 120;
  const frac = Math.max(0, Math.min(1, speed / max));
  $("speedValue").textContent = Math.round(speed);
  $("speedoValue").style.strokeDasharray = `${(frac * ARC_LEN).toFixed(1)} 528`;

  // Car motion (road scroll scales with km/h)
  const road = $("road");
  if (speed > 0.5) {
    road.classList.add("moving");
    road.style.setProperty("--road-speed", `${Math.max(0.25, 14 / speed).toFixed(2)}s`);
  } else {
    road.classList.remove("moving");
  }

  // Cabin temperature + state icon
  const temp = d.drive.vehicleTempC;
  if (temp != null) {
    $("vehTemp").textContent = Math.round(temp);
    // Cabin comfort zones: < 18 = cold, 18-28 = comfortable, > 28 = hot
    const st = temp < 18 ? "cold" : temp > 28 ? "hot" : "normal";
    $("tempCard").className = `card stat temp-tile ${st}`;
    $("tempState").textContent = st === "normal" ? "comfort" : st;
    const ti = TEMP_SVG[st];
    const iconEl = $("tempIcon");
    iconEl.innerHTML = ti.svg;
    iconEl.style.color = ti.color;
  }

  // Weather (live preferred, data.json fallback)
  maybeFetchWeather(d.weather);
  renderWeatherFallback(d.weather);

  // Heading / compass
  const hdg = ((d.drive.heading % 360) + 360) % 360;
  $("headingValue").textContent = `${Math.round(hdg)}°`;
  $("headingDir").textContent = COMPASS_DIRS[Math.round(hdg / 45) % 8];
  $("compassNeedle").style.transform = `rotate(${hdg}deg)`;

  // Attitude (artificial horizon)
  const pitch = d.drive.pitch || 0;
  const roll = d.drive.roll || 0;
  $("pitchValue").textContent = `${Math.round(pitch)}°`;
  $("rollValue").textContent = `${Math.round(roll)}°`;
  const horizon = $("horizon");
  const size = horizon.parentElement.clientWidth || 160;
  const pitchPx = Math.max(-1, Math.min(1, pitch / 45)) * (size * 0.5);
  horizon.style.transform = `rotate(${-roll}deg) translateY(${pitchPx.toFixed(1)}px)`;
}

function setConn(state, text) {
  const el = $("conn");
  el.className = "conn " + state;
  el.innerHTML = `<i class="conn-dot"></i> ${text}`;
}

// Subscribe to the server's Server-Sent Events stream. The server pushes
// data.json the instant it changes, so there is no polling delay.
// EventSource reconnects on its own if the connection drops.
function connectStream() {
  const es = new EventSource("/events");
  es.onmessage = (e) => {
    try {
      render(JSON.parse(e.data));
      setConn("ok", "live");
    } catch (_) { /* ignore a malformed frame; next push will be clean */ }
  };
  es.onerror = () => setConn("err", "no data");
}

buildAdasGrid();
buildSmartGrid();
buildHazards();
connectStream();
setInterval(() => { if (wxLoc) { const [la, lo] = wxLoc.split(","); fetchWeather(la, lo, $("wxCity").textContent); } }, 600000);
