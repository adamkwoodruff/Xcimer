
from flask import Flask, request, jsonify, render_template
from flask_socketio import SocketIO, emit
import eventlet;
eventlet.monkey_patch()
import socket
import threading
import hashlib
import struct
import json
import time
import os
import webbrowser

eventlet.monkey_patch()
# Path where uploaded configs will be stored
CONFIG_FILE_PATH = os.path.join(os.path.dirname(__file__), "config.json")
# --- App and SocketIO Setup ---
app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='eventlet')


# --- Broadcast Deduplication Setup ---
last_values = {}

def handle_incoming_signal(name, value):
    global last_values
    if name not in last_values or last_values[name] != value:
        last_values[name] = value
        print(f"[UDP] Broadcast: {name}={value}")
        socketio.emit("update_value", {"name": name, "value": value})

# --- NEW: Updated Network and Protocol Configuration ---
PORTENTA_IP = "192.168.3.6"  # Portenta IP
PORTENTA_PORT = 17751  # The NEW fixed port for the bridge server

LOCAL_PORT = 10006
CONFIG_LOCAL_PORT = 10007


udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

udp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)

udp_sock.bind(("", LOCAL_PORT))
#print(f"[Init] Web client socket bound to port {LOCAL_PORT}")

DEFAULT_KEY = bytes([0x57, 0x4F, 0x4F, 0x44, 0x52, 0x55, 0x46, 0x46] * 16)
CONFIG_KEY = bytes([0x3A, 0x7F, 0x0C, 0xD5])

# --- Protocol Maps ---
CMD_MAP_INV = {'CONFIG': 0x83}  # Only need to know the response command
CMD_MAP = {v: k for k, v in CMD_MAP_INV.items()}
SIGNAL_MAP = {
    "volt_set": 0x04, "curr_set": 0x05, "volt_act": 0x06, "curr_act": 0x07,
    "mode_set": 0x08, "inter_enable": 0x09, "extern_enable": 0x0A,
    "warn_lamp": 0x0B, "ps_disable": 0x0C, "pwm_enable": 0x0D, "pwm_reset": 0x0E,
    "output_enable": 0x0F,
    "SW_GET_VERSION": 0x01,
    "t1": 0x13, "th": 0x14, "t2": 0x15,
    "a1": 0x16, "b1": 0x17, "c1": 0x18, "d1": 0x19,
    "a2": 0x20, "b2": 0x21, "c2": 0x23, "d2": 0x24,
    "run_current_wave": 0x25
}
SIGNAL_ID_TO_NAME = {v: k for k, v in SIGNAL_MAP.items()}
LATEST_VALUES = {}


# --- NEW: Updated Packet Signature Functions ---

def _calc_json_sign(data: bytes) -> bytes:
    """Signature function for JSON-based packets (used for decoding config response)."""
    print(f"[UDP] Raw incoming packet: {data.hex()}, len={len(data)}")

    sign = hashlib.sha256(DEFAULT_KEY + data).digest()[:4]
    print(f"[UDP] Calculated JSON sign: {sign.hex()}")
    return sign



def _calc_binary_sign(data: bytes) -> bytes:
    """Signature function for raw binary packets (used for encoding commands)."""
    # This matches the new rule: SHA256(first 10 bytes + key)
    return hashlib.sha256(data + DEFAULT_KEY).digest()[-4:]


def handle_udp_ack_packet(packet: bytes):
    """Handles 14-byte ACK packets and emits value updates via Socket.IO."""
    if len(packet) != 14 or packet[5] != 0xA0:
        return

    sig_id = packet[4]
    value = struct.unpack('>f', packet[6:10])[0]
    name = SIGNAL_ID_TO_NAME.get(sig_id)

    if name:
        prev = LATEST_VALUES.get(name)
        if prev != value:
            LATEST_VALUES[name] = value
            print(f"[UDP] ACK 0xA0: {name} changed {prev} → {value}")
            socketio.emit("update_value", {"name": name, "value": value})


def decode_json_packet(packet: bytes):
    """Decodes a JSON-based packet (e.g., for config)."""
    if len(packet) < 5:
        # Return None for cmd to indicate failure
        return None, None

    sign, body = packet[:4], packet[4:]

    # --- FIX: Use the correct function name _calc_json_sign ---
    if _calc_json_sign(body) != sign:
        print("[UDP] Invalid signature on JSON packet.")
        return None, None

    cmd = CMD_MAP.get(body[0])
    payload = json.loads(body[1:].decode()) if len(body) > 1 else {}
    print(f"[UDP] Decoded cmd={cmd}, payload={payload}")
    return cmd, payload



# --- UDP Listener and API Endpoints ---

