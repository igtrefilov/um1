function showNotification(message, isError=false){
  const notif = document.getElementById('notif');
  notif.textContent = message;
  notif.className = 'notification' + (isError ? ' error' : ' success');
  notif.style.display = 'block';
  setTimeout(()=>notif.style.display='none',3000);
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
    toggleAllFields();
  }catch(err){
    showNotification('❌ Ошибка загрузки настроек', true);
  }
}
document.addEventListener('DOMContentLoaded',()=>{
  ['mqtt_enabled','tcp_enabled','udp_enabled','sntp_enabled'].forEach(id=>{
    document.getElementById(id).addEventListener('change', toggleAllFields);
  });
  toggleAllFields();
  loadConfigFromServer();
});
