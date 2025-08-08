function createInterfaceSelect(cls, value=''){
  const sel = document.createElement('select');
  sel.className = cls;
  ['','UART1','UART2','LAN','AP','STA'].forEach(v=>{
    const opt = document.createElement('option');
    opt.value = v;
    opt.textContent = v || '—';
    if(v===value) opt.selected = true;
    sel.appendChild(opt);
  });
  return sel;
}

function updateEndpointExtra(sel){
  const prefix = sel.classList.contains('src-if') ? 'src' : 'dst';
  const container = sel.parentElement.querySelector('.'+prefix+'-extra');
  const val = sel.value;
  let html = '';
  if(val==='UART1' || val==='UART2'){
    html = `Baudrate: <input type="number" class="${prefix}-baudrate" value="115200"/> `+
           `Parity: <select class="${prefix}-parity"><option value="none">none</option><option value="even">even</option><option value="odd">odd</option></select> `+
           `Bits: <select class="${prefix}-bits"><option value="8">8</option><option value="7">7</option></select>`;
  }else if(['LAN','AP','STA'].includes(val)){
    html = `Protocol: <select class="${prefix}-protocol"><option value="TCP">TCP</option><option value="UDP">UDP</option><option value="MQTT">MQTT</option></select> <span class="${prefix}-proto-extra"></span>`;
  }
  container.innerHTML = html;
  if(['LAN','AP','STA'].includes(val)){
    const psel = container.querySelector('.'+prefix+'-protocol');
    const updateProto = ()=>{
      const pv = psel.value;
      const protoDiv = container.querySelector('.'+prefix+'-proto-extra');
      let phtml='';
      if(pv==='MQTT'){
        phtml = `Role: <select class="${prefix}-role"><option value="publish">Publish</option><option value="subscribe">Subscribe</option></select> Topic: <input type="text" class="${prefix}-topic"/>`;
      }else{
        phtml = `Role: <select class="${prefix}-role"><option value="client">Client</option><option value="server">Server</option></select> IP: <input type="text" class="${prefix}-ip"/> Port: <input type="number" class="${prefix}-port"/>`;
      }
      protoDiv.innerHTML = phtml;
    };
    psel.addEventListener('change', updateProto);
    updateProto();
  }
}

function addRow(route){
  const tbody = document.getElementById('route_body');
  const tr = document.createElement('tr');

  const tdSrc = document.createElement('td');
  const srcSel = createInterfaceSelect('src-if', route?.source?.interface);
  tdSrc.appendChild(srcSel);
  const srcExtra = document.createElement('div');
  srcExtra.className = 'src-extra';
  tdSrc.appendChild(srcExtra);
  tr.appendChild(tdSrc);

  const tdDst = document.createElement('td');
  const dstSel = createInterfaceSelect('dst-if', route?.destination?.interface);
  tdDst.appendChild(dstSel);
  const dstExtra = document.createElement('div');
  dstExtra.className = 'dst-extra';
  tdDst.appendChild(dstExtra);
  tr.appendChild(tdDst);

  const tdAct = document.createElement('td');
  const rm = document.createElement('button');
  rm.textContent = 'Удалить';
  rm.addEventListener('click', ()=>tr.remove());
  tdAct.appendChild(rm);
  tr.appendChild(tdAct);

  tbody.appendChild(tr);
  srcSel.addEventListener('change', ()=>updateEndpointExtra(srcSel));
  dstSel.addEventListener('change', ()=>updateEndpointExtra(dstSel));
  if(route) {
    updateEndpointExtra(srcSel);
    updateEndpointExtra(dstSel);
    if(route.source){
      if(route.source.baudrate) tr.querySelector('.src-baudrate').value = route.source.baudrate;
      if(route.source.parity) tr.querySelector('.src-parity').value = route.source.parity;
      if(route.source.bits) tr.querySelector('.src-bits').value = route.source.bits;
      if(route.source.protocol){
        tr.querySelector('.src-protocol').value = route.source.protocol;
        updateEndpointExtra(srcSel); // rerender proto
        const srcProto = tr.querySelector('.src-protocol');
        srcProto.value = route.source.protocol;
        srcProto.dispatchEvent(new Event('change'));
        const protoDiv = tr.querySelector('.src-proto-extra');
        if(route.source.role) protoDiv.querySelector('.src-role').value = route.source.role;
        if(route.source.ip) protoDiv.querySelector('.src-ip').value = route.source.ip;
        if(route.source.port) protoDiv.querySelector('.src-port').value = route.source.port;
        if(route.source.topic) protoDiv.querySelector('.src-topic').value = route.source.topic;
      }
    }
    if(route.destination){
      if(route.destination.baudrate) tr.querySelector('.dst-baudrate').value = route.destination.baudrate;
      if(route.destination.parity) tr.querySelector('.dst-parity').value = route.destination.parity;
      if(route.destination.bits) tr.querySelector('.dst-bits').value = route.destination.bits;
      if(route.destination.protocol){
        tr.querySelector('.dst-protocol').value = route.destination.protocol;
        updateEndpointExtra(dstSel);
        const dstProto = tr.querySelector('.dst-protocol');
        dstProto.value = route.destination.protocol;
        dstProto.dispatchEvent(new Event('change'));
        const protoDiv = tr.querySelector('.dst-proto-extra');
        if(route.destination.role) protoDiv.querySelector('.dst-role').value = route.destination.role;
        if(route.destination.ip) protoDiv.querySelector('.dst-ip').value = route.destination.ip;
        if(route.destination.port) protoDiv.querySelector('.dst-port').value = route.destination.port;
        if(route.destination.topic) protoDiv.querySelector('.dst-topic').value = route.destination.topic;
      }
    }
  }
}

