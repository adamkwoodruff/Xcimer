import time
import json
import serial
import os
import threading
import copy  # Needed for deep copying templates
from msgpackrpc import Address as RpcAddress, Client as RpcClient, error as RpcError
import socket
import hashlib
import struct
import csv
from dataclasses import dataclass
from threading import Timer
import asyncio 
# --- Debug helpers ---
import threading as _thr_mod
def _t(x): return type(x).__name__
def _thr(): return _thr_mod.current_thread().name


UDP_PORT = 17751
WEB_CLIENTS = set()
DATA_LOCK = threading.RLock()  # Use a re-entrant lock for safety
POLL_NAME_MAP = {}
udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udp_sock.bind(("0.0.0.0", UDP_PORT))
udp_sock.setblocking(False)
giga_ui_ready = False

# CSV logging setup → /home/fio/portenta_linux_bridge/pid_log.csv
LOG_LOCK = threading.Lock()
RPC_LOCK = threading.RLock()
LOG_FILE_PATH = os.getenv("PID_LOG_PATH", "/home/fio/portenta_linux_bridge/pid_log.csv")
os.makedirs(os.path.dirname(LOG_FILE_PATH), exist_ok=True)
log_file = open(LOG_FILE_PATH, "a", newline="", buffering=1)  # line-buffered
csv_writer = csv.writer(log_file)
if log_file.tell() == 0:
    csv_writer.writerow(["time_ms", "set_current", "duty"])
print(f"[LOG] Writing CSV to: {LOG_FILE_PATH}")


# Central store for latest values
TRUE_VALUES = {
    "SW_GET_VERSION": 0.0,
    "volt_set": 0.0,
    "curr_set": 0.0,
    "volt_act": 0.0,
    "curr_act": 0.0,
    "igbt_fault": 0.0,
    "ext_enable": 0.0,
    "charger_relay": 0.0,
    "dump_relay": 0.0,
    "dump_fan": 0.0,
    "warn_lamp": 0.0,
    "scr_trig": 0.0,
    "meas_voltage_pwm": 0.0,
    "meas_current_pwm": 0.0,
}
# Store latest RPC results by name (e.g. "rpc_result_volt_set" -> value)
RPC_RESULTS = {}

# Track whether the system is in local or remote control mode.  A value of
# ``None`` means no mode has been set yet (useful for unit tests that do not
# load the full configuration).  The variable is updated whenever the
# ``mode_set`` signal changes.
CURRENT_MODE = None

# Mapping of signal names to binary protocol IDs
SIGNAL_IDS = {
    "SW_GET_VERSION": 0x01,
    "volt_set": 0x04,
    "curr_set": 0x05,
    "volt_act": 0x06,
    "curr_act": 0x07,
    "igbt_fault": 0x08,
    "ext_enable": 0x09,
    "charger_relay": 0x0A,
    "dump_relay": 0x0B,
    "dump_fan": 0x0C,
    "warn_lamp": 0x0D,
    "scr_trig": 0x0E,
    "meas_voltage_pwm": 0x0F,
    "meas_current_pwm": 0x10,
}

EXPECTED_TYPES = {
    "volt_act": (float, int),
    "curr_act": (float, int),
    "ext_enable": (int, bool),
    "has_sync_completed": (int, bool),
    "process_event_in_uc": (int, type(None)),  # often returns ack int or None
}

SIGNAL_ID_TO_NAME = {v: k for k, v in SIGNAL_IDS.items()}

import builtins
from datetime import datetime

# ---- Non-blocking sleep alias (works with Flask-SocketIO/eventlet; safe fallback) ----
try:
    from flask_socketio import sleep as nb_sleep
except Exception:
    try:
        import eventlet
        nb_sleep = eventlet.sleep
    except Exception:
        from time import sleep as nb_sleep


def send_log_message(message: str) -> None:
    """Broadcast a log message to all known WEB_CLIENTS via UDP."""
    if os.environ.get("PYTEST_CURRENT_TEST"):
        return
    log_packet = json.dumps({
        "type": "log",
        "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "message": message,
    }).encode()
    for client in WEB_CLIENTS:
        try:
            udp_sock.sendto(log_packet, client)
        except Exception as e:
            builtins.print(f"[LOG] Failed to send log to {client}: {e}")


def print_ts(*args, **kwargs):
    """Replacement print() that timestamps and forwards logs via UDP."""
    msg = " ".join(str(a) for a in args)
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    builtins.print(f"{timestamp} {msg}", **kwargs)
    send_log_message(msg)


# Override global print so all existing calls also broadcast logs
print = print_ts

local_uid = os.urandom(4)


def log_pid_sample(time_ms: int, set_current: float, duty: float) -> None:
    """Append a PID sample to the CSV log."""
    with LOG_LOCK:
        csv_writer.writerow([time_ms, set_current, duty])
        log_file.flush()

# Signals that represent boolean values. When a toggle operation is requested
# their current state will be inverted regardless of any provided increment
# value.
BOOL_NAMES = {
    "ext_enable",
    "charger_relay",
    "dump_relay",
    "dump_fan",
    "warn_lamp",
    "scr_trig",
}


@dataclass(frozen=True)
class SignalInfo:
    """Immutable description for a protocol signal."""
    name: str
    id: int


# Build immutable signal registry for safer lookups
SIGNALS = {name: SignalInfo(name, sid) for name, sid in SIGNAL_IDS.items()}
SIGNALS_BY_ID = {info.id: info for info in SIGNALS.values()}


