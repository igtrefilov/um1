const logDiv = document.getElementById('log');
let ws;
let pendingReboot = false;
const summaryData = {};

function formatUptime(ms){
  if(!ms) return '—';
  let s = Math.floor(ms/1000);
  const d = Math.floor(s/86400); s%=86400;
  const h = Math.floor(s/3600); s%=3600;
  const m = Math.floor(s/60); s%=60;
  const parts=[];
  if(d) parts.push(d+'д');
  if(h) parts.push(h+'ч');
  if(m) parts.push(m+'м');
  parts.push(s+'с');
  return parts.join(' ');
}

async function loadSummary(){
  try{
    const resp = await fetch('/api/system_info');
    if(!resp.ok) throw new Error('HTTP '+resp.status);
    const data = await resp.json();
    summaryData.system = data;
    document.getElementById('uptime').textContent = 'Время работы: '+formatUptime(data.uptime_ms);
    document.getElementById('firmware').textContent = 'Прошивка: '+(data.running_partition || '—');
    document.getElementById('cpu').textContent = 'CPU: '+(data.cpu_freq_mhz ? data.cpu_freq_mhz+' МГц' : '—');
    document.getElementById('heap').textContent = 'Свободная память: '+(data.free_heap ? Math.round(data.free_heap/1024)+' КБ' : '—');
  }catch(err){
    console.error('summary load error', err);
  }
}

async function loadIfaces(){
  try{
    const resp = await fetch('/api/netinfo');
    if(!resp.ok) throw new Error('HTTP '+resp.status);
    const data = await resp.json();
    summaryData.net = data;
    const box = document.getElementById('ifaces');
    box.innerHTML = ['lan','sta','ap'].map(k => {
      const info = data[k] || {};
      const title = k.toUpperCase();
      let rows;
      if(info.up){
        rows = `<tr><td>IP</td><td>${info.ip || '-'}</td></tr>`+
               `<tr><td>MAC</td><td>${info.mac || '-'}</td></tr>`;
      }else{
        rows = '<tr><td colspan="2">down</td></tr>';
      }
      const actions = `<div class="btn-row" style="margin-top:8px;"><button onclick="renewDHCP('${k}')">Renew DHCP</button><button class="secondary" onclick="openSettings('${k}')">Настройки</button></div>`;
      return `<div class="card iface-card"><h3>${title}</h3><table>${rows}</table>${actions}</div>`;
    }).join('');
  }catch(err){
    console.error('iface load error', err);
  }
}

function copySummary(){
  const lines=[];
  const sys = summaryData.system || {};
  lines.push('Uptime: '+formatUptime(sys.uptime_ms));
  lines.push('Firmware: '+(sys.running_partition || '-'));
  if(sys.cpu_freq_mhz) lines.push('CPU: '+sys.cpu_freq_mhz+' MHz');
  if(sys.free_heap) lines.push('Free heap: '+sys.free_heap);
  const net = summaryData.net || {};
  ['lan','sta','ap'].forEach(k => {
    const info = net[k];
    if(info && info.up){
      lines.push(`${k.toUpperCase()}: IP ${info.ip || '-'} MAC ${info.mac || '-'}`);
    }else{
      lines.push(`${k.toUpperCase()}: down`);
    }
  });
  const text = lines.join('\n');
  navigator.clipboard.writeText(text).then(() => {
    notify('Техническая сводка скопирована','success');
  }).catch(() => {
    notify('Не удалось скопировать','error');
  });
}

function openSettings(iface){
  location.href = '/settings/settings.html#'+iface;
}

function renewDHCP(iface){
  fetch(`/api/renew_dhcp?iface=${iface}`).then(() => {
    notify('Команда отправлена','success');
  }).catch(err => {
    console.error(err);
    notify('Ошибка отправки','error');
  });
}

function start_socket() {
  ws = new WebSocket(`ws://${location.host}/ws`);
  ws.onopen = () => log('✅ Соединение открыто');
  ws.onmessage = e => log(e.data);
  ws.onclose = () => log('❌ Соединение закрыто');
  ws.onerror = err => log('⚠️ Ошибка: ' + err);
}

