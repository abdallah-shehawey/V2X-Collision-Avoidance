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
  bsw:  svg(`<rect x="8" y="4" width="8" height="16" rx="2.5"/><path d="M5 9c-1.1 1-1.1 5 0 6M19 9c1.1 1 1.1 5 0 6"/>`),
  dnpw: svg(`<rect x="3.5" y="6" width="6" height="12" rx="1.5"/><rect x="14.5" y="6" width="6" height="12" rx="1.5"/><path d="M12 3v18" stroke-dasharray="2.5 2.5"/>`),
  ima:  svg(`<path d="M12 3v18M3 12h18"/><path d="M9.5 8.5L12 6l2.5 2.5M9.5 15.5L12 18l2.5-2.5"/>`),
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

function showToast(key, name) {
  const existing = activeToasts.get(key);
  if (existing) {                 // already up: just keep the watchdog alive
    clearTimeout(existing.timer);
    existing.timer = setTimeout(() => removeToast(key), TOAST_AUTO_MS);
    return;
  }
  const el = document.createElement("div");
  el.className = "toast";
  el.innerHTML = `
    <span class="toast-icon">⚠️</span>
    <div class="toast-body">
      <div class="toast-title">${name}</div>
      <div class="toast-desc">Warning detected — monitor situation</div>
    </div>
    <button class="toast-close" onclick="removeToast('${key}')">✕</button>`;
  $("toastContainer").appendChild(el);
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
function showCritical(key, name) {
  if (criticalTimer) clearTimeout(criticalTimer);
  criticalTimer = setTimeout(hideCritical, CRITICAL_AUTO_MS);   // re-arm watchdog
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
    if (flag === 2)      { card.className = "adas-card crit"; status.textContent = "ACTIVE"; }
    else if (flag === 1) { card.className = "adas-card warn"; status.textContent = "WARNING"; }
    else                 { card.className = "adas-card safe"; status.textContent = "SAFE"; }
  }
}

// ==================== Alert + event logic ====================
const prevStates = {}, prevSensor = {};
function processAlerts(adas) {
  let highestCritKey = null, highestCritName = null;
  for (const [key, abbr, name] of WARNINGS) {
    const flag = adas[key] || 0;
    const prev = prevStates[key] || 0;
    if (flag === 1) showToast(key, `${abbr} — ${name}`);
    else removeToast(key);   // any non-warning state clears it right away (no-op if not shown)
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

// drive the ultrasonic spotlight beams; returns worst state seen
function processUltrasonic(ultra) {
  let worst = "clear";
  for (const [key, label] of SENSORS) {
    const cm = ultra && ultra[key] != null ? ultra[key] : RANGE_CM + 1;
    let state = "off";
    if (cm < CLOSE_CM)       state = "close";
    else if (cm < NEAR_CM)   state = "near";
    else if (cm <= RANGE_CM) state = "clear";

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
connectStream();
setInterval(() => { if (wxLoc) { const [la, lo] = wxLoc.split(","); fetchWeather(la, lo, $("wxCity").textContent); } }, 600000);