@dataclass
class SignalEntry:
    """Mutable entry storing the current value for a signal."""
    name: str
    id: int
    value: float = 0.0


# Dedicated database of known signals and their latest values
SIGNAL_DB = {name: SignalEntry(name, sid, 0) for name, sid in SIGNAL_IDS.items()}
SIGNAL_DB_BY_ID = {entry.id: entry for entry in SIGNAL_DB.values()}


def set_signal_value(name: str, value: float, src: str = "unknown") -> None:
    """Safely update both the signal database and generic value store with debug."""
    with DATA_LOCK:
        prev = SIGNAL_DB[name].value if name in SIGNAL_DB else TRUE_VALUES.get(name, 0.0)
        if name in SIGNAL_DB:
            SIGNAL_DB[name].value = float(value)
        TRUE_VALUES[name] = float(value)
        delta = value - prev
        print(f"[Store] {src} updated '{name}': prev={prev} delta={delta} new={value}")

        # Automatically propagate boolean value changes to the M4 when they
        # originate from the Linux side.  Updates coming from the M4 itself are
        # marked with src="rpc" or src="uc" and must not be echoed back.
        if name in BOOL_NAMES and src not in ("rpc", "uc"):
            payload = json.dumps({"display_event": {"name": name, "value": int(value)}})
            call_m4_rpc("process_event_in_uc", payload)


def get_signal_value(name: str):
    """Retrieve the latest value for a signal or generic entry."""
    with DATA_LOCK:
        if name in SIGNAL_DB:
            return SIGNAL_DB[name].value
        return TRUE_VALUES.get(name)


# Binary UDP protocol constants
DEFAULT_SIGN_KEY = bytes.fromhex(('57 4F 4F 44 52 55 46 46 ' * 16).replace(' ', ''))
CONFIG_KEY = bytes([0x3A, 0x7F, 0x0C, 0xD5])
VERSION_SIG_ID = 0x01
SIG_VERSION = 0x57450301  # VERSION 3, INTERFACE 1
TRUE_VALUES["SW_GET_VERSION"] = SIG_VERSION
ACK_OK               = 0xA0
ACK_ERROR_UNKNOWN    = 0xE0
ACK_ERROR_NOTALLOWED = 0xE1
ACK_ERROR_OUTOFRANGE = 0xE2
ACK_ERROR_NOTREADY   = 0xE3
ACK_ERROR_SIGN       = 0xE4

# Utility -------------------------------------------------------------

def version_int_to_ascii(version: int) -> str:
    """Convert integer firmware version to ASCII string like 'WE0301'."""
    prefix = chr((version >> 24) & 0xFF) + chr((version >> 16) & 0xFF)
    ver = (version >> 8) & 0xFF
    interface = version & 0xFF
    return f"{prefix}{ver:02d}{interface:02d}"

# --- Signal operation types ---
# 0x10 retains its original meaning of setting a value.  The protocol now also
# supports explicit add and multiply operations which are interpreted by the
# bridge when packets arrive.
SIG_TYPE_SET  = 0x10  # Value is set to the provided amount
SIG_TYPE_ADD  = 0x11  # Provided amount is added to the current value
SIG_TYPE_MULT = 0x12  # Current value is multiplied by the provided amount
SIG_TYPE_GET  = 0x20  # Request the current value

CMD_MAP = {
    0x01: 'SET',
    0x02: 'GET',
    0x03: 'CONFIG_REQUEST',
    0x80: 'ACK_OK',
    0x81: 'ACK_ERROR',
    0x82: 'ACK_ERROR_SIGN',
    0x83: 'CONFIG',
}
CMD_MAP_INV = {v: k for k, v in CMD_MAP.items()}

CONFIG_CHUNK_SIZE = 800  # bytes of JSON data per CONFIG page

def push_truth_table_to_m4():
    """Push the current TRUE_VALUES to the M4 core via RPC."""
    with DATA_LOCK:
        truth_subset = {
            "volt_set": get_signal_value("volt_set"),
            "curr_set": get_signal_value("curr_set"),
            "ext_enable": bool(get_signal_value("ext_enable")),
            "warn_lamp": bool(get_signal_value("warn_lamp")),
            "charger_relay": bool(get_signal_value("charger_relay")),
            "dump_relay": bool(get_signal_value("dump_relay")),
            "dump_fan": bool(get_signal_value("dump_fan")),
        }

    try:
        truth_json = json.dumps(truth_subset)
        print(f"[SYNC] Sending truth table to M4: {truth_json}")
        call_m4_rpc("set_truth_table", truth_json)
        print("[SYNC] Truth table successfully pushed to M4.")
    except Exception as e:
        print(f"[SYNC] Failed to push truth table to M4: {e}")



def check_and_sync_m4():
    print("[SYNC] check_and_sync_m4 started – will push truth table every 1s")

    while True:
        try:
            synced = call_m4_rpc("has_sync_completed")

            if bool(synced):
                print("[SYNC] M4 confirms sync is complete. Not sending table.")
            else:
                print("[SYNC] M4 not yet synced. Pushing truth table...")
                push_truth_table_to_m4()

        except Exception as e:
            print(f"[SYNC] Error during sync check or push: {e}")

        nb_sleep(0.1) # Syncing doesn't need to be super fast



def _calc_sign(data: bytes) -> bytes:
    return hashlib.sha256(DEFAULT_SIGN_KEY + data).digest()[:4]


def encode_packet(cmd: str, payload: dict) -> bytes:
    """Encodes a command and payload into the binary UDP format."""
    body = bytes([CMD_MAP_INV.get(cmd, 0x81)]) + json.dumps(payload).encode()
    sign = _calc_sign(body)
    return sign + body


