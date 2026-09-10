"""
Train Dashboard - Windows GUI (Tkinter) + built-in bridge listener

One program that both:
  (a) reads JSON lines from the ESP32 bridge over serial/Bluetooth SPP and
      republishes them to the MQTT broker (the old bridge_listener.py's job)
  (b) subscribes to that same broker and displays live status:
        1. MQTT connection status (green = connected, red = down)
        2. Live device list: bridge, Ramzor (traffic light), train_ctrl_c3
        3. Ramzor traffic light state (R/Y/G), drawn as an actual light
        4. Train control's latest direction/speed/location

A "Show Listener" button reveals a log panel with the same timestamped
lines the old console listener printed. The last 100 lines are also kept
in bridge_listener.log (overwritten each update, not appended forever).

Setup:
  pip install pyserial paho-mqtt
  python train_dashboard.py
"""

import json
import queue
import threading
import time
from collections import deque
from datetime import datetime
import tkinter as tk
from tkinter import ttk, scrolledtext

import serial
import paho.mqtt.client as mqtt

# ---------------- Serial port (bridge connection) ----------------
PORT = "COM11"   # <-- Fallback only if AUTO_DETECT is False or finds nothing.
AUTO_DETECT = True   # <-- Currently on the classic ESP32 (generic CP2102/CH340 chip,
                      #     no unique identity) - matching is by known chip VID:PID
                      #     first, which is reliable even with several ports present.
BAUD = 115200

# "Ramzor_Bridge" / "RAMZOR_BRIDGE_01" would be the custom product name and
# serial number set in firmware on a future native-USB (S2/S3) board - not
# applicable to the current classic ESP32, kept here for when that upgrade happens.
BRIDGE_IDENTITY_HINTS = ["Ramzor_Bridge", "RAMZOR_BRIDGE_01"]

# Known USB-serial chip VID:PID pairs (far more reliable than description
# text, which can vary by driver/Windows version): CP2102 (Silicon Labs),
# CH340/CH341, CH9102.
KNOWN_CHIP_VIDPID = ["10C4:EA60", "1A86:7523", "1A86:55D4"]
USB_SERIAL_HINTS = ["CP210", "CH340", "CH9102", "USB-SERIAL", "Silicon Labs"]
# No longer matching Bluetooth ports - this project is wired-USB only now
# (see chat history for why Bluetooth Classic SPP was dropped). Stale BT
# ports from that earlier phase can still show up in the port list and were
# incorrectly matching here before this was removed.

IDLE_RECONNECT_SECONDS = 40
RECONNECT_RETRY_SECONDS = 5
POST_CLOSE_PAUSE_SECONDS = 3

# ---------------- MQTT ----------------
BROKER_HOST = "localhost"
BROKER_PORT = 1883
EVENTS_TOPIC = "ramzor/bridge2/events"
STATUS_TOPIC = "ramzor/bridge2/status"
BRIDGE_HEARTBEAT_INTERVAL = 5

# ---------------- Device identification ----------------
DEVICE_SLOTS = [
    {"id": "bridge", "label": "Bridge", "match": lambda d: d == "bridge"},
    {"id": "ramzor", "label": "Ramzor (Traffic Light)",
     "match": lambda d: "traffic" in d.lower() or "ramzor" in d.lower()},
    {"id": "train_ctrl", "label": "Train Control",
     "match": lambda d: "train" in d.lower()},
]

# Devices heartbeat every 20s (see HEARTBEAT_INTERVAL_MS in
# projects/shared/EspNowTiming.h - keep these two in sync by hand, this
# file has no way to include that header directly).
#
# Three-tier liveness, evaluated from this dashboard's own wall-clock gap
# since each device's last message (NOT any device-reported time):
#   age < DEVICE_MAYBE_SECONDS               -> alive
#   DEVICE_MAYBE_SECONDS <= age < DEVICE_OFFLINE_SECONDS -> maybe
#   age >= DEVICE_OFFLINE_SECONDS             -> not connected
DEVICE_MAYBE_SECONDS = 30
DEVICE_OFFLINE_SECONDS = 60

