# AI / Computer-Vision Technical Audit — V2X Collision Avoidance

**Scope of this document.** This repository contains exactly **two** AI/CV subsystems.
There is no training code, no dataset, and no model-export pipeline anywhere else in the
project (verified by a full-repo search for `*.pt`, `*.onnx`, `*.h5`, `*.pkl`, `train*.py`,
`*.ipynb`, `data.yaml` — nothing else matched):

| Subsystem | File(s) | Model | Runtime |
| --- | --- | --- | --- |
| Traffic-light camera (vehicle/plate/ambulance) | `Traffic_Light/distance.py` | `yolov8n.pt` + EasyOCR | PyTorch (Ultralytics) |
| Pedestrian/motorcycle safety (V2P) | `RPI/V2P/V2P.py` | `model2.onnx` | ONNX Runtime (CPU) |

Both are read against the actual current code on `feat/traffic-light-ambulance-preemption`
(branched from `main`), not against the separate, unmerged `feat/traffic_light` branch's
own `CODE_REVIEW.md` — every claim below was independently re-verified against files as
they exist right now, including by parsing `model2.onnx` with the `onnx` package and by
checking `git log` for file history where a doc claim looked stale.

**Headline finding, stated up front because it governs every score below:** neither model
was trained, fine-tuned, or evaluated for this project. Both are stock, unmodified
Ultralytics YOLOv8n checkpoints trained on generic COCO (confirmed via the ONNX file's own
embedded metadata: `'description': 'Ultralytics YOLOv8n model trained on coco.yaml'`), plus
EasyOCR's bundled pretrained English reader. Everything domain-specific — ambulance
identification, distance estimation, pedestrian intent, proximity zones — is hand-written
heuristic post-processing bolted onto generic detections, not learned. This is a valid
engineering approach for a student/demo project, but it means most of the ML-research
rubric below (dataset quality, training correctness, overfitting, evaluation statistics)
has **no artifact in this repo to audit** — I say so explicitly rather than inventing one.

---

## 1. Project Architecture

**`Traffic_Light/distance.py`** (PC/laptop, not edge hardware):
```
video file → cv2.VideoCapture → every 10th frame
  → YOLOv8n (car_detector, PyTorch) detects class 2 "car" boxes (conf > 0.5)
  → crop lower 30% of box → grayscale → equalizeHist → Otsu threshold
  → EasyOCR reads plate text from the thresholded crop
  → is_ambulance = (plate text == "T4RR")
  → publish JSON to MQTT topic v2n/camera/vehicle_data
  → if is_ambulance: pause the whole pipeline 2–20s (proportional to distance)
```

**`Intelligent_Gateway.py`** (PC/laptop): subscribes to the camera topic above and to
`v2n/traffic/light/state` (from `Traffic_light_GUI.py`), keeps an in-memory
`vehicle_registry`, resolves an `is_emergency` flag from three independent latches
(`ambulance_active`, `camera_confirmed`, `camera_ambulance`), and republishes a merged
packet to `V2X/zone1/traffic/processed` for `RPI/V2N/Car_client.py`.

**`Traffic_light_GUI.py`** (as of this session): also subscribes directly to
`v2n/camera/vehicle_data` so the light itself can preempt its own RED/GREEN/YELLOW cycle
on an ambulance detection (see the "Emergency preemption" section of
`Traffic_Light/README.md` added this session).

**`RPI/V2P/V2P.py`** (Raspberry Pi, edge hardware):
```
Pi Camera (picamera2, "RGB888") → every 3rd frame
  → cv2.resize to 640x640 (NO letterbox) → /255.0 → CHW → batch dim
  → model2.onnx via onnxruntime (CPUExecutionProvider) → raw [1,84,8400] output
  → manual box decode + cv2.dnn.NMSBoxes
  → CentroidTracker (IoU + distance greedy matching) assigns persistent IDs
  → analyze_intent() / estimate_proximity() / estimate_distance_meters() heuristics
  → publish safety flags over a custom Unix-socket pub/sub hub (RPI/hub/ipc_node.py)
  → consumed by dashboard_bridge.py → RPI/DashBoard/data.json
```

**How the files interact:** `distance.py` and `V2P.py` do **not** share any code, despite
solving the same underlying problem (YOLOv8n object detection + heuristic distance/risk
estimation) in two independently-written, already-diverged implementations. This is a
concrete maintainability cost, not just a style note — see §11 and §14.

---

## 2. Dataset

**There is no dataset in this repository.** No image folders, no annotation files, no
`data.yaml`, no train/val/test split, no augmentation code.

- `yolov8n.pt` — confirmed via `zipfile` inspection (`torch.save` container,
  `yolov8n/data.pkl` + `yolov8n/data/*` tensors) to be the standard Ultralytics release
  format; file size (6.55 MB) matches the well-known public YOLOv8n checkpoint.
