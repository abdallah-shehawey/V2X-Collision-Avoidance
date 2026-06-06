// ============================================================
//  V2X Dashboard - front-end
//  Reads ALL vehicle values from data.json (single source of
//  truth). data.json is updated by server.py (simulation now,
//  STM-over-UART later). The UI just polls the file & renders.
//
//  ADAS flag meaning:
//    flag 0 -> safe (no alert)
//    flag 1 -> WARNING  -> toast + warning beep + log
//    flag 2 -> CRITICAL -> fullscreen popup + alarm + log
// ============================================================

const POLL_MS = 1000;

// ADAS warnings: [key in data.adas, abbreviation, full name]
const WARNINGS = [
  ["eebl", "EEBL", "Emergency Electronic Brake Light"],
  ["fcw",  "FCW",  "Forward Collision Warning"],
  ["bsw",  "BSW",  "Blind Spot Warning"],
  ["dnpw", "DNPW", "Do Not Pass Warning"],
  ["ima",  "IMA",  "Intersection Movement Assist"],
];

// Messages for critical popup
const CRITICAL_MESSAGES = {
  eebl: "Vehicle ahead is hard braking!\nReduce speed immediately!",
  fcw:  "Collision imminent!\nBrake now!",
  bsw:  "Vehicle detected in blind spot!\nDo not change lane!",
  dnpw: "Oncoming traffic detected!\nDo not overtake!",
  ima:  "Cross traffic at intersection!\nStop immediately!",
};

// Ultrasonic sensors: [key in data.ultrasonic, short label]
const SENSORS = [
  ["front",      "Front"],
  ["frontLeft",  "Front-L"],
  ["frontRight", "Front-R"],
  ["rear",       "Rear"],
  ["rearLeft",   "Rear-L"],
  ["rearRight",  "Rear-R"],
];
const NEAR_CM  = 120;   // < this -> caution (yellow)
const CLOSE_CM = 50;    // < this -> danger  (red)
const RANGE_CM = 250;   // > this -> nothing detected

const COMPASS_DIRS = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"];
const $ = (id) => document.getElementById(id);
const ARC_LEN  = 396;   // speedometer arc length
const RISK_ARC = 232;   // risk gauge arc length (pi * r, r=74)

// ==================== Inline SVG icons ====================
const ADAS_ICONS = {
  fcw:  `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><rect x="5" y="12" width="14" height="7" rx="2"/><path d="M7 12l1.6-4h6.8L17 12"/><path d="M12 2v3M9 3.5l1.4 2.2M15 3.5l-1.4 2.2"/></svg>`,
  eebl: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="9"/><path d="M12 7v6"/><circle cx="12" cy="16.4" r="0.7" fill="currentColor" stroke="none"/></svg>`,
  bsw:  `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><rect x="8" y="4" width="8" height="16" rx="2.5"/><path d="M5 9c-1.1 1-1.1 5 0 6M19 9c1.1 1 1.1 5 0 6"/></svg>`,
  dnpw: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><rect x="3.5" y="6" width="6" height="12" rx="1.5"/><rect x="14.5" y="6" width="6" height="12" rx="1.5"/><path d="M12 3v18" stroke-dasharray="2.5 2.5"/></svg>`,
  ima:  `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><path d="M12 3v18M3 12h18"/><path d="M9.5 8.5L12 6l2.5 2.5M9.5 15.5L12 18l2.5-2.5"/></svg>`,
};