# The bridge isn't an ESP-NOW device sending REASON_HEARTBEAT - its
# "liveness" here just means the listener thread is still reading fresh
# bytes off the serial port. It already gets a status ping every
# BRIDGE_HEARTBEAT_INTERVAL (5s), so a tighter pair of thresholds is fine.
BRIDGE_MAYBE_SECONDS = 10
BRIDGE_OFFLINE_SECONDS = 20

TRAFFIC_LIGHT_COLORS = {"R": "red", "Y": "#e0a800", "G": "green"}

# ---------------- Log panel / file ----------------
LOG_MAX_LINES = 100
LOG_FILE_PATH = "bridge_listener.log"

# ---------------- Shared state across threads ----------------
event_queue: "queue.Queue[dict]" = queue.Queue()
log_queue: "queue.Queue[str]" = queue.Queue()
port_status_queue: "queue.Queue[str]" = queue.Queue()   # listener -> GUI: "connected to COMx"
port_override_queue: "queue.Queue[str]" = queue.Queue() # GUI -> listener: "use this port instead"
mqtt_connected = threading.Event()


def log(message: str):
    """Timestamp a line and hand it to the GUI thread for display/logging."""
    timestamp = datetime.now().strftime("%H:%M:%S")
    log_queue.put(f"[{timestamp}] {message}")


def classify_device(raw_name: str):
    for slot in DEVICE_SLOTS:
        if slot["match"](raw_name):
            return slot["id"]
    return None


# ==================== Bridge listener (background thread) ====================

def list_ports():
    from serial.tools import list_ports as lp
    for p in lp.comports():
        log(f"  {p.device} - {p.description}")


def find_port():
    if not AUTO_DETECT:
        return PORT
    from serial.tools import list_ports as lp
    ports = list(lp.comports())

    # 1. Highest priority: bridge's own unique product name/serial number
    #    (native-USB boards only - not applicable to the current classic
    #    ESP32, but checked first for whenever that upgrade happens).
    for p in ports:
        text = f"{p.description} {p.manufacturer or ''} {p.serial_number or ''}"
        if any(hint.lower() in text.lower() for hint in BRIDGE_IDENTITY_HINTS):
            return p.device

    # 2. Known USB-serial chip VID:PID - far more reliable than description
    #    text, since hwid strings are consistent regardless of driver wording.
    for p in ports:
        hwid = (p.hwid or "").upper()
        if any(vidpid in hwid for vidpid in KNOWN_CHIP_VIDPID):
            return p.device

    # 3. Linux device paths
    for p in ports:
        if "ttyUSB" in p.device or "ttyACM" in p.device:
            return p.device

    # 4. Last resort: generic description text match
    for p in ports:
        text = f"{p.description} {p.manufacturer or ''}"
        if any(hint.lower() in text.lower() for hint in USB_SERIAL_HINTS):
            return p.device

    return PORT


def open_serial(port):
    """Open the given port, retrying indefinitely until it succeeds. While
    retrying, also checks for a manual port override from the GUI on every
    attempt - otherwise a stuck retry loop on a wrong/dead port would never
    notice an override sitting in the queue. Returns (serial_obj, actual_port)
    since the port actually opened may differ from the one passed in."""
    while True:
        try:
            new_port = port_override_queue.get_nowait()
            if new_port != port:
                log(f"Switching to manually selected port {new_port}...")
                port = new_port
        except queue.Empty:
            pass

        try:
            ser = serial.Serial(port, BAUD, timeout=1)
            log(f"Connected to {port}.")
            return ser, port
        except serial.SerialException as e:
            log(f"Could not open {port}: {e} - retrying in {RECONNECT_RETRY_SECONDS}s...")
            time.sleep(RECONNECT_RETRY_SECONDS)