function collectRoutes(){
  const routes = [];
  document.querySelectorAll('#route_body tr').forEach(tr=>{
    const srcIf = tr.querySelector('.src-if').value;
    const dstIf = tr.querySelector('.dst-if').value;
    const route = { source:{ interface: srcIf }, destination:{ interface: dstIf }, active:true };
    if(srcIf.startsWith('UART')){
      route.source.baudrate = parseInt(tr.querySelector('.src-baudrate').value)||0;
      route.source.parity = tr.querySelector('.src-parity').value;
      route.source.bits = parseInt(tr.querySelector('.src-bits').value)||0;
    }else if(['LAN','AP','STA'].includes(srcIf)){
      route.source.protocol = tr.querySelector('.src-protocol').value;
      const div = tr.querySelector('.src-proto-extra');
      route.source.role = div.querySelector('.src-role').value;
      if(route.source.protocol === 'MQTT'){
        route.source.topic = div.querySelector('.src-topic').value;
      }else{
        route.source.ip = div.querySelector('.src-ip').value;
        route.source.port = parseInt(div.querySelector('.src-port').value)||0;
      }
    }
    if(dstIf.startsWith('UART')){
      route.destination.baudrate = parseInt(tr.querySelector('.dst-baudrate').value)||0;
      route.destination.parity = tr.querySelector('.dst-parity').value;
      route.destination.bits = parseInt(tr.querySelector('.dst-bits').value)||0;
    }else if(['LAN','AP','STA'].includes(dstIf)){
      route.destination.protocol = tr.querySelector('.dst-protocol').value;
      const div = tr.querySelector('.dst-proto-extra');
      route.destination.role = div.querySelector('.dst-role').value;
      if(route.destination.protocol === 'MQTT'){
        route.destination.topic = div.querySelector('.dst-topic').value;
      }else{
        route.destination.ip = div.querySelector('.dst-ip').value;
        route.destination.port = parseInt(div.querySelector('.dst-port').value)||0;
      }
    }
    routes.push(route);
  });
  return routes;
}

function loadRoutes(){
  fetch('/api/config')
    .then(r=>r.json())
    .then(cfg=>{
      (cfg.routes||[]).forEach(r=>addRow(r));
    })
    .catch(_=>{});
}

const addBtn = document.getElementById('add_route');
if(addBtn) addBtn.addEventListener('click', ()=>addRow());
const saveBtn = document.getElementById('save_routes');
if(saveBtn) saveBtn.addEventListener('click', ()=>{
  const routes = collectRoutes();
  fetch('/config', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({routes})})
    .then(r=>{ if(r.ok) notify('✅ Маршруты сохранены'); else notify('❌ Ошибка', 'error'); })
    .catch(_=>notify('❌ Ошибка соединения', 'error'));
});