function weatherIcon(cond) {
  const c = (cond || "").toLowerCase();
  const s = (p) => `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">${p}</svg>`;
  if (c.includes("rain") || c.includes("drizzle"))
    return { color: "#4aa6ff", svg: s(`<path d="M7 15a4 4 0 0 1 0-8 5 5 0 0 1 9.6-1.3A3.5 3.5 0 0 1 17.5 15Z"/><path d="M8 18l-1 2.5M12 18l-1 2.5M16 18l-1 2.5"/>`) };
  if (c.includes("thunder") || c.includes("storm"))
    return { color: "#7c5cff", svg: s(`<path d="M7 14a4 4 0 0 1 0-8 5 5 0 0 1 9.6-1.3A3.5 3.5 0 0 1 17.5 14"/><path d="M12 12l-2 4h3l-2 4"/>`) };
  if (c.includes("snow"))
    return { color: "#9fd2ff", svg: s(`<path d="M7 15a4 4 0 0 1 0-8 5 5 0 0 1 9.6-1.3A3.5 3.5 0 0 1 17.5 15Z"/><path d="M9 19v.01M12 20.5v.01M15 19v.01"/>`) };
  if (c.includes("cloud") || c.includes("overcast") || c.includes("fog") || c.includes("mist"))
    return { color: "#8b93a1", svg: s(`<path d="M7 18a4 4 0 0 1 0-8 5 5 0 0 1 9.6-1.3A3.5 3.5 0 0 1 17.5 18Z"/>`) };
  // default: clear / sunny
  return { color: "#f5a623", svg: s(`<circle cx="12" cy="12" r="4.5"/><path d="M12 2v2.5M12 19.5V22M2 12h2.5M19.5 12H22M4.6 4.6l1.8 1.8M17.6 17.6l1.8 1.8M19.4 4.6l-1.8 1.8M6.4 17.6l-1.8 1.8"/>`) };
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
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
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
      const osc = ctx.createOscillator();
      const gain = ctx.createGain();
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

// ==================== Toast Notifications ====================
const activeToasts = new Map();
function showToast(key, name) {
  if (activeToasts.has(key)) return;
  const container = $("toastContainer");
  const el = document.createElement("div");
  el.className = "toast";
  el.innerHTML = `
    <span class="toast-icon">⚠️</span>
    <div class="toast-body">
      <div class="toast-title">${name}</div>
      <div class="toast-desc">Warning detected — monitor situation</div>
    </div>
    <button class="toast-close" onclick="this.parentElement.classList.add('toast-out'); setTimeout(() => this.parentElement.remove(), 300)">✕</button>`;
  container.appendChild(el);
  activeToasts.set(key, el);
  playWarningBeep();
}
function removeToast(key) {
  const el = activeToasts.get(key);
  if (!el) return;
  el.classList.add("toast-out");
  setTimeout(() => { el.remove(); activeToasts.delete(key); }, 300);
}

// ==================== Critical Popup ====================
let activeCritical = null;
let criticalSoundInterval = null;
function showCritical(key, name) {
  if (activeCritical === key) return;
  activeCritical = key;
  $("criticalTitle").textContent = "CRITICAL ALERT";
  $("criticalMsg").textContent = CRITICAL_MESSAGES[key] || `${name} — Critical danger!`;
  $("criticalSystem").textContent = name;
  $("criticalOverlay").style.display = "flex";
  playErrorAlarm();
  if (criticalSoundInterval) clearInterval(criticalSoundInterval);
  criticalSoundInterval = setInterval(playErrorAlarm, 2000);
}
function hideCritical() {
  if (!activeCritical) return;
  activeCritical = null;
  $("criticalOverlay").style.display = "none";
  if (criticalSoundInterval) { clearInterval(criticalSoundInterval); criticalSoundInterval = null; }
}

// ==================== Event Log ====================
const MAX_EVENTS = 40;
function nowTime() {
  return new Date().toLocaleTimeString("en-GB", { hour12: false });
}
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
    card.innerHTML = `
      <div class="adas-name">${abbr}</div>
      <div class="adas-ic">${ADAS_ICONS[key]}</div>
      <div class="adas-status">SAFE</div>`;
    grid.appendChild(card);
  }
}
function updateAdasGrid(adas) {
  for (const key of ADAS_ORDER) {
    const flag = adas[key] || 0;
    const card = $(`adas-${key}`);
    if (!card) continue;
    const status = card.querySelector(".adas-status");
    if (flag === 2)      { card.className = "adas-card crit"; status.textContent = "ACTIVE"; }
    else if (flag === 1) { card.className = "adas-card warn"; status.textContent = "WARNING"; }
    else                 { card.className = "adas-card safe"; status.textContent = "SAFE"; }
  }
}

// ==================== Alert + event logic ====================
const prevStates = {};      // adas flag transitions
const prevSensor = {};      // ultrasonic state transitions

function processAlerts(adas) {
  let highestCritKey = null, highestCritName = null;

  for (const [key, abbr, name] of WARNINGS) {
    const flag = adas[key] || 0;
    const prev = prevStates[key] || 0;

    if (flag === 1) showToast(key, `${abbr} — ${name}`);
    else if (prev === 1) removeToast(key);

    if (flag !== prev) {
      if (flag === 2)      addEvent(`${abbr} Triggered`, "crit");
      else if (flag === 1) addEvent(`${abbr} Warning`, "warn");
      else                 addEvent(`${abbr} Cleared`, "safe");
    }

    if (flag === 2 && !highestCritKey) { highestCritKey = key; highestCritName = `${abbr} — ${name}`; }
    prevStates[key] = flag;
  }

  if (highestCritKey) { removeToast(highestCritKey); showCritical(highestCritKey, highestCritName); }
  else hideCritical();
}

