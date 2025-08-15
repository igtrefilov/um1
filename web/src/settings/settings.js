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
  const groups = [
    { enabledId: 'mqtt_enabled', fieldIds: ['mqtt_broker','mqtt_username','mqtt_password'] },
    { enabledId: 'sntp_enabled', fieldIds: ['sntp_server_ip','sntp_interval'] }
  ];
  groups.forEach(({enabledId, fieldIds}) => {
    const en = document.getElementById(enabledId);
    const on = en ? en.checked : false;
    fieldIds.forEach(id => {
      const el = document.getElementById(id);
      if (el) el.disabled = !on;
    });
  });
}

function collectSettings(){
  const settings={
    lan:{
      dhcp: document.getElementById('lan_dhcp').checked,
      static_ip: document.getElementById('lan_static_ip').value.trim(),
      subnet: document.getElementById('lan_subnet').value.trim(),
      gateway: document.getElementById('lan_gateway').value.trim()
    },
    wifi:{
      enabled: document.getElementById('wifi_enabled').checked,
      mode: document.getElementById('wifi_mode').value,
      ap:{
        ssid: document.getElementById('wifi_ap_ssid').value.trim(),
        password: document.getElementById('wifi_ap_password').value,
        authmode: document.getElementById('wifi_ap_authmode').value,
        dhcp: document.getElementById('wifi_ap_dhcp').checked,
        static_ip: document.getElementById('wifi_ap_static_ip').value.trim(),
        subnet: document.getElementById('wifi_ap_subnet').value.trim(),
        gateway: document.getElementById('wifi_ap_gateway').value.trim()
      },
      sta:{
        ssid: document.getElementById('wifi_sta_ssid').value.trim(),
        password: document.getElementById('wifi_sta_password').value,
        authmode: document.getElementById('wifi_sta_authmode').value,
        dhcp: document.getElementById('wifi_sta_dhcp').checked,
        static_ip: document.getElementById('wifi_sta_static_ip').value.trim(),
        subnet: document.getElementById('wifi_sta_subnet').value.trim(),
        gateway: document.getElementById('wifi_sta_gateway').value.trim()
      }
    },
    uart1:{
      baudrate:+document.getElementById('uart1_baudrate').value,
      parity:document.getElementById('uart1_parity').value,
      stop_bits:+document.getElementById('uart1_stop_bits').value
    },
    uart2:{
      baudrate:+document.getElementById('uart2_baudrate').value,
      parity:document.getElementById('uart2_parity').value,
      stop_bits:+document.getElementById('uart2_stop_bits').value
    },
    mqtt:{
      enabled:document.getElementById('mqtt_enabled').checked,
      broker:document.getElementById('mqtt_broker').value.trim(),
      username:document.getElementById('mqtt_username').value.trim(),
      password:document.getElementById('mqtt_password').value
    },
    sntp:{
      enabled:document.getElementById('sntp_enabled').checked,
      server_ip:document.getElementById('sntp_server_ip').value.trim(),
      sync_interval_sec:+document.getElementById('sntp_interval').value
    },
    ip_profile:{
      ip1:{
        client:document.getElementById('ip1_client').checked,
        ip:document.getElementById('ip1_address').value.trim(),
        address:document.getElementById('ip1_address').value.trim(),
        port:+document.getElementById('ip1_port').value,
        transport:document.getElementById('ip1_transport').value
      },
      ip2:{
        client:document.getElementById('ip2_client').checked,
        ip:document.getElementById('ip2_address').value.trim(),
        address:document.getElementById('ip2_address').value.trim(),
        port:+document.getElementById('ip2_port').value,
        transport:document.getElementById('ip2_transport').value
      }
    },
    mqtt_profile:{
      mqtt1:{
        tx_topic:document.getElementById('mqtt1_pub_topic').value.trim(),
        rx_topic:document.getElementById('mqtt1_sub_topic').value.trim()
      },
      mqtt2:{
        tx_topic:document.getElementById('mqtt2_pub_topic').value.trim(),
        rx_topic:document.getElementById('mqtt2_sub_topic').value.trim()
      }
    }
  };
  fetch('/config', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(settings)})
    .then(r=>{ if(r.ok) showNotification('✅ Настройки успешно сохранены'); else showNotification('❌ Ошибка сохранения',true); })
    .catch(()=>showNotification('❌ Ошибка соединения с сервером',true));
}

