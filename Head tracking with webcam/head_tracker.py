import cv2
import mediapipe as mp
from mediapipe.tasks import python as mp_tasks
from mediapipe.tasks.python.vision import FaceLandmarker, FaceLandmarkerOptions, RunningMode
import numpy as np
from flask import Flask, jsonify
import threading
import time
import urllib.request
import os
import math
import json
import tkinter as tk
from tkinter import ttk
from PIL import Image, ImageTk

app = Flask(__name__)

MODEL_PATH = "face_landmarker.task"
MODEL_URL = "https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/1/face_landmarker.task"

MODEL_POINTS = np.array([
    (  0.0,    0.0,    0.0),  # Nose tip           4
    (  0.0, -330.0,  -65.0),  # Chin               152
    (-225.0, 170.0, -135.0),  # Left eye outer     33
    ( 225.0, 170.0, -135.0),  # Right eye outer    263
    (-150.0,-150.0, -125.0),  # Left mouth corner  61
    ( 150.0,-150.0, -125.0),  # Right mouth corner 291
    (-110.0, 170.0, -105.0),  # Left eye inner     133
    ( 110.0, 170.0, -105.0),  # Right eye inner    362
    (   0.0, 250.0, -115.0),  # Nose bridge top    168
    (-250.0,  30.0, -175.0),  # Left cheek         234
    ( 250.0,  30.0, -175.0),  # Right cheek        454
], dtype=np.float64)

LANDMARK_IDS = [4, 152, 33, 263, 61, 291, 133, 362, 168, 234, 454]

shared = {"raw_yaw": 0.0, "raw_pitch": 0.0, "raw_roll": 0.0,
          "yaw": 0.0, "pitch": 0.0, "roll": 0.0, "active": False,
          "enabled": True}
neutral = {"yaw": 0.0, "pitch": 0.0, "roll": 0.0}
lock = threading.Lock()
last_face_time = 0.0
FACE_TIMEOUT = 1.0

ONE_EURO_MIN_CUTOFF = 0.8
ONE_EURO_BETA       = 0.15


class OneEuro:
    def __init__(self, min_cutoff, beta, d_cutoff=1.0):
        self.min_cutoff = min_cutoff
        self.beta = beta
        self.d_cutoff = d_cutoff
        self.x_prev = 0.0
        self.dx_prev = 0.0
        self.t_prev = None

    @staticmethod
    def _alpha(dt, cutoff):
        tau = 1.0 / (2.0 * math.pi * cutoff)
        return 1.0 / (1.0 + tau / dt)

    def __call__(self, x, t):
        if self.t_prev is None:
            self.t_prev = t
            self.x_prev = x
            return x
        dt = t - self.t_prev
        if dt <= 0:
            return self.x_prev
        dx = (x - self.x_prev) / dt
        ad = self._alpha(dt, self.d_cutoff)
        dx_hat = ad * dx + (1 - ad) * self.dx_prev
        cutoff = self.min_cutoff + self.beta * abs(dx_hat)
        a = self._alpha(dt, cutoff)
        x_hat = a * x + (1 - a) * self.x_prev
        self.t_prev = t
        self.x_prev = x_hat
        self.dx_prev = dx_hat
        return x_hat


yaw_filter   = OneEuro(ONE_EURO_MIN_CUTOFF, ONE_EURO_BETA)
pitch_filter = OneEuro(ONE_EURO_MIN_CUTOFF, ONE_EURO_BETA)
roll_filter  = OneEuro(ONE_EURO_MIN_CUTOFF, ONE_EURO_BETA)

INVERT_YAW   = True
INVERT_PITCH = False
INVERT_ROLL  = False
SWAP_AXES    = False

SETTINGS_PATH = "head_tracker_settings.json"
settings = {
    "yaw_gain":   1.2,
    "pitch_gain": 1.2,
    "roll_gain":  1.0,
    "max_yaw":    80.0,
    "max_pitch":  60.0,
    "max_roll":   45.0,
    "deadzone":   0.0,
}


def load_settings():
    if not os.path.exists(SETTINGS_PATH):
        return
    try:
        with open(SETTINGS_PATH, "r") as f:
            data = json.load(f)
        for k in settings:
            if k in data:
                settings[k] = float(data[k])
    except Exception as e:
        print(f"Failed to load settings: {e}")


