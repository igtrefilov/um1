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
    return;
  }

  const file = input.files[0];

  try {
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
  } catch (err) {
    log("❌ HTTP upload failed: " + err.message);
  }

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
