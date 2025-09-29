const socket = io("ttp://0.0.0.0:8009", {
  transports: ["websocket", "polling"]
});

let logBuffer = [];
let uiReady = false;
let pendingUpdates = [];

const deviceInfo = window.DEVICE_INFO || {};
const allDeviceIds = Object.keys(deviceInfo);
const selectedDevices = new Set();
const deviceValues = {};
const deviceModes = {};

const deviceStatus = {};
const DEVICE_OFFLINE_TIMEOUT_MS = 8000;

let trackedSignals = [];
const signalDisplayMap = {};
const valueDefinitions = {};

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

function getDeviceLabel(deviceId) {
  const info = deviceInfo[deviceId];
  return info ? info.label : deviceId;
}

function getSelectedDeviceOrder() {
  return Array.from(selectedDevices).sort((a, b) => getDeviceLabel(a).localeCompare(getDeviceLabel(b)));
}


function getDeviceStatus(deviceId) {
  if (!deviceStatus[deviceId]) {
    deviceStatus[deviceId] = { online: false, lastSeen: 0 };
  }
  return deviceStatus[deviceId];
}

function getModeLabel(deviceId) {
  const status = getDeviceStatus(deviceId);
  if (!status.online) return "--";
  const modeValue = parseFloat(deviceModes[deviceId]);
  if (Number.isNaN(modeValue)) return "Pending";
  return modeValue >= 0.5 ? "Remote" : "Local";
}

function getStatusText(deviceId) {
  const status = getDeviceStatus(deviceId);
  return status.online ? "Online" : "Offline";
}

function updateDeviceRowState(deviceId) {
  const status = getDeviceStatus(deviceId);
  const table = document.getElementById("device-values-table");
  if (!table) return;
  const row = table.querySelector(`tr[data-device-row="${deviceId}"]`);
  if (!row) return;
  row.classList.toggle("offline", !status.online);
  const statusCell = row.querySelector(`td[data-status-for="${deviceId}"]`);
  if (statusCell) {
    statusCell.textContent = getStatusText(deviceId);
  }
  const modeCell = row.querySelector(`td[data-mode-for="${deviceId}"]`);
  if (modeCell) {
    modeCell.textContent = getModeLabel(deviceId);
  }
}

function updateAllDeviceRowStates() {
  allDeviceIds.forEach(updateDeviceRowState);
}

function updateDeviceRowInfo(deviceId) {
  const table = document.getElementById("device-values-table");
  if (!table) return;
  const labelCell = table.querySelector(`td[data-label-for="${deviceId}"]`);
  if (labelCell) {
    labelCell.textContent = getDeviceLabel(deviceId);
  }
  const ipCell = table.querySelector(`td[data-ip-for="${deviceId}"]`);
  if (ipCell) {
    const info = deviceInfo[deviceId];
    ipCell.textContent = info?.ip || "--";
  }
}

function updateDeviceCardStatus(deviceId) {
  const status = getDeviceStatus(deviceId);
  const indicator = document.querySelector(`.status-indicator[data-status-indicator="${deviceId}"]`);
  const statusText = document.querySelector(`.status-text[data-status-text="${deviceId}"]`);
  const modeText = document.querySelector(`.status-mode[data-status-mode="${deviceId}"]`);

  if (indicator) {
    indicator.classList.toggle("online", status.online);
    indicator.classList.toggle("offline", !status.online);
  }

  if (statusText) {
    statusText.textContent = getStatusText(deviceId);
  }

  if (modeText) {
    modeText.textContent = `Mode: ${getModeLabel(deviceId)}`;
  }
}

function setDeviceOnline(deviceId) {
  const status = getDeviceStatus(deviceId);
  status.online = true;
  status.lastSeen = Date.now();
  updateDeviceCardStatus(deviceId);
  updateDeviceRowState(deviceId);
  updateModeIndicator();
  updateCommandButtonStates();
}

