function showNotification(message, isError=false){
  const notif = document.getElementById('notif');
  notif.textContent = message;
  notif.className = 'notification' + (isError ? ' error' : ' success');
  notif.style.display = 'block';
  notif.scrollIntoView({behavior:'smooth', block:'center'});
  setTimeout(()=>notif.style.display='none',3000);
}

function toggleLanFields() {
  const dhcpEnabled = document.getElementById('lan_dhcp').checked;
  ['lan_static_ip','lan_subnet','lan_gateway'].forEach(id => {
    const el = document.getElementById(id);
    if(el) el.disabled = dhcpEnabled;
  });
}

function toggleWifiFields() {
  const wifiEnabled = document.getElementById('wifi_enabled').checked;
  ['wifi_mode'].forEach(id => {
    const el = document.getElementById(id);
    if(el) el.disabled = !wifiEnabled;
  });
  ['wifi_ap_section','wifi_sta_section'].forEach(id => {
    const section = document.getElementById(id);
    if(section) section.querySelectorAll('input, select').forEach(el => el.disabled = !wifiEnabled);
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
    const el = document.getElementById(prefix + '_' + f);
    if(el) el.disabled = dhcp;
  });
}
function toggleAllFields(){
  const toggles=[
    {enabledId:'mqtt_enabled', fieldIds:['mqtt_broker','mqtt_username','mqtt_password']},
    {enabledId:'tcp_enabled', fieldIds:['tcp_server','tcp_port']},
    {enabledId:'udp_enabled', fieldIds:['udp_server','udp_port']},
    {enabledId:'sntp_enabled', fieldIds:['sntp_server_ip','sntp_interval']}
  ];
  toggles.forEach(group=>{
    const on=document.getElementById(group.enabledId).checked;
    group.fieldIds.forEach(id=>{ const el=document.getElementById(id); if(el) el.disabled=!on; });
  });
}
function collectSettings(){
  const settings={
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
    },
    uart1:{ baudrate:+document.getElementById('uart1_baudrate').value, parity:document.getElementById('uart1_parity').value, stop_bits:+document.getElementById('uart1_stop_bits').value },
    uart2:{ baudrate:+document.getElementById('uart2_baudrate').value, parity:document.getElementById('uart2_parity').value, stop_bits:+document.getElementById('uart2_stop_bits').value },
    mqtt:{ enabled:document.getElementById('mqtt_enabled').checked, broker:document.getElementById('mqtt_broker').value, username:document.getElementById('mqtt_username').value, password:document.getElementById('mqtt_password').value },
    tcp:{ enabled:document.getElementById('tcp_enabled').checked, server:document.getElementById('tcp_server').value, port:+document.getElementById('tcp_port').value },
    udp:{ enabled:document.getElementById('udp_enabled').checked, server:document.getElementById('udp_server').value, port:+document.getElementById('udp_port').value },
    sntp:{ enabled:document.getElementById('sntp_enabled').checked, server_ip:document.getElementById('sntp_server_ip').value, sync_interval_sec:+document.getElementById('sntp_interval').value }
  };
  fetch('/config', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(settings)})
    .then(r=>{ if(r.ok) showNotification('✅ Настройки успешно сохранены'); else showNotification('❌ Ошибка сохранения',true); })
    .catch(_=>showNotification('❌ Ошибка соединения с сервером',true));
}
async function loadConfigFromServer(){
  try{
    const response=await fetch('/api/config');
    const config=await response.json();
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

    document.getElementById('uart1_baudrate').value=config.uart1.baudrate;
    document.getElementById('uart1_parity').value=config.uart1.parity;
    document.getElementById('uart1_stop_bits').value=config.uart1.stop_bits;
    document.getElementById('uart2_baudrate').value=config.uart2.baudrate;
    document.getElementById('uart2_parity').value=config.uart2.parity;
    document.getElementById('uart2_stop_bits').value=config.uart2.stop_bits;
    document.getElementById('mqtt_enabled').checked=config.mqtt.enabled;
    document.getElementById('mqtt_broker').value=config.mqtt.broker;
    document.getElementById('mqtt_username').value=config.mqtt.username;
    document.getElementById('mqtt_password').value=config.mqtt.password;
    document.getElementById('tcp_enabled').checked=config.tcp.enabled;
    document.getElementById('tcp_server').value=config.tcp.server;
    document.getElementById('tcp_port').value=config.tcp.port;
    document.getElementById('udp_enabled').checked=config.udp.enabled;
    document.getElementById('udp_server').value=config.udp.server;
    document.getElementById('udp_port').value=config.udp.port;
    document.getElementById('sntp_enabled').checked=config.sntp.enabled;
    document.getElementById('sntp_server_ip').value=config.sntp.server_ip;
    document.getElementById('sntp_interval').value=config.sntp.sync_interval_sec;

    toggleLanFields();
    toggleWifiFields();
    toggleWifiModeFields();
    toggleDhcp('wifi_ap');
    toggleDhcp('wifi_sta');
    toggleAllFields();
  }catch(err){
    showNotification('❌ Ошибка загрузки настроек', true);
  }
}
document.addEventListener('DOMContentLoaded',()=>{
  ['mqtt_enabled','tcp_enabled','udp_enabled','sntp_enabled'].forEach(id=>{
    document.getElementById(id).addEventListener('change', toggleAllFields);
  });
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
  toggleAllFields();
  loadConfigFromServer();

  const btn = document.getElementById('change_creds_btn');
  if(btn){
    btn.addEventListener('click', async () => {
      const newu = document.getElementById('new_username').value.trim();
      const oldp = document.getElementById('old_password').value.trim();
      const newp = document.getElementById('new_password').value.trim();
      const conf = document.getElementById('confirm_password').value.trim();

      if(!newu && !newp){
        showNotification('❌ Нет новых данных', true);
        return;
      }
      if(newp && newp !== conf){
        showNotification('❌ Пароли не совпадают', true);
        return;
      }

      const body = new URLSearchParams({ old_password: oldp });
      if(newu) body.append('new_username', newu);
      if(newp) body.append('new_password', newp);

      try{
        const r = await fetch('/api/change_password', {
          method:'POST',
          headers:{'Content-Type':'application/x-www-form-urlencoded'},
          body: body.toString(),
          credentials:'include'
        });
        if(r.ok) showNotification('✅ Данные обновлены');
        else showNotification('❌ Не удалось обновить данные', true);
      }catch(_){
        showNotification('❌ Ошибка соединения', true);
      }
    });
  }
});
