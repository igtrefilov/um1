function loadSidebar(){
	fetch('../sidebar.html')
	      .then(resp => resp.text())
	      .then(html => {
		document.getElementById('sidebar').innerHTML = html;
	      })
	      .catch(err => console.error('Failed to load sidebar:', err));
}

function toggleLanFields() {
  const dhcpEnabled = document.getElementById('lan_dhcp').checked;
  const fieldsToToggle = [
    'lan_static_ip',
    'lan_subnet',
    'lan_gateway'
  ];

  fieldsToToggle.forEach(id => {
    const el = document.getElementById(id);
    el.disabled = dhcpEnabled;
    if (dhcpEnabled) {
      el.classList.add('disabled-field');
    } else {
      el.classList.remove('disabled-field');
    }
  });
}

function toggleWifiFields() {
  const wifiEnabled = document.getElementById('wifi_enabled').checked;
  const wifiFields = [
    'wifi_ssid',
    'wifi_password',
    'wifi_mode',
    'wifi_authmode',
    
  ];

  wifiFields.forEach(id => {
    const el = document.getElementById(id);
    el.disabled = !wifiEnabled;
    if (!wifiEnabled) {
      el.classList.add('disabled-field');
    } else {
      el.classList.remove('disabled-field');
    }
  });
}

document.addEventListener('DOMContentLoaded', () => {
  document.getElementById('lan_dhcp').addEventListener('change', toggleLanFields);
  toggleLanFields();
  
  document.getElementById('wifi_enabled').addEventListener('change', toggleWifiFields);
  toggleWifiFields();
});

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
  const config = {
    lan: {
      dhcp: document.getElementById("lan_dhcp").checked,
      static_ip: document.getElementById("lan_static_ip").value,
      subnet: document.getElementById("lan_subnet").value,
      gateway: document.getElementById("lan_gateway").value,
      mode: document.getElementById("lan_mode").value
    },
    wifi: {
      enabled: document.getElementById("wifi_enabled").checked,
      ssid: document.getElementById("wifi_ssid").value,
      password: document.getElementById("wifi_password").value,
      authmode: document.getElementById("wifi_authmode").value,
      mode: document.getElementById("wifi_mode").value
    }
  };

  fetch('/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(config)
  })
  .then(response => {
    if (response.ok) {
      showNotification('✅ Настройки сети сохранены');
    } else {
      showNotification('❌ Ошибка при сохранении настроек', true);
    }
  })
  .catch(err => {
    console.error('Ошибка:', err);
    showNotification('❌ Ошибка соединения с сервером', true);
  });
}

function loadConfigFromServer() {
  fetch("/config.json")
    .then(res => res.json())
    .then(config => {
      document.getElementById("lan_dhcp").checked = config.lan.dhcp;
      document.getElementById("lan_static_ip").value = config.lan.static_ip;
      document.getElementById("lan_subnet").value = config.lan.subnet;
      document.getElementById("lan_gateway").value = config.lan.gateway;
      document.getElementById("lan_mode").value = config.lan.mode;

      document.getElementById("wifi_enabled").checked = config.wifi.enabled;
      document.getElementById("wifi_ssid").value = config.wifi.ssid;
      document.getElementById("wifi_password").value = config.wifi.password;
      document.getElementById("wifi_authmode").value = config.wifi.authmode;
      document.getElementById("wifi_mode").value = config.wifi.mode;

	  toggleLanFields();
      toggleWifiFields();
    });
}