// returns the worst sensor state seen this tick: "clear" | "near" | "close"
function processUltrasonic(ultra) {
  let worst = "clear";
  for (const [key, label] of SENSORS) {
    const cm = ultra && ultra[key] != null ? ultra[key] : RANGE_CM + 1;
    let state = "off";
    if (cm < CLOSE_CM)      state = "close";
    else if (cm < NEAR_CM)  state = "near";
    else if (cm <= RANGE_CM) state = "clear";

    const g = $(`sn-${key}`);
    if (g) g.setAttribute("class", state === "off" ? "sensor" : `sensor ${state}`);

    const txt = $(`dist-${key}`);
    if (txt) {
      txt.textContent = cm > RANGE_CM ? "—" : `${Math.round(cm)}`;
      txt.style.fill = state === "close" ? "var(--crit)" : state === "near" ? "var(--warn)" : "var(--muted)";
    }

    if (state === "close" && prevSensor[key] !== "close") addEvent(`${label} Obstacle`, "crit");
    prevSensor[key] = state;

    if (state === "close") worst = "close";
    else if (state === "near" && worst !== "close") worst = "near";
  }
  return worst;
}

// ==================== Overall risk ====================
function updateRisk(adas, ultraWorst) {
  const anyCrit = WARNINGS.some(([k]) => (adas[k] || 0) === 2) || ultraWorst === "close";
  const anyWarn = WARNINGS.some(([k]) => (adas[k] || 0) === 1) || ultraWorst === "near";

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

// ==================== Render ====================
function render(d) {
  // Header
  $("vehicleTag").textContent = `Vehicle #${d.meta.vehicleId}`;

  // Alerts + events
  processAlerts(d.adas);
  updateAdasGrid(d.adas);
  const ultraWorst = processUltrasonic(d.ultrasonic || {});
  updateRisk(d.adas, ultraWorst);

  // Speedometer
  const speed = d.drive.speedKmh;
  const max = d.drive.maxGaugeKmh || 120;
  const frac = Math.max(0, Math.min(1, speed / max));
  $("speedValue").textContent = Math.round(speed);
  $("speedoValue").style.strokeDasharray = `${(frac * ARC_LEN).toFixed(1)} 528`;

  // Car motion (road scroll speed scales with km/h)
  const road = $("road");
  if (speed > 0.5) {
    road.classList.add("moving");
    road.style.setProperty("--road-speed", `${Math.max(0.18, 9 / speed).toFixed(2)}s`);
  } else {
    road.classList.remove("moving");
  }

  // Engine temperature
  const temp = d.drive.vehicleTempC;
  if (temp != null) {
    $("vehTemp").textContent = Math.round(temp);
    const tcard = $("tempCard");
    const bar = $("tempBar");
    const tFrac = Math.max(0, Math.min(1, (temp - 40) / 80)); // 40..120 °C
    bar.style.width = `${(tFrac * 100).toFixed(0)}%`;
    if (temp >= 105)     { tcard.className = "card stat temp-card hot";  bar.style.background = "var(--crit)"; }
    else if (temp >= 95) { tcard.className = "card stat temp-card warm"; bar.style.background = "var(--warn)"; }
    else                 { tcard.className = "card stat temp-card";      bar.style.background = "var(--accent)"; }
  }

  // Weather
  const w = d.weather;
  if (w) {
    $("weatherTemp").textContent = Math.round(w.tempC);
    $("weatherCond").textContent = w.condition || "--";
    $("weatherHum").textContent = w.humidity != null ? Math.round(w.humidity) : "--";
    const ic = weatherIcon(w.condition);
    const iconEl = $("weatherIcon");
    iconEl.innerHTML = ic.svg;
    iconEl.style.color = ic.color;
  }

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

async function poll() {
  try {
    const res = await fetch(`data.json?t=${Date.now()}`, { cache: "no-store" });
    if (!res.ok) throw new Error(res.status);
    render(await res.json());
    setConn("ok", "live");
  } catch (e) {
    setConn("err", "no data");
  }
}

buildAdasGrid();
poll();
setInterval(poll, POLL_MS);