def decode_packet(packet: bytes):
    """Decode a binary UDP packet. Returns (cmd, payload) or raises ValueError."""
    if len(packet) < 5:
        raise ValueError("packet too short")
    sign = packet[:4]
    body = packet[4:]
    if _calc_sign(body) != sign:
        raise ValueError("invalid_sign")
    cmd_byte = body[0]
    cmd = CMD_MAP.get(cmd_byte, 'UNKNOWN')
    try:
        payload = json.loads(body[1:].decode()) if len(body) > 1 else {}
    except Exception:
        payload = {}
    return cmd, payload

# --- Configuration ---
GIGA_UART_PORT = "/dev/ttymxc1"  # Serial port for Giga Display
GIGA_BAUD_RATE = 115200
M4_PROXY_ADDRESS = 'm4-proxy'    # Default RPC proxy address
M4_PROXY_PORT = 5001             # Default RPC proxy port
CONFIG_FILE_PATH = "config.json"   # Path to display config file (relative to script)
# POLL_M4_INTERVAL_S = 0.5       # No longer used - intervals are per-poll config

# --- Global Variables ---
ser = None
# Consider making rpc_client more persistent if stable, but per-call is robust
# rpc_client = None
# last_config_content = None # Config is loaded once at start now

# --- RPC Function ---

def call_m4_rpc(function_name, *args, retries=1, timeout=0.05):
    """Single-flight, typed RPC call to the M4 (prevents reply cross-talk)."""
    import threading as _thr
    _tname = _thr.current_thread().name

    with RPC_LOCK:
        last_err = None
        for attempt in range(retries + 1):
            client = None
            try:
                print(f"[RPC->] {_tname} {function_name} args={args} "
                      f"timeout={timeout} attempt={attempt+1}/{retries+1} "
                      f"dest={M4_PROXY_ADDRESS}:{M4_PROXY_PORT}")

                client = RpcClient(
                    RpcAddress(M4_PROXY_ADDRESS, M4_PROXY_PORT),
                    timeout=timeout,
                    reconnect_limit=3
                )

                result = client.call(function_name, *args)
                print(f"[RPC<-] {_tname} {function_name} type={type(result).__name__} value={result}")

                exp = EXPECTED_TYPES.get(function_name)
                if exp and not isinstance(result, exp):
                    print(f"[RPC!!] {_tname} {function_name} TYPE_MISMATCH "
                          f"got={type(result).__name__} value={result} expected={exp}")
                    raise TypeError("RPC type mismatch")

                # Coerce where needed
                if function_name in ("volt_act", "curr_act") and isinstance(result, int):
                    result = float(result)
                    print(f"[RPC~ ] {_tname} {function_name} COERCE int->float -> {result}")

                if function_name in ("ext_enable", "has_sync_completed"):
                    result = bool(result)
                    print(f"[RPC~ ] {_tname} {function_name} COERCE -> bool -> {result}")

                print(f"[RPC= ] {_tname} {function_name} RETURN type={type(result).__name__} value={result}")
                return result

            except RpcError.TimeoutError as e:
                print(f"[RPC!!] {_tname} {function_name} TIMEOUT attempt={attempt+1}/{retries+1} err={e}")
                last_err = e
            except Exception as e:
                print(f"[RPC!!] {_tname} {function_name} ERROR attempt={attempt+1}/{retries+1} "
                      f"err={e.__class__.__name__}: {e}")
                last_err = e
            finally:
                try:
                    if client:
                        client.close()
                        print(f"[RPC]  {_tname} {function_name} client.close()")
                except Exception as e2:
                    print(f"[RPC]  {_tname} {function_name} client.close() error: {e2}")

            time.sleep(0.05)

        print(f"[RPC!!] {_tname} {function_name} giving up after {retries+1} attempts; last_err={last_err}")
        return None



# --- UART Functions (send_to_giga, read_from_giga) ---
# Keep these exactly as they were in the previous Python version that worked
# (assuming they correctly send/receive JSON lines with the Giga display)
def send_to_giga(json_data):
    """Sends a JSON object or string to the Giga Display via UART."""
    global ser, giga_ui_ready, giga_ui_ready_time

    if isinstance(json_data, dict) and "display_event" in json_data:
        if not giga_ui_ready or time.time() < giga_ui_ready_time:
            print("[UART] Skipping display_event — GIGA not ready.")
            return

    if not ser or not ser.is_open:
        print("[UART] Error: Serial port not open. Cannot send to Giga.")
        return
    try:
        if isinstance(json_data, dict):
            message = json.dumps(json_data)
        elif isinstance(json_data, str):
            message = json_data
        else:
            print(f"[UART] Error: Invalid data type for send_to_giga: {type(json_data)}")
            return

        ser.write((message + '\n').encode('utf-8'))  # Send with newline
    except serial.SerialException as e:
        print(f"[UART] Error writing to Giga: {e}")
    except Exception as e:
        print(f"[UART] Unexpected error sending to Giga: {e}")


