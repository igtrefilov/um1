document.addEventListener('DOMContentLoaded', () => {
  const toggles = [
    { intf: 'gw_uart1_intf', profile: 'gw_uart1_profile' },
    { intf: 'gw_uart2_intf', profile: 'gw_uart2_profile' },
    { intf: 'mon_uart1_intf', profile: 'mon_uart1_profile' },
    { intf: 'mon_uart2_intf', profile: 'mon_uart2_profile' },
  ];

  toggles.forEach(({ intf, profile }) => {
    const intfSelect = document.getElementById(intf);
    const profileSelect = document.getElementById(profile);

    function update() {
      const value = intfSelect.value;
      profileSelect.disabled = value === 'uart1' || value === 'uart2';
    }

    intfSelect.addEventListener('change', update);
    update();
  });
});
