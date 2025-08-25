function showNotification(message, isError=false){
  const notif = document.getElementById('notif');
  if(!notif) return;
  notif.textContent = message;
  notif.className = 'notification' + (isError ? ' error' : ' success');
  notif.style.display = 'block';
  notif.scrollIntoView({behavior:'smooth', block:'center'});
  setTimeout(()=>{ if (notif) notif.style.display='none'; },3000);
}

function toggleLanFields() {
  const dhcpEnabled = document.getElementById('lan_dhcp').checked;
  ['lan_static_ip','lan_subnet','lan_gateway'].forEach(id => {
    const el = document.getElementById(id);
    if(el) {
      el.disabled = dhcpEnabled;
      el.title = dhcpEnabled
        ? 'Отключено: DHCP включён'
        : el.title.replace('Отключено: DHCP включён','').trim();
    }
  });
}

function toggleWifiFields() {
  const wifiEnabled = document.getElementById('wifi_enabled').checked;
  const mode = document.getElementById('wifi_mode');
  if (mode) mode.disabled = !wifiEnabled;

  ['wifi_ap_section','wifi_sta_section'].forEach(id => {
    const section = document.getElementById(id);
    if(section) {
      section.querySelectorAll('input, select').forEach(el => el.disabled = !wifiEnabled);
      section.title = wifiEnabled ? section.title : 'Отключено: Wi-Fi выключен';
    }
  });
}

function toggleWifiModeFields() {
  const mode = document.getElementById('wifi_mode').value;
  const ap = document.getElementById('wifi_ap_section');
  const sta = document.getElementById('wifi_sta_section');
  if (ap) ap.style.display  = (mode === 'ap' || mode === 'apsta') ? 'block' : 'none';
  if (sta) sta.style.display = (mode === 'sta' || mode === 'apsta') ? 'block' : 'none';
}

function toggleDhcp(prefix) {
  const dhcp = document.getElementById(prefix + '_dhcp').checked;
  ['static_ip','subnet','gateway'].forEach(f => {
    const el = document.getElementById(prefix + '_' + f);
    if(el) {
      el.disabled = dhcp;
      el.title = dhcp
        ? 'Отключено: DHCP включён'
        : el.title.replace('Отключено: DHCP включён','').trim();
    }
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
      if (el) {
        el.disabled = !on;
        el.title = !on ? 'Отключено: функция выключена' : el.title.replace('Отключено: функция выключена','').trim();
      }
    });
  });
}

function toggleIpProfile(prefix){
  const isClient  = document.getElementById(prefix + '_client').checked;
  const addrInput = document.getElementById(prefix + '_address');
  const addrLabel = document.getElementById(prefix + '_addr_label');
  const portInput = document.getElementById(prefix + '_port');
  const portLabel = document.getElementById(prefix + '_port_label');

  if (!addrInput || !addrLabel || !portLabel || !portInput) return;

  if (isClient){
    // Client mode
    addrInput.disabled = false;
    addrInput.placeholder = 'например, 192.168.1.100';
    addrInput.title = 'Адрес удалённого сервера (режим Клиент)';
    addrLabel.textContent = 'IP сервера:';
    addrLabel.title = 'Адрес удалённого сервера (для режима Клиент). В режиме Сервер не используется.';

    portLabel.textContent = 'Порт:';
    portLabel.title = 'Порт удалённого сервера (в режиме Клиент).';
    portInput.title = 'Номер порта удалённого сервера (Client)';
  } else {
    // Server mode
    addrInput.disabled = true;
    addrInput.placeholder = 'не используется в режиме сервера';
    addrInput.title = 'В режиме Сервер адрес не требуется';
    addrLabel.textContent = 'IP сервера:';
    addrLabel.title = 'В режиме Сервер адрес не используется.';

    portLabel.textContent = 'Порт:';
    portLabel.title = 'Локальный порт прослушивания (в режиме Сервер).';
    portInput.title = 'Номер локального порта (Server)';
  }
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

  fetch('/config', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify(settings)
  })
    .then(r=>{
      if(r.ok) showNotification('✅ Настройки успешно сохранены');
      else showNotification('❌ Ошибка сохранения',true);
    })
    .catch(()=>showNotification('❌ Ошибка соединения с сервером',true));
}

