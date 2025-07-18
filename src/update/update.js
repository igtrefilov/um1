function loadSidebar(){
    fetch('../sidebar.html')
          .then(resp => resp.text())
          .then(html => {
        document.getElementById('sidebar').innerHTML = html;
          })
          .catch(err => console.error('Failed to load sidebar:', err));
}

async function sendFile() {
  const input = document.getElementById('fileInput');
  if (!input.files.length) {
    alert('Пожалуйста, выберите файл.');
    throw new Error('No file selected');
  }

  const file = input.files[0];

  // Отправляем файл на сервер через HTTP
  const response = await fetch('/upload', {
    method: 'POST',
    headers: {
      'X-FILENAME': encodeURIComponent(file.name),
      'Content-Type': file.type || 'application/octet-stream'
    },
    body: file
  });

  if (!response.ok) {
    const txt = await response.text();
    throw new Error(`HTTP ${response.status}: ${txt}`);
  }

  log("✅ HTTP upload OK: " + await response.text());

  // Отправляем файл по WebSocket (если необходимо)
  if (ws && ws.readyState === WebSocket.OPEN) {
    const reader = new FileReader();
    reader.onload = () => {
      const arrayBuffer = reader.result;
      ws.send(arrayBuffer);
      log(`📤 WS file sent: ${file.name} (${file.size} bytes)`);
    };
    reader.onerror = () => log("❌ Ошибка при чтении файла");
    reader.readAsArrayBuffer(file);
  } else {
    log("⚠️ WS не открыт — пропускаем WS-отправку");
  }
}

async function updateFirmware() {
  const input = document.getElementById('fileInput');
  const progressBar = document.getElementById('otaProgress');
  const statusText = document.getElementById('otaStatus');

  progressBar.value = 0;
  progressBar.style.display = 'block';
  statusText.textContent = '🚀 Начало обновления...';

  if (!input.files.length) {
    alert('Выберите файл');
    return;
  }

  const file = input.files[0];

  try {
    const response = await fetch('/ota', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/octet-stream'
      },
      body: file
    });

    if (!response.ok) {
      const text = await response.text();
      throw new Error(`OTA failed: ${text}`);
    }

    const reader = response.body.getReader();
    const decoder = new TextDecoder();

    while (true) {
      const { value, done } = await reader.read();
      if (done) break;

      const chunk = decoder.decode(value, { stream: true });
      chunk.split('\n').forEach(line => {
        if (line.startsWith('progress:')) {
          const percent = parseInt(line.split(':')[1]);
          progressBar.value = percent;
          statusText.textContent = `⏳ Обновление: ${percent}%`;
        } else if (line.trim() === 'done') {
          progressBar.value = 100;
          statusText.textContent = '✅ Обновление завершено. Перезагрузка...';
        }
      });
    }

  } catch (err) {
    progressBar.style.display = 'none';
    statusText.textContent = '❌ Ошибка OTA: ' + err.message;
  }
}

// Помощная функция для логирования на страницу
function log(msg) {
  const pre = document.createElement('pre');
  pre.textContent = msg;
  document.body.appendChild(pre);
}