function setDeviceOffline(deviceId) {
  const status = getDeviceStatus(deviceId);
  if (!status.online) return;
  status.online = false;
  status.lastSeen = Date.now();
  updateDeviceCardStatus(deviceId);
  updateDeviceRowState(deviceId);
  updateModeIndicator();
  updateCommandButtonStates();
}

function checkDeviceTimeouts() {
  const now = Date.now();
  allDeviceIds.forEach((deviceId) => {
    const status = getDeviceStatus(deviceId);
    if (!status.online) return;
    if (now - status.lastSeen > DEVICE_OFFLINE_TIMEOUT_MS) {
      setDeviceOffline(deviceId);
    }
  });
}


function updateModeIndicator() {
  const indicator = document.getElementById("mode-indicator");
  const header = document.querySelector("header");
  if (!indicator || !header) return;

  if (selectedDevices.size === 0) {
    indicator.textContent = "Mode: Select target(s)";

    indicator.classList.remove("remote", "local");
    header.classList.remove("mode-remote", "mode-local");
    return;
  }


  const selectedOrder = getSelectedDeviceOrder();
  const remoteDevices = [];
  const localDevices = [];
  const pendingDevices = [];
  const offlineDevices = [];

  selectedOrder.forEach((deviceId) => {
    const label = getDeviceLabel(deviceId);
    const status = getDeviceStatus(deviceId);

    if (!status.online) {
      offlineDevices.push(label);
      return;
    }

    const modeValue = parseFloat(deviceModes[deviceId]);

    if (Number.isNaN(modeValue)) {
      pendingDevices.push(label);
    } else if (modeValue >= 0.5) {
      remoteDevices.push(label);
    } else {
      localDevices.push(label);
    }
  });

  const segments = [];
  if (remoteDevices.length) segments.push(`Remote: ${remoteDevices.join(", ")}`);
  if (localDevices.length) segments.push(`Local: ${localDevices.join(", ")}`);
  if (pendingDevices.length) segments.push(`Pending: ${pendingDevices.join(", ")}`);
  if (offlineDevices.length) segments.push(`Offline: ${offlineDevices.join(", ")}`);

  if (!segments.length) {
    indicator.textContent = "Mode: Awaiting data...";
    indicator.classList.remove("remote", "local");
    header.classList.remove("mode-remote", "mode-local");
    return;
  }

  indicator.textContent = `Modes — ${segments.join(" | ")}`;

  const allRemote = remoteDevices.length && !localDevices.length && !pendingDevices.length;
  const allLocal = localDevices.length && !remoteDevices.length && !pendingDevices.length;

  indicator.classList.toggle("remote", allRemote);
  indicator.classList.toggle("local", allLocal);

  if (allRemote) {
    header.classList.add("mode-remote");
    header.classList.remove("mode-local");
  } else if (allLocal) {
    header.classList.add("mode-local");
    header.classList.remove("mode-remote");
  } else {
    header.classList.remove("mode-remote", "mode-local");
    indicator.classList.remove("remote", "local");
  }
}

function updateCommandButtonStates() {
  const buttons = document.querySelectorAll("button.btn-command");
  const hasSelection = selectedDevices.size > 0;
  const selectedOrder = getSelectedDeviceOrder();
  const allRemote = hasSelection && selectedOrder.every((deviceId) => {
    const status = getDeviceStatus(deviceId);
    if (!status.online) return false;
    const modeValue = parseFloat(deviceModes[deviceId]);
    return !Number.isNaN(modeValue) && modeValue >= 0.5;
  });


  buttons.forEach((btn) => {
    const name = btn.dataset.name;
    const mode = btn.dataset.mode || "set";

    if (!name) {
      btn.disabled = false;
      return;
    }

    const isGet = mode === "get";
    const isModeCommand = name === "mode_set";

    if (!hasSelection) {
      btn.disabled = true;
      return;
    }

    if (isGet || isModeCommand) {
      btn.disabled = false;
      return;
    }

    btn.disabled = !allRemote;
  });
}