def read_from_giga():
    """Reads and parses a line of JSON from the Giga Display via UART."""
    global ser
    if not ser or not ser.is_open or not ser.in_waiting:
        return None
    try:
        line = ser.readline().decode('utf-8').strip()
        if line:
            if line.startswith("PID_LOG"):
                send_log_message(line)
                parts = line.split(',')
                if len(parts) >= 4:
                    try:
                        t_ms = int(parts[1])
                        set_curr = float(parts[2])
                        duty = float(parts[3])
                        log_pid_sample(t_ms, set_curr, duty)
                    except ValueError:
                        print(f"[UART] Invalid PID_LOG data: {line}")
                else:
                    print(f"[UART] Malformed PID_LOG line: {line}")
                return None
            # print(f"[UART] Received from Giga: {line}") # Verbose
            try:
                data = json.loads(line)
                return data
            except json.JSONDecodeError:
                print(f"[UART] Invalid JSON received from Giga: {line}")
                # Return something to indicate error, but don't crash
                return {"error": "invalid_json", "raw": line}
        return None # No complete line read
    except serial.SerialException as e:
        print(f"[UART] Error reading from Giga: {e}")
        return None
    except UnicodeDecodeError as e:
        print(f"[UART] Error decoding UTF-8 from Giga: {e}")
        # Optionally read more or clear buffer if needed
        return None
    except Exception as e:
        print(f"[UART] Unexpected error reading from Giga: {e}")
        return None


# --- Configuration Handling ---
def load_config(config_path):
    """Loads and parses the JSON configuration file."""
    print(f"[Config] Attempting to load config file: {config_path}")
    if not os.path.exists(config_path):
        print(f"[Config] Error: Config file not found at {config_path}")
        return None
    try:
        with open(config_path, 'r') as f:
            config_data = json.load(f)
        print(f"[Config] Config file loaded successfully.")
        # Basic validation (check if key sections exist)
        if "display_config" not in config_data:
             print("[Config] Warning: 'display_config' section missing.")
        if "m4_data_polls" not in config_data:
             print("[Config] Warning: 'm4_data_polls' section missing - M4 polling disabled.")
             config_data["m4_data_polls"] = [] # Ensure it exists as empty list
        return config_data
    except json.JSONDecodeError as e:
        print(f"[Config] Error: Invalid JSON in config file {config_path}: {e}")
        return None
    except Exception as e:
        print(f"[Config] Error reading config file {config_path}: {e}")
        return None

# --- JSON Template Helper ---
def fill_json_template(template, value, rpc_name=None):
    """Recursively replaces rpc result placeholders in a template dict/list.

    ``value`` may be a single value or a dict of ``rpc_result_*`` entries.

    Two placeholder forms are supported:
      * ``{rpc_result}`` – the legacy single value placeholder
      * ``{rpc_result_<rpc_name>}`` – explicit placeholders for each RPC
    """
    if isinstance(template, dict):
        return {k: fill_json_template(v, value, rpc_name) for k, v in template.items()}
    elif isinstance(template, list):
        return [fill_json_template(item, value, rpc_name) for item in template]
    elif isinstance(template, str):
        # When a dict of results is provided, match explicit placeholders
        if isinstance(value, dict):
            if template.startswith("{rpc_result_") and template.endswith("}"):
                key = template[1:-1]  # strip braces
                val = value.get(key)
                if isinstance(val, bool):
                    return 1 if val else 0
                return val if val is not None else template
            if template == "{rpc_result}" and rpc_name:
                val = value.get(f"rpc_result_{rpc_name}")
                if isinstance(val, bool):
                    return 1 if val else 0
                return val if val is not None else template
        else:
            # Single value replacement for legacy behaviour
            if template == "{rpc_result}" or (
                rpc_name and template == f"{{rpc_result_{rpc_name}}}"
            ):
                if isinstance(value, bool):
                    return 1 if value else 0
                return value
        return template
    else:
        # Return numbers/bools/None unchanged
        return template

# --- True Value Store Helpers ---
def initialize_true_values(cfg):
    with DATA_LOCK:
        panels = cfg.get("display_config", {}).get("panels", [])
        for panel in panels:
            for val in panel.get("values", []):
                name = val.get("name")
                if not name:
                    continue
                default = val.get("default_value", 0)
                set_signal_value(name, default)

        # Honour the configured default mode if provided.  Store the numeric
        # value in the signal table and update the CURRENT_MODE helper used by
        # the gating logic.
        global CURRENT_MODE
        mode_str = cfg.get("default_mode", "local")
        mode_val = 1 if str(mode_str).lower() == "remote" else 0
        set_signal_value("mode_set", mode_val)
        CURRENT_MODE = "remote" if mode_val else "local"

        for name in SIGNAL_IDS:
            if name == "SW_GET_VERSION":
                # Always ensure the software version is stored correctly so
                # periodic broadcasts transmit the expected constant.
                set_signal_value(name, SIG_VERSION)
            elif name not in TRUE_VALUES:
                set_signal_value(name, 0)

def log_all_configurable_values():
    """Emit the current values of all known signals for startup snapshot."""
    with DATA_LOCK:
        for name, entry in SIGNAL_DB.items():
            print(f"\u2192 set [init] {name} = {entry.value}")
        for name, value in TRUE_VALUES.items():
            if name not in SIGNAL_DB:
                print(f"\u2192 set [init] {name} = {value}")

def update_and_broadcast(name, value, src="linux"):
    with DATA_LOCK:
        if name in ("volt_set", "curr_set", "warn_lamp") and value < 0:
            value = 0
        if name in (
            "volt_act",
            "curr_act",
            "ext_enable",
            "igbt_fault",
        ) and src == "udp":
            print(f"[Store] Skipping update of {name} from UDP (M4-owned).")
            return
        set_signal_value(name, value, src=src)
        if name == "mode_set":
            global CURRENT_MODE
            CURRENT_MODE = "remote" if float(value) >= 0.5 else "local"
        broadcast_binary_value(name, value)
        send_to_giga({"display_event": {"type": "set_value", "name": name, "value": value, "src": src}})
        print(f"\u2192 set [{src}] {name} = {value}")


