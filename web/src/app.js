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
  window.notify = function(msg, type){
    const box = document.getElementById('notif');
    if(!box) return;
    box.textContent = msg;
    box.className = 'notification' + (type ? ' ' + type : '');
    box.style.display = 'block';
    setTimeout(() => box.style.display = 'none', 4000);
  };
  document.addEventListener('DOMContentLoaded', () => {
    const btn = document.getElementById('logout_btn');
    if(btn){
      btn.addEventListener('click', async (e) => {
        e.preventDefault();
        try {
          localStorage.removeItem('um1_auth');
          await fetch('/logout', { method: 'GET', credentials: 'include' });
        } catch(_){}
        location.href = '/login.html';
      });
    }
  });
})();