function formatValueWithFormat(value, fmt) {
  const formatStr = typeof fmt === "string" ? fmt : "";
  const numericValue = typeof value === "number" ? value : parseFloat(value);

  if (!Number.isFinite(numericValue)) {
    if (value === null || value === undefined || value === "") {
      return "--";
    }
    return String(value);
  }

  if (formatStr.toLowerCase() === "ascii") {
    const intVal = Math.floor(numericValue);
    const bytes = [(intVal >> 24) & 0xff, (intVal >> 16) & 0xff, (intVal >> 8) & 0xff, intVal & 0xff];
    const prefix = String.fromCharCode(bytes[0], bytes[1]);
    return prefix + bytes[3].toString().padStart(2, "0") + bytes[2].toString().padStart(2, "0");
  }

  if (formatStr.includes("%08X")) {
    const intVal = Math.floor(numericValue);
    return "0x" + intVal.toString(16).toUpperCase().padStart(8, "0");

  }

  return Number(numericValue).toFixed(2);
}

function formatSignalValue(name, value) {
  const def = valueDefinitions[name] || {};
  return formatValueWithFormat(value, def.display_format);
}

function updateSpanDisplay(span, value) {
  const fmt = span.dataset.displayFormat || "";
  const formattedText = formatValueWithFormat(value, fmt);
  if (span.textContent !== formattedText) {
    span.textContent = formattedText;
  }


  const numericValue = typeof value === "number" ? value : parseFloat(value);
  const upperLimit = parseFloat(span.dataset.upperOverrideVal);
  const lowerLimit = parseFloat(span.dataset.lowerOverrideVal);
  let newColor = "#111827";

  if (Number.isFinite(numericValue)) {
    if (Number.isFinite(upperLimit) && numericValue >= upperLimit) {
      newColor = "red";
    } else if (Number.isFinite(lowerLimit) && numericValue < lowerLimit) {
      newColor = "blue";
    }
  }

  if (span.style.color !== newColor) {
    span.style.color = newColor;
  }
}

// --- Polling (Uses the dedicated /api/get_signal endpoint) ---
function getActiveTargets(shouldShowError = true) {
  if (selectedDevices.size === 0) {
    if (shouldShowError) {
      showErrorMessage("Select at least one device to send commands.");
    }
    return null;
  }
  return getSelectedDeviceOrder();
}

function sendGetRequestSignal(name, targets, suppressError = false) {
  let targetIds = Array.isArray(targets) ? targets.slice() : null;
  if (!targetIds || !targetIds.length) {
    targetIds = getActiveTargets(!suppressError);
    if (!targetIds) return;
  }

  fetch("/api/get_signal", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ name, value: 0, do: "get", targets: targetIds })
  })
  .then(res => res.ok ? res.json() : Promise.reject(res.statusText))
  .catch(err => {
    if (!suppressError) {
      showErrorMessage(`Error requesting ${name}: ${err}`);
    }
  });
}

function startGetPolling(names, interval = 1000) {
  if (!names.length || !allDeviceIds.length) return;
  setInterval(() => {
    names.forEach((name) => sendGetRequestSignal(name, allDeviceIds, true));
  }, interval);
}

