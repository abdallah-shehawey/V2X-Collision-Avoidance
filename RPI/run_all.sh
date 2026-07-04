#!/usr/bin/env bash
# =============================================================================
# run_all.sh — Launch the whole RPI V2X stack in the correct order.
#
# Startup order (from dashboard_bridge.py's documented spec):
#   1. hub/hub.py                 IPC pub/sub broker      — MUST be first
#   2. hub/dashboard_bridge.py    hub frames → data.json
#   3. V2N/Car_client.py          publishes v2n_frame     (MQTT + hub)
#   4. V2P/V2P.py                 publishes v2p_frame + motorcycle_alert (camera)
#   5. DashBoard/server.py        telemetry web server + STM32 UART reader (:8000)
#   6. Control/control_server.py  phone drive remote      (:8001)
#
# Every process runs from its OWN directory (each resolves paths relative to
# its file, and V2N/V2P/DashBoard add ../hub to sys.path themselves).
#
# Usage:
#   ./run_all.sh            start everything, stream all logs, Ctrl+C stops all
#   ./run_all.sh --no-v2p   skip V2P (no camera / onnxruntime on this machine)
#   ./run_all.sh --no-v2n   skip Car_client (no MQTT / internet)
#
# Logs are written to ./logs/<name>.log and also tailed to the console.
# Ctrl+C tears the whole stack down cleanly.
# =============================================================================
set -u

# Resolve the directory this script lives in, so it works from anywhere.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="$ROOT/logs"
mkdir -p "$LOG_DIR"

PYTHON="${PYTHON:-python3}"

# --- optional skips ----------------------------------------------------------
RUN_V2N=1
RUN_V2P=1
for arg in "$@"; do
  case "$arg" in
    --no-v2n) RUN_V2N=0 ;;
    --no-v2p) RUN_V2P=0 ;;
    -h|--help)
      grep '^#' "$0" | sed 's/^# \{0,1\}//'
      exit 0 ;;
    *) echo "unknown option: $arg (try --help)"; exit 1 ;;
  esac
done

PIDS=()   # child PIDs, in launch order
NAMES=()  # matching human-readable names

# start <name> <workdir> <script> [wait_seconds]
#   Launch "$PYTHON <script>" from <workdir>, log to logs/<name>.log,
#   record its PID, then pause so the next stage sees it up.
start() {
  local name="$1" workdir="$2" script="$3" wait_s="${4:-1}"
  local log="$LOG_DIR/$name.log"
  echo "▶ starting $name  ($script)"
  ( cd "$ROOT/$workdir" && exec "$PYTHON" "$script" ) >"$log" 2>&1 &
  local pid=$!
  PIDS+=("$pid")
  NAMES+=("$name")
  # Tail this log to the console, prefixed with the component name.
  tail -n +1 -F "$log" 2>/dev/null | sed "s/^/[$name] /" &
  sleep "$wait_s"
  # Surface an immediate crash (e.g. missing dependency) instead of failing silently.
  if ! kill -0 "$pid" 2>/dev/null; then
    echo "✗ $name exited immediately — see $log"
  fi
}

# --- clean shutdown ----------------------------------------------------------
shutdown() {
  echo ""
  echo "■ shutting down stack …"
  # Stop children in reverse launch order (controllers first, hub last).
  for (( i=${#PIDS[@]}-1; i>=0; i-- )); do
    local pid="${PIDS[$i]}"
    if kill -0 "$pid" 2>/dev/null; then
      echo "  stopping ${NAMES[$i]} (pid $pid)"
      kill "$pid" 2>/dev/null
    fi
  done
  sleep 1
  # Force-kill any stragglers.
  for pid in "${PIDS[@]}"; do
    kill -9 "$pid" 2>/dev/null
  done
  # Kill the background tail processes spawned by this script.
  pkill -P $$ tail 2>/dev/null
  echo "done."
  exit 0
}
trap shutdown INT TERM

# --- launch sequence ---------------------------------------------------------
echo "============================================================"
echo "  RPI V2X stack — launching (logs in $LOG_DIR)"
echo "============================================================"

# 1. IPC hub — everything else connects to it, so give it a moment to bind.
start hub          hub       hub.py             2

# 2. Bridge — the only writer to data.json.
start bridge       hub       dashboard_bridge.py 1

# 3. Car_client (V2N) — MQTT + hub. Skip with --no-v2n if there's no network.
if [[ "$RUN_V2N" == 1 ]]; then
  start car_client V2N       Car_client.py       1
else
  echo "⊘ skipping Car_client (--no-v2n)"
fi

# 4. V2P — camera + ONNX. Skip with --no-v2p on a machine without a camera.
if [[ "$RUN_V2P" == 1 ]]; then
  start v2p        V2P       V2P.py              1
else
  echo "⊘ skipping V2P (--no-v2p)"
fi

# 5. Dashboard web server (:8000) + STM32 UART reader.
start dashboard    DashBoard server.py           1

# 6. Control remote (:8001).
start control      Control   control_server.py    1

echo "============================================================"
echo "  all components launched."
echo "  dashboard : http://localhost:8000"
echo "  control   : http://localhost:8001"
echo "  Press Ctrl+C to stop everything."
echo "============================================================"

# Wait for any child to exit; keep the script (and log tails) alive until then.
wait