def apply_increment(name, delta, src="linux"):
    with DATA_LOCK:
        current = TRUE_VALUES.get(name, 0.0)
        new_val = current + delta
        if name in ("volt_set", "curr_set") and new_val < 0:
            new_val = 0
        set_signal_value(name, new_val, src=src)
        broadcast_binary_value(name, new_val)
        send_to_giga({"display_event": {"type": "set_value", "name": name, "value": new_val, "src": src}})
    return new_val

def apply_math_operation(current: float, increment: float, op: str = "set") -> float:
    """Applies a mathematical operation to a value and returns the result."""
    try:
        if op == "add":
            return current + increment
        elif op == "subtract":
            return current - increment
        elif op == "mult":
            if increment == -1 and current in (0, 1):
                # Treat multiply-by-minus-one as a boolean toggle
                return 0 if current else 1
            return current * increment
        elif op == "toggle":
            return 0 if current else 1
        else:  # default to set
            return increment
    except Exception as e:
        print(f"[Math] Error applying operation '{op}' to {current} and {increment}: {e}")
        return current


def broadcast_all_values():
    """Safely broadcasts all signal values to Giga and UDP clients."""
    with DATA_LOCK:
        # Create a snapshot of the values to ensure consistency during broadcast
        all_values = {**TRUE_VALUES, **{name: entry.value for name, entry in SIGNAL_DB.items()}}

    for name, value in all_values.items():
        if name in SIGNAL_DB:
            broadcast_binary_value(name, value)
        send_to_giga({"display_event": {"type": "set_value", "name": name, "value": value, "src": "uc"}})


def build_poll_name_map(m4_polls):
    """Create a lookup of RPC function names to display value names."""
    POLL_NAME_MAP.clear()
    for poll in m4_polls:
        func = poll.get("poll_rpc_func")
        name = poll.get("giga_json_template", {}).get("display_event", {}).get("name")
        if func and name:
            POLL_NAME_MAP[func] = name
            if func != name:
                print(f"[Init] WARNING: RPC '{func}' mapped to '{name}'")

# --- UDP Integration ---




def broadcast_to_web_clients(message):
    """Send JSON message to all known web clients."""
    print(f"[UDP] Broadcasting JSON to {len(WEB_CLIENTS)} clients: {message}")
    for client in WEB_CLIENTS:
        try:
            encoded = json.dumps(message).encode()
            udp_sock.sendto(encoded, client)
            print(f"[UDP] JSON sent to {client}: {encoded}")
        except Exception as e:
            print(f"[UDP] Failed to send JSON to {client}: {e}")


def broadcast_binary_value(name: str, value, target_addr=None):
    """Send a 14-byte ACK packet with the signal's value to a specific client."""
    with DATA_LOCK:
        entry = SIGNAL_DB.get(name)
        if entry is None:
            # print(f"[UDP] Unknown signal name: {name}. Skipping broadcast.")
            return

        expected = SIGNALS_BY_ID.get(entry.id)
        if not expected or expected.name != name:
            raise ValueError(
                f"Signal ID mismatch for '{name}': expected '{expected.name if expected else 'N/A'}',"
                f" got 0x{entry.id:02X}"
            )

        uid = os.urandom(4)
        sig_type = ACK_OK
        if name == "SW_GET_VERSION":
            # Version is transmitted as a raw 32-bit integer rather than a float.
            value_bytes = struct.pack('>I', int(value) & 0xFFFFFFFF)
        else:
            value_bytes = struct.pack('>f', float(value))
        payload = uid + bytes([entry.id, sig_type]) + value_bytes
        sign = hashlib.sha256(payload + DEFAULT_SIGN_KEY).digest()[-4:]
        packet = payload + sign

    if name == "SW_GET_VERSION":
        value_repr = version_int_to_ascii(int.from_bytes(value_bytes, 'big'))
    else:
        value_repr = struct.unpack('>f', value_bytes)[0]

    decoded_view = {
        "uid": uid.hex(),
        "sig_id": f"0x{entry.id:02X}",
        "name": name,
        "sig_type": f"0x{sig_type:02X}",
        "value": value_repr,
        "sign": sign.hex(),
    }

    # print(f"[UDP] ACK packet prepared for {name}: {decoded_view}")

    if name != "SW_GET_VERSION" and abs(decoded_view["value"] - float(value)) > 1e-6:
        print(
            f"[UDP] WARNING: Value mismatch encoding {name}: {value} != {decoded_view['value']}"
        )

    try:
        if target_addr:
            udp_sock.sendto(packet, target_addr)
            # print(f"[UDP] ACK packet sent to {target_addr}")
        else:
            for client in WEB_CLIENTS:
                udp_sock.sendto(packet, client)
                # print(f"[UDP] Binary packet broadcasted to {client}")
    except Exception as e:
        print(f"[UDP] Failed binary send: {e}")



def encode_udp_packet(uid: bytes, sig_id: int, name: str, sig_type: int, value: float) -> bytes:
    value_bytes = struct.pack('>f', float(value))
    payload = uid + bytes([sig_id, sig_type]) + value_bytes
    sign = hashlib.sha256(payload + DEFAULT_SIGN_KEY).digest()[-4:]
    return payload + sign


