(function(){
  const _fetch = window.fetch.bind(window);
  window.fetch = async function(resource, init = {}){
    init.headers = new Headers(init.headers || {});
    const cfg = JSON.parse(localStorage.getItem('um1_auth') || '{}');
    if(cfg.b64 && !init.headers.has('Authorization')){
      init.headers.set('Authorization', 'Basic ' + cfg.b64);
    }
    const res = await _fetch(resource, init);
    if(res.status === 401 && location.pathname !== '/login.html'){
      location.href = '/login.html';
    }
    return res;
  };

  // ------- THEME -------
  const THEME_KEY = 'um1_theme';

  function currentTheme(){
    if (document.documentElement.classList.contains('theme-light')) return 'light';
    return 'dark';
  }
  function applyTheme(theme){
    const root = document.documentElement;
    root.classList.toggle('theme-light', theme === 'light');
    root.classList.toggle('theme-dark', theme !== 'light');
  }
  function loadTheme(){
    const saved = localStorage.getItem(THEME_KEY);
    if (saved) { applyTheme(saved); return saved; }
    return currentTheme();
  }
  function toggleTheme(){
    const next = currentTheme() === 'light' ? 'dark' : 'light';
    localStorage.setItem(THEME_KEY, next);
    applyTheme(next);
    updateThemeButton(next);
  }
  function ensureThemeButton(){
    const header = document.querySelector('header');
    if(!header) return;

    let btn = document.getElementById('theme_toggle_btn');
    if(!btn){
      btn = document.createElement('button');
      btn.id = 'theme_toggle_btn';
      btn.className = 'icon-btn theme-toggle';
      btn.type = 'button';
      btn.setAttribute('aria-label', 'Переключить тему');
      btn.addEventListener('click', toggleTheme);
      header.insertBefore(btn, header.firstChild);
    }
    updateThemeButton(currentTheme());
  }
  function updateThemeButton(theme){
    const btn = document.getElementById('theme_toggle_btn');
    if(!btn) return;
    btn.setAttribute('data-icon', theme === 'light' ? 'sun' : 'moon');
    btn.title = theme === 'light' ? 'Светлая тема (нажмите для тёмной)' : 'Тёмная тема (нажмите для светлой)';
  }
  // ------- end THEME -------

  window.notify = function(msg, type){
    const box = document.getElementById('notif');
    if(!box) return;
    box.textContent = msg;
    box.className = 'notification' + (type ? ' ' + type : '');
    box.style.display = 'block';
    box.scrollIntoView({behavior:'smooth', block:'center'});
    setTimeout(() => box.style.display = 'none', 4000);
  };

  document.addEventListener('DOMContentLoaded', () => {
    loadTheme();
    ensureThemeButton();

    const btn = document.getElementById('logout_btn');
    if(btn){
      btn.addEventListener('click', async (e) => {
        e.preventDefault();
        try {
          localStorage.removeItem('um1_auth');
          await fetch('/logout', { method: 'POST', credentials: 'include' });
        } catch(_){}
        location.href = '/login.html';
      });
    }
  });
})();