- `model2.onnx` — statically parsed with the `onnx` Python package (metadata dump below);
  its own embedded `description` field says **"Ultralytics YOLOv8n model trained on
  coco.yaml"** and `author: Ultralytics`. This is not a project-trained model.
- EasyOCR — the pip package's bundled pretrained English recognizer; no plate-format
  fine-tuning, no custom character set.
- The only data-like artifacts in the repo are `plate_detections_with_distance.csv` and
  `results.txt` — these are **unlabelled run logs from one manual pass over one video
  file**, not a dataset: no ground truth to compare against, and (see §14) the
  "Confidence (%)" column isn't even real OCR confidence.

**Issue — Finding D1**
- Severity: High
- Location: whole project (absence)
- Explanation: no project-specific dataset, labels, or held-out evaluation set exists for
  ambulance-plate detection, pedestrian detection, or motorcycle detection in the actual
  deployment conditions (camera height/angle, local plate font, lighting).
- Impact: there is no evidence — because there is no measurement — that this pipeline
  actually recognizes ambulances or pedestrians reliably in the real target environment.
  All "it works" claims are necessarily anecdotal (one test video, one manual observation).
- Recommendation: collect and label at least a few hundred frames from the actual target
  cameras/mounting positions; compute precision/recall against them before claiming the
  ambulance-preemption or pedestrian-safety features are reliable.

Because there is no dataset, **duplicate-image checking, class-balance analysis, and
data-leakage analysis are all N/A** — there is nothing to leak between, since there was
never a training split.

---

## 3. Preprocessing (training vs. inference consistency)

Since neither model was trained in this repo, "training vs inference" reduces to: does the
inference-time preprocessing match what each model actually expects internally? I checked
this concretely and found one confirmed mismatch and one high-risk unverified assumption.

**Finding P1 — Aspect-ratio distortion in V2P.py (no letterboxing)**
- Severity: **High**
- Location: `RPI/V2P/V2P.py`, main loop, `blob = cv2.resize(rgb_frame, (MODEL_INPUT_SIZE, MODEL_INPUT_SIZE))`
- Explanation: the camera frame is 640×480 (4:3). `cv2.resize` to 640×640 **stretches the
  image vertically by ~1.33x** with no aspect-ratio preservation. Ultralytics YOLO models
  (including this exact export, confirmed via its embedded `imgsz: [640, 640]` /
  `stride: 32` metadata) are trained and normally exported assuming **letterboxed**
  (pad-to-square) input, not stretched input. Feeding a distorted image changes how object
  shapes appear to the network relative to its training distribution.
- Impact: degraded detection accuracy/confidence for people, bicycles, and motorcycles —
  exactly the four classes this subsystem exists to detect — with no error or warning; box
  coordinates are still correctly un-scaled back onto the original frame (`scale_x`,
  `scale_y` are internally consistent with the distortion), so this bug is invisible unless
  someone compares detection quality against a properly letterboxed run.
- Recommendation:
  ```python
  def letterbox(img, size=640, color=(114, 114, 114)):
      h, w = img.shape[:2]
      r = size / max(h, w)
      nh, nw = int(h * r), int(w * r)
      resized = cv2.resize(img, (nw, nh))
      canvas = np.full((size, size, 3), color, dtype=np.uint8)
      top, left = (size - nh) // 2, (size - nw) // 2
      canvas[top:top+nh, left:left+nw] = resized
      return canvas, r, left, top
  # then invert (r, left, top) when mapping boxes back to frame coordinates
  ```

**Finding P2 — picamera2 "RGB888" may actually be BGR (unverified, needs on-device check)**
- Severity: High (flagged as unverified — cannot confirm without the physical Pi camera)
- Location: `RPI/V2P/V2P.py`, `picam2.create_preview_configuration(main={"format": "RGB888", ...})`, and `frame = rgb_frame  # ★★★ التغيير الأساسي: من غير تحويل ألوان ★★★`
- Explanation: picamera2's `"RGB888"` format is a widely-reported gotcha — several
  picamera2/libcamera versions deliver bytes in **BGR** memory order despite the format
  name, because the name describes byte layout as seen by some encoders, not channel
  order as OpenCV expects. The code comment explicitly says color conversion was
  *removed* ("no color conversion") — if this Pi/libcamera version is one of the ones that
  actually delivers BGR, the model receives channel-swapped input for every single frame.
- Impact: if triggered, this silently degrades detection accuracy in a color-dependent way
  (e.g. worse discrimination on skin tones / clothing colors) without any crash or error —
  exactly the kind of bug that "seems to mostly work" in casual testing but underperforms
  systematically.
- Recommendation: on the actual Pi, capture one frame, `cv2.imwrite` it, and visually
  confirm known-color reference objects render correctly; or explicitly request `"BGR888"`
  and let a single, explicit `cv2.cvtColor(..., COLOR_BGR2RGB)` make the channel order a
  documented decision instead of an assumption embedded in a removed line.

