document.addEventListener('DOMContentLoaded', () => {
  const btn = document.getElementById('login_btn');

  const doLogin = async () => {
    const u = document.getElementById('login_user').value.trim();
    const p = document.getElementById('login_pass').value.trim();

    const body = new URLSearchParams({ username: u, password: p });

    try {
      const resp = await fetch('/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: body.toString(),
        credentials: 'include'  // <-- важно: отправлять/принимать cookie
      });

      if (resp.ok) {
        notify('✅ Вход выполнен', 'success');
        setTimeout(() => location.href = '/gateway/gateway.html', 800);
      } else {
        notify('❌ Неверный логин или пароль', 'error');
      }
    } catch (e) {
      notify('⚠️ Ошибка соединения', 'error');
    }
  };

  btn.addEventListener('click', doLogin);
  ['login_user', 'login_pass'].forEach(id => {
    const el = document.getElementById(id);
    el.addEventListener('keydown', e => {
      if (e.key === 'Enter') {
        e.preventDefault();
        doLogin();
      }
    });
  });
});