def reconnect(ser, port):
    try:
        ser.close()
    except Exception:
        pass
    time.sleep(POST_CLOSE_PAUSE_SECONDS)
    return open_serial(port)


def handle_message(msg: dict, mqtt_client: mqtt.Client):
    device = msg.get("device", "unknown")
    mtype = msg.get("type", "unknown")

    if mtype == "traffic_light":
        log(f"[{device}] traffic light -> {msg.get('state', '?')}  (reason: {msg.get('reason', '?')})")
    elif mtype == "train_control":
        log(f"[{device}] train control -> direction={msg.get('direction')} "
            f"speed={msg.get('speed')} location={msg.get('location')}")
    elif mtype == "unknown":
        log(f"[{device}] unrecognized message type: {msg}")
    else:
        log(f"(bridge) {msg}")

    try:
        mqtt_client.publish(EVENTS_TOPIC, json.dumps(msg))
    except Exception as e:
        log(f"(mqtt publish error) {e}")


def connect_publisher_mqtt() -> mqtt.Client:
    client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)

    def on_connect(c, userdata, flags, reason_code, properties=None):
        if reason_code == 0:
            log(f"Connected to MQTT broker at {BROKER_HOST}:{BROKER_PORT}")
        else:
            log(f"MQTT connection failed: {reason_code}")

    client.on_connect = on_connect
    client.connect(BROKER_HOST, BROKER_PORT, keepalive=60)
    client.loop_start()
    return client


def bridge_listener_thread():
    mqtt_client = connect_publisher_mqtt()

    port = find_port()
    log(f"Connecting to {port} at {BAUD} baud...")
    ser, port = open_serial(port)
    port_status_queue.put(port)

    time.sleep(2)
    log("Listening for messages...")

    last_heartbeat = 0
    last_data_time = time.time()

    while True:
        try:
            # Check for a manual port override from the GUI before each read.
            try:
                new_port = port_override_queue.get_nowait()
                if new_port != port:
                    log(f"Switching to manually selected port {new_port}...")
                    ser, port = reconnect(ser, new_port)
                    port_status_queue.put(port)
                    last_data_time = time.time()
            except queue.Empty:
                pass

            raw = ser.readline()

            if not raw:
                if time.time() - last_data_time > IDLE_RECONNECT_SECONDS:
                    log(f"No data for over {IDLE_RECONNECT_SECONDS}s - reconnecting to {port}...")
                    ser, port = reconnect(ser, port)
                    port_status_queue.put(port)
                    last_data_time = time.time()
                continue

            last_data_time = time.time()

            now = time.time()
            if now - last_heartbeat >= BRIDGE_HEARTBEAT_INTERVAL:
                last_heartbeat = now
                try:
                    mqtt_client.publish(STATUS_TOPIC, json.dumps({"device": "bridge", "type": "heartbeat"}))
                except Exception as e:
                    log(f"(mqtt publish error) {e}")

            line = raw.decode("utf-8", errors="ignore").strip()
            if not line:
                continue

            try:
                msg = json.loads(line)
                handle_message(msg, mqtt_client)
            except json.JSONDecodeError:
                log(f"(raw) {line}")

        except (serial.SerialException, OSError) as e:
            log(f"Serial error: {e} - reconnecting to {port}...")
            ser, port = reconnect(ser, port)
            port_status_queue.put(port)
            last_data_time = time.time()
        except Exception as e:
            log(f"Error: {e}")