function renderDeviceTable() {
  const table = document.getElementById("device-values-table");
  const emptyMessage = document.getElementById("device-table-empty");
  if (!table || !emptyMessage) return;

  table.innerHTML = "";


  const hasSignals = trackedSignals.length > 0;
  emptyMessage.style.display = hasSignals ? "none" : "block";

  const thead = document.createElement("thead");
  const headerRow = document.createElement("tr");
  ["Device", "IP", "Status", "Mode"].forEach((heading) => {
    const th = document.createElement("th");
    th.textContent = heading;
    headerRow.appendChild(th);
  });

  trackedSignals.forEach((signalName) => {
    const th = document.createElement("th");
    th.textContent = signalDisplayMap[signalName] || signalName;

    headerRow.appendChild(th);
  });

  thead.appendChild(headerRow);
  table.appendChild(thead);

  const tbody = document.createElement("tbody");

  const sortedDevices = allDeviceIds.slice().sort((a, b) => getDeviceLabel(a).localeCompare(getDeviceLabel(b)));

  sortedDevices.forEach((deviceId) => {
    const row = document.createElement("tr");
    row.dataset.deviceRow = deviceId;

    const labelCell = document.createElement("td");
    labelCell.dataset.labelFor = deviceId;
    labelCell.textContent = getDeviceLabel(deviceId);
    row.appendChild(labelCell);

    const ipCell = document.createElement("td");
    ipCell.dataset.ipFor = deviceId;
    const info = deviceInfo[deviceId];
    ipCell.textContent = info?.ip || "--";
    row.appendChild(ipCell);

    const statusCell = document.createElement("td");
    statusCell.dataset.statusFor = deviceId;
    statusCell.textContent = getStatusText(deviceId);
    row.appendChild(statusCell);

    const modeCell = document.createElement("td");
    modeCell.dataset.modeFor = deviceId;
    modeCell.textContent = getModeLabel(deviceId);
    row.appendChild(modeCell);

    trackedSignals.forEach((signalName) => {

      const cell = document.createElement("td");
      cell.dataset.device = deviceId;
      cell.dataset.signal = signalName;
      const value = deviceValues[deviceId]?.[signalName];
      cell.textContent = formatSignalValue(signalName, value);
      row.appendChild(cell);
    });

    tbody.appendChild(row);
  });

  table.appendChild(tbody);

  updateAllDeviceRowStates();

}

function updateDeviceTableCell(deviceId, signalName) {
  const table = document.getElementById("device-values-table");
  if (!table) return;
  const cell = table.querySelector(`td[data-device="${deviceId}"][data-signal="${signalName}"]`);
  if (!cell) return;
  const value = deviceValues[deviceId]?.[signalName];
  cell.textContent = formatSignalValue(signalName, value);
}

function refreshValueGrid() {
  const selectedOrder = getSelectedDeviceOrder();
  const primaryDevice = selectedOrder.length === 1 ? selectedOrder[0] : null;
  const namesToUpdate = trackedSignals.length ? trackedSignals : Object.keys(valueDefinitions);

  namesToUpdate.forEach((signalName) => {
    const span = document.getElementById(`value-${signalName}`);
    if (!span) return;

    if (!primaryDevice) {
      span.textContent = selectedOrder.length ? "—" : "--";
      span.style.color = "#111827";
      return;
    }

    const value = deviceValues[primaryDevice]?.[signalName];
    if (value === undefined) {
      span.textContent = "--";
      span.style.color = "#111827";
    } else {
      updateSpanDisplay(span, value);
    }
  });
}

function initializeDeviceSelector() {
  const checkboxes = document.querySelectorAll(".device-checkbox");

  allDeviceIds.forEach((deviceId) => {
    if (!deviceValues[deviceId]) {
      deviceValues[deviceId] = {};
    }
  });

  checkboxes.forEach((checkbox) => {
    const deviceId = checkbox.dataset.device;
    if (!deviceId) return;
    const option = checkbox.closest(".device-option");

    const syncState = () => {
      if (checkbox.checked) {
        selectedDevices.add(deviceId);
      } else {
        selectedDevices.delete(deviceId);
      }
      if (option) {
        option.classList.toggle("selected", checkbox.checked);
      }
    };


    getDeviceStatus(deviceId);
    updateDeviceCardStatus(deviceId);

    syncState();

    checkbox.addEventListener("change", () => {
      syncState();
      if (checkbox.checked && trackedSignals.length) {
        trackedSignals.forEach((signalName) => sendGetRequestSignal(signalName, [deviceId], true));
      }
      if (selectedDevices.size > 0) {
        clearErrorMessage();
      }

      refreshValueGrid();
      updateModeIndicator();
      updateCommandButtonStates();
    });
  });

  renderDeviceTable();
  refreshValueGrid();
  updateModeIndicator();
  updateCommandButtonStates();
}

