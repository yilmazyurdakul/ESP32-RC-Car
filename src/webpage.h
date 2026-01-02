#pragma once

const char webpageHTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">

<title>RC Car Controller</title>

<style>
  body {
    background:#0d0d0d;
    color:#eaeaea;
    font-family:'Arial', sans-serif;
    text-align:center;
    margin:0;
    padding:0;
  }

  .status-bar {
    background:#1a1a1a;
    padding:10px;
    font-size:15px;
    border-bottom:2px solid #333;
  }

  #conn {
    color:#ff0;
    display:block;
  }

  #vinText {
    font-size:14px;
    color:#ffb347;
    margin-top:4px;
  }

  /* Battery bar */
  #batteryBox {
    width:120px;
    height:20px;
    border:2px solid #555;
    margin:6px auto;
    border-radius:4px;
    position:relative;
  }
  #batteryFill {
    height:100%;
    width:0%;
    background:#44ff44;
    border-radius:2px;
    transition:0.3s;
  }
  #batteryCap {
    width:6px;
    height:10px;
    background:#555;
    position:absolute;
    right:-8px;
    top:5px;
    border-radius:2px;
  }

  .title {
    font-size:26px;
    margin-top:15px;
    letter-spacing:1px;
    color:#fff;
  }

  /* Joystick */
  #joystick {
    margin:20px auto;
    width:220px;
    height:220px;
    background:#181818;
    border-radius:12px;
    position:relative;
    touch-action:none;
    user-select:none;
    border:2px solid #333;
  }
  #stick {
    width:75px;
    height:75px;
    background:#444;
    border-radius:50%;
    position:absolute;
    left:50%;
    top:50%;
    transform:translate(-50%, -50%);
    transition:0.08s ease-out;
    box-shadow:0 0 12px #000 inset;
  }

  .section-title {
    margin-top:25px;
    font-size:18px;
    color:#ccc;
  }

  .btn {
    width:150px;
    height:42px;
    margin:8px;
    font-size:16px;
    background:#222;
    color:#eee;
    border:none;
    border-radius:8px;
    box-shadow:0 0 10px #000;
    transition:0.15s;
  }
  .btn:hover {
    background:#2d2d2d;
    box-shadow:0 0 12px #09f;
  }
  .btn:active {
    background:#444;
    transform:scale(0.97);
  }

  input[type=range] {
    width:240px;
    margin-top:10px;
  }
</style>
</head>

<body>

<!-- Status bar -->
<div class="status-bar">
  <span id="conn">Connecting...</span>

  <div id="batteryBox">
    <div id="batteryFill"></div>
    <div id="batteryCap"></div>
  </div>

  <span id="vinText">VIN: -- V (--)%</span>
</div>

<div class="title">RC Car Controller</div>

<!-- Joystick -->
<div id="joystick">
  <div id="stick"></div>
</div>

<div class="section-title">Joystick Sensitivity</div>
<input type="range" min="0.5" max="2" step="0.1" value="1" id="sens"
       oninput="updateSensitivity()">

<div class="section-title">Max Power</div>
<input type="range" min="20" max="255" value="255" id="maxPowerSlider"
       oninput="send('MAXPOWER:'+this.value)">

<div class="section-title">Lights</div>
<button class="btn" onclick="send('HEAD_ON')">Low Beam</button>
<button class="btn" onclick="flashHighBeam()">Flash High Beam</button>
<button class="btn" onclick="send('HEAD_HIGH')">High Beam</button>
<button class="btn" onclick="send('HEAD_OFF')">Lights OFF</button>


<script>
// ============================================================
// WEBSOCKET
// ============================================================

let socket;
let sensitivity = 1.0;