def save_settings():
    try:
        with open(SETTINGS_PATH, "w") as f:
            json.dump(settings, f, indent=2)
    except Exception as e:
        print(f"Failed to save settings: {e}")


load_settings()

AUTO_RECENTER_ENABLED   = True
AUTO_RECENTER_THRESHOLD = 3.0
AUTO_RECENTER_DELAY     = 2.0
AUTO_RECENTER_RATE      = 0.5

_recenter_still_since = None
_has_calibrated = False

latest_frame_rgb = None
frame_lock = threading.Lock()


def wrap180(d):
    return (d + 180.0) % 360.0 - 180.0


DEBUG_LOG_PATH = "head_tracker_debug.log"
debug_file = open(DEBUG_LOG_PATH, "w", buffering=1)


def dlog(msg):
    debug_file.write(f"{time.time():.3f} {msg}\n")


def do_calibrate():
    global _has_calibrated
    with lock:
        neutral["yaw"]   = shared["raw_yaw"]
        neutral["pitch"] = shared["raw_pitch"]
        neutral["roll"]  = shared["raw_roll"]
    _has_calibrated = True


def compute_pose(landmarks, w, h):
    pts_2d = np.array(
        [(landmarks[i].x * w, landmarks[i].y * h) for i in LANDMARK_IDS],
        dtype=np.float64
    )
    focal = w
    cam_matrix = np.array([[focal, 0, w / 2],
                            [0, focal, h / 2],
                            [0, 0,     1    ]], dtype=np.float64)
    dist_coeffs = np.zeros((4, 1))
    ok, rvec, _ = cv2.solvePnP(MODEL_POINTS, pts_2d, cam_matrix, dist_coeffs,
                                flags=cv2.SOLVEPNP_SQPNP)
    if not ok:
        return None, None, None
    rmat, _ = cv2.Rodrigues(rvec)
    fz = rmat @ np.array([0.0, 0.0, 1.0])
    yaw   = float(np.degrees(np.arctan2(fz[0], -fz[2])))
    pitch = float(np.degrees(np.arctan2(-fz[1], np.sqrt(fz[0]**2 + fz[2]**2))))
    rx = rmat @ np.array([1.0, 0.0, 0.0])
    roll = float(np.degrees(np.arctan2(-rx[1], rx[0])))
    if SWAP_AXES:
        yaw, pitch = pitch, yaw
    if INVERT_YAW:
        yaw = -yaw
    if INVERT_PITCH:
        pitch = -pitch
    if INVERT_ROLL:
        roll = -roll
    return yaw, pitch, roll


