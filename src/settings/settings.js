function loadSidebar(){
	fetch('../sidebar.html')
	      .then(resp => resp.text())
	      .then(html => {
		document.getElementById('sidebar').innerHTML = html;
	      })
	      .catch(err => console.error('Failed to load sidebar:', err));
}

function showNotification(message, isError = false) {
  const notif = document.getElementById('notif');
  notif.textContent = message;
  notif.className = 'notification' + (isError ? ' error' : '');
  notif.style.display = 'block';

  setTimeout(() => {
    notif.style.display = 'none';
  }, 3000);
}

function collectSettings() {
  const settings = {
    uart: {
      baudrate: parseInt(document.getElementById('uart_baudrate').value),
      parity: document.getElementById('uart_parity').value,
      stop_bits: parseInt(document.getElementById('uart_stop_bits').value)
    }
  };

  fetch('/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(settings)
  })
  .then(response => {
    if (response.ok) {
      showNotification('✅ Настройки успешно сохранены');
    } else {
      showNotification('❌ Ошибка сохранения', true);
    }
  })
  .catch(err => {
    console.error('Ошибка:', err);
    showNotification('❌ Ошибка соединения с сервером', true);
  });
}

async function loadConfigFromServer() {
    try {
        const response = await fetch('/api/config');
        const config = await response.json();

        document.getElementById("uart_baudrate").value = config.uart.baudrate;
        document.getElementById("uart_parity").value = config.uart.parity;
        document.getElementById("uart_stop_bits").value = config.uart.stop_bits;

    } catch (err) {
        showNotification("❌ Ошибка загрузки настроек", true);
    }
}
