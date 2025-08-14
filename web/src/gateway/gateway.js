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

  toggles.forEach(({ intf, profile }) => {
    const intfSelect = document.getElementById(intf);
    const profileSelect = document.getElementById(profile);

    function update() {
      const value = intfSelect.value;
      profileSelect.disabled = value === 'uart1' || value === 'uart2';
      render();
    }

    intfSelect.addEventListener('change', update);
    profileSelect.addEventListener('change', render);
    update();
  });

  fetch('/config.json')
    .then(r => r.json())
    .then(data => { cfg = data; render(); })
    .catch(() => {});
});
