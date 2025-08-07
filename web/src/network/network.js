function toggleLanFields() {
  const dhcpEnabled = document.getElementById('lan_dhcp').checked;
  ['lan_static_ip','lan_subnet','lan_gateway'].forEach(id => {
    const el = document.getElementById(id);
    el.disabled = dhcpEnabled;
  });
}
function toggleWifiFields() {
  const wifiEnabled = document.getElementById('wifi_enabled').checked;
  ['wifi_mode'].forEach(id => {
    document.getElementById(id).disabled = !wifiEnabled;
  });
  ['wifi_ap_section','wifi_sta_section'].forEach(id => {
    const section = document.getElementById(id);
    section.querySelectorAll('input, select').forEach(el => el.disabled = !wifiEnabled);
  });
}

function toggleWifiModeFields() {
  const mode = document.getElementById('wifi_mode').value;
  document.getElementById('wifi_ap_section').style.display = (mode === 'ap' || mode === 'apsta') ? 'block' : 'none';
  document.getElementById('wifi_sta_section').style.display = (mode === 'sta' || mode === 'apsta') ? 'block' : 'none';
}

function toggleDhcp(prefix) {
  const dhcp = document.getElementById(prefix + '_dhcp').checked;
  ['static_ip','subnet','gateway'].forEach(f => {
    document.getElementById(prefix + '_' + f).disabled = dhcp;
  });
}
function showNotification(message, isError=false){
  const notif = document.getElementById('notif');
  notif.textContent = message;
  notif.className = 'notification' + (isError ? ' error' : ' success');
  notif.style.display = 'block';
  notif.scrollIntoView({behavior:'smooth', block:'center'});
  setTimeout(()=>notif.style.display='none',3000);
}
function collectSettings(){
  const config = {
    lan:{
      dhcp: document.getElementById('lan_dhcp').checked,
      static_ip: document.getElementById('lan_static_ip').value,
      subnet: document.getElementById('lan_subnet').value,
      gateway: document.getElementById('lan_gateway').value
    },
    wifi:{
      enabled: document.getElementById('wifi_enabled').checked,
      mode: document.getElementById('wifi_mode').value,
      ap:{
        ssid: document.getElementById('wifi_ap_ssid').value,
        password: document.getElementById('wifi_ap_password').value,
        authmode: document.getElementById('wifi_ap_authmode').value,
        dhcp: document.getElementById('wifi_ap_dhcp').checked,
        static_ip: document.getElementById('wifi_ap_static_ip').value,
        subnet: document.getElementById('wifi_ap_subnet').value,
        gateway: document.getElementById('wifi_ap_gateway').value
      },
      sta:{
        ssid: document.getElementById('wifi_sta_ssid').value,
        password: document.getElementById('wifi_sta_password').value,
        authmode: document.getElementById('wifi_sta_authmode').value,
        dhcp: document.getElementById('wifi_sta_dhcp').checked,
        static_ip: document.getElementById('wifi_sta_static_ip').value,
        subnet: document.getElementById('wifi_sta_subnet').value,
        gateway: document.getElementById('wifi_sta_gateway').value
      }
    }
  };
  fetch('/config', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify(config)
  }).then(r=>{
    if(r.ok) showNotification('✅ Настройки сети сохранены');
    else showNotification('❌ Ошибка при сохранении настроек', true);
  }).catch(err=>{
    console.error('Ошибка:', err);
    showNotification('❌ Ошибка соединения с сервером', true);
  });
}
function loadConfigFromServer(){
  fetch('/config.json')
    .then(res=>res.json())
    .then(config=>{
      document.getElementById('lan_dhcp').checked = config.lan.dhcp;
      document.getElementById('lan_static_ip').value = config.lan.static_ip;
      document.getElementById('lan_subnet').value = config.lan.subnet;
      document.getElementById('lan_gateway').value = config.lan.gateway;
      document.getElementById('wifi_enabled').checked = config.wifi.enabled;
      document.getElementById('wifi_mode').value = config.wifi.mode;
      document.getElementById('wifi_ap_ssid').value = config.wifi.ap.ssid;
      document.getElementById('wifi_ap_password').value = config.wifi.ap.password;
      document.getElementById('wifi_ap_authmode').value = config.wifi.ap.authmode;
      document.getElementById('wifi_ap_dhcp').checked = config.wifi.ap.dhcp;
      document.getElementById('wifi_ap_static_ip').value = config.wifi.ap.static_ip;
      document.getElementById('wifi_ap_subnet').value = config.wifi.ap.subnet;
      document.getElementById('wifi_ap_gateway').value = config.wifi.ap.gateway;
      document.getElementById('wifi_sta_ssid').value = config.wifi.sta.ssid;
      document.getElementById('wifi_sta_password').value = config.wifi.sta.password;
      document.getElementById('wifi_sta_authmode').value = config.wifi.sta.authmode;
      document.getElementById('wifi_sta_dhcp').checked = config.wifi.sta.dhcp;
      document.getElementById('wifi_sta_static_ip').value = config.wifi.sta.static_ip;
      document.getElementById('wifi_sta_subnet').value = config.wifi.sta.subnet;
      document.getElementById('wifi_sta_gateway').value = config.wifi.sta.gateway;
      toggleLanFields();
      toggleWifiFields();
      toggleWifiModeFields();
      toggleDhcp('wifi_ap');
      toggleDhcp('wifi_sta');
    });
}
document.addEventListener('DOMContentLoaded', () => {
  document.getElementById('lan_dhcp').addEventListener('change', toggleLanFields);
  document.getElementById('wifi_enabled').addEventListener('change', toggleWifiFields);
  document.getElementById('wifi_mode').addEventListener('change', toggleWifiModeFields);
  document.getElementById('wifi_ap_dhcp').addEventListener('change', () => toggleDhcp('wifi_ap'));
  document.getElementById('wifi_sta_dhcp').addEventListener('change', () => toggleDhcp('wifi_sta'));
  toggleLanFields();
  toggleWifiFields();
  toggleWifiModeFields();
  toggleDhcp('wifi_ap');
  toggleDhcp('wifi_sta');
  loadConfigFromServer();
});
