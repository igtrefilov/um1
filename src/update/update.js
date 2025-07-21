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

  // 1. Обновление Web UI
  const webOk = await uploadWebFiles(srcFiles);
  if (!webOk) {
    alert("Ошибка загрузки Web UI. OTA отменено.");
    return;
  }

  // 2. Обновление прошивки
  await uploadFirmware(firmwareFile);
}

async function uploadFirmware(file) {
  const progressBar = document.getElementById('otaProgress');
  const statusText = document.getElementById('otaStatus');

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
          statusText.textContent = `⏳ Обновление: ${percent}%`;
        }
      } else if (msg === "done") {
        progressBar.value = 100;
        statusText.textContent = '✅ Прошивка завершена. Перезагрузка...';
      }
    };

    otaSocket.onerror = (err) => {
      console.error("WebSocket error:", err);
      statusText.textContent = "⚠️ Ошибка WebSocket";
    };

    // 2. Отправим прошивку через fetch
    const res = await fetch('/ota', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/octet-stream'
      },
      body: file
    });

    if (!res.ok) {
      const text = await res.text();
      throw new Error(`Ошибка OTA: ${text}`);
    }

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
    const rel = file.webkitRelativePath.replace("update/", ""); // src/...

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