**Not a bug — `distance.py` (verified correct):** it passes raw BGR frames from
`cv2.VideoCapture` straight into `car_detector(frame, verbose=False)`. This is Ultralytics'
documented supported input path — the library does its own internal letterboxing and
BGR→RGB conversion for you when given a raw OpenCV frame. No mismatch here.

**Finding P3 — Manual OCR preprocessing not part of EasyOCR's expected input**
- Severity: Medium
- Location: `Traffic_Light/distance.py`, plate-crop block (`cv2.cvtColor` → `cv2.equalizeHist` → `cv2.threshold(..., THRESH_OTSU)`)
- Explanation: EasyOCR's recognizer was trained on ordinary color/grayscale text crops and
  does its own internal normalization; it was not trained on pre-binarized (pure black/white)
  crops. Aggressive Otsu thresholding on a small, often-blurry plate crop can clip thin
  character strokes and reduce accuracy rather than help it — and no ablation (with vs.
  without this preprocessing) exists in the repo to justify it.
- Recommendation: try feeding EasyOCR the grayscale (or even original color) crop directly
  and compare read rates before keeping the binarization step.

---

## 4. Training Pipeline

**N/A — no training pipeline exists anywhere in this repository.** No optimizer, scheduler,
loss function, epoch loop, checkpoint saving, mixed precision, or seed-setting code was
found for either model. Both are used exactly as downloaded.