def tracking_loop():
    global last_face_time, _recenter_still_since, latest_frame_rgb

    if not os.path.exists(MODEL_PATH):
        print("Downloading face_landmarker.task (~29 MB)...")
        urllib.request.urlretrieve(MODEL_URL, MODEL_PATH)
        print("Download complete.")

    options = FaceLandmarkerOptions(
        base_options=mp_tasks.BaseOptions(model_asset_path=MODEL_PATH),
        running_mode=RunningMode.VIDEO,
        num_faces=1
    )

    cap = None
    for idx in range(10):
        c = cv2.VideoCapture(idx)
        ret, _ = c.read()
        if ret:
            cap = c
            print(f"Opened camera index {idx}")
            break
        c.release()
    if cap is None:
        print("ERROR: No webcam found on indices 0-9. Check Windows camera privacy settings.")
        return

    with FaceLandmarker.create_from_options(options) as landmarker:
        while cap.isOpened():
            ok, frame = cap.read()
            if not ok:
                continue
            h, w = frame.shape[:2]
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
            timestamp_ms = int(time.time() * 1000)
            result = landmarker.detect_for_video(mp_image, timestamp_ms)

            with lock:
                is_enabled = shared["enabled"]
            if not is_enabled:
                with lock:
                    shared["active"] = False
                with frame_lock:
                    latest_frame_rgb = rgb
                continue

            if result.face_landmarks:
                landmarks = result.face_landmarks[0]
                yaw, pitch, roll = compute_pose(landmarks, w, h)
                if yaw is not None and pitch is not None and roll is not None:
                    MAX_FRAME_JUMP = 35.0
                    dy = abs(wrap180(yaw   - shared["raw_yaw"]))
                    dp = abs(wrap180(pitch - shared["raw_pitch"]))
                    dr = abs(wrap180(roll  - shared["raw_roll"]))
                    if dy > MAX_FRAME_JUMP or dp > MAX_FRAME_JUMP or dr > MAX_FRAME_JUMP:
                        dlog(f"REJECT jump dy={dy:.1f} dp={dp:.1f} dr={dr:.1f}")
                    else:
                        now = time.time()
                        fy = yaw_filter(yaw,     now)
                        fp = pitch_filter(pitch, now)
                        fr = roll_filter(roll,   now)
                        with lock:
                            shared["raw_yaw"]   = fy
                            shared["raw_pitch"] = fp
                            shared["raw_roll"]  = fr
                            ry = wrap180(fy - neutral["yaw"])   * settings["yaw_gain"]
                            rp = wrap180(fp - neutral["pitch"]) * settings["pitch_gain"]
                            rr = wrap180(fr - neutral["roll"])  * settings["roll_gain"]
                            dz = settings["deadzone"]
                            if abs(ry) < dz: ry = 0.0
                            if abs(rp) < dz: rp = 0.0
                            if abs(rr) < dz: rr = 0.0
                            shared["yaw"]   = max(-settings["max_yaw"],   min(settings["max_yaw"],   ry))
                            shared["pitch"] = max(-settings["max_pitch"], min(settings["max_pitch"], rp))
                            shared["roll"]  = max(-settings["max_roll"],  min(settings["max_roll"],  rr))
                            shared["active"] = True

                            if AUTO_RECENTER_ENABLED:
                                within = (abs(shared["yaw"])   < AUTO_RECENTER_THRESHOLD and
                                          abs(shared["pitch"]) < AUTO_RECENTER_THRESHOLD and
                                          abs(shared["roll"])  < AUTO_RECENTER_THRESHOLD)
                                if within:
                                    if _recenter_still_since is None:
                                        _recenter_still_since = now
                                    elif now - _recenter_still_since > AUTO_RECENTER_DELAY:
                                        dt_rc = min(now - last_face_time, 0.1) if last_face_time else 0.05
                                        k = 1.0 - math.exp(-AUTO_RECENTER_RATE * dt_rc)
                                        neutral["yaw"]   += (fy - neutral["yaw"])   * k
                                        neutral["pitch"] += (fp - neutral["pitch"]) * k
                                        neutral["roll"]  += (fr - neutral["roll"])  * k
                                else:
                                    _recenter_still_since = None
                        last_face_time = now
                    dlog(f"POSE yaw={shared['yaw']:+.2f} pitch={shared['pitch']:+.2f} roll={shared['roll']:+.2f}")

                for i in LANDMARK_IDS:
                    lm = landmarks[i]
                    if lm.x is not None and lm.y is not None:
                        cv2.circle(rgb, (int(lm.x * w), int(lm.y * h)), 4, (0, 255, 0), -1)
            else:
                if time.time() - last_face_time > FACE_TIMEOUT:
                    with lock:
                        shared["active"] = False

            with frame_lock:
                latest_frame_rgb = rgb
    cap.release()


@app.route("/pose")
def get_pose():
    with lock:
        payload = {"yaw": shared["yaw"], "pitch": shared["pitch"],
                   "roll": shared["roll"], "active": shared["active"]}
    dlog(f"GET /pose -> yaw={payload['yaw']:+.2f} pitch={payload['pitch']:+.2f} roll={payload['roll']:+.2f} active={payload['active']}")
    return jsonify(payload)


@app.route("/calibrate", methods=["POST"])
def calibrate():
    do_calibrate()
    return "", 204


@app.route("/start", methods=["POST"])
def start_tracking():
    with lock:
        shared["enabled"] = True
    return "", 204


@app.route("/stop", methods=["POST"])
def stop_tracking():
    with lock:
        shared["enabled"] = False
    return "", 204


