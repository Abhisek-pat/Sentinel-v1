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
  $("uptime").textContent = duration(device.uptime_sec);
  setMeter("memory-meter", "memory-label", device.memory_used_percent);
  setMeter("disk-meter", "disk-label", device.disk_used_percent);

  $("unique-tracks").textContent = quality.unique_tracks ?? "--";
  $("scene-entries").textContent = quality.scene_entries ?? "--";
  $("scene-exits").textContent = quality.scene_exits ?? "--";
  $("short-exits").textContent = quality.short_zone_exits ?? "--";
  $("loitering").textContent = quality.loitering_events ?? "--";
  $("clips").textContent = quality.clip_events ?? "--";

  const stale = data.telemetry_age_sec == null || data.telemetry_age_sec > 90;
  const throttled = device.throttled && device.throttled !== "0x0";
  const status = $("system-status");
  status.className = `status ${stale || throttled ? "bad" : "good"}`;
  status.innerHTML = `<span></span>${stale ? "Telemetry stale" : throttled ? "Throttling detected" : "System healthy"}`;
  $("updated").textContent = data.telemetry_age_sec == null ? "No telemetry yet" : `Updated ${number(data.telemetry_age_sec, 0)} seconds ago`;

  const rows = (data.events || []).map((event) => {
    const time = new Date(event.timestamp_ms).toLocaleTimeString();
    return `<tr><td>${escapeHtml(time)}</td><td>${escapeHtml(event.category)}</td><td>${escapeHtml(event.message)}</td></tr>`;
  });
  $("events-table").innerHTML = rows.join("") || '<tr><td colspan="3">No events recorded</td></tr>';
  drawChart(data.history || []);
}

async function refresh() {
  try {
    const response = await fetch("/api/dashboard", { cache: "no-store" });
    update(await response.json());
  } catch (error) {
    const status = $("system-status");
    status.className = "status bad";
    status.innerHTML = "<span></span>Dashboard API unavailable";
  }
}

window.addEventListener("resize", () => refresh());
refresh();
setInterval(refresh, 10000);