def encode_ack_reply(original_packet: bytes, ack_code: int, value_override: float | None = None) -> bytes:
    """Builds an ACK response. Optionally override the value field."""
    if len(original_packet) != 14:
        raise ValueError("Expected 14-byte binary packet")

    uid = original_packet[0:4]
    sig_id = original_packet[4]

    if value_override is None:
        value = original_packet[6:10]
    else:
        if sig_id == VERSION_SIG_ID:
            # Version constant must be sent as raw 32-bit integer
            value = struct.pack('>I', int(value_override) & 0xFFFFFFFF)
        else:
            value = struct.pack('>f', float(value_override))

    ack_type = bytes([ack_code])
    payload = uid + bytes([sig_id]) + ack_type + value
    sign = hashlib.sha256(payload + DEFAULT_SIGN_KEY).digest()[-4:]
    return payload + sign




def _build_config_pages(config_json: str):
    """Split the config JSON string into pages and return encoded packets."""
    total_len = len(config_json)
    total_pages = (total_len + CONFIG_CHUNK_SIZE - 1) // CONFIG_CHUNK_SIZE
    packets = []
    for page in range(total_pages):
        start = page * CONFIG_CHUNK_SIZE
        chunk = config_json[start:start + CONFIG_CHUNK_SIZE]
        payload = {
            "page": page + 1,
            "total_pages": total_pages,
            "len": total_len,
            "flags": 0,
            "data": chunk,
        }
        packets.append(encode_packet("CONFIG", payload))
    return packets


def send_config_pages(addr, config_json: str):
    """Send the config JSON to the correct client address and port."""
    total_len = len(config_json)
    # This log now correctly shows the client's actual ephemeral port
    print(f"[UDP] CONFIG_REQUEST: Sending {total_len} bytes back to {addr[0]}:{addr[1]}")

    config_packets = _build_config_pages(config_json)
    for idx, packet in enumerate(config_packets):
        # *** FIX: Send directly to the client's address (addr) ***
        udp_sock.sendto(packet, addr)
        print(f"[UDP]   → Sent CONFIG page {idx+1}/{len(config_packets)} ({len(packet)} bytes) to {addr}")
        nb_sleep(0.01) # Keep a small delay to prevent flooding


# --- UDP Packet Handling ---

def handle_udp_packet(data: bytes, addr) -> None:
    """Process a single 14-byte UDP packet from a web client."""
    if len(data) != 14:
        return

    first_10_bytes = data[0:10]
    signature = data[10:14]
    expected_sig = hashlib.sha256(first_10_bytes + DEFAULT_SIGN_KEY).digest()[-4:]

    sig_id = first_10_bytes[4]
    sig_type = first_10_bytes[5]
    value_bytes = first_10_bytes[6:10]

    if sig_id == 0x00 and sig_type == 0x21 and value_bytes == CONFIG_KEY:
        cfg = load_config(CONFIG_FILE_PATH)
        if cfg and "display_config" in cfg:
            dat = json.dumps(cfg["display_config"])
            for packet in _build_config_pages(dat):
                udp_sock.sendto(packet, addr)
                nb_sleep(0.01)
        return

    if sig_id > 0x01 and signature != expected_sig:
        ack = encode_ack_reply(data, ACK_ERROR_SIGN)
        udp_sock.sendto(ack, addr)
        return

    if sig_id == VERSION_SIG_ID and sig_type == SIG_TYPE_GET:
        ack = encode_ack_reply(data, ACK_OK, SIG_VERSION)
        udp_sock.sendto(ack, addr)
        return

    info = SIGNALS_BY_ID.get(sig_id)
    if not info:
        ack = encode_ack_reply(data, ACK_ERROR_UNKNOWN)
        udp_sock.sendto(ack, addr)
        return

    name = info.name
    value = struct.unpack('>f', value_bytes)[0]

    print(f"[UDP] RX cmd 0x{sig_type:02X} for {name} → {value} from {addr}")

    if CURRENT_MODE == "local" and sig_type in (SIG_TYPE_SET, SIG_TYPE_ADD, SIG_TYPE_MULT) and name != "mode_set":
        ack = encode_ack_reply(data, ACK_ERROR_NOTALLOWED)
        udp_sock.sendto(ack, addr)
        print(f"[UDP] Ignoring {name} command while in local mode")
        return

    if sig_type in (SIG_TYPE_SET, SIG_TYPE_ADD, SIG_TYPE_MULT):
        with DATA_LOCK:
            current = get_signal_value(name)

            if sig_type == SIG_TYPE_SET:
                # Preserve legacy behaviour where boolean values toggle when a SET
                # operation is received.
                op = "toggle" if name in BOOL_NAMES else "set"
            elif sig_type == SIG_TYPE_ADD:
                op = "add"
            else:  # SIG_TYPE_MULT
                op = "mult"

            new_val = apply_math_operation(current, value, op=op)
            if name in ("volt_set", "curr_set") and new_val < 0:
                new_val = 0
            update_and_broadcast(name, new_val, src="udp")


    elif sig_type == SIG_TYPE_GET:
        with DATA_LOCK:
            current_val = get_signal_value(name)
        ack = encode_ack_reply(data, ACK_OK, current_val)
        udp_sock.sendto(ack, addr)

    elif sig_type in (
        ACK_OK,
        ACK_ERROR_UNKNOWN,
        ACK_ERROR_NOTALLOWED,
        ACK_ERROR_OUTOFRANGE,
        ACK_ERROR_NOTREADY,
        ACK_ERROR_SIGN,
    ):
        print(f"[UDP] Received ACK packet type 0x{sig_type:02X}; ignoring")
        return

    else:
        print(f"[UDP] Unknown sig_type 0x{sig_type:02X}; ignoring")
        return

