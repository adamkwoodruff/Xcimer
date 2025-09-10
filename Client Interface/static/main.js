const socket = io("http://0.0.0.0:8009", {
  transports: ["websocket", "polling"]
});

let logBuffer = [];
let uiReady = false;
let pendingUpdates = [];

// --- WebSocket Handlers ---
socket.on("connect", () => console.log("[WS]  Connected to Socket.IO server"));
socket.on("disconnect", () => console.warn("[WS]  Disconnected from Socket.IO server"));

socket.on("update_value", (data) => {
  if (!uiReady) {
    pendingUpdates.push(data);
    return;
  }
  // Using a small timeout can help batch rapid updates smoothly
  setTimeout(() => applyUpdate(data), 10);
});

socket.on("log_entry", (entry) => handleLogEntry(entry));

/**
 * UPDATED: This function now controls the header border, the mode text,
 * and enables/disables remote-only command buttons.
 * @param {string|number} value The mode value from the server.
 */
function updateModeIndicator(value) {
  const indicator = document.getElementById("mode-indicator");
  const header = document.querySelector("header");
  if (!indicator || !header) return;

  const num = parseFloat(value);
  if (isNaN(num) || num < 0) {
    indicator.textContent = "Mode: Unknown";
    indicator.classList.remove("remote", "local");
    header.classList.remove("mode-remote", "mode-local");
    return;
  }

  const isRemote = num >= 0.5;

  // Update text and classes for text color
  indicator.textContent = isRemote ? "Remote Mode" : "Local Mode";
  indicator.classList.toggle("remote", isRemote);
  indicator.classList.toggle("local", !isRemote);

  // Update header class to control the border color
  header.classList.toggle("mode-remote", isRemote);
  header.classList.toggle("mode-local", !isRemote);

  // Disable buttons that are not 'get' requests when not in remote mode
  document.querySelectorAll(".btn-command").forEach(btn => {
    const name = btn.dataset.name;
    const mode = btn.dataset.mode || "set";
    // Any button that is not a 'get' request and isn't the mode switch
    // itself is considered a remote-only action.
    if (name !== "mode_set" && mode !== "get") {
      btn.disabled = !isRemote;
    }
  });
}

function updateSpanDisplay(span, value) {
  const valNum = typeof value === "number" ? value : parseFloat(value);
  const upperLimit = parseFloat(span.dataset.upperOverrideVal);
  const lowerLimit = parseFloat(span.dataset.lowerOverrideVal);
  const fmt = span.dataset.displayFormat || "";

  let newTextContent;
  if (fmt.toLowerCase() === "ascii") {
    const intVal = Math.floor(valNum);
    const bytes = [(intVal >> 24) & 0xff, (intVal >> 16) & 0xff, (intVal >> 8) & 0xff, intVal & 0xff];
    const prefix = String.fromCharCode(bytes[0], bytes[1]);
    newTextContent = prefix + bytes[3].toString().padStart(2, "0") + bytes[2].toString().padStart(2, "0");
  } else if (fmt.includes("%08X")) {
    const intVal = Math.floor(valNum);
    newTextContent = "0x" + intVal.toString(16).toUpperCase().padStart(8, "0");
  } else {
    newTextContent = !isNaN(valNum) ? Number(valNum).toFixed(2) : value;
  }
  
  // Only update the DOM if the text has changed
  if (span.textContent !== newTextContent) {
    span.textContent = newTextContent;
  }

  let newColor = "black";
  if (!isNaN(upperLimit) && valNum >= upperLimit) newColor = "red";
  else if (!isNaN(lowerLimit) && valNum < lowerLimit) newColor = "blue";
  
  if (span.style.color !== newColor) {
    span.style.color = newColor;
  }
}

// --- Polling (Uses the dedicated /api/get_signal endpoint) ---
function sendGetRequestSignal(name) {
  fetch("/api/get_signal", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ name, value: 0, do: "get" })
  })
  .then(res => res.ok ? res.json() : Promise.reject(res.statusText))
  .catch(err => showError(`Error requesting ${name}: ${err}`));
}

function startGetPolling(names, interval = 1000) {
  setInterval(() => {
    names.forEach(name => sendGetRequestSignal(name));
  }, interval);
}

