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
  try {

    await sendFile();

    // 2. Запрашиваем запуск OTA-обновления
    const otaResponse = await fetch('/update', {
      method: 'POST'
    });

    if (!otaResponse.ok) {
      const txt = await otaResponse.text();
      throw new Error(`OTA failed: HTTP ${otaResponse.status}: ${txt}`);
    }

    log('✅ OTA update initiated');
  } catch (err) {
    log('❌ Обновление не удалось: ' + err.message);
  }
}

// Помощная функция для логирования на страницу
function log(msg) {
  const pre = document.createElement('pre');
  pre.textContent = msg;
  document.body.appendChild(pre);
}

// Инициализация WebSocket при загрузке страницы
let ws;
(function initWs() {
  ws = new WebSocket(`ws://${location.host}/ws`);
  ws.onopen = () => log('🔌 WS connected');
  ws.onmessage = (evt) => log('📥 WS: ' + evt.data);
  ws.onclose = () => log('🔌 WS disconnected');
})();