# In portenta_linux_bridge.py

def udp_listener():
    """UDP listener that handles 14-byte binary packets."""
    print(f"[UDP] Listening for Web clients on UDP port {UDP_PORT}...")

    while True:
        try:
            data, addr = udp_sock.recvfrom(8192)

            # Immediately register any device that sends a packet
            WEB_CLIENTS.add(addr)
            # print(f"[UDP] Received packet from {addr}, client list size: {len(WEB_CLIENTS)}")

            if len(data) == 14:
                handle_udp_packet(data, addr)
                continue

        except BlockingIOError:
            nb_sleep(0.01) # Can be faster
        except Exception as e:
            print(f"[UDP] Listener error: {e}")
            nb_sleep(0.05)


def process_giga_event(event_data):
    """Processes an event received from the Giga Display."""
    if not isinstance(event_data, dict):
        print(f"[Logic] Invalid event data structure from Giga: {event_data}")
        return
    if event_data.get("error") == "invalid_json":
        return

    event = event_data.get("display_event") or event_data
    if not isinstance(event, dict):
        print(f"[Logic] Received invalid 'display_event': {event}")
        return

    print(f"[Logic]  Full Giga UI event: {json.dumps(event, indent=2)}")

    event_type = event.get("type")
    event_action = event.get("action")
    event_dest = event.get("dest", "linux")
    name = event.get("name") or event.get("action")
    value = event.get("value", 0)
    do_type = event.get("do", "set")

    # When the system is in remote mode, ignore interactive inputs coming from
    # the Giga's touch UI.  The only exception is the mode_set signal itself so
    # that the user can always switch back to local mode.
    if CURRENT_MODE == "remote" and event_type in ("set_value", "button_press") and name != "mode_set":
        print(f"[Logic] Ignoring Giga event '{name}' while in remote mode")
        return

    # --- Config Request ---
    if event_type == "get" and event_action == "config":
        print("[Logic] Giga requested config file.")
        current_config_data = load_config(CONFIG_FILE_PATH)
        if current_config_data and "display_config" in current_config_data:
            print("[Logic] Sending display_config section back to Giga...")
            nb_sleep(0.1)
            send_to_giga({"display_config": current_config_data["display_config"]})

            global giga_ui_ready, giga_ui_ready_time
            giga_ui_ready_time = time.time() + 4.0
            giga_ui_ready = True
            print("[Logic] Config sent. UI updates will begin in 4 seconds.")
            return
        elif current_config_data:
            send_to_giga({"display_event": {"type": "error", "message": "Config invalid - missing display_config"}})
        else:
            send_to_giga({"display_event": {"type": "error", "message": "Config file not found or unreadable"}})
        return

    # --- Value Query ---
    if event_type == "get_value" and name:
        with DATA_LOCK:
            if name == "SW_GET_VERSION":
                current_val = SIG_VERSION
            else:
                current_val = TRUE_VALUES.get(name, 0)
        send_to_giga({
            "display_event": {
                "type": "set_value",
                "name": name,
                "value": current_val,
                "src": "linux"
            }
        })
        print(f"[Logic] GIGA requested value for {name} → {current_val}")
        return

    # --- Deprecated Set ---
    if event_type == "set_value" and name:
        print(f"[Logic] GIGA set {name} = {value}")
        update_and_broadcast(name, value, src="giga")
        return

    # --- Button Press ---
    if event_type == "button_press":
        if not name:
            print("[Logic] Missing 'name' in button_press event.")
            return

        if event_dest == "uc":
            with DATA_LOCK:
                current_val = TRUE_VALUES.get(name, 0)

            op = do_type
            if do_type == "toggle" or name in BOOL_NAMES:
                op = "toggle"

            new_val = apply_math_operation(current_val, value, op=op)
            print(f"[Logic] Applied {op} on '{name}': {current_val} -> {new_val}")

            payload = json.dumps({"display_event": {"name": name, "value": new_val}})
            call_m4_rpc("process_event_in_uc", payload)
            update_and_broadcast(name, new_val, src="uc")
            return

        elif event_dest == "linux":
            if name == "SW_GET_VERSION":
                # Always return the correct version and block overwrites
                print(f"[Logic] SW_GET_VERSION = {SIG_VERSION} ({version_int_to_ascii(SIG_VERSION)})")
                send_to_giga({
                    "display_event": {
                        "type": "set_value",
                        "name": name,
                        "value": 0x57450301,
                        "src": "linux"
                    }
                })
                return

            with DATA_LOCK:
                current_val = TRUE_VALUES.get(name, 0)

            if do_type == "get":
                print(f"[Logic] Button requested GET on '{name}' → {current_val}")
                send_to_giga({
                    "display_event": {
                        "type": "set_value",
                        "name": name,
                        "value": current_val,
                        "src": "linux"
                    }
                })
                return

            new_val = apply_math_operation(current_val, value, op=do_type)
            print(f"[Logic] Applied {do_type} on '{name}': {current_val} -> {new_val}")
            update_and_broadcast(name, new_val, src="linux")
            return

        else:
            print(f"[Logic] Unknown destination for button press: {event_dest}")


