#pragma once

const char MAIN_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Status</title><style>
body{font-family:sans-serif;margin:20px;background:#f7f7f7;}
nav a{margin-right:10px;}
.gascard{display:flex;background:#fff;padding:15px;margin-bottom:20px;border-radius:10px;box-shadow:0 0 10px rgba(0,0,0,0.1);}
.gasbottle{width:120px;height:300px;position:relative;margin-right:20px;}
.details{flex-grow:1;} .details p{margin:6px 0;font-size:18px;}
.error{color:red;font-weight:bold;}
</style><script>
let current = { gewicht1: 0, gewicht2: 0, sensor1_error: false, sensor2_error: false, rohgewicht1: 0,
  rohgewicht2: 0 };
function getMax(i) {
  let val = document.querySelector('input[name="bottle'+i+'"]:checked');
  return val ? parseInt(val.value) : 11000;
}
function updateSVG() {
  ['1','2'].forEach(i => {
    let error = current['sensor'+i+'_error'];
    let gewicht = Math.max(0, current['gewicht' + i]);
    let max = getMax(i);
    let level = Math.min(100, (gewicht / max) * 100);
    let fillHeight = level * 2.3;
    let fillColor = level > 70 ? '#4caf50' : (level > 30 ? '#ffeb3b' : '#f44336');
    let fillRect = document.getElementById('fillLevel' + i);
    fillRect.setAttribute('y', 280 - fillHeight);
    fillRect.setAttribute('height', fillHeight);
    fillRect.setAttribute('fill', fillColor);
    let gasText = document.getElementById('gas' + i);
    if (error) {
      gasText.innerHTML = "<span class='error'>Sensor nicht gefunden!</span>";
    } else {
      gasText.innerText = level.toFixed(1) + ' %';
    }
    let roh = current['rohgewicht' + i];
document.getElementById('gewicht' + i).innerText =
  (gewicht / 1000).toFixed(2) + ' kg (roh: ' + (roh / 1000).toFixed(2) + ' kg)';
  });
}
function updateData() {
  fetch('/data').then(r => r.json()).then(d => {
    current = d;
    updateSVG();
  });
}
function setupRadioHandlers() {
  document.querySelectorAll('input[type="radio"]').forEach(r => {
    r.addEventListener('change', () => {
      updateSVG();                  // Füllstand aktualisieren
      r.closest("form").submit();   // Formular sofort absenden
    });
  });
}
setInterval(updateData, 1000);
window.onload = function() {
  updateData();
  setupRadioHandlers();
};
</script></head><body>
<h2>Status</h2>%HTML_MENU%
<form method='POST' action='/setbottles'>
<div class='gascard'>
  <h3>linke Flasche</h3>
  <div class='gasbottle'>
    <svg width='120' height='300'>
      <rect x='20' y='50' width='80' height='230' rx='30' ry='30' fill='#7a6753' stroke='#4a3f31' stroke-width='3'/>
      <rect id='fillLevel1' x='20' y='280' width='80' height='0' rx='30' ry='30'/>
      <rect x='45' y='10' width='30' height='40' rx='10' ry='10' fill='#d42f2f' stroke='#a32222' stroke-width='2'/>
      <ellipse cx='60' cy='280' rx='45' ry='15' fill='#5b4b3a'/>
    </svg>
  </div>
  <div class='details'>
    <p><strong>Füllstand:</strong> <span id='gas1'>...</span></p>
    <p><strong>Gewicht:</strong> <span id='gewicht1'>...</span></p>
    <p><strong>Flaschengröße:</strong><br>
      <input type='radio' name='bottle1' value='5000' %B1_5%> 5 kg<br>
      <input type='radio' name='bottle1' value='11000' %B1_11%> 11 kg
    </p>
  </div>
</div>

<div class='gascard'>
  <h3>rechte Flasche</h3>
  <div class='gasbottle'>
    <svg width='120' height='300'>
      <rect x='20' y='50' width='80' height='230' rx='30' ry='30' fill='#7a6753' stroke='#4a3f31' stroke-width='3'/>
      <rect id='fillLevel2' x='20' y='280' width='80' height='0' rx='30' ry='30'/>
      <rect x='45' y='10' width='30' height='40' rx='10' ry='10' fill='#d42f2f' stroke='#a32222' stroke-width='2'/>
      <ellipse cx='60' cy='280' rx='45' ry='15' fill='#5b4b3a'/>
    </svg>
  </div>
  <div class='details'>
    <p><strong>Füllstand:</strong> <span id='gas2'>...</span></p>
    <p><strong>Gewicht:</strong> <span id='gewicht2'>...</span></p>
    <p><strong>Flaschengröße:</strong><br>
      <input type='radio' name='bottle2' value='5000' %B2_5%> 5 kg<br>
      <input type='radio' name='bottle2' value='11000' %B2_11%> 11 kg
    </p>
  </div>
</div>

</form>
</body></html>
)rawliteral";