async function loadConfigFromServer(){
  try{
    const response=await fetch('/api/config');
    const config=await response.json();

    document.getElementById('lan_dhcp').checked = !!(config.lan?.dhcp);
    document.getElementById('lan_static_ip').value = config.lan?.static_ip ?? '';
    document.getElementById('lan_subnet').value = config.lan?.subnet ?? '';
    document.getElementById('lan_gateway').value = config.lan?.gateway ?? '';

    document.getElementById('wifi_enabled').checked = !!(config.wifi?.enabled);
    document.getElementById('wifi_mode').value = config.wifi?.mode ?? 'ap';
    document.getElementById('wifi_ap_ssid').value = config.wifi?.ap?.ssid ?? '';
    document.getElementById('wifi_ap_password').value = config.wifi?.ap?.password ?? '';
    document.getElementById('wifi_ap_authmode').value = config.wifi?.ap?.authmode ?? 'open';
    document.getElementById('wifi_ap_dhcp').checked = !!(config.wifi?.ap?.dhcp);
    document.getElementById('wifi_ap_static_ip').value = config.wifi?.ap?.static_ip ?? '';
    document.getElementById('wifi_ap_subnet').value = config.wifi?.ap?.subnet ?? '';
    document.getElementById('wifi_ap_gateway').value = config.wifi?.ap?.gateway ?? '';
    document.getElementById('wifi_sta_ssid').value = config.wifi?.sta?.ssid ?? '';
    document.getElementById('wifi_sta_password').value = config.wifi?.sta?.password ?? '';
    document.getElementById('wifi_sta_authmode').value = config.wifi?.sta?.authmode ?? 'open';
    document.getElementById('wifi_sta_dhcp').checked = !!(config.wifi?.sta?.dhcp);
    document.getElementById('wifi_sta_static_ip').value = config.wifi?.sta?.static_ip ?? '';
    document.getElementById('wifi_sta_subnet').value = config.wifi?.sta?.subnet ?? '';
    document.getElementById('wifi_sta_gateway').value = config.wifi?.sta?.gateway ?? '';

    document.getElementById('uart1_baudrate').value = config.uart1?.baudrate ?? '';
    document.getElementById('uart1_parity').value = config.uart1?.parity ?? 'none';
    document.getElementById('uart1_stop_bits').value = config.uart1?.stop_bits ?? '1';
    document.getElementById('uart2_baudrate').value = config.uart2?.baudrate ?? '';
    document.getElementById('uart2_parity').value = config.uart2?.parity ?? 'none';
    document.getElementById('uart2_stop_bits').value = config.uart2?.stop_bits ?? '1';

    document.getElementById('mqtt_enabled').checked = !!(config.mqtt?.enabled);
    document.getElementById('mqtt_broker').value = config.mqtt?.broker ?? '';
    document.getElementById('mqtt_username').value = config.mqtt?.username ?? '';
    document.getElementById('mqtt_password').value = config.mqtt?.password ?? '';

    document.getElementById('sntp_enabled').checked = !!(config.sntp?.enabled);
    document.getElementById('sntp_server_ip').value = config.sntp?.server_ip ?? '';
    document.getElementById('sntp_interval').value = config.sntp?.sync_interval_sec ?? '';

    document.getElementById('ip1_client').checked = !!(config.ip_profile?.ip1?.client);
    document.getElementById('ip1_address').value = (config.ip_profile?.ip1?.ip ?? config.ip_profile?.ip1?.address ?? '');
    document.getElementById('ip1_port').value = config.ip_profile?.ip1?.port ?? '';
    document.getElementById('ip1_transport').value = config.ip_profile?.ip1?.transport ?? 'TCP';

    document.getElementById('ip2_client').checked = !!(config.ip_profile?.ip2?.client);
    document.getElementById('ip2_address').value = (config.ip_profile?.ip2?.ip ?? config.ip_profile?.ip2?.address ?? '');
    document.getElementById('ip2_port').value = config.ip_profile?.ip2?.port ?? '';
    document.getElementById('ip2_transport').value = config.ip_profile?.ip2?.transport ?? 'TCP';

    document.getElementById('mqtt1_pub_topic').value = config.mqtt_profile?.mqtt1?.tx_topic ?? '';
    document.getElementById('mqtt1_sub_topic').value = config.mqtt_profile?.mqtt1?.rx_topic ?? '';
    document.getElementById('mqtt2_pub_topic').value = config.mqtt_profile?.mqtt2?.tx_topic ?? '';
    document.getElementById('mqtt2_sub_topic').value = config.mqtt_profile?.mqtt2?.rx_topic ?? '';

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
  ['mqtt_enabled','sntp_enabled'].forEach(id=>{
    const el=document.getElementById(id);
    if(el) el.addEventListener('change', toggleAllFields);
  });
  const lanDhcp=document.getElementById('lan_dhcp');
  if(lanDhcp) lanDhcp.addEventListener('change', toggleLanFields);
  const wifiEnabled=document.getElementById('wifi_enabled');
  if(wifiEnabled) wifiEnabled.addEventListener('change', toggleWifiFields);
  const wifiMode=document.getElementById('wifi_mode');
  if(wifiMode) wifiMode.addEventListener('change', toggleWifiModeFields);
  const wifiApDhcp=document.getElementById('wifi_ap_dhcp');
  if(wifiApDhcp) wifiApDhcp.addEventListener('change', () => toggleDhcp('wifi_ap'));
  const wifiStaDhcp=document.getElementById('wifi_sta_dhcp');
  if(wifiStaDhcp) wifiStaDhcp.addEventListener('change', () => toggleDhcp('wifi_sta'));

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