async function loadConfigFromServer(){
  try{
    const response=await fetch('/api/config', { cache: 'no-store' });
    const config=await response.json();

    // LAN
    document.getElementById('lan_dhcp').checked = !!(config.lan?.dhcp);
    document.getElementById('lan_static_ip').value = config.lan?.static_ip ?? '';
    document.getElementById('lan_subnet').value = config.lan?.subnet ?? '';
    document.getElementById('lan_gateway').value = config.lan?.gateway ?? '';

    // Wi-Fi
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

    // UART
    document.getElementById('uart1_baudrate').value = config.uart1?.baudrate ?? '';
    document.getElementById('uart1_parity').value = config.uart1?.parity ?? 'none';
    document.getElementById('uart1_stop_bits').value = config.uart1?.stop_bits ?? '1';

    document.getElementById('uart2_baudrate').value = config.uart2?.baudrate ?? '';
    document.getElementById('uart2_parity').value = config.uart2?.parity ?? 'none';
    document.getElementById('uart2_stop_bits').value = config.uart2?.stop_bits ?? '1';

    // MQTT (broker)
    document.getElementById('mqtt_enabled').checked = !!(config.mqtt?.enabled);
    document.getElementById('mqtt_broker').value = config.mqtt?.broker ?? '';
    document.getElementById('mqtt_username').value = config.mqtt?.username ?? '';
    document.getElementById('mqtt_password').value = config.mqtt?.password ?? '';

    // SNTP
    document.getElementById('sntp_enabled').checked = !!(config.sntp?.enabled);
    document.getElementById('sntp_server_ip').value = config.sntp?.server_ip ?? '';
    document.getElementById('sntp_interval').value = config.sntp?.sync_interval_sec ?? '';

    // IP profile
    document.getElementById('ip1_client').checked = !!(config.ip_profile?.ip1?.client);
    document.getElementById('ip1_address').value = (config.ip_profile?.ip1?.ip ?? config.ip_profile?.ip1?.address ?? '');
    document.getElementById('ip1_port').value = config.ip_profile?.ip1?.port ?? '';
    document.getElementById('ip1_transport').value = config.ip_profile?.ip1?.transport ?? 'TCP';

    document.getElementById('ip2_client').checked = !!(config.ip_profile?.ip2?.client);
    document.getElementById('ip2_address').value = (config.ip_profile?.ip2?.ip ?? config.ip_profile?.ip2?.address ?? '');
    document.getElementById('ip2_port').value = config.ip_profile?.ip2?.port ?? '';
    document.getElementById('ip2_transport').value = config.ip_profile?.ip2?.transport ?? 'TCP';

    // MQTT profiles
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
    toggleIpProfile('ip1');
    toggleIpProfile('ip2');
  }catch(err){
    showNotification('❌ Ошибка загрузки настроек', true);
  }
}

document.addEventListener('DOMContentLoaded',()=>{
  const map = [
    { id:'lan_dhcp', fn: () => toggleLanFields() },
    { id:'wifi_enabled', fn: () => toggleWifiFields() },
    { id:'wifi_mode', fn: () => toggleWifiModeFields() },
    { id:'wifi_ap_dhcp', fn: () => toggleDhcp('wifi_ap') },
    { id:'wifi_sta_dhcp', fn: () => toggleDhcp('wifi_sta') },
    { id:'mqtt_enabled', fn: () => toggleAllFields() },
    { id:'sntp_enabled', fn: () => toggleAllFields() },
    { id:'ip1_client', fn: () => toggleIpProfile('ip1') },
    { id:'ip2_client', fn: () => toggleIpProfile('ip2') },
  ];
  map.forEach(({id, fn}) => {
    const el = document.getElementById(id);
    if (el) el.addEventListener('change', fn);
  });

  const btn = document.getElementById('change_creds_btn');
  if(btn){
    btn.addEventListener('click', async () => {
      const newu = document.getElementById('new_username').value.trim();
      const oldp = document.getElementById('old_password').value.trim();
      const newp = document.getElementById('new_password').value.trim();
      const conf = document.getElementById('confirm_password').value.trim();

      if(!newu && !newp){
        showNotification('❌ Нет новых данных для изменения', true);
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
          credentials:'include',
          cache: 'no-store'
        });
        if(r.ok) showNotification('✅ Данные обновлены');
        else showNotification('❌ Не удалось обновить данные', true);
      }catch(_){
        showNotification('❌ Ошибка соединения', true);
      }
    });
  }

  toggleLanFields();
  toggleWifiFields();
  toggleWifiModeFields();
  toggleDhcp('wifi_ap');
  toggleDhcp('wifi_sta');
  toggleAllFields();
  toggleIpProfile('ip1');
  toggleIpProfile('ip2');

  loadConfigFromServer();
});

