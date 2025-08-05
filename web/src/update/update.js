const logDiv = document.getElementById("log");
const otaSocket = new WebSocket(`ws://${location.host}/ws`);

function start_socket(){
	otaSocket.onopen = () => log("✅ Соединение с сервером установлено");
	otaSocket.onerror = err => log("⚠️ Ошибка соединения с сервером: " + err);
}

function log(msg) {
	const line = document.createElement("div");
	line.textContent = msg;
	logDiv.appendChild(line);
	logDiv.scrollTop = logDiv.scrollHeight;
}

function loadSidebar(){
    fetch('../sidebar.html')
        .then(resp => resp.text())
        .then(html => {
            document.getElementById('sidebar').innerHTML = html;
        })
        .catch(err => console.error('Failed to load sidebar:', err));
}

async function uploadAll() {
  const input = document.getElementById('updateFolder');
  const files = input.files;
  if (!files.length) {
    alert("Выберите папку update с firmware.bin и src/");
    return;
  }

  let firmwareFile = null;
  const srcFiles = [];

  for (let f of files) {
    if (f.webkitRelativePath.endsWith("firmware.bin")) {
      firmwareFile = f;
    } else if (f.webkitRelativePath.startsWith("update/src/")) {
      srcFiles.push(f);
    }
  }

  if (!firmwareFile) {
    alert("Файл firmware.bin не найден в папке update");
    return;
  }

  const webOk = await uploadWebFiles(srcFiles);
  if (!webOk) {
    alert("Ошибка загрузки Web UI. OTA отменено.");
    return;
  }

  await uploadFirmware(firmwareFile);
}

function showRebootOverlay(show) {
  const el = document.getElementById('reboot_overlay');
  if (el) el.style.display = show ? 'flex' : 'none';
}
function setRebootProgress(percent, text) {
  const bar = document.getElementById('rb_progress');
  const st  = document.getElementById('rb_status');
  if (bar) bar.value = Math.max(0, Math.min(100, percent|0));
  if (st && text) st.textContent = text;
}
function fetchWithTimeout(url, { timeout = 1200, ...opts } = {}) {
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
      const r = await fetchWithTimeout('/api/system_info');
      if (r.ok) return true;
    } catch (_) {}
    await new Promise(r => setTimeout(r, intervalMs));
  }
  return false;
}
async function waitAndReload() {
  showRebootOverlay(true);
  setRebootProgress(10, 'Контроллер уходит в перезагрузку…');

  const up = await waitForDevice({ estimateMs: 25000, timeoutMs: 60000 });
  if (up) {
    setRebootProgress(100, 'Готово! Перезагружаем страницу…');
    setTimeout(() => {
      location.replace(`${location.origin}/index.html`);
    }, 800);
  } else {
    setRebootProgress(100, '⏱ Не дождались. Обновите страницу вручную.');
  }
}

async function uploadFirmware(file) {
  const progressBar = document.getElementById('otaProgress');
  const statusText = document.getElementById('otaStatus');
  
  document.querySelector('#fwSection h3').style.display = 'block';

  progressBar.style.display = 'block';
  progressBar.value = 0;
  statusText.textContent = '🚀 Отправка прошивки...';

  try {
    otaSocket.send("OTA_PROGRESS");

    otaSocket.onmessage = (event) => {
      const msg = event.data;
      if (msg.startsWith("progress:")) {
        const percent = parseInt(msg.split(":")[1]);
        if (!isNaN(percent)) {
          progressBar.value = percent;
          statusText.textContent = `🔄 OTA ${percent}%`;
        }
      } else if (msg === "done") {
        statusText.textContent = "✅ OTA завершено. Перезагрузка…";
        waitAndReload();
      }
    };

    const res = await fetch('/ota', {
      method: 'POST',
      headers: { 'Content-Type': 'application/octet-stream' },
      body: file
    });
    if (!res.ok) {
      const text = await res.text();
      throw new Error(`Ошибка OTA: ${text}`);
    }

    setTimeout(() => {
      if (document.getElementById('reboot_overlay').style.display !== 'flex') {
        waitAndReload();
      }
    }, 500);

  } catch (err) {
    progressBar.style.display = 'none';
    statusText.textContent = '❌ Ошибка OTA: ' + err.message;

  } finally {
    if (otaSocket && otaSocket.readyState === WebSocket.OPEN) {
      otaSocket.close();
    }
  }
}
async function uploadWebFiles(srcFiles) {
  const progressBar = document.getElementById('webProgress');
  const statusText = document.getElementById('webStatus');

  document.querySelector('#webSection h3').style.display = 'block';
  
  if (!srcFiles.length) {
    statusText.textContent = "⚠️ Нет файлов src для загрузки";
    return false;
  }

  progressBar.style.display = 'block';
  progressBar.value = 0;
  statusText.textContent = '🚀 Загрузка Web UI...';

  const total = srcFiles.length;
  let uploaded = 0;

  for (const file of srcFiles) {
    const rel = file.webkitRelativePath.replace("update/", "");

    try {
      const res = await fetch('/upload', {
        method: 'POST',
        headers: {
          'X-FILENAME': rel,
          'Content-Type': 'application/octet-stream'
        },
        body: file
      });

      if (!res.ok) {
        const text = await res.text();
        statusText.textContent = `❌ Ошибка при загрузке ${rel}: ${text}`;
        return false;
      }

      uploaded++;
      const percent = Math.round((uploaded / total) * 100);
      progressBar.value = percent;
      statusText.textContent = `📁 Загружено ${uploaded}/${total} (${percent}%)`;

    } catch (err) {
      statusText.textContent = `❌ Ошибка загрузки ${file.name}: ${err.message}`;
      return false;
    }
  }

  statusText.textContent = "✅ Web UI успешно обновлён!";
  return true;
}

document.addEventListener("DOMContentLoaded", start_socket);