// --- UI Building ---
function buildUI(displayConfig) {
  const container = document.getElementById("panels");
  container.innerHTML = "";
  document.getElementById('title').textContent = displayConfig.title || 'PSU Control Dashboard';
  document.title = displayConfig.title || 'PSU Control';

  displayConfig.panels?.forEach((panel) => {
    const panelDiv = document.createElement("div");
    panelDiv.className = "panel";
    const title = document.createElement("h3");
    title.textContent = panel.title || "Unnamed Panel";
    panelDiv.appendChild(title);

    if (panel.buttons?.length > 0) {
      const buttonGrid = document.createElement("div");
      buttonGrid.className = "button-grid";
      panel.buttons.forEach(btn => {
        const button = document.createElement("button");
        button.className = "btn-command";
        button.textContent = btn.text;
        if (btn.action) {
          button.dataset.name = btn.action.name;
          button.dataset.value = btn.action.amount || "0";
          button.dataset.mode = btn.action.do || "set";
        }
        buttonGrid.appendChild(button);
      });
      panelDiv.appendChild(buttonGrid);
    }
    if (panel.values?.length > 0) {
      const valueGrid = document.createElement("div");
      valueGrid.className = "value-grid";
      panel.values.forEach(val => {
        const label = document.createElement("div");
        label.className = "value-label";
        label.textContent = val.name + ":";
        const field = document.createElement("div");
        field.className = "value-field";
        const span = document.createElement("span");
        span.id = "value-" + val.name;
        span.dataset.upperOverrideVal = val.upper_override_val;
        span.dataset.lowerOverrideVal = val.lower_override_val;
        span.dataset.displayFormat = val.display_format;
        field.appendChild(span);
        valueGrid.appendChild(label);
        valueGrid.appendChild(field);
        updateSpanDisplay(span, val.default_value ?? 0);
      });
      panelDiv.appendChild(valueGrid);
    }
    container.appendChild(panelDiv);
  });
  uiReady = true;
  pendingUpdates.forEach(applyUpdate);
  pendingUpdates = [];
}

function applyUpdate(data) {
  if (data.name === "mode_set") {
    updateModeIndicator(data.value);
  }
  const el = document.getElementById("value-" + data.name);
  if (el) {
    updateSpanDisplay(el, data.value);
  }
}

function initialFetchConfig() {
  fetch("/api/get_config")
    .then(res => {
      if (!res.ok) throw new Error("HTTP error " + res.status);
      return res.json();
    })
    .then(data => {
      if (data.display_config) {
        buildUI(data.display_config);
        const names = data.display_config.panels?.flatMap(p => p.values?.map(v => v.name).filter(Boolean)) || [];
        startGetPolling(names, 1000);
      } else {
        showError("Invalid config from server");
      }
    })
    .catch(err => {
      showError("Failed to fetch config: " + err.message);
    });
}

// --- Log Handling ---
function handleLogEntry(entry) {
  const container = document.getElementById("logContainer");
  if (!container) return;
  const div = document.createElement("div");
  div.textContent = `[${entry.timestamp}] ${entry.message}`;
  container.prepend(div);
  logBuffer.unshift(entry);
  if (logBuffer.length > 150) {
    logBuffer.pop();
    if (container.childNodes.length > 15) {
      container.removeChild(container.lastChild);
    }
  }
}

function downloadLogs() {
  let csv = "Timestamp,Message\n";
  logBuffer.slice().reverse().forEach((l) => {
    const msg = String(l.message).replace(/"/g, '""');
    csv += `${l.timestamp},"${msg}"\n`;
  });
  const blob = new Blob([csv], { type: "text/csv" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
  a.download = `session_logs_${timestamp}.csv`;
  a.click();
  URL.revokeObjectURL(url);
}

// --- Event Listeners ---
document.addEventListener("click", (e) => {
  if (e.target.id === "downloadLogsBtn") {
    downloadLogs();
  }
  if (e.target.classList.contains("btn-command")) {
    const { name, value, mode } = e.target.dataset;
    if (!name) return;

    // Use the dedicated 'get' function for polling buttons
    if (mode === "get") {
      sendGetRequestSignal(name);
      return;
    }

    // Use the /api/command endpoint for all other actions
    fetch("/api/command", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ name, value: parseFloat(value || "0"), do: mode }),
    })
    .then(res => res.json())
    .then(resp => {
      if (resp.status !== "sent") showError("Command failed");
    })
    .catch(err => showError("Command error: " + err.message));
  }
});

function showError(message) {
  const err = document.getElementById("error");
  if (err) {
    err.textContent = message;
    err.style.display = "block";
  } else {
    alert(message);
  }
}

document.addEventListener("DOMContentLoaded", initialFetchConfig);