# This thread listens for UDP broadcast updates on the fixed local port
def udp_listener_thread():
    while True:
        try:
            data, addr = udp_sock.recvfrom(8192)
            if len(data) == 14:
                payload_bytes = data[:10]
                signature = data[10:14]

                if _calc_binary_sign(payload_bytes) != signature:
                    print(f"[UDP] ⚠ Invalid signature from {addr}, ignoring")
                    continue

                uid = payload_bytes[0:4]
                sig_id = payload_bytes[4]
                sig_type = payload_bytes[5]
                value = struct.unpack('>f', payload_bytes[6:10])[0]

                name = SIGNAL_ID_TO_NAME.get(sig_id)
                if not name:
                    print(f"[UDP] ⚠ Unknown sig_id=0x{sig_id:02X}")
                    continue

                prev = LATEST_VALUES.get(name)
                if sig_type == 0xA0:  # ACK_OK → treat as update
                    if prev != value:
                        LATEST_VALUES[name] = value
                        print(f"[UDP] ✅ ACK 0xA0: {name} changed {prev} → {value}")
                        print(f"[WS]   → Emitting to clients: update_value = {{ name: {name}, value: {value} }}")
                        socketio.emit("update_value", {"name": name, "value": value})
                        print(f"[EMIT] update_value → name: {name}, value: {value}")
                    else:
                        print(f"[UDP] ACK 0xA0: {name} unchanged at {value}")
                else:
                    if prev != value:
                        LATEST_VALUES[name] = value
                        print(f"[UDP] 🔁 {name} changed: {prev} → {value}")
                        print(f"[WS]   → Emitting to clients: update_value = {{ name: {name}, value: {value} }}")
                        socketio.emit("update_value", {"name": name, "value": value})
                        print(f"[EMIT] update_value → name: {name}, value: {value}")
            else:
                try:
                    msg = json.loads(data.decode())
                    if msg.get("type") == "log":
                        socketio.emit("log_entry", msg)
                except Exception:
                    pass

        except Exception as e:
            print(f"[UDP] ❌ Error in udp_listener_thread: {e}")



@app.route("/")
def index():
    return render_template("index.html")


@app.route("/builder")
def builder():
    """Serve the visual configuration builder interface."""
    return render_template("builder.html")


@socketio.on('connect')
def handle_connect():
    """Send the latest known values to a newly connected client."""
    print(f"[SOCKETIO] Client connected: {request.sid}. Sending initial values")
    for name, value in LATEST_VALUES.items():
        emit("update_value", {"name": name, "value": value})

@app.route("/api/get_config", methods=["GET"])
def api_get_config():
    """Request the display configuration from the Portenta."""
    uid = os.urandom(4)
    payload = uid + bytes([0x00, 0x21]) + CONFIG_KEY
    sign = _calc_binary_sign(payload)
    packet = payload + sign

    pages = {}
    total = None
    end_time = time.time() + 2.0

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as cfg_sock:
        cfg_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        cfg_sock.bind(("", CONFIG_LOCAL_PORT))  # Use a dedicated source port
        cfg_sock.settimeout(0.5)
        cfg_sock.sendto(packet, (PORTENTA_IP, PORTENTA_PORT))

        while time.time() < end_time:
            try:
                data, _ = cfg_sock.recvfrom(8192)
            except socket.timeout:
                continue

            cmd, payload = decode_json_packet(data)
            if cmd != 'CONFIG':
                continue

            pg = payload.get('page')
            total = payload.get('total_pages', total)
            pages[pg] = payload.get('data', '')

            if total and len(pages) >= total:
                break

    if not total or len(pages) < total:
        return jsonify({"error": "config unavailable or timed out"}), 500

    config_str = ''.join(pages[i] for i in sorted(pages.keys()))
    return jsonify({"display_config": json.loads(config_str)})

@app.route("/api/command", methods=["POST"])
def api_command():
    """Send a command packet to the Portenta, supporting simple delta modes."""
    try:
        message = request.get_json(force=True)
        name = message.get("name")
        if not name:
            return jsonify({"error": "missing name"}), 400

        mode = message.get("do", "set")
        try:
            value = float(message.get("value", 0))
        except (TypeError, ValueError):
            value = 0

        sig_id = SIGNAL_MAP.get(name)
        if sig_id is None:
            return jsonify({"error": "unknown signal"}), 400

        uid = os.urandom(4)

        if mode == "add":
            sig_type = 0x11
        elif mode == "mult":
            sig_type = 0x12
        else:  # default to set (includes "set" and unknown modes)
            sig_type = 0x10

        value_bytes = struct.pack('>f', float(value))
        payload = uid + bytes([sig_id, sig_type]) + value_bytes
        sign = _calc_binary_sign(payload)
        packet = payload + sign

        udp_sock.sendto(packet, (PORTENTA_IP, PORTENTA_PORT))
        return jsonify({"status": "sent"})
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/get_signal", methods=["POST"])
def api_get_signal():
    """Request the current value of a signal from the Portenta."""
    try:
        message = request.get_json(force=True)
        name = message.get("name")
        sig_id = SIGNAL_MAP.get(name)
        if sig_id is None:
            return jsonify({"error": "unknown signal"}), 400

        uid = os.urandom(4)
        sig_type = 0x20  # GET
        value_bytes = struct.pack('>f', 0.0)
        payload = uid + bytes([sig_id, sig_type]) + value_bytes
        sign = _calc_binary_sign(payload)
        packet = payload + sign
        print(f"[API] Sending GET for {name} (id=0x{sig_id:02X})")
        udp_sock.sendto(packet, (PORTENTA_IP, PORTENTA_PORT))
        return jsonify({"status": "sent"})
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/upload_config", methods=["POST"])
def api_upload_config():
    """Receive a display configuration and store it on disk."""
    try:
        cfg = request.get_json(force=True)
        with open(CONFIG_FILE_PATH, "w") as f:
            json.dump(cfg, f, indent=2)
        return jsonify({"status": "saved"})
    except Exception as e:
        return jsonify({"error": str(e)}), 500



@app.route("/waveform")
def waveform_page():
    """Simple page for editing waveform parameters."""
    return render_template("waveform.html")






if __name__ == "__main__":
    print("Initializing UDP listener thread...")
    listener = threading.Thread(target=udp_listener_thread, daemon=True)
    listener.start()

    ip = "0.0.0.0"
    port = 8009
    url = f"http://{ip}:{port}"
    print(f"Starting Flask server — access the web interface at: {url}")
    webbrowser.open(url)  # Optional: will open in default browser on Linux with GUI

    socketio.run(app, host="0.0.0.0", port=port)
