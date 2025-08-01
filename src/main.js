const logDiv = document.getElementById("log");
const ws = new WebSocket(`ws://${location.host}/ws`);

function start_socket(){
	ws.onopen = () => log("✅ Соединение открыто");
	ws.onmessage = e => log(e.data);
	ws.onclose = () => log("❌ Соединение закрыто");
	ws.onerror = err => log("⚠️ Ошибка: " + err);
}

function log(msg) {
  const line = document.createElement("div");
  const text = String(msg);

  // Ищем ведущий timestamp в распространённых форматах:
  // [YYYY-MM-DD HH:MM:SS(.ms)], [HH:MM:SS(.ms)], YYYY-MM-DD HH:MM:SS(.ms), HH:MM:SS(.ms)
  const m = text.match(/^(\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}(?:[.,]\d+)?\]|\[\d{2}:\d{2}:\d{2}(?:[.,]\d+)?\]|(?:\d{4}-\d{2}-\d{2} )?\d{2}:\d{2}:\d{2}(?:[.,]\d+)?)/);

  if (m) {
    const tsSpan = document.createElement('span');
    tsSpan.textContent = m[1] + ' ';
    tsSpan.className = 'ts'; // стилизуем через CSS

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

function loadSidebar(){
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

  if (ws.readyState === WebSocket.OPEN) {
    ws.send(msg);
    log("📤 Sent: " + msg);
  } else {
    log("⚠️ WebSocket не подключен");
  }
}

function stopStream() {
	ws.send("STOP_STREAM");
	log("📤 Sent: STOP_STREAM");
}

function sendMessage() {
	const text = document.getElementById("message").value;
	if (text.trim()) {
	ws.send(text);
	log("📤 Sent: " + text);
	}
}

function sendReboot() {
  fetch('/reboot')
    .then(resp => resp.text())
    .then(text => {
      console.log('Server response:', text);
      alert(text);
    })
    .catch(err => {
      console.error('Reboot request failed:', err);
      alert('Failed to send reboot command');
    });
}

document.addEventListener("DOMContentLoaded", start_socket);
