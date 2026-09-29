const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>UmiTech IoT</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; font-family: 'Segoe UI', sans-serif; }
    body { background-color: #f1f8e9; color: #33691e; padding: 20px; display: flex; flex-direction: column; align-items: center; min-height: 100vh; }
    header { display: flex; align-items: center; gap: 10px; margin-bottom: 25px; }
    header h1 { color: #2e7d32; font-size: 1.6rem; }
    
    /* Grid de Leituras */
    .cards-container { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; width: 100%; max-width: 900px; margin-bottom: 25px; }
    .card { background-color: white; padding: 20px; border-radius: 12px; box-shadow: 0 2px 10px rgba(0,0,0,0.08); text-align: center; }
    
    .label { font-size: 0.75rem; text-transform: uppercase; color: #7cb342; font-weight: bold; display: block; margin-bottom: 10px; }
    .value { font-size: 1.6rem; font-weight: bold; }
    .bomba-ligada .value { color: #1b5e20 !important; } 
    .bomba-desligada .value { color: #b71c1c !important; }
    
    /* Container da Tabela */
    .table-container { width: 100%; max-width: 900px; background: white; border-radius: 12px; overflow: hidden; box-shadow: 0 2px 10px rgba(0,0,0,0.08); margin-bottom: 25px; }
    table { width: 100%; border-collapse: collapse; font-size: 0.85rem; }
    thead { background-color: #c5e1a5; }
    th, td { padding: 12px; text-align: center; }
    tbody tr:nth-child(even) { background-color: #f9fbe7; }
    .tabela-ligada { color: #1b5e20; font-weight: bold; }
    .tabela-desligada { color: #b71c1c; font-weight: bold; }
    
    /* Seção de Controles Inferiores */
    .control-container { width: 100%; max-width: 900px; display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }
    .btn-control { background-color: white; padding: 15px; border-radius: 12px; box-shadow: 0 2px 10px rgba(0,0,0,0.08); border: none; font-size: 0.95rem; font-weight: bold; cursor: pointer; transition: transform 0.1s, background-color 0.2s; text-align: center; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 5px; user-select: none; }
    .btn-control:active { transform: scale(0.98); }
    
    /* Cores do Botão de Automação */
    .rotina-ativa { background-color: #e8f5e9; border: 2px solid #a5d6a7; color: #2e7d32; }
    .rotina-bloqueada { background-color: #ffebee; border: 2px solid #ef9a9a; color: #c62828; }
    
    /* CORES CORRIGIDAS DO INTERRUPTOR MANUAL */
    /* 1. Quando Travado/Desativado */
    .btn-manual-desativado { opacity: 0.4; cursor: not-allowed !important; background-color: #eceff1; border: 2px solid #b0bec5; color: #78909c; pointer-events: none; }
    
    /* 2. Quando liberado e a Bomba está Desligada (Sugere ligar - Verde) */
    .btn-manual-bomba-desligada { background-color: #e8f5e9; border: 2px solid #81c784; color: #212121; }
    
    /* 3. Quando liberado e a Bomba está Ligada (Alerta para desligar - Vermelho) */
    .btn-manual-bomba-ligada { background-color: #ffebee; border: 2px solid #e57373; color: #212121; }
    
    .btn-sub-label { font-size: 0.7rem; text-transform: uppercase; opacity: 0.8; font-weight: normal; }

    @keyframes flash { 0% { background-color: #dcedc8; } 100% { background-color: transparent; } }
    .new-row { animation: flash 1.5s ease-out; }
  </style>
</head>
<body>
  <header><h1>🌿 UmiTech IoT: Controle de Solo 🌿</h1></header>
  
  <div class="cards-container">
    <div class="card"><span class="label">Temperatura Ar</span><span id="temp" class="value" style="color:#1b5e20">--</span></div>
    <div class="card"><span class="label">Umidade Solo</span><span id="solo" class="value" style="color:#1b5e20">--</span></div>
    <div id="status-card" class="card"><span class="label">Bomba d'água</span><span id="bomba" class="value">--</span></div>
  </div>

  <div class="table-container">
    <table>
      <thead><tr><th>Data / Hora</th><th>Temp.</th><th>Umidade (%)</th><th>Status Bomba</th></tr></thead>
      <tbody id="tabela-corpo"></tbody>
    </table>
  </div>

  <div class="control-container">
    <button id="rotina-btn" class="btn-control" onclick="AlternarRotina()">
      <span class="btn-sub-label">⚙️ Modo Automação</span>
      <span id="rotina-status" style="font-size: 1.1rem;">--</span>
    </button>
    
    <button id="manual-btn" class="btn-control" onclick="AcionarBombaManual()">
      <span class="btn-sub-label">⚡ Interruptor Manual</span>
      <span id="manual-status" style="font-size: 1.1rem;">BLOQUEADO</span>
    </button>
  </div>

  <script>
    let historicoLocal = [];
    let falhas = 0;
    let automacaoAtiva = true;

    function AlternarRotina() {
      fetch('/toggleRotina')
      .then(response => response.text())
      .then(() => carregar()) 
      .catch(err => console.log("Erro ao alternar modo: " + err.message));
    }

    function AcionarBombaManual() {
      if(automacaoAtiva) return; 
      fetch('/toggleBomba')
      .then(() => carregar())
      .catch(err => console.log("Erro no comando manual: " + err.message));
    }

    function carregar() {
      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), 4000);

      fetch('/dados', { signal: controller.signal })
      .then(response => {
        clearTimeout(timeoutId);
        if (!response.ok) throw new Error('Erro na resposta');
        return response.json();
      })
      .then(d => {
        falhas = 0;
        const agora = new Date().toLocaleString('pt-BR', {
          day: '2-digit', month: '2-digit', year: '2-digit',
          hour: '2-digit', minute: '2-digit', second: '2-digit'
        });
        
        automacaoAtiva = d.atual.rotina;
        const statusBomba = d.atual.bomba;
        
        // Atualização dos Cards Superiores
        document.getElementById('temp').innerText = d.atual.temp > 0 ? d.atual.temp.toFixed(1) + "°C" : "ERRO";
        document.getElementById('solo').innerText = (typeof d.atual.umid === "number") ? (d.atual.umid.toFixed(0) + "%") : "--";
        
        const bCard = document.getElementById('status-card');
        const bTxt = document.getElementById('bomba');
        bTxt.innerText = statusBomba ? "LIGADA" : "DESLIGADA";
        bCard.className = statusBomba ? "card bomba-ligada" : "card bomba-desligada";

        // Atualização do Botão de Modo
        const rBtn = document.getElementById('rotina-btn');
        const rTxt = document.getElementById('rotina-status');
        if (automacaoAtiva) {
          rTxt.innerText = "DISPONÍVEL";
          rBtn.className = "btn-control rotina-ativa";
        } else {
          rTxt.innerText = "INOPERANTE";
          rBtn.className = "btn-control rotina-bloqueada";
        }

        // LÓGICA DE CORES DO BOTÃO MANUAL ATUALIZADA
        const mBtn = document.getElementById('manual-btn');
        const mTxt = document.getElementById('manual-status');
        if (automacaoAtiva) {
          mTxt.innerText = "BLOQUEADO";
          mBtn.className = "btn-control btn-manual-desativado";
        } else {
          if (statusBomba) {
            mTxt.innerText = "🔴 DESLIGAR BOMBA";
            mBtn.className = "btn-control btn-manual-bomba-ligada";
          } else {
            mTxt.innerText = "🟢 LIGAR BOMBA";
            mBtn.className = "btn-control btn-manual-bomba-desligada";
          }
        }

        // Histórico Dinâmico Contínuo
        const novo = { h: agora, t: d.atual.temp, s: d.atual.umid, b: statusBomba };
        historicoLocal.unshift(novo);
        if(historicoLocal.length > 8) historicoLocal.pop(); 
        render(true);
      })
      .catch(error => {
        falhas++;
        console.log("Tentativa de reconexão (" + falhas + "): " + error.message);
      });
    }

    function render(efeito) {
      let h = "";
      historicoLocal.forEach((l, i) => {
        const classeBomba = l.b ? 'tabela-ligada' : 'tabela-desligada';
        const textoBomba = l.b ? 'LIGADA' : 'DESLIGADA';
        const tempTexto = l.t > 0 ? l.t.toFixed(1) + '°C' : '---';
        const umidTexto = (typeof l.s === "number") ? (l.s.toFixed(0) + '%') : '---';
        
        h += `<tr ${efeito && i==0 ? "class='new-row'":""}>
                <td>${l.h}</td>
                <td>${tempTexto}</td>
                <td>${umidTexto}</td>
                <td class="${classeBomba}">${textoBomba}</td>
              </tr>`;
      });
      document.getElementById('tabela-corpo').innerHTML = h;
    }

    setInterval(carregar, 5000); 
    carregar();
  </script>
</body>
</html>
)rawliteral";