function initWS() {
  socket = new WebSocket("ws://" + location.host + "/ws");

  socket.onopen = () => {
    document.getElementById("conn").innerHTML = "Connected";
    document.getElementById("conn").style.color = "#0f0";
  };

  socket.onclose = () => {
    document.getElementById("conn").innerHTML = "Disconnected";
    document.getElementById("conn").style.color = "#f00";
    setTimeout(initWS, 1000);
  };
}

function send(msg) {
  if (socket && socket.readyState === WebSocket.OPEN) {
    socket.send(msg);
  }
}

window.onload = initWS;


// ============================================================
// HEARTBEAT (ALIVE)
// ============================================================
setInterval(() => {
  send("ALIVE");
}, 80);


// ============================================================
// HIGH BEAM FLASH
// ============================================================
function flashHighBeam() {
  send("HEAD_HIGH");
  setTimeout(()=>send("HEAD_ON"), 180);
  setTimeout(()=>send("HEAD_HIGH"), 350);
  setTimeout(()=>send("HEAD_ON"), 520);
  setTimeout(()=>send("HEAD_HIGH"), 700);
  setTimeout(()=>send("HEAD_ON"), 950);
}


// ============================================================
// BATTERY STATUS
// ============================================================

setInterval(() => {
  fetch("/vin")
    .then(r => r.json())
    .then(j => {
      let v = j.voltage.toFixed(2);
      let p = j.percent;

      document.getElementById("vinText").innerHTML =
        "VIN: " + v + " V (" + p + "%)";

      document.getElementById("batteryFill").style.width = p + "%";

      if (p < 20) document.getElementById("batteryFill").style.background = "#ff4444";
      else if (p < 50) document.getElementById("batteryFill").style.background = "#ffcc44";
      else document.getElementById("batteryFill").style.background = "#44ff44";
    })
    .catch(e => {
      document.getElementById("vinText").innerHTML = "VIN: -- V (--%)";
      document.getElementById("batteryFill").style.width = "0%";
      document.getElementById("batteryFill").style.background = "#555";
    });
}, 2000);


// ============================================================
// SENSITIVITY
// ============================================================

function updateSensitivity() {
  sensitivity = parseFloat(document.getElementById('sens').value);
}


// ============================================================
// JOYSTICK CONTROL
// ============================================================

const joy = document.getElementById('joystick');
const stick = document.getElementById('stick');
let dragging = false;

function resetStick(){
  stick.style.left="50%";
  stick.style.top ="50%";
  stick.style.transform="translate(-50%, -50%)";
}

function updateStick(dx,dy){
  stick.style.left=(50+dx)+"%";
  stick.style.top =(50+dy)+"%";
  stick.style.transform="translate(-50%, -50%)";
}

function handleMove(clientX, clientY){
  const rect = joy.getBoundingClientRect();
  const cx = rect.left + rect.width/2;
  const cy = rect.top  + rect.height/2;

  let dx = (clientX - cx) * sensitivity;
  let dy = (clientY - cy) * sensitivity;

  const max = rect.width/2 - 40;
  const dist = Math.sqrt(dx*dx+dy*dy);

  if(dist > max){
    dx = dx * max / dist;
    dy = dy * max / dist;
  }

  let nx = (dx/max)*50;
  let ny = (dy/max)*50;

  updateStick(nx, ny);

  const steer = Math.round((nx/50)*100);
  const throttle = Math.round((-ny/50)*100);

  send(`JOY:${steer},${throttle}`);
}

joy.addEventListener('pointerdown', e => {
  dragging=true;
  stick.style.transition="0s";
  handleMove(e.clientX, e.clientY);
});

window.addEventListener('pointermove', e => {
  if(dragging) handleMove(e.clientX,e.clientY);
});

window.addEventListener('pointerup', () => {
  dragging=false;
  stick.style.transition="0.1s";
  resetStick();
  send("JOY:0,0");
});

window.addEventListener('pointerleave', () => {
  dragging=false;
  stick.style.transition="0.1s";
  resetStick();
  send("JOY:0,0");
});

</script>

</body>
</html>
)HTML";
