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
	  uart1: {
		baudrate: parseInt(document.getElementById('uart1_baudrate').value),
		parity: document.getElementById('uart1_parity').value,
		stop_bits: parseInt(document.getElementById('uart1_stop_bits').value)
	  },
	  uart2: {
		baudrate: parseInt(document.getElementById('uart2_baudrate').value),
		parity: document.getElementById('uart2_parity').value,
		stop_bits: parseInt(document.getElementById('uart2_stop_bits').value)
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

        document.getElementById("uart1_baudrate").value = config.uart1.baudrate;
		document.getElementById("uart1_parity").value = config.uart1.parity;
		document.getElementById("uart1_stop_bits").value = config.uart1.stop_bits;

		document.getElementById("uart2_baudrate").value = config.uart2.baudrate;
		document.getElementById("uart2_parity").value = config.uart2.parity;
		document.getElementById("uart2_stop_bits").value = config.uart2.stop_bits;

    } catch (err) {
        showNotification("❌ Ошибка загрузки настроек", true);
    }
}
