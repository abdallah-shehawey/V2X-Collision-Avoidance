/* =====================================================================
   V2X Car · phone remote — front-end logic.

   Driving model (matches the server's watchdog):
     • HOLD an arrow → we POST /cmd repeatedly every REPEAT_MS while held.
     • RELEASE → posts stop → the server's watchdog stops the car within IDLE_T.
   So latency is bounded and the car stops by itself if the page freezes, the
   Wi-Fi drops, or the tab is closed. The Stop button forces an immediate stop.

   Extras for a stable "dashboard on a sleeping screen":
     • Screen Wake Lock so the phone never dims/locks while driving.
     • Re-acquires the lock when the tab becomes visible again.
   ===================================================================== */

(() => {
  "use strict";

  const REPEAT_MS = 120;          // how often we re-send the held direction
  const POLL_MS   = 1500;         // connection heartbeat when idle
  const DIR_NAME  = { F: "FORWARD", B: "REVERSE", L: "LEFT", R: "RIGHT", S: "IDLE" };
  const GEAR_OF   = { 0.15: 1, 0.30: 2, 0.50: 3, 0.75: 4, 1.00: 5 };

  // ── state ──────────────────────────────────────────────────────────
  let speed       = 0.22;         // current commanded speed (0.15..1.0)
  let heldDir     = "S";          // direction currently held by a finger
  let repeatTimer = null;         // setInterval handle while a direction is held
  let online      = true;         // last known link state (to update UI on change)

  // ── DOM ────────────────────────────────────────────────────────────
  const $ = (s) => document.querySelector(s);
  const linkDot   = $("#link-dot");
  const statusTxt = $("#status-text");
  const gearLabel = $("#gear-label");
  const speedPct  = $("#speed-pct");
  const dirLabel  = $("#dir-label");
  const wakeBtn   = $("#wake-btn");

  // ── networking ─────────────────────────────────────────────────────
  // One in-flight request at a time is fine; we don't queue. A failed POST just
  // flips the link indicator — the server's watchdog handles safety regardless.
  async function post(path, body) {
    try {
      const r = await fetch(path, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(body),
        keepalive: true,           // let a release-stop fly even as the page hides
      });
      if (!r.ok) throw new Error(r.status);
      setLink(true);
      return await r.json();
    } catch (e) {
      setLink(false);
      return null;
    }
  }

  function setLink(up) {
    if (up === online) return;     // only touch the DOM on a real change
    online = up;
    linkDot.classList.toggle("live", up);
    linkDot.classList.toggle("down", !up);
    statusTxt.textContent = up ? "Connected" : "Reconnecting…";
  }

  // ── driving ────────────────────────────────────────────────────────
  function sendCmd(dir) {
    post("/cmd", { dir, speed });
  }

  function startHold(dir) {
    if (heldDir === dir) return;
    heldDir = dir;
    updateReadout();
    sendCmd(dir);                  // fire immediately, then keep it alive
    clearInterval(repeatTimer);
    if (dir !== "S") {
      repeatTimer = setInterval(() => sendCmd(dir), REPEAT_MS);
    }
  }

  function releaseHold() {
    if (heldDir === "S") return;
    heldDir = "S";
    clearInterval(repeatTimer);
    repeatTimer = null;
    sendCmd("S");                  // explicit stop (don't wait for the watchdog)
    updateReadout();
  }

  function updateReadout() {
    speedPct.textContent = Math.round(speed * 100);
    gearLabel.textContent = heldDir === "S" ? "N" : (GEAR_OF[speed] ?? "—");
    dirLabel.textContent = DIR_NAME[heldDir] || "IDLE";
  }

  // ── arrow buttons: pointer events cover touch + mouse uniformly ─────
  document.querySelectorAll(".arrow").forEach((btn) => {
    const dir = btn.dataset.dir;

    const press = (e) => {
      e.preventDefault();
      btn.classList.add("active");
      if (dir === "S") { releaseHold(); flash(btn); return; }
      startHold(dir);
    };
    const release = (e) => {
      e && e.preventDefault();
      btn.classList.remove("active");
      if (dir !== "S" && heldDir === dir) releaseHold();
    };

    btn.addEventListener("pointerdown", press);
    btn.addEventListener("pointerup", release);
    btn.addEventListener("pointercancel", release);
    btn.addEventListener("pointerleave", release);  // finger slid off the button
  });

  function flash(el) {
    el.classList.add("active");
    setTimeout(() => el.classList.remove("active"), 120);
  }

  // ── gear shifter ───────────────────────────────────────────────────
  const gears = document.querySelectorAll(".gear");
  function selectGear(el) {
    speed = parseFloat(el.dataset.speed);
    gears.forEach((g) => g.classList.toggle("active", g === el));
    updateReadout();
    // push the new speed; keeps the car moving if a direction is currently held
    post("/speed", { speed });
  }
  gears.forEach((g) =>
    g.addEventListener("pointerdown", (e) => { e.preventDefault(); selectGear(g); })
  );

  // ── screen wake lock (keep the dashboard awake) ────────────────────
  let wakeLock = null;
  async function acquireWake() {
    if (!("wakeLock" in navigator)) {
      wakeBtn.style.display = "none";   // unsupported (e.g. desktop) → hide button
      return;
    }
    try {
      wakeLock = await navigator.wakeLock.request("screen");
      wakeBtn.classList.add("on");
      wakeLock.addEventListener("release", () => wakeBtn.classList.remove("on"));
    } catch (_) { /* user gesture may be required; the button retries */ }
  }
  wakeBtn.addEventListener("click", acquireWake);

  // Re-grab the lock when we come back to the foreground (it auto-releases on hide).
  document.addEventListener("visibilitychange", () => {
    if (document.visibilityState === "visible") {
      acquireWake();
    } else {
      releaseHold();   // tab hidden → make sure the car isn't left driving
    }
  });

  // Safety net: stop on any page teardown.
  window.addEventListener("pagehide", releaseHold);
  window.addEventListener("blur", releaseHold);

  // ── idle heartbeat so the link dot is honest even when parked ──────
  setInterval(() => { if (heldDir === "S") fetch("/state").then(
    () => setLink(true), () => setLink(false)); }, POLL_MS);

  // ── boot ───────────────────────────────────────────────────────────
  // Start in gear 1 (slowest) and push it to the server, so the phone's gauge
  // and the car agree from the first frame. Then probe the link & grab wake-lock.
  selectGear([...gears].find((g) => parseFloat(g.dataset.speed) === 0.15) || gears[0]);
  acquireWake();
  fetch("/state").then(() => setLink(true), () => setLink(false));
})();