def start_dashboard_mqtt_subscriber():
    """Separate MQTT client for the GUI's own subscription (device status)."""
    client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)

    def on_connect(c, userdata, flags, reason_code, properties=None):
        if reason_code == 0:
            mqtt_connected.set()
            c.subscribe(EVENTS_TOPIC)
            c.subscribe(STATUS_TOPIC)
        else:
            mqtt_connected.clear()

    def on_disconnect(c, userdata, flags, reason_code, properties=None):
        mqtt_connected.clear()

    def on_message(c, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode("utf-8", errors="ignore"))
            event_queue.put(payload)
        except json.JSONDecodeError:
            pass

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message

    def loop():
        while True:
            try:
                client.connect(BROKER_HOST, BROKER_PORT, keepalive=60)
                client.loop_forever()
            except Exception:
                mqtt_connected.clear()
                time.sleep(3)

    threading.Thread(target=loop, daemon=True).start()


# ==================== GUI ====================

def format_age(age_seconds: float) -> str:
    """Explicit unit labels throughout, so "3:45" is never ambiguous
    between mm:ss and hh:mm - always sec / min / h."""
    total = int(age_seconds)
    if total < 60:
        return f"{total}s"
    elif total < 3600:
        minutes, seconds = divmod(total, 60)
        return f"{minutes}m {seconds:02d}s"
    else:
        hours, remainder = divmod(total, 3600)
        minutes = remainder // 60
        return f"{hours}h {minutes:02d}m"


