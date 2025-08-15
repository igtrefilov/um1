function showNotification(message, isError = false) {
  const n = document.getElementById('notif');
  if (!n) return;
  n.textContent = message;
  n.className = 'notification' + (isError ? ' error' : ' success');
  n.style.display = 'block';
  setTimeout(() => (n.style.display = 'none'), 3000);
}

function showRebootOverlay(show) {
  const o = document.getElementById('reboot_overlay');
  if (o) o.style.display = show ? 'flex' : 'none';
}

function setRebootProgress(p, text) {
  const bar = document.getElementById('rb_progress');
  const st = document.getElementById('rb_status');
  if (bar) bar.value = Math.max(0, Math.min(100, p | 0));
  if (st && text) st.textContent = text;
}

function fetchWithTimeout(url, { timeout = 1000, ...opts } = {}) {
  const ctl = new AbortController();
  const id = setTimeout(() => ctl.abort(), timeout);
  return fetch(url, { ...opts, signal: ctl.signal, cache: 'no-store' }).finally(() =>
    clearTimeout(id)
  );
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
    await new Promise((r) => setTimeout(r, intervalMs));
  }
  return false;
}

async function sendReboot() {
  showRebootOverlay(true);
  setRebootProgress(5, 'Отправляем команду перезагрузки…');
  try {
    const resp = await fetch('/reboot');
    if (!resp.ok) throw new Error('HTTP ' + resp.status);
    setRebootProgress(15, 'Контроллер уходит в перезагрузку…');
    const up = await waitForDevice({ estimateMs: 25000, timeoutMs: 60000 });
    if (up) {
      setRebootProgress(100, 'Готово! Перезагружаем страницу…');
      setTimeout(() => location.reload(), 800);
    } else {
      setRebootProgress(100, '⏱ Не дождались. Обновите страницу вручную.');
    }
  } catch (err) {
    setRebootProgress(0, '❌ Ошибка: ' + err.message);
  }
}

const PROFILE_PAIRS = [
  { intf: 'gw_uart1_intf',  profile: 'gw_uart1_profile'  },
  { intf: 'gw_uart2_intf',  profile: 'gw_uart2_profile'  },
  { intf: 'mon_uart1_intf', profile: 'mon_uart1_profile' },
  { intf: 'mon_uart2_intf', profile: 'mon_uart2_profile' },
];

let BASE_PROFILES = [];

function ensureEmptyOption(profileId) {
  const sel = document.getElementById(profileId);
  if (!sel) return;
  const hasEmpty = Array.from(sel.options).some((o) => o.value === '');
  if (!hasEmpty) {
    const opt = document.createElement('option');
    opt.value = '';
    opt.textContent = '— пусто —';
    sel.insertBefore(opt, sel.firstChild);
  }
}

function getUsedProfiles(excludeProfileId) {
  const used = new Set();
  PROFILE_PAIRS.forEach(({ intf, profile }) => {
    if (profile === excludeProfileId) return;
    const intfVal = document.getElementById(intf)?.value;
    if (intfVal === 'uart1' || intfVal === 'uart2') return;
    const sel = document.getElementById(profile);
    if (sel && sel.value && sel.value !== '') used.add(sel.value);
  });
  return used;
}

function allowedProfilesFor(profileId) {
  const usedElsewhere = getUsedProfiles(profileId);
  return BASE_PROFILES.filter((v) => v === '' || !usedElsewhere.has(v));
}

function refreshProfileSelect(profileId, intfVal) {
  const sel = document.getElementById(profileId);
  if (!sel) return;

  if (intfVal === 'uart1' || intfVal === 'uart2') {
    sel.disabled = true;
    return;
  }

  sel.disabled = false;
  const allowed = allowedProfilesFor(profileId);
  const allowedSet = new Set(allowed);
  const current = sel.value;

  Array.from(sel.options).forEach((opt) => {
    const ok = allowedSet.has(opt.value);
    opt.disabled = !ok;
    opt.hidden = !ok;
  });

  if (!allowedSet.has(current)) {
    if (allowed.length > 0) {
      sel.value = allowed[0];
    } else {
      sel.value = '';
      sel.disabled = true;
      showNotification('Нет свободных профилей для этого слота — освободите один из других.', true);
    }
  }
}

