document.addEventListener('DOMContentLoaded', () => {
  document.getElementById('login_btn').addEventListener('click', async () => {
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
        setTimeout(() => location.href = '/index.html', 800);
      } else {
        notify('❌ Неверный логин или пароль', 'error');
      }
    } catch (e) {
      notify('⚠️ Ошибка соединения', 'error');
    }
  });
});