function log(msg) {
  const line = document.createElement('div');
  const text = String(msg);
  const m = text.match(/^(\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}(?:[.,]\d+)?\]|\[\d{2}:\d{2}:\d{2}(?:[.,]\d+)?\]|(?:\d{4}-\d{2}-\d{2} )?\d{2}:\d{2}:\d{2}(?:[.,]\d+)?)/);
  if (m) {
    const tsSpan = document.createElement('span');
    tsSpan.textContent = m[1] + ' ';
    tsSpan.className = 'ts';
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

function startStream() {
  const uart1 = document.getElementById('uart1').checked;
  const uart2 = document.getElementById('uart2').checked;
  const msg = JSON.stringify({ action: 'START_STREAM', uart1, uart2 });
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(msg);
    log('📤 Sent: ' + msg);
  } else {
    log('⚠️ WebSocket не подключен');
  }
}

function stopStream() {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send('STOP_STREAM');
    log('📤 Sent: STOP_STREAM');
  }
}

function sendMessage() {
  const text = document.getElementById('message').value.trim();
  const uart1 = document.getElementById('uart1').checked;
  const uart2 = document.getElementById('uart2').checked;
  if (!text || !(uart1 || uart2)) return;
  if (text.length % 2 !== 0) {
    log('⚠️ Нечётная длина ввода');
    return;
  }
  const bytes = text.match(/.{1,2}/g).map(h => parseInt(h, 16));
  const payload = new Uint8Array(bytes.length + 1);
  payload[0] = (uart1 ? 1 : 0) | (uart2 ? 2 : 0);
  payload.set(bytes, 1);
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(payload);
    const targets = [];
    if (uart1) targets.push('UART1');
    if (uart2) targets.push('UART2');
  } else {
    log('⚠️ WebSocket не подключен');
  }
}

function showRebootOverlay(show) {
  document.getElementById('reboot_overlay').style.display = show ? 'flex' : 'none';
}

function setRebootProgress(percent, text) {
  const bar = document.getElementById('rb_progress');
  const st = document.getElementById('rb_status');
  if (bar) bar.value = Math.max(0, Math.min(100, percent | 0));
  if (st && text) st.textContent = text;
}

function fetchWithTimeout(url, { timeout = 1000, ...opts } = {}) {
  const ctl = new AbortController();
  const id = setTimeout(() => ctl.abort(), timeout);
  return fetch(url, { ...opts, signal: ctl.signal, cache: 'no-store' })
    .finally(() => clearTimeout(id));
}

async function waitForDevice({ estimateMs = 25000, timeoutMs = 60000, intervalMs = 1000 } = {}) {
  const t0 = Date.now();
  while (Date.now() - t0 < timeoutMs) {
    const elapsed = Date.now() - t0;
    const pct = Math.min(100, Math.round((elapsed / estimateMs) * 100));
    setRebootProgress(pct);
    try {
      const r = await fetchWithTimeout('/api/system_info', { timeout: 1200 });
      if (r.ok) return true;
    } catch (_) {}
    await new Promise(r => setTimeout(r, intervalMs));
  }
  return false;
}

async function sendReboot() {
  pendingReboot = true;
  showRebootOverlay(true);
  setRebootProgress(5, 'Отправляем команду перезагрузки…');
  try {
    const resp = await fetch('/reboot');
    if (!resp.ok) throw new Error('HTTP ' + resp.status);
    setRebootProgress(15, 'Контроллер уходит в перезагрузку…');
    const up = await waitForDevice({ estimateMs: 25000, timeoutMs: 60000 });
    if (up) {
      setRebootProgress(100, 'Готово! Перезагружаем страницу…');
      setTimeout(() => {
        location.replace(`${location.origin}/gateway/gateway.html`);
      }, 800);
    } else {
      setRebootProgress(100, '⏱ Не дождались. Обновите страницу вручную.');
    }
  } catch (err) {
    setRebootProgress(0, '❌ Ошибка: ' + err.message);
  }
}

document.addEventListener('DOMContentLoaded', () => {
  loadSummary();
  loadIfaces();
  document.getElementById('copy_btn').addEventListener('click', copySummary);
  if (!pendingReboot) start_socket();
});

function clearLog(){
  logDiv.innerHTML = '';
}