function updateProfilesUniqueness() {
  PROFILE_PAIRS.forEach(({ intf, profile }) => {
    const intfVal = document.getElementById(intf)?.value;
    refreshProfileSelect(profile, intfVal);
  });
}

document.addEventListener('DOMContentLoaded', () => {
  const toggles = [
    { intf: 'gw_uart1_intf', profile: 'gw_uart1_profile' },
    { intf: 'gw_uart2_intf', profile: 'gw_uart2_profile' },
    { intf: 'mon_uart1_intf', profile: 'mon_uart1_profile' },
    { intf: 'mon_uart2_intf', profile: 'mon_uart2_profile' },
  ];

  let cfg = null;

  function fmtUart(key) {
    const u = cfg[key];
    return `${key.toUpperCase()}[${u.baudrate}][${u.parity}][${u.stop_bits}]`;
  }

  function fmtIp(intf, profile) {
    const p = cfg.ip_profile[profile];
    const role = p.client ? 'Client' : 'Server';
    const serverIp = p.client
      ? p.address
      : intf === 'lan'
      ? cfg.lan.static_ip
      : intf === 'ap'
      ? cfg.wifi.ap.static_ip
      : cfg.wifi.sta.static_ip;
    return `${intf.toUpperCase()}[${role}][${serverIp}][${p.port}][${p.transport}]`;
  }

  function fmtMqtt(intf, profile) {
    const p = cfg.mqtt_profile[profile];
    return `${intf.toUpperCase()}[MQTT][${p.tx_topic}][${p.rx_topic}]`;
  }

  function fmtIntf(intf, profile) {
    if (intf === 'uart1' || intf === 'uart2') return fmtUart(intf);
    if (!profile) return intf.toUpperCase();
    if (profile.startsWith('ip')) return fmtIp(intf, profile);
    if (profile.startsWith('mqtt')) return fmtMqtt(intf, profile);
    return intf.toUpperCase();
  }

  function render() {
    if (!cfg) return;
    const gw1 = document.getElementById('gw_uart1_intf').value;
    const gw1p = document.getElementById('gw_uart1_profile').value;
    const mon1 = document.getElementById('mon_uart1_intf').value;
    const mon1p = document.getElementById('mon_uart1_profile').value;
    const gw2 = document.getElementById('gw_uart2_intf').value;
    const gw2p = document.getElementById('gw_uart2_profile').value;
    const mon2 = document.getElementById('mon_uart2_intf').value;
    const mon2p = document.getElementById('mon_uart2_profile').value;
    const line1 = `Gate:${fmtUart('uart1')}<->${fmtIntf(gw1, gw1 === 'uart2' ? null : gw1p)} Monitor:${fmtIntf(mon1, mon1 === 'uart1' || mon1 === 'uart2' ? null : mon1p)}`;
    const line2 = `Gate:${fmtUart('uart2')}<->${fmtIntf(gw2, gw2 === 'uart1' ? null : gw2p)} Monitor:${fmtIntf(mon2, mon2 === 'uart1' || mon2 === 'uart2' ? null : mon2p)}`;
    document.getElementById('route_info').textContent = `${line1}\n\n${line2}`;
  }

  function saveRoutes() {
    const routing = {
      gateway: {
        uart1: { intf: document.getElementById('gw_uart1_intf').value, profile: document.getElementById('gw_uart1_profile').value },
        uart2: { intf: document.getElementById('gw_uart2_intf').value, profile: document.getElementById('gw_uart2_profile').value },
      },
      monitor: {
        uart1: { intf: document.getElementById('mon_uart1_intf').value, profile: document.getElementById('mon_uart1_profile').value },
        uart2: { intf: document.getElementById('mon_uart2_intf').value, profile: document.getElementById('mon_uart2_profile').value },
      },
    };
    fetch('/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ routing }) })
      .then((r) => {
        if (r.ok) showNotification('✅ Маршруты сохранены');
        else showNotification('❌ Ошибка сохранения', true);
      })
      .catch((_) => showNotification('❌ Ошибка соединения', true));
  }

  document.getElementById('save_routes_btn')?.addEventListener('click', saveRoutes);

  toggles.forEach(({ intf, profile }) => {
    const intfSelect = document.getElementById(intf);
    const profileSelect = document.getElementById(profile);

    function update() {
      const value = intfSelect.value;
      profileSelect.disabled = value === 'uart1' || value === 'uart2';
      updateProfilesUniqueness();
      render();
    }

    intfSelect.addEventListener('change', update);
    profileSelect.addEventListener('change', () => {
      updateProfilesUniqueness();
      render();
    });

    update();
  });

  fetch('/config.json')
    .then((r) => r.json())
    .then((data) => {
      cfg = data;

      const intfIds = ['gw_uart1_intf', 'gw_uart2_intf', 'mon_uart1_intf', 'mon_uart2_intf'];
      const profIds = ['gw_uart1_profile', 'gw_uart2_profile', 'mon_uart1_profile', 'mon_uart2_profile'];

      const setValue = (id, val) => {
        const el = document.getElementById(id);
        el.value = (val === undefined || val === null) ? '' : val;
        if (el.value !== ((val === undefined || val === null) ? '' : val)) el.selectedIndex = 0;
        return el;
      };

      if (!cfg.wifi || !cfg.wifi.enabled) {
        intfIds.forEach((id) => {
          const sel = document.getElementById(id);
          ['ap', 'sta'].forEach((v) => sel.querySelector(`option[value="${v}"]`)?.remove());
        });
      } else {
        if (cfg.wifi.mode !== 'ap' && cfg.wifi.mode !== 'apsta') {
          intfIds.forEach((id) => document.getElementById(id).querySelector('option[value="ap"]')?.remove());
        }
        if (cfg.wifi.mode !== 'sta' && cfg.wifi.mode !== 'apsta') {
          intfIds.forEach((id) => document.getElementById(id).querySelector('option[value="sta"]')?.remove());
        }
      }

      if (!cfg.mqtt || !cfg.mqtt.enabled) {
        profIds.forEach((id) => {
          const sel = document.getElementById(id);
          ['mqtt1', 'mqtt2'].forEach((v) => sel.querySelector(`option[value="${v}"]`)?.remove());
        });
      }

      profIds.forEach(ensureEmptyOption);

      if (cfg.routing) {
        const r = cfg.routing;
        if (r.gateway) {
          if (r.gateway.uart1) {
            setValue('gw_uart1_intf', r.gateway.uart1.intf);
            setValue('gw_uart1_profile', r.gateway.uart1.profile ?? '');
          }
          if (r.gateway.uart2) {
            setValue('gw_uart2_intf', r.gateway.uart2.intf);
            setValue('gw_uart2_profile', r.gateway.uart2.profile ?? '');
          }
        }
        if (r.monitor) {
          if (r.monitor.uart1) {
            setValue('mon_uart1_intf', r.monitor.uart1.intf);
            setValue('mon_uart1_profile', r.monitor.uart1.profile ?? '');
          }
          if (r.monitor.uart2) {
            setValue('mon_uart2_intf', r.monitor.uart2.intf);
            setValue('mon_uart2_profile', r.monitor.uart2.profile ?? '');
          }
        }
      }

      ['gw_uart1', 'gw_uart2', 'mon_uart1', 'mon_uart2'].forEach((p) => {
        const intf = document.getElementById(p + '_intf');
        const profile = document.getElementById(p + '_profile');
        profile.disabled = intf.value === 'uart1' || intf.value === 'uart2';
      });

      const base = new Set(['']);
      ['gw_uart1_profile', 'gw_uart2_profile', 'mon_uart1_profile', 'mon_uart2_profile'].forEach((id) => {
        const sel = document.getElementById(id);
        if (sel) sel.querySelectorAll('option').forEach((o) => base.add(o.value));
      });
      BASE_PROFILES = Array.from(base);

      updateProfilesUniqueness();
      render();
    })
    .catch(() => {});
});