class Dashboard(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Train Dashboard")
        self.geometry("480x700")
        self.resizable(False, True)

        self.last_seen = {}
        self.active_since = {}   # slot id -> start of current unbroken "alive" streak
        self.traffic_state = None
        self.train_data = {}
        self.log_lines: deque[str] = deque(maxlen=LOG_MAX_LINES)
        self.log_visible = False

        self._build_ui()
        self.after(200, self._poll_queues)
        self.after(500, self._refresh_status)

    def _build_ui(self):
        pad = {"padx": 12, "pady": 8}

        # --- Bridge serial port: detected port + manual override ---
        port_frame = ttk.LabelFrame(self, text="Bridge Serial Port")
        port_frame.pack(fill="x", **pad)
        self.port_label = ttk.Label(port_frame, text="Detecting...")
        self.port_label.pack(side="left", padx=8, pady=6)
        self.port_combo = ttk.Combobox(port_frame, width=10, state="readonly")
        self.port_combo.pack(side="left", padx=(12, 4), pady=6)
        ttk.Button(port_frame, text="Refresh", command=self._refresh_ports).pack(side="left", padx=2)
        ttk.Button(port_frame, text="Connect", command=self._apply_port_override).pack(side="left", padx=2)
        self._refresh_ports()

        mqtt_frame = ttk.LabelFrame(self, text="MQTT Broker")
        mqtt_frame.pack(fill="x", **pad)
        self.mqtt_dot = tk.Canvas(mqtt_frame, width=18, height=18, highlightthickness=0)
        self.mqtt_dot.pack(side="left", padx=8, pady=6)
        self.mqtt_dot_id = self.mqtt_dot.create_oval(2, 2, 16, 16, fill="gray")
        self.mqtt_label = ttk.Label(mqtt_frame, text="Connecting...")
        self.mqtt_label.pack(side="left")

        dev_frame = ttk.LabelFrame(self, text="Devices")
        dev_frame.pack(fill="x", **pad)
        self.device_rows = {}
        for slot in DEVICE_SLOTS:
            row = ttk.Frame(dev_frame)
            row.pack(fill="x", padx=8, pady=4)
            dot = tk.Canvas(row, width=16, height=16, highlightthickness=0)
            dot.pack(side="left", padx=(0, 8))
            dot_id = dot.create_oval(2, 2, 14, 14, fill="gray")
            name_label = ttk.Label(row, text=slot["label"], width=22)
            name_label.pack(side="left")
            seen_label = ttk.Label(row, text="never seen", foreground="gray")
            seen_label.pack(side="left")
            self.device_rows[slot["id"]] = {"dot": dot, "dot_id": dot_id, "seen_label": seen_label}

        tl_frame = ttk.LabelFrame(self, text="Ramzor Traffic Light")
        tl_frame.pack(fill="x", **pad)
        self.tl_canvas = tk.Canvas(tl_frame, width=100, height=220, bg="#222", highlightthickness=0)
        self.tl_canvas.pack(pady=10)
        self.tl_canvas.create_rectangle(20, 10, 80, 210, fill="#111", outline="#555", width=2)
        self.tl_circles = {
            "R": self.tl_canvas.create_oval(30, 20, 70, 60, fill="#400000", outline=""),
            "Y": self.tl_canvas.create_oval(30, 85, 70, 125, fill="#403000", outline=""),
            "G": self.tl_canvas.create_oval(30, 150, 70, 190, fill="#004000", outline=""),
        }

        tc_frame = ttk.LabelFrame(self, text="Train Control")
        tc_frame.pack(fill="x", **pad)
        self.direction_label = ttk.Label(tc_frame, text="Direction: --", font=("Segoe UI", 11))
        self.direction_label.pack(anchor="w", padx=8, pady=4)
        self.speed_label = ttk.Label(tc_frame, text="Speed: --", font=("Segoe UI", 11))
        self.speed_label.pack(anchor="w", padx=8, pady=4)
        self.location_label = ttk.Label(tc_frame, text="Location: --", font=("Segoe UI", 11))
        self.location_label.pack(anchor="w", padx=8, pady=4)

        # --- Listener log panel (hidden by default) + Exit ---
        button_row = ttk.Frame(self)
        button_row.pack(pady=(0, 4))
        self.toggle_button = ttk.Button(button_row, text="Show Listener", command=self._toggle_log)
        self.toggle_button.pack(side="left", padx=4)
        ttk.Button(button_row, text="Exit", command=self.destroy).pack(side="left", padx=4)

        self.log_frame = ttk.LabelFrame(self, text="Listener Log")
        self.log_text = scrolledtext.ScrolledText(self.log_frame, height=12, width=58,
                                                    font=("Consolas", 9), state="disabled")
        self.log_text.pack(fill="both", expand=True, padx=6, pady=6)
        # log_frame is not packed yet - _toggle_log() does that when shown

    def _toggle_log(self):
        self.log_visible = not self.log_visible
        if self.log_visible:
            self.log_frame.pack(fill="both", expand=True, padx=12, pady=(0, 8))
            self.toggle_button.config(text="Hide Listener")
        else:
            self.log_frame.pack_forget()
            self.toggle_button.config(text="Show Listener")

    def _refresh_ports(self):
        from serial.tools import list_ports as lp
        ports = [p.device for p in lp.comports()]
        self.port_combo["values"] = ports
        # Pre-select whatever auto-detect would currently pick, as a starting point.
        guess = find_port()
        if guess in ports:
            self.port_combo.set(guess)
        elif ports:
            self.port_combo.set(ports[0])

    def _apply_port_override(self):
        chosen = self.port_combo.get()
        if chosen:
            port_override_queue.put(chosen)

    # ---------------- Data handling ----------------
    def _poll_queues(self):
        try:
            while True:
                msg = event_queue.get_nowait()
                self._handle_message(msg)
        except queue.Empty:
            pass

        try:
            while True:
                connected_port = port_status_queue.get_nowait()
                self.port_label.config(text=f"Connected: {connected_port}")
        except queue.Empty:
            pass

        new_lines = []
        try:
            while True:
                new_lines.append(log_queue.get_nowait())
        except queue.Empty:
            pass

        if new_lines:
            for line in new_lines:
                self.log_lines.append(line)
            if self.log_visible:
                self.log_text.config(state="normal")
                self.log_text.insert("end", "\n".join(new_lines) + "\n")
                self.log_text.see("end")
                self.log_text.config(state="disabled")
            self._write_log_file()

        self.after(200, self._poll_queues)

    def _write_log_file(self):
        try:
            with open(LOG_FILE_PATH, "w", encoding="utf-8") as f:
                f.write("\n".join(self.log_lines) + "\n")
        except Exception:
            pass  # logging to disk is best-effort, never crash the GUI over it

    def _handle_message(self, msg: dict):
        device = msg.get("device", "")
        slot_id = classify_device(device)
        if slot_id is None:
            return

        self.last_seen[slot_id] = time.time()

        mtype = msg.get("type")
        if slot_id == "ramzor" and mtype == "traffic_light":
            self.traffic_state = msg.get("state")
        elif slot_id == "train_ctrl" and mtype == "train_control":
            self.train_data = {
                "direction": msg.get("direction"),
                "speed": msg.get("speed"),
                "location": msg.get("location"),
            }

    # ---------------- Periodic UI refresh ----------------
    def _refresh_status(self):
        if mqtt_connected.is_set():
            self.mqtt_dot.itemconfig(self.mqtt_dot_id, fill="green")
            self.mqtt_label.config(text=f"Connected ({BROKER_HOST}:{BROKER_PORT})")
        else:
            self.mqtt_dot.itemconfig(self.mqtt_dot_id, fill="red")
            self.mqtt_label.config(text="Disconnected - retrying...")

        now = time.time()
        for slot in DEVICE_SLOTS:
            slot_id = slot["id"]
            row = self.device_rows[slot_id]
            seen = self.last_seen.get(slot_id)

            if slot_id == "bridge":
                maybe_threshold, offline_threshold = BRIDGE_MAYBE_SECONDS, BRIDGE_OFFLINE_SECONDS
            else:
                maybe_threshold, offline_threshold = DEVICE_MAYBE_SECONDS, DEVICE_OFFLINE_SECONDS

            if seen is None:
                self.active_since.pop(slot_id, None)
                row["dot"].itemconfig(row["dot_id"], fill="gray")
                row["seen_label"].config(text="never seen", foreground="gray")
                continue

            age = now - seen
            formatted = format_age(age)

            if age < maybe_threshold:
                # Alive.
                if slot_id not in self.active_since:
                    # Just became alive (first time, or after a broken streak) -
                    # approximate the streak's start as the message that revived it.
                    self.active_since[slot_id] = seen
                active_duration = format_age(now - self.active_since[slot_id])
                row["dot"].itemconfig(row["dot_id"], fill="green")
                row["seen_label"].config(
                    text=f"alive - active for {active_duration}, last seen {formatted} ago",
                    foreground="black")
            elif age < offline_threshold:
                # Maybe: no message in a while, but not long enough to call it offline yet.
                self.active_since.pop(slot_id, None)  # streak broken either way
                row["dot"].itemconfig(row["dot_id"], fill="#e0a800")
                row["seen_label"].config(
                    text=f"maybe - last seen {formatted} ago", foreground="#8a6d00")
            else:
                # Not connected.
                self.active_since.pop(slot_id, None)
                row["dot"].itemconfig(row["dot_id"], fill="red")
                row["seen_label"].config(
                    text=f"not connected - last seen {formatted} ago", foreground="red")

        for color, circle_id in self.tl_circles.items():
            lit = TRAFFIC_LIGHT_COLORS[color] if self.traffic_state == color else {
                "R": "#400000", "Y": "#403000", "G": "#004000"
            }[color]
            self.tl_canvas.itemconfig(circle_id, fill=lit)

        d = self.train_data
        if d:
            self.direction_label.config(text=f"Direction: {d.get('direction', '--')}")
            self.speed_label.config(text=f"Speed: {d.get('speed', '--')}")
            self.location_label.config(text=f"Location: {d.get('location', '--')}")

        self.after(1000, self._refresh_status)


if __name__ == "__main__":
    threading.Thread(target=bridge_listener_thread, daemon=True).start()
    start_dashboard_mqtt_subscriber()
    app = Dashboard()
    app.mainloop()
