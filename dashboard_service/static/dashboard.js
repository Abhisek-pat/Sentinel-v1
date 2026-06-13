const $ = (id) => document.getElementById(id);
const number = (value, digits = 1) => value == null ? "--" : Number(value).toFixed(digits);
const escapeHtml = (value) => String(value)
  .replaceAll("&", "&amp;")
  .replaceAll("<", "&lt;")
  .replaceAll(">", "&gt;")
  .replaceAll('"', "&quot;")
  .replaceAll("'", "&#039;");

function duration(seconds) {
  if (seconds == null) return "--";
  const hours = Math.floor(seconds / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  return `${hours}h ${minutes}m`;
}

function setMeter(id, labelId, value) {
  const percent = value == null ? 0 : Math.min(100, Math.max(0, value));
  $(id).style.width = `${percent}%`;
  $(labelId).textContent = value == null ? "--" : `${number(value)}%`;
}

function drawChart(history) {
  const canvas = $("performance-chart");
  const ratio = window.devicePixelRatio || 1;
  canvas.width = canvas.clientWidth * ratio;
  canvas.height = canvas.clientHeight * ratio;
  const ctx = canvas.getContext("2d");
  ctx.scale(ratio, ratio);
  const width = canvas.clientWidth;
  const height = canvas.clientHeight;
  const pad = { left: 32, right: 10, top: 12, bottom: 22 };
  const plotW = width - pad.left - pad.right;
  const plotH = height - pad.top - pad.bottom;
  const max = Math.max(16, ...history.map((row) => row.capture_fps || 0));

  ctx.strokeStyle = "#252b31";
  ctx.fillStyle = "#71808d";
  ctx.font = "10px system-ui";
  for (let i = 0; i <= 4; i++) {
    const y = pad.top + plotH * i / 4;
    ctx.beginPath(); ctx.moveTo(pad.left, y); ctx.lineTo(width - pad.right, y); ctx.stroke();
    ctx.fillText(number(max * (1 - i / 4), 0), 3, y + 3);
  }

  function line(key, color) {
    ctx.strokeStyle = color; ctx.lineWidth = 2; ctx.beginPath();
    history.forEach((row, index) => {
      const x = pad.left + plotW * index / Math.max(1, history.length - 1);
      const y = pad.top + plotH * (1 - (row[key] || 0) / max);
      index ? ctx.lineTo(x, y) : ctx.moveTo(x, y);
    });
    ctx.stroke();
  }
  line("capture_fps", "#43a6c6");
  line("detection_fps", "#65c18c");
}

function update(data) {
  const latest = data.latest || {};
  const device = data.device || {};
  const quality = data.event_quality || {};
  $("capture-fps").textContent = number(latest.capture_fps, 2);
  $("detection-fps").textContent = number(latest.detection_fps, 2);
  $("inference-ms").textContent = number(latest.inference_ms, 1);
  $("persons").textContent = latest.persons ?? "--";
  $("temperature").textContent = number(device.temperature_c, 1);
  $("reconnects").textContent = latest.rtsp_reconnects ?? "--";
  $("throttled").textContent = device.throttled ?? "Unavailable";
  $("frame-age").textContent = latest.last_frame_age_ms == null ? "--" : `${latest.last_frame_age_ms} ms`;
  $("source-fps").textContent = latest.source_fps == null ? "--" : `${number(latest.source_fps, 2)} FPS`;
  $("delivery-percent").textContent = latest.capture_delivery_percent == null ? "--" : `${number(latest.capture_delivery_percent, 1)}%`;
  $("read-average").textContent = latest.capture_read_avg_ms == null ? "--" : `${number(latest.capture_read_avg_ms, 1)} ms`;
  $("read-maximum").textContent = latest.capture_read_max_ms == null ? "--" : `${number(latest.capture_read_max_ms, 1)} ms`;
  $("slow-read-percent").textContent = latest.capture_slow_read_percent == null ? "--" : `${number(latest.capture_slow_read_percent, 1)}%`;
  $("uptime").textContent = duration(device.uptime_sec);
  setMeter("memory-meter", "memory-label", device.memory_used_percent);
  setMeter("disk-meter", "disk-label", device.disk_used_percent);

  $("unique-tracks").textContent = quality.unique_tracks ?? "--";
  $("scene-entries").textContent = quality.scene_entries ?? "--";
  $("scene-exits").textContent = quality.scene_exits ?? "--";
  $("short-exits").textContent = quality.short_zone_exits ?? "--";
  $("loitering").textContent = quality.loitering_events ?? "--";
  $("clips").textContent = quality.clip_events ?? "--";

  const alerts = data.alerts || [];
  const critical = alerts.some((alert) => alert.severity === "critical");
  const status = $("system-status");
  status.className = `status ${alerts.length ? "bad" : "good"}`;
  status.innerHTML = `<span></span>${critical ? "Critical alert" : alerts.length ? `${alerts.length} warning${alerts.length > 1 ? "s" : ""}` : "System healthy"}`;
  $("updated").textContent = data.telemetry_age_sec == null ? "No telemetry yet" : `Updated ${number(data.telemetry_age_sec, 0)} seconds ago`;
  $("chart-subtitle").textContent = `${$("window-select").selectedOptions[0].text} of capture and detection throughput`;

  const alertsPanel = $("alerts");
  alertsPanel.hidden = alerts.length === 0;
  alertsPanel.innerHTML = alerts.map((alert) =>
    `<div class="alert ${escapeHtml(alert.severity)}"><strong>${escapeHtml(alert.code.replaceAll("_", " "))}</strong> ${escapeHtml(alert.message)}</div>`
  ).join("");

  const rows = (data.events || []).map((event) => {
    const time = new Date(event.timestamp_ms).toLocaleTimeString();
    return `<tr><td>${escapeHtml(time)}</td><td>${escapeHtml(event.category)}</td><td>${escapeHtml(event.message)}</td></tr>`;
  });
  $("events-table").innerHTML = rows.join("") || '<tr><td colspan="3">No events recorded</td></tr>';

  const reasoningRows = (data.reasoning_results || []).map((result) => {
    const time = new Date(result.timestamp_ms).toLocaleTimeString();
    const provider = `${result.provider}/${result.model || "--"}${result.fallback_used ? " (fallback)" : ""}`;
    const assessment = result.success ? `${result.summary} ${result.recommended_action}` : `Failed: ${result.error || result.primary_error}`;
    return `<tr><td>${escapeHtml(time)}</td><td>${escapeHtml(result.risk_level)}</td><td>${escapeHtml(provider)}</td><td>${number(result.latency_ms, 0)} ms</td><td>${escapeHtml(assessment)}</td></tr>`;
  });
  $("reasoning-table").innerHTML = reasoningRows.join("") || '<tr><td colspan="5">No reasoning results recorded</td></tr>';
  drawChart(data.history || []);
}

async function refresh() {
  try {
    const windowMinutes = $("window-select").value;
    const response = await fetch(`/api/dashboard?window_minutes=${windowMinutes}`, { cache: "no-store" });
    update(await response.json());
  } catch (error) {
    const status = $("system-status");
    status.className = "status bad";
    status.innerHTML = "<span></span>Dashboard API unavailable";
  }
}

window.addEventListener("resize", () => refresh());
$("window-select").addEventListener("change", refresh);
refresh();
setInterval(refresh, 10000);