**Finding T1**
- Severity: Informational (but important — states a fact the project's own evaluation
  doc doesn't make explicit)
- Location: whole project
- Explanation: `V2X_Graduation_Project_Evaluation.md` scores "Innovation & Originality"
  10/10 and frames the project as AI-driven; the AI components are, concretely, two
  unmodified public checkpoints plus hand-written rule-based post-processing.
- Impact: none for functionality (using pretrained models is a legitimate, common choice),
  but it matters for how the project should be *described* — the innovation here is in the
  systems integration (MQTT/IPC fusion, the ambulance-preemption state machine, the sensor
  fusion across ultrasonic + V2V + camera) and heuristic engineering, not in model training.
- Recommendation: if the presentation/evaluation claims custom "AI models," either fine-tune
  on project-specific data (see D1) or reframe the claim to what's actually true: applied
  computer vision integration, not custom model development.

---

## 5. Model Architecture

Both models are **YOLOv8n** (the smallest Ultralytics YOLOv8 variant — reasonable choice
for CPU-only edge/PC deployment). Verified programmatically from `model2.onnx`:

```
IR version: 7        Opset: 12        Producer: pytorch 2.11.0
Input:  images  float32  [1, 3, 640, 640]
Output: output0 float32  [1, 84, 8400]        (4 box coords + 80 COCO class scores, no NMS baked in)
Op histogram: Conv×64, Sigmoid×58 + Mul×58 (SiLU activations), Split×8 (C2f blocks),
              MaxPool×3 (SPPF), Resize×2 (FPN upsample), Concat/Reshape/Transpose/Softmax/Div (DFL head)
onnx.checker.check_model(): PASSES (structurally valid graph)
```
This matches the publicly documented YOLOv8n architecture exactly (CSPDarknet-style
backbone with C2f blocks, SPPF neck, decoupled anchor-free head with Distribution Focal
Loss box regression) — ~3.2M parameters / ~8.7 GFLOPs at 640×640, the standard published
spec for this architecture (not measured in this repo, but this is a known, fixed public
model, not something whose FLOPs are in question).

**Why chosen:** reasonable — YOLOv8n is the fastest/smallest variant in its family, suited
to CPU-bound edge inference (Raspberry Pi) and to a demo running on a laptop.

**Suggested improvement:** only 4 of the model's 80 trained COCO classes are ever used
(person/bicycle/car/motorcycle). A model fine-tuned or distilled on just those classes
would very likely be both smaller and more accurate for this specific task than an
unmodified 80-class general detector — and would remove the class-filtering bug in §9/§14
(missed ambulances classified as "truck"/"bus").

---

## 6. Evaluation

**N/A in the formal sense — no Precision/Recall/mAP/F1/confusion matrix/PR curve/ROC is
computed anywhere in this repository, on any dataset, for either model.** This is a fact,
not an oversight I'm inferring — a full-repo search found no evaluation script, no metrics
output, no validation split.

**Finding E1**
- Severity: Critical (for any claim of accuracy/reliability)
- Location: whole project (absence)
- Explanation: the only quantitative artifact (`plate_detections_with_distance.csv`) is an
  unlabelled log, and its one numeric column ("Confidence (%)") is a fabricated
  string-length formula, not measured OCR accuracy (see Finding H1 in §14) — so even that
  cannot stand in for an evaluation.
- Impact: it is currently **impossible to state, with any evidence, how often this system
  correctly identifies an ambulance, a pedestrian, or a motorcycle** in real conditions.
  Every behavioral claim in the presentation about detection is unvalidated.
- Recommendation: minimum viable evaluation — label ~100–200 frames per subsystem with
  ground truth, run the existing pipeline, compute precision/recall per class, and publish
  the numbers instead of (or alongside) the current confidence-formula log.

---

## 7. Overfitting / Underfitting

**N/A for the model weights** — no training occurred, so there is no loss curve, no
train/val gap, no gradient behavior to inspect.

There is, however, a direct analogue worth flagging: **every heuristic threshold in this
project is a hand-picked constant with no calibration record**, e.g. `V2P.py`'s
`SPEED_THRESHOLD_FAST=0.45`, `PROXIMITY_DANGER=0.60`, `FOCAL_PX=600.0`,
`REAL_HEIGHT_M={0:1.70,...}`; `distance.py`'s `FOCAL_LENGTH=700`, `KNOWN_PLATE_WIDTH=0.45`.
None of these have a derivation comment or calibration script behind them — they are, in
effect, "overfit" to whatever single camera/video the author was looking at when picking
numbers, and will silently mis-classify safe/dangerous or near/far on any different camera
resolution, mounting height, or lens. This is the practical, non-ML equivalent of poor
generalization, and it's real: change the camera and every one of these thresholds is
unvalidated again.

---

## 8. Export

**`model2.onnx`** (verified with the `onnx` package, see §5 for the dump):
- Valid, checker-passes ONNX graph, IR version 7.
- **Opset 12** — usable, but dated (current Ultralytics defaults to opset ≥17); not wrong,
  just means it misses newer ONNX Runtime graph-optimization passes available at later opsets.
- **Static shape** (`dynamic: False` in embedded export args) — batch fixed at 1, input
  fixed at 640×640. Fine for this single-frame-at-a-time Pi loop; means the model **cannot**
  be reused for a different resolution or for batched inference without re-exporting.
- **NMS not baked in** (`nms: False`) — consistent with, and required by, `V2P.py`'s manual
  `cv2.dnn.NMSBoxes` call. Not a bug, just means postprocessing correctness rests entirely
  on hand-written Python rather than a validated exported op.
- **No quantization at all** (`int8: False`, `half: False` in embedded metadata) — full FP32
  on a Raspberry Pi CPU, leaving a real, free performance win unclaimed (see §13).
- **No TensorRT artifact anywhere** (no `.engine` file, no `trtexec` script) — not
  attempted; plausible compatibility given static opset-12 ONNX, but unverified.

**`yolov8n.pt`**: never exported/converted at all — used directly via the Ultralytics
Python API, which means `distance.py` requires the full PyTorch + Ultralytics stack (far
heavier than `V2P.py`'s ONNX-Runtime-only path) for what is architecturally the same
detection task. Two different deployment strategies for the same model family, maintained
independently.

**EasyOCR**: no export/optimization applied at all; runs its bundled PyTorch model at full
precision, forced to CPU (`gpu=False`) — no ONNX export attempted despite this being, by a
wide margin, the slowest component in either pipeline.

---

## 9. Inference Pipeline

**Finding I1 — Ambulance filter only matches COCO class "car" (id 2)**
- Severity: **High**
- Location: `Traffic_Light/distance.py`, detection loop: `if int(box.cls[0]) == 2 and float(box.conf[0]) > 0.5:`
- Explanation: only detections YOLO classifies as COCO class 2 ("car") are ever passed to
  the plate-OCR/ambulance-matching step. COCO's "car" class is trained primarily on sedans
  and hatchbacks; larger box-bodied vehicles (which many real ambulances are) are commonly
  classified as "truck" (7) or "bus" (5) by general-purpose detectors, not "car".
- Impact: a real ambulance shaped like a van/box truck can be **completely invisible** to
  this pipeline — no plate read attempted, no `is_ambulance` flag ever set, meaning the
  ambulance-preemption logic built into `Traffic_light_GUI.py` and `Intelligent_Gateway.py`
  this session would never even be triggered for such a vehicle.
- Recommendation: `if int(box.cls[0]) in (2, 5, 7) and float(box.conf[0]) > 0.5:` (car, bus,
  truck), or better, fine-tune on local ambulance imagery (see D1/T1).

**Finding I2 — distance.py's pause-on-ambulance can starve the traffic light's own
ambulance-presence timeout**
- Severity: **High** (cross-file interaction, directly affects this session's feature)
- Location: `Traffic_Light/distance.py` (`pause_seconds = max(2.0, min(20.0, distance / SPEED_ASSUMED))`) interacting with `Traffic_Light/Traffic_light_GUI.py` (`AMBULANCE_PRESENCE_TIMEOUT = 5`)
- Explanation: once an ambulance is detected, `distance.py` stops running YOLO entirely
  (just writes raw frames) for up to **20 seconds** to "model" the ambulance driving past.
  Meanwhile `Traffic_light_GUI.py` expires its own `ambulance_present` flag after only
  **5 seconds** without a fresh detection message.
- Impact: for any pause longer than 5s, the traffic light will conclude the ambulance is
  gone and cancel the GREEN extension / stop honoring RED preemption **while the ambulance
  is still, per `distance.py`'s own model, physically approaching** — the two files'
  timing assumptions directly contradict each other.
- Recommendation: either have `distance.py` keep publishing (at a reduced rate) during the
  pause instead of going fully silent, or raise `AMBULANCE_PRESENCE_TIMEOUT` to comfortably
  exceed the maximum pause window (20s), with a comment cross-referencing why.

**Device/threshold notes (not bugs, but worth recording):**
- `distance.py` never pins a device for YOLO (`YOLO('yolov8n.pt')` auto-selects
  CUDA-if-available) while explicitly forcing EasyOCR to `gpu=False` — inconsistent device
  policy inside the same script; on a GPU machine, YOLO would accelerate but OCR would not.
- `V2P.py` explicitly and correctly pins `providers=["CPUExecutionProvider"]` — the more
  deliberate of the two.
- Both `distance.py`'s `> 0.5` check and `V2P.py`'s `CONF_THRESH=0.40` are plain hand-picked
  constants with no PR-curve or threshold sweep behind them (ties back to §6/§7).

---

## 10. Deployment Readiness

| Target | Verdict | Why |
| --- | --- | --- |
| Raspberry Pi | `V2P.py`: **yes, by design** | Uses `picamera2` + ONNX Runtime CPU provider, explicitly built for the Pi, systemd-managed. |
| Raspberry Pi | `distance.py`: **no** | Full PyTorch + Ultralytics + EasyOCR stack; README itself says this runs "on a separate machine," i.e. a laptop, not the Pi. |
| Jetson / GPU edge | Neither uses GPU intentionally (`V2P.py` pins CPU; `distance.py`'s YOLO device is unpinned/implicit) | No CUDA-specific path exists. |
| CPU deployment | `V2P.py`: workable at reduced FPS (`skip_frames=3`) | `distance.py`: workable on a PC, not a Pi. |

**Finding DEP1 — no dependency pinning anywhere in the repository**
- Severity: **High**
- Location: whole project (absence of `requirements.txt` / `environment.yml` — confirmed
  by a full-repo search; none exist)
- Explanation: a fresh clone has no record of which `opencv-python`, `ultralytics`,
  `easyocr`, `onnxruntime`, `picamera2`, or `paho-mqtt` versions this was built/tested
  against.
- Impact: classic "works on my machine" risk — a newer Ultralytics release changing its
  ONNX output layout, or an OpenCV API change, could silently break inference with no
  warning, and nobody could reproduce the exact working environment from this repo alone.
- Recommendation: `pip freeze > requirements.txt` per subsystem (`Traffic_Light/`,
  `RPI/V2P/`) at minimum for the exact versions currently known to work.

**Latency/RAM — estimates, not measurements** (no benchmark exists in-repo to cite instead):
YOLOv8n via ONNX Runtime on a Pi 4/5 CPU typically runs tens-of-ms per 640×640 frame;
EasyOCR CPU inference is commonly several hundred ms to over a second per crop — meaning
`distance.py`'s end-to-end per-detection latency (YOLO + OCR) is very plausibly
**seconds**, not milliseconds, which matters if it's ever asked to run in real time rather
than against a pre-recorded video.

---

## 11. Code Quality

**Finding CQ1 — Both AI scripts execute all setup (model load, camera open, network
connect) at module import time, not inside a guarded `main()`**
- Severity: Medium
- Location: `RPI/V2P/V2P.py` (camera + ONNX session load at top level), `Traffic_Light/distance.py` (MQTT connect + model load at top level)
- Explanation: there is no `if __name__ == "__main__":` guard around the real work in
  either file.
- Impact: neither file can be imported (e.g., for a unit test of `analyze_intent()` or
  `estimate_proximity()`) without also opening a physical camera / connecting to a live
  broker / loading a multi-MB model — makes the actual logic effectively untestable in
  isolation, which is very likely why no tests exist for either.
- Recommendation: wrap hardware/network/model setup in a `main()` and put pure functions
  (`analyze_intent`, `estimate_proximity`, `CentroidTracker`) somewhere importable without
  side effects.

**Finding CQ2 — Duplicated detection/distance pipeline between `distance.py` and `V2P.py`**
- Severity: Medium
- Location: both files
- Explanation: both independently re-implement "run YOLOv8n → estimate real-world distance
  from a pinhole-camera formula → threshold into risk levels," with different formulas,
  different constants, and no shared module.
- Impact: a fix or accuracy improvement made in one is invisible to the other; already
  visibly diverged (e.g., only one of the two forces EasyOCR onto CPU-only; only one does
  manual NMS).
- Recommendation: factor the shared parts (YOLO inference wrapper, pinhole-distance
  formula) into one small shared module imported by both.

**Positive note (deserves acknowledgment, not just criticism):** both subsystems have
genuinely useful top-level README files explaining the data flow — better documentation
than the median student project — the gap is entirely at the inline/magic-number level
(§7), not the architectural-overview level.

---

## 12. Security

**Finding SEC1 — Live MQTT broker credentials committed in plaintext, in three files**
- Severity: **Critical**
- Location: `Traffic_Light/Traffic_light_GUI.py`, `Traffic_Light/distance.py`, `Traffic_Light/Intelligent_Gateway.py` — all three hardcode `USERNAME = "v2n_admin"`, `PASSWORD = "V2n@2026!"`
- Explanation: these are real, working credentials to a live HiveMQ Cloud broker, committed
  to git and (per the repo's remote) pushed to GitHub.
- Impact: anyone with read access to this repository/history can connect to this broker,
  publish forged traffic-light or ambulance-detection packets, or simply flood/pollute it —
  directly relevant here since this exact channel now drives real-world-acting logic (the
  ambulance-preemption state machine built this session trusts messages on this broker).
- Recommendation: rotate the password immediately, move credentials to environment
  variables or a local untracked config file, and add that file to `.gitignore`.

**Finding SEC2 — Unsanitized console input controls file access (low real-world risk, worth noting)**
- Severity: Low
- Location: `Traffic_Light/distance.py`: `video_name = input("Enter your video file name: ").strip()` → `cv2.VideoCapture(video_name)`
- Explanation: direct, unsanitized user input controls which file is opened.
- Impact: low today (local interactive console prompt, not attacker-reachable), but this
  exact pattern becomes a path-traversal/arbitrary-file-read risk the moment this input
  source is ever swapped for a network parameter or API field — a common real-world
  regression path worth guarding against now rather than after the fact.

**Serialization:** `RPI/hub/ipc_node.py` uses `json.loads`/`json.dumps` exclusively — no
`pickle`/`eval`, no unsafe deserialization. Good.

**Supply chain note:** `torch.load` (used internally when `ultralytics.YOLO('yolov8n.pt')`
loads the `.pt` checkpoint) executes arbitrary pickle bytecode by design. The file here
matches the well-known public Ultralytics release structure, so risk in *this* repo is low,
but it's worth documenting that `.pt` files are inherently a code-execution trust boundary
if ever sourced from anywhere less trusted than Ultralytics' own release.

---

## 13. Performance

Concrete, specific to what's actually here (not generic advice):

1. **Bake NMS into the ONNX export** (`nms=True` at Ultralytics export time) — removes the
   Python-side `cv2.dnn.NMSBoxes` loop from every processed frame on the Pi's CPU.
2. **Quantize `model2.onnx`** (`onnxruntime.quantization`, dynamic or static INT8) — the
   model's own embedded metadata confirms it was exported FP32 with no quantization
   (`int8: False`); this is a real, currently-unclaimed CPU latency win on a Raspberry Pi.
3. **Fix the letterboxing bug (§3, P1)** — beyond the accuracy fix, correct letterboxing
   also avoids wasting model capacity on a distorted image.
4. **Replace or export EasyOCR** — it is, by a wide margin, the heaviest, slowest step in
   `distance.py`; a smaller purpose-built plate-OCR model or an ONNX-exported recognizer
   would materially cut per-detection latency.
5. **Fix the infinite video-write growth in `distance.py`** (`out.write(frame)` runs
   forever across an infinitely-looping video with no size/time cap) — a real disk-fill
   risk during any long-running or unattended demo (see §14, Finding H2).
6. **`V2P.py`'s `skip_frames=3` is a blunt, fixed throttle** — adaptive throttling (detect
   less when nothing is near the crossing zone, more when something is) would give a better
   safety/latency trade-off than a flat ratio.

**Acknowledged good practice:** `distance.py` already gates the expensive EasyOCR call
behind a cheap YOLO detection (only OCRs when a car-class box is found) — correctly
ordering cheap-before-expensive, worth keeping.

---

## 14. Hidden Bugs

**Finding H1 — Real OCR confidence is computed, then thrown away, in favor of a fabricated formula**
- Severity: **High**
- Location: `Traffic_Light/distance.py`: `for (bbox, text, conf) in ocr_results:` ... `confidence = min(95, 60 + len(cleaned) * 5)`
- Explanation: EasyOCR's real per-read confidence is already unpacked into `conf` on the
  same line — and then never used again. A fake "confidence" is recomputed purely from the
  cleaned string's *length* (a 4-character read always scores 80%, regardless of whether
  EasyOCR was actually 30% or 99% sure).
- Impact: the "Confidence (%)" column in `plate_detections_with_distance.csv` — the one
  quantitative artifact in the whole project — measures string length, not detection
  quality. It cannot be used to judge OCR reliability at all.
- Recommendation: one-line fix — `confidence = round(conf * 100, 1)`.

**Finding H2 — Output video grows forever; the source video also loops forever**
- Severity: Medium
- Location: `Traffic_Light/distance.py` main loop: `while running:` restarts the video from frame 0 on EOF; `out.write(frame)` runs on every iteration with no cap.
- Explanation: there is no maximum runtime, frame count, or file-size limit.
- Impact: left running unattended (e.g., an overnight demo), `processed_output.mp4` grows
  without bound until disk space is exhausted.
- Recommendation: cap total frames written, or stop writing after the first full pass and
  keep only detecting/publishing on subsequent loops.

**Finding H3 — `CentroidTracker.history` leaks memory for the lifetime of the process**
- Severity: **High** (this runs as an always-on systemd service)
- Location: `RPI/V2P/V2P.py`, `CentroidTracker.deregister()`
- Explanation: `deregister()` pops `self.objects`, `self.objects_bbox`, and
  `self.disappeared` for a lost object ID, but never pops `self.history[obj_id]` — and
  `self.history` is a `defaultdict`, so every *new* object ID ever seen (IDs are
  monotonically increasing, never reused) leaves a `deque(maxlen=30)` behind forever.
- Impact: on a long-running Pi service (this is explicitly systemd-managed, meant to run
  continuously), memory grows without bound in proportion to the total number of distinct
  objects ever tracked, not the number currently visible.
- Recommendation: `self.history.pop(obj_id, None)` inside `deregister()`.

**Finding H4 — Fragile centroid-equality re-matching after tracking update**
- Severity: Medium
- Location: `RPI/V2P/V2P.py`, `CentroidTracker.update()`, final `result` rebuild: `if ox == cx and oy == cy:`
- Explanation: after correctly re-associating objects via IoU+distance matching earlier in
  the same function, the method re-derives the id↔detection mapping a second time by exact
  integer centroid equality. Two adjacent/overlapping detections that happen to round to the
  same integer centroid — or any future floating-point path divergence — will silently drop
  that object from the returned `result` for that frame, with no error.
- Recommendation: build `result` directly from the `(obj_id, col)` pairs already known from
  the row/col assignment loop above, instead of re-deriving the association afterward.

**Finding H5 — `ipc_node.py`'s `publish()` has no lock around the socket write**
- Severity: Medium (latent — not currently triggered by `V2P.py`'s own call pattern, but a
  real risk in the shared library every IPC-connected process depends on)
- Location: `RPI/hub/ipc_node.py`, `IPCNode.publish()` → `self._send_raw(...)` → `self.sock.sendall(...)`
- Explanation: no lock protects concurrent calls to `sendall()` on the same socket. `V2P.py`
  itself only ever calls `publish()` from its single main thread, so it isn't triggered
  today — but the library offers no protection if any node (present or future) ever
  publishes from more than one thread (e.g., a callback thread and a main loop both
  publishing).
- Recommendation: add a `threading.Lock()` around `_send_raw()`.

**Finding H6 — `ipc_node.py._recv_one()` silently drops bytes after the first newline**
- Severity: Low-Medium
- Location: `RPI/hub/ipc_node.py`, `_recv_one()`: `return json.loads(buf.split(b"\n")[0])`
- Explanation: used only during the synchronous connect/subscribe handshake. If the OS
  delivers more than one line in a single `recv()` chunk (e.g., an ack immediately followed
  by a fast pub/sub message), everything after the first `\n` is discarded — that
  frame is gone, since the persistent read loop (`_recv_loop`) hasn't started yet.
- Recommendation: buffer and re-inject any bytes past the first newline into the object's
  persistent receive buffer before starting `_recv_loop`, instead of discarding them.

**Doc/code mismatch found and resolved for the record (not a current bug):**
`git log` shows `RPI/V2P/model2.onnx` really was a 0-byte placeholder as of commit
`1db13a4` (2026-07-02), matching the old warning in `RPI/V2P/README.md` and the other
branch's `CODE_REVIEW.md`. Commit `dc66114` ("fixed model ,size frame", 2026-07-03)
replaced it with a real, valid 12.85 MB Ultralytics YOLOv8n-on-COCO ONNX export (confirmed
structurally valid via `onnx.checker`) — but `RPI/V2P/README.md`'s "Known issue" section
was never updated and still describes the file as a 0-byte placeholder. **Recommend
deleting that stale "Known issue" section** — it no longer describes reality and will
mislead anyone reading the README today.

---

## 15. Production Readiness

**Not production-ready**, for concrete, fixable reasons — not because the underlying model
choice (YOLOv8n) is bad:

1. Zero evaluation ever performed against the actual target conditions (§2, §6).
2. Live broker credentials committed in plaintext, in three files (§12, SEC1).
3. An unbounded memory leak in a service explicitly designed to run continuously (§14, H3).
4. A confirmed image-distortion bug feeding the model data unlike what it was trained on
   (§3, P1), plus an unverified but well-known color-channel risk (§3, P2).
5. The core "detect an ambulance" filter only matches one COCO class, plausibly missing
   real ambulances shaped like vans/trucks (§9, I1).
6. No dependency pinning anywhere, so the working environment isn't reproducible (§10, DEP1).

None of these require redesigning the architecture — they're all concrete, itemized fixes.
That's also why this is not a "rewrite it" verdict: it's a "fix these specific things
before calling it done" verdict.

---

## Final Report — Scores (/10)

| Category | Score | Why |
| --- | --- | --- |
| Overall Architecture | 5 | Sound MQTT/IPC systems design; weakened by two divergent, unshared detection pipelines that duplicate the same logic (§1, §11). |
| Dataset | 1 | None exists for this project's actual task; score reflects that the underlying pretrained weights at least come from a real, well-curated public dataset (COCO). |
| Training | 0 | No training pipeline exists anywhere in the repo. |
| Evaluation | 1 | No formal metric computed anywhere; the one numeric log column is fabricated (§6, §14 H1). |
| Inference | 5 | Functionally works and mostly matches each model's expected input, but has a confirmed image-distortion bug (P1) and a real class-coverage gap for the core ambulance use case (I1). |
| Performance | 4 | No quantization, no baked-in NMS, heaviest component (EasyOCR) unoptimized, unbounded disk growth in one pipeline. |
| Deployment | 4 | V2P.py reasonably fits its Pi target; distance.py explicitly does not (by its own README); zero dependency pinning blocks reproducibility everywhere. |
| Code Quality | 5 | Good top-level READMEs; undermined by untestable module-level side effects and duplicated logic. |
| Production Readiness | 2 | Live credentials in git, an unbounded leak in an always-on service, zero validation against real conditions. |
| Maintainability | 4 | Decent docs at the README level; magic numbers and duplication hurt anyone changing this later. |

---

## Final Verdict

- **Is the model trained correctly?** N/A — no model was trained in this project; both
  are unmodified pretrained checkpoints used as-is for inference.
- **Are there training mistakes?** N/A for the same reason. The closest analogue —
  hand-picked heuristic thresholds with no calibration record — is a real generalization
  risk (§7).
- **Is there evidence of data leakage?** No — and there can't be, because there is no
  training/validation split in this repository to leak between.
- **Is preprocessing correct?** Partially. `distance.py`'s YOLO path is correct (relies on
  Ultralytics' own internal handling). `V2P.py` has one **confirmed** preprocessing bug
  (naive stretch-resize instead of letterboxing, Finding P1) and one **unverified but
  credible** one (picamera2 RGB/BGR channel order, Finding P2).
- **Does inference match training?** As well as it can, given neither model was trained
  here — inference matches each model's *documented* expected input, except for the two
  issues above.
- **Is the exported model valid?** Yes — `model2.onnx` passes `onnx.checker.check_model()`
  and its tensor shapes/op graph are consistent with how `V2P.py` actually parses its
  output. It is a legitimate, if unmodified, YOLOv8n-COCO export.
- **Would I approve this project for production?** **No**, in its current state — but the
  reasons are a short, concrete, fixable list (§15), not a fundamental architecture problem.
- **Top 10 improvements, ranked by impact:**
  1. Rotate the leaked MQTT credentials and stop committing secrets (SEC1) — the single
     highest-severity, easiest-to-exploit issue in the repo.
  2. Fix the `CentroidTracker.history` memory leak (H3) — this runs as an always-on service.
  3. Fix the letterboxing/aspect-ratio bug in `V2P.py` (P1) — directly degrades the
     accuracy of the pedestrian/motorcycle safety feature.
  4. Verify (and fix if needed) the picamera2 RGB/BGR channel order on real hardware (P2).
  5. Widen the ambulance vehicle-class filter beyond COCO "car" (I1) — otherwise the
     preemption feature built this session can simply never trigger for some real ambulances.
  6. Reconcile `distance.py`'s pause duration with `Traffic_light_GUI.py`'s presence
     timeout (I2) — currently contradictory by design.
  7. Stop discarding EasyOCR's real confidence score in favor of a fake formula (H1) — free
     fix, restores the only quantitative signal in the project.
  8. Add `requirements.txt`/pinned dependencies for both subsystems (DEP1) — currently
     unreproducible from a fresh clone.
  9. Build even a minimal labeled evaluation set and report real precision/recall (D1, E1)
     — every accuracy claim about this project is currently unverifiable.
  10. Cap `distance.py`'s output-video growth and looping (H2) — real disk-fill risk on any
      unattended run.