// --- UI Building ---
function buildUI(displayConfig) {
  const container = document.getElementById("panels");
  container.innerHTML = "";
  document.getElementById('title').textContent = displayConfig.title || 'PSU Control Dashboard';
  document.title = displayConfig.title || 'PSU Control';

  trackedSignals = [];
  const seenSignals = new Set();
  Object.keys(signalDisplayMap).forEach((key) => delete signalDisplayMap[key]);
  Object.keys(valueDefinitions).forEach((key) => delete valueDefinitions[key]);

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
      panel.values.forEach(val => {

        if (val.name && !seenSignals.has(val.name)) {
          seenSignals.add(val.name);
          trackedSignals.push(val.name);
          signalDisplayMap[val.name] = val.label || val.display_name || val.name;
        }

        if (val.name) {
          valueDefinitions[val.name] = {
            display_format: val.display_format,
            upper_override_val: val.upper_override_val,
            lower_override_val: val.lower_override_val,
          };
        }

      });

      const helper = document.createElement("p");
      helper.className = "panel-helper";
      helper.textContent = "Live readings for these signals are available in the C&I Status Overview table.";
      panelDiv.appendChild(helper);
    }
    container.appendChild(panelDiv);
  });
  renderDeviceTable();
  refreshValueGrid();
  updateModeIndicator();
  updateCommandButtonStates();

  uiReady = true;
  pendingUpdates.forEach(applyUpdate);
  pendingUpdates = [];
}

function applyUpdate(data) {
  const { device, name } = data;
  if (!device || !name) return;

  const rawValue = data.value;
  const numericValue = typeof rawValue === "number" ? rawValue : parseFloat(rawValue);
  const storedValue = Number.isFinite(numericValue) ? numericValue : rawValue;

  if (data.device_label || data.ip) {
    if (!deviceInfo[device]) {
      deviceInfo[device] = { label: data.device_label || device, ip: data.ip };
    } else {
      if (data.device_label) deviceInfo[device].label = data.device_label;
      if (data.ip) deviceInfo[device].ip = data.ip;
    }

    updateDeviceRowInfo(device);
    updateDeviceCardStatus(device);

  }

  if (!deviceValues[device]) {
    deviceValues[device] = {};

  }

  deviceValues[device][name] = storedValue;

  setDeviceOnline(device);

  if (name === "mode_set") {
    deviceModes[device] = parseFloat(rawValue);
    updateDeviceCardStatus(device);
  }

  const selectedOrder = getSelectedDeviceOrder();
  if (selectedOrder.length === 1 && selectedOrder[0] === device) {
    const span = document.getElementById("value-" + name);
    if (span) {
      updateSpanDisplay(span, storedValue);
    }
  }

  updateDeviceTableCell(device, name);
  updateDeviceRowState(device);

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
        const uniqueNames = [...new Set(names)];
        startGetPolling(uniqueNames, 1000);
      } else {
        showErrorMessage("Invalid config from server");
      }
    })
    .catch(err => {
      showErrorMessage("Failed to fetch config: " + err.message);
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
      const targets = getActiveTargets();
      if (!targets) return;
      sendGetRequestSignal(name, targets);
      return;
    }

    const targets = getActiveTargets();
    if (!targets) return;

    const numericValue = parseFloat(value ?? "0");
    const payloadValue = Number.isFinite(numericValue) ? numericValue : 0;

    // Use the /api/command endpoint for all other actions
    fetch("/api/command", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ name, value: payloadValue, do: mode, targets }),
    })
    .then(res => res.json())
    .then(resp => {
      if (resp.status !== "sent") {
        showErrorMessage("Command failed");
      } else {
        clearErrorMessage();
      }
    })
    .catch(err => showErrorMessage("Command error: " + err.message));
  }
});

function showErrorMessage(message) {
  const err = document.getElementById("error");
  if (err) {
    err.textContent = message;
    err.style.display = "block";
  } else {
    alert(message);
  }
}

function clearErrorMessage() {
  const err = document.getElementById("error");
  if (err) {
    err.style.display = "none";
  }
}

document.addEventListener("DOMContentLoaded", () => {
  initializeDeviceSelector();
  initialFetchConfig();

  setInterval(checkDeviceTimeouts, 2000);

});