def poll_m4_signals():
    """Poll voltage (2dp), current (2dp), and enable using one 64-bit packed RPC."""
    packed = call_m4_rpc("get_poll_data", retries=0, timeout=0.5)
    if isinstance(packed, int):
        mask19 = (1 << 19) - 1

        volt_x100 =  packed        & mask19          # 19 bits
        curr_x100 = (packed >> 19) & mask19          # 19 bits
        ext_en    = (packed >> 38) & 0x1             # 1 bit

        volt = volt_x100 / 100.0
        curr = curr_x100 / 100.0

        update_and_broadcast("volt_act", round(volt, 2), src="rpc")
        update_and_broadcast("curr_act", round(curr, 2), src="rpc")
        update_and_broadcast("ext_enable", int(ext_en), src="rpc")
        

def verify_m4_rpc_bindings():
    """Print a simple PASS/FAIL table for key M4 RPC bindings."""
    # Build a truth-table payload that doesn't change state (uses current values)
    with DATA_LOCK:
        truth_subset = {
            "volt_set": get_signal_value("volt_set"),
            "curr_set": get_signal_value("curr_set"),
            "ext_enable": bool(get_signal_value("ext_enable")),
            "warn_lamp": bool(get_signal_value("warn_lamp")),
            "charger_relay": bool(get_signal_value("charger_relay")),
            "dump_relay": bool(get_signal_value("dump_relay")),
            "dump_fan": bool(get_signal_value("dump_fan")),
        }
    noop_evt = json.dumps({"display_event": {"name": "noop", "value": 0}})
    truth_json = json.dumps(truth_subset)

    checks = [
        ("get_poll_data",      ()),
        ("has_sync_completed", ()),
        ("process_event_in_uc",(noop_evt,)),
        ("set_truth_table",    (truth_json,)),
        ("volt_act",           ()),
        ("curr_act",           ()),
        ("ext_enable",      ()),
        ("igbt_fault",       ()),
        # add/remove bindings as needed
    ]

    name_w = max(len(n) for n, _ in checks)
    print("[Init][RPC] Binding check:")
    print(f"  {'Function':{name_w}}  Status")
    print(f"  {'-'*name_w}  ------")

    passed = 0
    for name, args in checks:
        res = call_m4_rpc(name, *args, retries=1, timeout=0.3)
        ok = (res is not None)
        print(f"  {name:{name_w}}  {'PASS' if ok else 'FAIL'}")
        if ok:
            passed += 1

    print(f"[Init][RPC] Summary: {passed}/{len(checks)} passed.")



if __name__ == "__main__":
    print("\n============================================")
    print("== Portenta Linux Giga/M4 Comms Hub (Config-Driven Polling) ==")
    print("============================================\n")

    config_data = load_config(CONFIG_FILE_PATH)
    if config_data is None:
        print("[Init] CRITICAL: Failed to load configuration. Exiting.")
        exit(1)

    initialize_true_values(config_data)
    log_all_configurable_values()

    # Load polling configuration from config before running validation
    m4_polls_config = config_data.get("m4_data_polls", [])
    build_poll_name_map(m4_polls_config)

    print("[Init] Checking m4_data_polls name/func pairs...")
    for poll in m4_polls_config:
        func = poll.get("poll_rpc_func")
        name = poll.get("giga_json_template", {}).get("display_event", {}).get("name")
        print(f"  - func: {func:16s} → name: {name}")
        if func != name:
            print(" MISMATCH: Poll function name does not match UI target name!") 
            
    initialize_true_values(config_data)
    log_all_configurable_values() 
    
    verify_m4_rpc_bindings()
    

    udp_thread = threading.Thread(target=udp_listener, daemon=True)
    udp_thread.start()

    #sync_thread = threading.Thread(target=check_and_sync_m4, daemon=True)
    #sync_thread.start()

    try:
        ser = serial.Serial(GIGA_UART_PORT, GIGA_BAUD_RATE, timeout=0.05) # Reduced timeout
        print(f"[Init] Serial port {GIGA_UART_PORT} opened successfully.")
        send_to_giga({"display_status": {"stage": "Waiting for config", "detail": ""}})
        if config_data and "display_config" in config_data:
            print("[Init] Sending display_config to Giga...")
            send_to_giga({"display_config": config_data["display_config"]})
            giga_ui_ready_time = time.time() + 4.0
            giga_ui_ready = True
            print("[Init] Display_config sent. UI updates will begin in 4 seconds.")
        send_to_giga({"display_status": {"stage": "Bridge running", "detail": ""}})
    except serial.SerialException as e:
        print(f"[Init] Error opening serial port {GIGA_UART_PORT}: {e}")
        exit(1)

    print("[Init] Starting main loop...")
    last_broadcast_time = 0
    last_poll_time = 0
    BROADCAST_INTERVAL = 0.2  # 5 Hz
    POLL_INTERVAL = 0.05 # 10 Hz

    try:
        while True:
            current_time = time.time()

            # --- Read from Giga (as fast as possible) ---
            giga_event = read_from_giga()
            if giga_event:
                process_giga_event(giga_event)

            # --- Poll M4 at a fixed interval ---
            if current_time - last_poll_time > POLL_INTERVAL:
                poll_m4_signals()
                last_poll_time = current_time

            # --- Broadcast to Giga at a fixed interval ---
            if current_time - last_broadcast_time > BROADCAST_INTERVAL:
                broadcast_all_values()
                last_broadcast_time = current_time

            # A small sleep to prevent the loop from busy-waiting and consuming 100% CPU
            nb_sleep(0.01)

    except KeyboardInterrupt:
        print("\n[Shutdown] Keyboard interrupt received. Exiting.")
    except Exception as e:
        print(f"\n[Error] Unexpected error in main loop: {e}")
    finally:
        if ser and ser.is_open:
            ser.close()
            print("[Shutdown] Serial port closed.")
        print("[Shutdown] Program terminated.")