@app.route("/toggle", methods=["POST"])
def toggle_tracking():
    with lock:
        shared["enabled"] = not shared["enabled"]
        new_state = shared["enabled"]
    return jsonify({"enabled": new_state})


# ---------- Tkinter UI ----------

class HeadTrackerUI:
    VIDEO_W = 640
    VIDEO_H = 480

    def __init__(self, root):
        self.root = root
        root.title("Head Tracker")
        root.geometry("1100x720")
        root.configure(bg="#1e1e1e")

        style = ttk.Style()
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass
        style.configure("TFrame", background="#1e1e1e")
        style.configure("TLabel", background="#1e1e1e", foreground="#e6e6e6", font=("Segoe UI", 10))
        style.configure("Title.TLabel", font=("Segoe UI", 14, "bold"), foreground="#ffffff")
        style.configure("Warn.TLabel",  font=("Segoe UI", 13, "bold"), foreground="#ffcc33", background="#3a2a00")
        style.configure("OK.TLabel",    font=("Segoe UI", 10, "bold"), foreground="#66dd66")
        style.configure("Bad.TLabel",   font=("Segoe UI", 10, "bold"), foreground="#dd6666")
        style.configure("Big.TButton",  font=("Segoe UI", 12, "bold"), padding=10)
        style.configure("TCheckbutton", background="#1e1e1e", foreground="#e6e6e6", font=("Segoe UI", 10))
        style.configure("Horizontal.TScale", background="#1e1e1e")

        root.columnconfigure(0, weight=0)
        root.columnconfigure(1, weight=1)
        root.rowconfigure(0, weight=1)

        # Left: video
        left = ttk.Frame(root, padding=10)
        left.grid(row=0, column=0, sticky="nsew")
        self.video_label = tk.Label(left, width=self.VIDEO_W, height=self.VIDEO_H,
                                    bg="#000000", fg="#888888",
                                    text="Waiting for camera...",
                                    font=("Segoe UI", 12))
        self.video_label.pack()

        self.pose_label = ttk.Label(left, text="Yaw: +0.0   Pitch: +0.0   Roll: +0.0",
                                    style="Title.TLabel")
        self.pose_label.pack(pady=(10, 4))

        self.status_label = ttk.Label(left, text="Face: --", style="Bad.TLabel")
        self.status_label.pack()

        # Right: controls
        right = ttk.Frame(root, padding=14)
        right.grid(row=0, column=1, sticky="nsew")
        right.columnconfigure(0, weight=1)

        self.warn_label = ttk.Label(right,
            text="  Look straight ahead, then press  Calibrate  ",
            style="Warn.TLabel", anchor="center")
        self.warn_label.grid(row=0, column=0, sticky="ew", pady=(0, 10), ipady=8)

        calib_btn = ttk.Button(right, text="Calibrate (set neutral)",
                                style="Big.TButton", command=self.on_calibrate)
        calib_btn.grid(row=1, column=0, sticky="ew", pady=(0, 6))

        self.enabled_var = tk.BooleanVar(value=True)
        self.toggle_btn = ttk.Checkbutton(right, text="Tracking enabled (sending pose to game)",
                                            variable=self.enabled_var, command=self.on_toggle)
        self.toggle_btn.grid(row=2, column=0, sticky="w", pady=(0, 14))

        sens = ttk.LabelFrame(right, text="Sensitivity", padding=10)
        sens.grid(row=3, column=0, sticky="ew", pady=(0, 10))
        sens.columnconfigure(1, weight=1)
        self._add_slider(sens, 0, "Look Left / Right",      "yaw_gain",   0.0, 10.0, "{:.2f}x")
        self._add_slider(sens, 1, "Look Up / Down",         "pitch_gain", 0.0, 10.0, "{:.2f}x")
        self._add_slider(sens, 2, "Head Tilt (roll)",       "roll_gain",  0.0,  5.0, "{:.2f}x")

        limits = ttk.LabelFrame(right, text="Maximum Turn Angle", padding=10)
        limits.grid(row=4, column=0, sticky="ew", pady=(0, 10))
        limits.columnconfigure(1, weight=1)
        self._add_slider(limits, 0, "Left / Right",   "max_yaw",   0.0, 120.0, "{:.0f}\u00b0")
        self._add_slider(limits, 1, "Up / Down",      "max_pitch", 0.0,  90.0, "{:.0f}\u00b0")
        self._add_slider(limits, 2, "Tilt",           "max_roll",  0.0,  90.0, "{:.0f}\u00b0")

        misc = ttk.LabelFrame(right, text="Deadzone (ignore tiny movements)", padding=10)
        misc.grid(row=5, column=0, sticky="ew", pady=(0, 10))
        misc.columnconfigure(1, weight=1)
        self._add_slider(misc, 0, "Deadzone",         "deadzone",  0.0, 5.0, "{:.1f}\u00b0")

        hint = ttk.Label(right,
            text="Game hotkeys: T = toggle apply   Z = recenter   Y = start/stop camera",
            foreground="#888888")
        hint.grid(row=6, column=0, sticky="w", pady=(6, 0))

        root.protocol("WM_DELETE_WINDOW", self.on_close)
        root.bind("<Key-c>", lambda _e: self.on_calibrate())
        root.bind("<Key-C>", lambda _e: self.on_calibrate())

        self._photo = None
        self._tick()

    def _add_slider(self, parent, row, label, key, vmin, vmax, value_fmt):
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky="w", padx=(0, 10), pady=3)
        var = tk.DoubleVar(value=settings[key])
        val_lbl = ttk.Label(parent, text=value_fmt.format(settings[key]), width=8, anchor="e")
        val_lbl.grid(row=row, column=2, sticky="e", padx=(8, 0))

        def on_change(v, k=key, lbl=val_lbl, fmt=value_fmt):
            f = float(v)
            settings[k] = f
            lbl.configure(text=fmt.format(f))
            save_settings()

        scale = ttk.Scale(parent, from_=vmin, to=vmax, orient="horizontal",
                            variable=var, command=on_change)
        scale.grid(row=row, column=1, sticky="ew")

    def on_calibrate(self):
        do_calibrate()
        self.warn_label.configure(text="  Calibrated  \u2713  ", style="OK.TLabel")

    def on_toggle(self):
        with lock:
            shared["enabled"] = bool(self.enabled_var.get())

    def on_close(self):
        self.root.destroy()
        os._exit(0)

    def _tick(self):
        # update pose readout + status
        with lock:
            yaw_v   = shared["yaw"]
            pitch_v = shared["pitch"]
            roll_v  = shared["roll"]
            active  = shared["active"]
            enabled = shared["enabled"]
        self.pose_label.configure(
            text=f"Yaw: {yaw_v:+6.1f}   Pitch: {pitch_v:+6.1f}   Roll: {roll_v:+6.1f}")
        if active:
            self.status_label.configure(text="Face: tracked", style="OK.TLabel")
        else:
            self.status_label.configure(text="Face: not detected", style="Bad.TLabel")
        if self.enabled_var.get() != enabled:
            self.enabled_var.set(enabled)
        if _has_calibrated:
            if "Calibrated" not in self.warn_label.cget("text"):
                self.warn_label.configure(text="  Calibrated  \u2713  ", style="OK.TLabel")

        # update video frame
        with frame_lock:
            frame = latest_frame_rgb
        if frame is not None:
            h, w = frame.shape[:2]
            scale = min(self.VIDEO_W / w, self.VIDEO_H / h)
            nw, nh = int(w * scale), int(h * scale)
            resized = cv2.resize(frame, (nw, nh))
            img = Image.fromarray(resized)
            self._photo = ImageTk.PhotoImage(img)
            self.video_label.configure(image=self._photo, text="",
                                        width=nw, height=nh)

        self.root.after(33, self._tick)  # ~30 fps


def run_ui():
    root = tk.Tk()
    HeadTrackerUI(root)
    root.mainloop()


if __name__ == "__main__":
    tracker_thread = threading.Thread(target=tracking_loop, daemon=True)
    tracker_thread.start()
    flask_thread = threading.Thread(
        target=lambda: app.run(host="localhost", port=7837, debug=False, use_reloader=False),
        daemon=True,
    )
    flask_thread.start()
    run_ui()
