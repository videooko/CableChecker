#include "WebConfigManager.h"

static const char WEB_UI_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CableChecker — Конфігуратор проєктів</title>
<style>
:root{--bg:#0f172a;--card:#1e293b;--border:#334155;--acc:#38bdf8;--ok:#22c55e;--err:#ef4444;--txt:#f8fafc;--mut:#94a3b8}
*{box-sizing:border-box;font-family:system-ui,-apple-system,sans-serif}
body{margin:0;padding:12px;background:var(--bg);color:var(--txt);max-width:760px;margin-inline:auto}
h1{font-size:1.25rem;margin:0 0 10px;color:var(--acc);display:flex;justify-content:space-between;align-items:center}
.card{background:var(--card);border:1px solid var(--border);border-radius:10px;padding:12px;margin-bottom:12px}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin-bottom:8px}
label{font-size:.85rem;color:var(--mut)}
input,select,button{background:#0b1120;color:var(--txt);border:1px solid var(--border);border-radius:6px;padding:7px 10px;font-size:.9rem}
input[type=text]{flex:1;min-width:130px}
button{cursor:pointer;background:#1d4ed8;border-color:#2563eb;font-weight:600}
button:hover{opacity:.92}
button.sec{background:#334155;border-color:#475569}
button.ok{background:#15803d;border-color:#16a34a}
button.danger{background:#b91c1c;border-color:#dc2626}
.grid-conns{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}
.chip{background:#0b1120;border:1px solid var(--acc);padding:4px 8px;border-radius:6px;font-size:.85rem;display:flex;align-items:center;gap:6px}
.chip span{cursor:pointer;color:var(--err);font-weight:bold}
.matrix-tbl{width:100%;border-collapse:collapse;margin-top:8px;font-size:.8rem;text-align:center}
.matrix-tbl th,.matrix-tbl td{border:1px solid var(--border);padding:5px 2px}
.matrix-tbl td.on{background:#0284c7;color:#fff;font-weight:bold}
.matrix-tbl td.off{cursor:pointer;color:var(--mut)}
.status{padding:8px;border-radius:6px;margin-bottom:10px;display:none;font-size:.9rem}
</style>
</head>
<body>
<h1><span>CableChecker v3</span><small style="font-size:.8rem;color:var(--mut)">192.168.4.1</small></h1>
<div id="statusBox" class="status"></div>

<div class="card">
  <div class="row">
    <label>Проєкт:</label>
    <select id="projSel" onchange="onSelectProject()" style="flex:1"></select>
    <button class="sec" onclick="addProject()">+ Проєкт</button>
    <button class="danger" onclick="delProject()">Видалити</button>
  </div>
  <div class="row">
    <label>Назва проєкту (до 15 лат. симв.):</label>
    <input type="text" id="projName" maxlength="15" oninput="updateProjName()">
  </div>
</div>

<div class="card">
  <div class="row">
    <label>Кабель у проєкті:</label>
    <select id="cabSel" onchange="onSelectCable()" style="flex:1"></select>
    <button class="sec" onclick="addCable()">+ Кабель</button>
    <button class="danger" onclick="delCable()">Видалити</button>
  </div>
  <div class="row">
    <label>Назва кабелю:</label>
    <input type="text" id="cabName" maxlength="15" oninput="updateCabMeta()">
    <label><input type="checkbox" id="cabProbe" onchange="updateCabMeta()"> Live Probe</label>
  </div>
  <div class="row">
    <label>Пінів Роз'єму А (RJ3.1):</label>
    <select id="pinsA" onchange="updateCabMeta()">
      <option>1</option><option>2</option><option>3</option><option>4</option>
      <option>5</option><option>6</option><option>7</option><option>8</option>
    </select>
    <label>Пінів Роз'єму Б (RJ3.2):</label>
    <select id="pinsB" onchange="updateCabMeta()">
      <option>1</option><option>2</option><option>3</option><option>4</option>
      <option>5</option><option>6</option><option>7</option><option>8</option>
    </select>
  </div>
</div>

<div class="card" id="wiringCard">
  <div class="row" style="justify-content:space-between">
    <strong>Схема з'єднань (1:1, крос, 1-на-N, перемички)</strong>
    <div style="display:flex;gap:6px;flex-wrap:wrap">
      <button class="ok" onclick="readLiveCable()">Зчитати з кабелю в тестері</button>
      <button class="sec" onclick="setStraight()">Прямий 1:1</button>
      <button class="sec" onclick="clearLinks()">Очистити</button>
    </div>
  </div>
  <div class="row" style="margin-top:8px">
    <label>Додати лінію:</label>
    <select id="pinFrom"></select>
    <span>&harr;</span>
    <select id="pinTo"></select>
    <button onclick="addLinkPair()">Додати з'єднання</button>
  </div>
  <div id="chipsBox" class="grid-conns"></div>
  <p style="font-size:.78rem;color:var(--mut);margin:8px 0 4px">Матриця A1..A8 &harr; B1..B8 (клікніть клітинку для увімк./вимк.):</p>
  <div style="overflow-x:auto"><table class="matrix-tbl" id="matTbl"></table></div>
</div>

<div class="card">
  <div class="row" style="justify-content:space-between">
    <button class="ok" style="padding:10px 18px;font-size:1rem" onclick="saveAllToDevice()">Зберегти все у тестер</button>
    <div style="display:flex;gap:6px">
      <button class="sec" onclick="exportJson()">Експорт JSON</button>
      <button class="sec" onclick="document.getElementById('impFile').click()">Імпорт JSON</button>
      <input type="file" id="impFile" accept=".json" style="display:none" onchange="importJson(event)">
    </div>
  </div>
</div>

<script>
let state={activeP:0,activeC:0,projects:[]};
let curP=0,curC=0;

function pinLabel(idx){return idx<8?('A'+(idx+1)):('B'+(idx-7));}

function normalizeNet(net){
  for(let i=0;i<16;i++){
    net[i]|=(1<<i);
    for(let j=0;j<16;j++)if(net[i]&(1<<j))net[j]|=(1<<i);
  }
  for(let k=0;k<16;k++)for(let i=0;i<16;i++)if(net[i]&(1<<k))net[i]|=net[k];
  for(let i=0;i<16;i++)net[i]&=~(1<<i);
}

function showMsg(txt,ok=true){
  const b=document.getElementById('statusBox');
  b.style.display='block';
  b.style.background=ok?'#14532d':'#7f1d1d';
  b.textContent=txt;
  setTimeout(()=>{b.style.display='none';},3500);
}

function initPinSelectors(){
  const f=document.getElementById('pinFrom'),t=document.getElementById('pinTo');
  f.innerHTML='';t.innerHTML='';
  for(let i=0;i<16;i++){
    f.innerHTML+=`<option value="${i}">${pinLabel(i)}</option>`;
    t.innerHTML+=`<option value="${i}">${pinLabel(i)}</option>`;
  }
  t.value='8';
}

async function loadData(){
  initPinSelectors();
  const r=await fetch('/api/data');
  state=await r.json();
  curP=state.activeP||0;
  curC=state.activeC||0;
  renderUI();
}

function renderUI(){
  const ps=document.getElementById('projSel');
  ps.innerHTML='';
  state.projects.forEach((p,idx)=>{
    ps.innerHTML+=`<option value="${idx}" ${idx===curP?'selected':''}>${idx+1}. ${p.name} (${p.cables.length})</option>`;
  });
  const proj=state.projects[curP];
  document.getElementById('projName').value=proj.name;

  const cs=document.getElementById('cabSel');
  cs.innerHTML='';
  proj.cables.forEach((c,idx)=>{
    cs.innerHTML+=`<option value="${idx}" ${idx===curC?'selected':''}>${idx+1}. ${c.name} [${c.pinsA}->${c.pinsB}]</option>`;
  });
  const cab=proj.cables[curC];
  document.getElementById('cabName').value=cab.name;
  document.getElementById('cabProbe').checked=!!cab.probe;
  document.getElementById('pinsA').value=cab.pinsA;
  document.getElementById('pinsB').value=cab.pinsB;
  renderWiring(cab);
}

function renderWiring(cab){
  const chips=document.getElementById('chipsBox');
  chips.innerHTML='';
  for(let i=0;i<16;i++){
    for(let j=i+1;j<16;j++){
      if(cab.net[i]&(1<<j)){
        chips.innerHTML+=`<div class="chip">${pinLabel(i)}&harr;${pinLabel(j)} <span onclick="togglePair(${i},${j})">&times;</span></div>`;
      }
    }
  }
  const tbl=document.getElementById('matTbl');
  let h='<tr><th>A \\ B</th>';
  for(let b=0;b<cab.pinsB;b++)h+=`<th>B${b+1}</th>`;
  h+='</tr>';
  for(let a=0;a<cab.pinsA;a++){
    h+=`<tr><th>A${a+1}</th>`;
    for(let b=0;b<cab.pinsB;b++){
      const gB=b+8;
      const on=(cab.net[a]&(1<<gB))!==0;
      h+=`<td class="${on?'on':'off'}" onclick="togglePair(${a},${gB})">${on?'&#10003;':'&middot;'}</td>`;
    }
    h+='</tr>';
  }
  tbl.innerHTML=h;
}

function togglePair(i,j){
  if(i===j)return;
  const cab=state.projects[curP].cables[curC];
  if(cab.net[i]&(1<<j)){
    cab.net[i]&=~(1<<j);
    cab.net[j]&=~(1<<i);
  }else{
    cab.net[i]|=(1<<j);
    cab.net[j]|=(1<<i);
    normalizeNet(cab.net);
  }
  renderWiring(cab);
}

function addLinkPair(){
  const i=parseInt(document.getElementById('pinFrom').value);
  const j=parseInt(document.getElementById('pinTo').value);
  if(i===j)return;
  const cab=state.projects[curP].cables[curC];
  cab.net[i]|=(1<<j);
  cab.net[j]|=(1<<i);
  normalizeNet(cab.net);
  renderWiring(cab);
}

function setStraight(){
  const cab=state.projects[curP].cables[curC];
  cab.net=new Array(16).fill(0);
  const n=Math.min(cab.pinsA,cab.pinsB);
  for(let i=0;i<n;i++){cab.net[i]|=(1<<(i+8));cab.net[i+8]|=(1<<i);}
  renderWiring(cab);
}

function clearLinks(){
  const cab=state.projects[curP].cables[curC];
  cab.net=new Array(16).fill(0);
  renderWiring(cab);
}

async function readLiveCable(){
  const r=await fetch('/api/live');
  const d=await r.json();
  if(d.empty){showMsg('Кабель не виявлено у роз’ємах RJ3.1 / RJ3.2!',false);return;}
  const cab=state.projects[curP].cables[curC];
  cab.net=d.net;
  cab.pinsA=d.pinsA;
  cab.pinsB=d.pinsB;
  normalizeNet(cab.net);
  renderUI();
  showMsg('Розпайку успішно зчитано з фізичного кабелю!');
}

function onSelectProject(){curP=parseInt(document.getElementById('projSel').value);curC=0;renderUI();}
function onSelectCable(){curC=parseInt(document.getElementById('cabSel').value);renderUI();}
function updateProjName(){state.projects[curP].name=document.getElementById('projName').value.replace(/[|\r\n]/g,'')||'Project';}
function updateCabMeta(){
  const cab=state.projects[curP].cables[curC];
  cab.name=document.getElementById('cabName').value.replace(/[|\r\n]/g,'')||'Cable';
  cab.probe=document.getElementById('cabProbe').checked?1:0;
  cab.pinsA=parseInt(document.getElementById('pinsA').value);
  cab.pinsB=parseInt(document.getElementById('pinsB').value);
  renderWiring(cab);
}

function addProject(){
  if(state.projects.length>=8){showMsg('Максимум 8 проєктів!',false);return;}
  state.projects.push({name:'New Project',cables:[{name:'Cable 1',pinsA:8,pinsB:8,probe:0,net:new Array(16).fill(0)}]});
  curP=state.projects.length-1;curC=0;setStraight();renderUI();
}
function delProject(){
  if(state.projects.length<=1){showMsg('Має залишитись хоча б 1 проєкт!',false);return;}
  state.projects.splice(curP,1);curP=0;curC=0;renderUI();
}
function addCable(){
  const p=state.projects[curP];
  if(p.cables.length>=8){showMsg('Максимум 8 кабелів у проєкті!',false);return;}
  p.cables.push({name:'Cable '+(p.cables.length+1),pinsA:4,pinsB:4,probe:0,net:new Array(16).fill(0)});
  curC=p.cables.length-1;setStraight();renderUI();
}
function delCable(){
  const p=state.projects[curP];
  if(p.cables.length<=1){showMsg('Має залишитись хоча б 1 кабель!',false);return;}
  p.cables.splice(curC,1);curC=0;renderUI();
}

async function saveAllToDevice(){
  let lines=[`SEL|${curP}|${curC}`];
  state.projects.forEach(p=>{
    lines.push(`PRJ|${p.name}`);
    p.cables.forEach(c=>{
      normalizeNet(c.net);
      lines.push(`CAB|${c.name}|${c.pinsA}|${c.pinsB}|${c.probe?1:0}|${c.net.join(',')}`);
    });
  });
  const body='data='+encodeURIComponent(lines.join('\n'));
  const r=await fetch('/api/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
  if(r.ok)showMsg('Конфігурацію збережено у LittleFS та активовано на тестері!');
  else showMsg('Помилка збереження!',false);
}

function exportJson(){
  const blob=new Blob([JSON.stringify(state,null,2)],{type:'application/json'});
  const a=document.createElement('a');
  a.href=URL.createObjectURL(blob);
  a.download='cablechecker_projects.json';
  a.click();
}

function importJson(ev){
  const f=ev.target.files[0];if(!f)return;
  const rd=new FileReader();
  rd.onload=e=>{
    try{
      const obj=JSON.parse(e.target.result);
      if(obj.projects&&obj.projects.length){state=obj;curP=0;curC=0;renderUI();showMsg('JSON імпортовано! Натисніть «Зберегти все у тестер».');}
    }catch(err){showMsg('Некоректний файл JSON!',false);}
  };
  rd.readAsText(f);
}

loadData();
</script>
</body>
</html>
)rawliteral";

void WebConfigManager::begin(ProfileManager* profileMgr, CableTester* tester) {
    _profileMgr = profileMgr;
    _tester = tester;
    // За замовчуванням Wi-Fi повністю вимкнений для економії струму та стабільності циклу
    WiFi.persistent(false);
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
}

bool WebConfigManager::startServer() {
    if (_running) return true;

    WiFi.forceSleepWake();
    delay(10);
    WiFi.mode(WIFI_AP);
    IPAddress localIp(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(localIp, gateway, subnet);

    bool apOk = false;
    if (strlen(WIFI_AP_PASS) >= 8) {
        apOk = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
    } else {
        apOk = WiFi.softAP(WIFI_AP_SSID);
    }
    if (!apOk) {
        WiFi.mode(WIFI_OFF);
        return false;
    }

    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/api/data", HTTP_GET, [this]() { handleGetCatalog(); });
    _server.on("/api/live", HTTP_GET, [this]() { handleGetLiveScan(); });
    _server.on("/api/save", HTTP_POST, [this]() { handleSaveCatalog(); });
    _server.on("/api/reset", HTTP_POST, [this]() { handleFactoryReset(); });
    _server.begin();

    _running = true;
    return true;
}

void WebConfigManager::stopServer() {
    if (!_running) return;
    _server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    _running = false;
}

uint8_t WebConfigManager::getConnectedClients() const {
    if (!_running) return 0;
    return WiFi.softAPgetStationNum();
}

bool WebConfigManager::consumeConfigUpdatedFlag() {
    bool v = _configUpdated;
    _configUpdated = false;
    return v;
}

void WebConfigManager::update() {
    if (_running) {
        _server.handleClient();
    }
}

void WebConfigManager::handleRoot() {
    _server.send_P(200, "text/html; charset=utf-8", WEB_UI_HTML);
}

void WebConfigManager::handleGetCatalog() {
    if (!_profileMgr) {
        _server.send(500, "application/json", "{}");
        return;
    }
    String json;
    json.reserve(2400);
    json += "{\"activeP\":";
    json += String(_profileMgr->getActiveProjectIdx());
    json += ",\"activeC\":";
    json += String(_profileMgr->getActiveCableIdx());
    json += ",\"projects\":[";

    uint8_t pCount = _profileMgr->getProjectCount();
    for (uint8_t p = 0; p < pCount; p++) {
        if (p > 0) json += ",";
        const ProjectDef& proj = _profileMgr->getProject(p);
        json += "{\"name\":\"";
        json += proj.name;
        json += "\",\"cables\":[";
        for (uint8_t c = 0; c < proj.cableCount; c++) {
            if (c > 0) json += ",";
            const CableProfile& cab = proj.cables[c];
            json += "{\"name\":\"";
            json += cab.name;
            json += "\",\"pinsA\":";
            json += String(cab.pinsA);
            json += ",\"pinsB\":";
            json += String(cab.pinsB);
            json += ",\"probe\":";
            json += cab.isProbeMode ? "1" : "0";
            json += ",\"net\":[";
            for (uint8_t i = 0; i < TOTAL_PINS; i++) {
                if (i > 0) json += ",";
                json += String(cab.expectedNet[i]);
            }
            json += "]}";
        }
        json += "]}";
    }
    json += "]}";
    _server.send(200, "application/json", json);
}

void WebConfigManager::handleGetLiveScan() {
    if (!_tester) {
        _server.send(500, "application/json", "{\"empty\":true}");
        return;
    }
    uint16_t rawNet[TOTAL_PINS];
    uint16_t stuckLow = 0;
    HardwareFault flt = _tester->scanMatrix(rawNet, stuckLow);
    bool empty = (flt != HW_OK) || CableTester::isMatrixEmpty(rawNet);

    uint8_t pA = 8, pB = 8;
    if (!empty) {
        ProfileManager::detectUsedPinCounts(rawNet, pA, pB);
        if (pA < 4) pA = 4;
        if (pB < 4) pB = 4;
    }

    String json = "{\"empty\":";
    json += empty ? "true" : "false";
    json += ",\"pinsA\":";
    json += String(pA);
    json += ",\"pinsB\":";
    json += String(pB);
    json += ",\"net\":[";
    for (uint8_t i = 0; i < TOTAL_PINS; i++) {
        if (i > 0) json += ",";
        json += String(rawNet[i]);
    }
    json += "]}";
    _server.send(200, "application/json", json);
}

void WebConfigManager::handleSaveCatalog() {
    if (!_profileMgr || !_server.hasArg("data")) {
        _server.send(400, "text/plain", "Bad Request");
        return;
    }
    String body = _server.arg("data");
    StorageImage& img = _profileMgr->rawStorage();

    int8_t curProj = -1;
    uint8_t selP = 0, selC = 0;

    int start = 0;
    while (start < static_cast<int>(body.length())) {
        int end = body.indexOf('\n', start);
        if (end < 0) end = body.length();
        String line = body.substring(start, end);
        line.trim();
        start = end + 1;
        if (line.length() == 0) continue;

        if (line.startsWith("SEL|")) {
            int p1 = line.indexOf('|', 4);
            if (p1 > 4) {
                selP = static_cast<uint8_t>(line.substring(4, p1).toInt());
                selC = static_cast<uint8_t>(line.substring(p1 + 1).toInt());
            }
        } else if (line.startsWith("PRJ|")) {
            if (curProj + 1 < MAX_PROJECTS) {
                curProj++;
                ProjectDef& p = img.projects[curProj];
                memset(&p, 0, sizeof(ProjectDef));
                String pName = line.substring(4);
                strncpy(p.name, pName.c_str(), NAME_MAX_LEN - 1);
                p.name[NAME_MAX_LEN - 1] = '\0';
                p.cableCount = 0;
            }
        } else if (line.startsWith("CAB|") && curProj >= 0) {
            ProjectDef& p = img.projects[curProj];
            if (p.cableCount < MAX_CABLES_PER_PROJECT) {
                CableProfile& c = p.cables[p.cableCount];
                memset(&c, 0, sizeof(CableProfile));

                // Формат: CAB|name|pinsA|pinsB|probe|n0,n1,...,n15
                int idx1 = 4;
                int idx2 = line.indexOf('|', idx1);
                int idx3 = (idx2 > 0) ? line.indexOf('|', idx2 + 1) : -1;
                int idx4 = (idx3 > 0) ? line.indexOf('|', idx3 + 1) : -1;
                int idx5 = (idx4 > 0) ? line.indexOf('|', idx4 + 1) : -1;

                if (idx5 > 0) {
                    String cName = line.substring(idx1, idx2);
                    strncpy(c.name, cName.c_str(), NAME_MAX_LEN - 1);
                    c.name[NAME_MAX_LEN - 1] = '\0';
                    c.pinsA = static_cast<uint8_t>(constrain(line.substring(idx2 + 1, idx3).toInt(), 1, 8));
                    c.pinsB = static_cast<uint8_t>(constrain(line.substring(idx3 + 1, idx4).toInt(), 1, 8));
                    c.isProbeMode = (line.substring(idx4 + 1, idx5).toInt() != 0);

                    String netStr = line.substring(idx5 + 1);
                    int nStart = 0;
                    for (uint8_t k = 0; k < TOTAL_PINS; k++) {
                        int comma = netStr.indexOf(',', nStart);
                        if (comma < 0) comma = netStr.length();
                        c.expectedNet[k] = static_cast<uint16_t>(netStr.substring(nStart, comma).toInt());
                        nStart = comma + 1;
                        if (nStart > static_cast<int>(netStr.length())) break;
                    }
                    ProfileManager::normalizeNetTransitive(c.expectedNet);
                    p.cableCount++;
                }
            }
        }
    }

    if (curProj >= 0) {
        img.projectCount = static_cast<uint8_t>(curProj + 1);
        _profileMgr->selectProjectAndCable(selP, selC, true);
        _configUpdated = true;
        _server.send(200, "text/plain", "OK");
    } else {
        _server.send(400, "text/plain", "Empty catalog");
    }
}

void WebConfigManager::handleFactoryReset() {
    if (_profileMgr) {
        _profileMgr->resetToFactoryDefaults();
        _configUpdated = true;
    }
    _server.send(200, "text/plain", "OK");
}
