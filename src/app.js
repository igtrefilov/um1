(function(){
  // Topbar injection (status placeholders; data from /api/system_info when available)
  function injectTopbar(){
    if (location.pathname === "/login.html") return;
    if(document.querySelector('.topbar')) return;

    const top = document.createElement('div');
    top.className = 'topbar';
    top.innerHTML = `
      <div class="title">УМ1 — Панель управления</div>
      <div class="status">
        <span class="badge" id="status-link">линк: —</span>
        <span class="badge" id="status-wifi">wifi: —</span>
        <span class="badge" id="status-time">—</span>
        <button class="badge" id="logout_btn" title="Выход" style="margin-left:10px;">🔒 Выйти</button>
      </div>`;
    document.body.prepend(top);
  }

  // Unified sidebar loader with active highlight
  async function loadSidebar(){
    const sidebar = document.getElementById('sidebar');
    if(!sidebar) return;
    try {
      const root = location.pathname.startsWith('/settings') || location.pathname.startsWith('/network') || location.pathname.startsWith('/update') || location.pathname.startsWith('/about') ? '..' : '.';
      const res = await fetch(root + '/sidebar.html', {cache:'no-store'});
      const html = await res.text();
      sidebar.innerHTML = html;

      const map = {
        '/index.html':'home', '/':'home',
        '/settings/settings.html':'settings',
        '/network/network.html':'network',
        '/update/update.html':'update',
        '/about/about.html':'about'
      };
      const key = map[location.pathname] || '';
      const active = sidebar.querySelector(`[data-nav="${key}"]`);
      if(active) active.classList.add('active');

      const btn = sidebar.querySelector('#sidebar-collapse');
      if(btn){
        btn.addEventListener('click', () => {
          const collapsed = document.documentElement.classList.toggle('nav-collapsed');
          document.documentElement.style.setProperty('--sidebar-w', collapsed ? 'var(--sidebar-w-compact)' : '260px');
        });
      }
    } catch(e) {
      console.error('Failed to load sidebar', e);
    }
  }

  // Fetch wrapper: inject Basic Auth header if saved; redirect to /login.html on 401
  const _fetch = window.fetch.bind(window);
  window.fetch = async function(resource, init){
    const cfg = JSON.parse(localStorage.getItem('um1_auth') || '{}');
    init = init || {};
    init.headers = new Headers(init.headers || {});
    if(cfg.b64){
      if(!init.headers.has('Authorization')){
        init.headers.set('Authorization', 'Basic ' + cfg.b64);
      }
    }
    const res = await _fetch(resource, init);
    if(res.status === 401 && location.pathname !== '/login.html'){
      location.href = '/login.html';
    }
    return res;
  };

  // Simple notifier
  window.notify = function(msg, type){
    const box = document.getElementById('notif');
    if(!box) return;
    box.textContent = msg;
    box.className = 'notification' + (type ? ' ' + type : '');
    box.style.display = 'block';
    setTimeout(() => box.style.display = 'none', 4000);
  };

  // Public API
  window.UIShell = { injectTopbar, loadSidebar };

  // DOM ready
  document.addEventListener('DOMContentLoaded', () => {
    injectTopbar();
    loadSidebar();

    const btn = document.getElementById('logout_btn');
    if (btn) {
      btn.addEventListener('click', async () => {
        try {
          localStorage.removeItem('um1_auth');
          await fetch('/logout', { method: 'GET', credentials: 'include' });
        } catch (_) {}
        location.href = '/login.html';
      });
    }
  });
})();

