function loadSystemInfo() {
  fetch('/api/system_info')
    .then(resp => resp.json())
    .then(data => {
      const table = document.createElement('table');
      table.className = 'sysinfo-table';
      for (const [key, value] of Object.entries(data)) {
        const row = document.createElement('tr');
        const keyCell = document.createElement('td');
        keyCell.textContent = formatKey(key);
        keyCell.className = 'key';
        const valueCell = document.createElement('td');
        if (key === 'flash_size_bytes') {
          valueCell.textContent = (value / (1024 * 1024)).toFixed(2) + ' МБ';
        } else if (key.includes('heap')) {
          valueCell.textContent = (value / 1024).toFixed(1) + ' КБ';
        } else if (key === 'uptime_ms') {
          valueCell.textContent = (value / 1000).toFixed(1) + ' сек';
        } else {
          valueCell.textContent = value;
        }
        row.appendChild(keyCell);
        row.appendChild(valueCell);
        table.appendChild(row);
      }
      const container = document.getElementById('sysinfo');
      container.innerHTML = '';
      container.appendChild(table);
    })
    .catch(err => {
      console.error('Failed to load system info:', err);
      document.getElementById('sysinfo').textContent = 'Ошибка загрузки информации о системе.';
    });
}
function formatKey(key) {
  return {
    cpu_freq_mhz: 'Частота CPU (МГц)',
    flash_size_bytes: 'Размер Flash (МБ)',
    free_heap: 'Свободная оперативная память (КБ)',
    internal_free_heap: 'Внутренняя свободная память (КБ)',
    dram_free_heap: 'DRAM свободная память (КБ)',
    uptime_ms: 'Время работы (сек)',
    sdk_version: 'Версия SDK',
    running_partition: 'Активный раздел',
    cores: 'Количество ядер',
    revision: 'Ревизия чипа',
    features: 'Функции чипа (битовая маска)',
    mac_address: 'MAC-адрес',
    wifi_rssi: 'Уровень сигнала Wi-Fi (dBm)',
    app_partitions: 'Разделы прошивки'
  }[key] || key;
}
document.addEventListener('DOMContentLoaded', loadSystemInfo);
