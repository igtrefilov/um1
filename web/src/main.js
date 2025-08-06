const logDiv = document.getElementById("log");
let ws;
let pendingReboot = false;
let statusInterval;

function start_socket() {
  ws = new WebSocket(`ws://${location.host}/ws`);
  ws.onopen = () => log("✅ Соединение открыто");
  ws.onmessage = e => log(e.data);
  ws.onclose = () => log("❌ Соединение закрыто");
  ws.onerror = err => log("⚠️ Ошибка: " + err);
}

function log(msg) {
  const line = document.createElement("div");
  const text = String(msg);
  const m = text.match(/^(\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}(?:[.,]\d+)?\]|\[\d{2}:\d{2}:\d{2}(?:[.,]\d+)?\]|(?:\d{4}-\d{2}-\d{2} )?\d{2}:\d{2}:\d{2}(?:[.,]\d+)?)/);

  if (m) {
    const tsSpan = document.createElement('span');
    tsSpan.textContent = m[1] + ' ';
    tsSpan.className = 'ts';

    const msgSpan = document.createElement('span');
    msgSpan.textContent = text.slice(m[1].length).replace(/^\s+/, '');

    line.appendChild(tsSpan);
    line.appendChild(msgSpan);
  } else {
    line.textContent = text;
  }

  logDiv.appendChild(line);
  logDiv.scrollTop = logDiv.scrollHeight;
}

function updateServerStatus() {
  fetch('/api/stream_status')
    .then(r => r.json())
    .then(s => {
      const el = document.getElementById('server_status');
      if (!el) return;
      const connected = s.tcp || s.udp;
      el.textContent = `TCP: ${s.tcp ? 'подключен' : 'нет'}, UDP: ${s.udp ? 'подключен' : 'нет'}`;
      el.className = 'server-status ' + (connected ? 'connected' : 'disconnected');
    })
    .catch(_ => {
      const el = document.getElementById('server_status');
      if (!el) return;
      el.textContent = 'TCP: нет, UDP: нет';
      el.className = 'server-status disconnected';
    });
}

function loadSidebar() {
  fetch('/sidebar.html')
    .then(resp => resp.text())
    .then(html => {
      document.getElementById('sidebar').innerHTML = html;
    })
    .catch(err => console.error('Failed to load sidebar:', err));
}

function startStream() {
  const uart1 = document.getElementById("uart1").checked;
  const uart2 = document.getElementById("uart2").checked;
  const msg = JSON.stringify({
    action: "START_STREAM",
    uart1,
    uart2
  });

  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(msg);
    log("📤 Sent: " + msg);
  } else {
    log("⚠️ WebSocket не подключен");
  }
}

function stopStream() {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send("STOP_STREAM");
    log("📤 Sent: STOP_STREAM");
  }
}

function sendMessage() {
  const text = document.getElementById("message").value;
  if (text.trim() && ws && ws.readyState === WebSocket.OPEN) {
    ws.send(text);
    log("📤 Sent: " + text);
  }
}

function showRebootOverlay(show) {
  document.getElementById('reboot_overlay').style.display = show ? 'flex' : 'none';
}

function setRebootProgress(percent, text) {
  const bar = document.getElementById('rb_progress');
  const st = document.getElementById('rb_status');
  if (bar) bar.value = Math.max(0, Math.min(100, percent | 0));
  if (st && text) st.textContent = text;
}

function fetchWithTimeout(url, { timeout = 1000, ...opts } = {}) {
  const ctl = new AbortController();
  const id = setTimeout(() => ctl.abort(), timeout);
  return fetch(url, { ...opts, signal: ctl.signal, cache: 'no-store' })
    .finally(() => clearTimeout(id));
}

async function waitForDevice({ estimateMs = 25000, timeoutMs = 60000, intervalMs = 1000 } = {}) {
  const t0 = Date.now();
  while (Date.now() - t0 < timeoutMs) {
    const elapsed = Date.now() - t0;
    const pct = Math.min(100, Math.round((elapsed / estimateMs) * 100));
    setRebootProgress(pct);

    try {
      const r = await fetchWithTimeout('/api/system_info', { timeout: 1200 });
      if (r.ok) return true;
    } catch (_) {}

    await new Promise(r => setTimeout(r, intervalMs));
  }
  return false;
}

async function sendReboot() {
  pendingReboot = true;
  showRebootOverlay(true);
  setRebootProgress(5, 'Отправляем команду перезагрузки…');

  try {
    const resp = await fetch('/reboot');
    if (!resp.ok) throw new Error('HTTP ' + resp.status);

    setRebootProgress(15, 'Контроллер уходит в перезагрузку…');

    const up = await waitForDevice({ estimateMs: 25000, timeoutMs: 60000 });

    if (up) {
      setRebootProgress(100, 'Готово! Перезагружаем страницу…');
      setTimeout(() => {
        location.replace(`${location.origin}/index.html`);
      }, 800);
    } else {
      setRebootProgress(100, '⏱ Не дождались. Обновите страницу вручную.');
    }
  } catch (err) {
    setRebootProgress(0, '❌ Ошибка: ' + err.message);
  }
}

document.addEventListener("DOMContentLoaded", () => {
  if (!pendingReboot) start_socket();
  fetch('/api/config')
    .then(r => r.json())
    .then(cfg => {
      if (cfg.tcp.enabled || cfg.udp.enabled) {
        const st = document.getElementById('server_status');
        if (st) {
          st.style.display = 'block';
          st.textContent = 'TCP: нет, UDP: нет';
          st.className = 'server-status disconnected';
          updateServerStatus();
          statusInterval = setInterval(updateServerStatus, 5000);
        }
      }
    })
    .catch(_ => {});
});
