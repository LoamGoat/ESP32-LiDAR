var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
var canvas, ctx;

window.addEventListener('load', onload);

function onload() {
  canvas = document.getElementById('lidarCanvas');
  ctx = canvas.getContext('2d');
  initWebSocket();
}

function initWebSocket() {
  websocket = new WebSocket(gateway);
  websocket.onmessage = onMessage;
  websocket.onclose = () => setTimeout(initWebSocket, 2000);
}

function onMessage(event) {
  var data = JSON.parse(event.data);
  drawScan(data.angles, data.distances);
}

function drawScan(angles, distances) {
  const cx = canvas.width / 2;
  const cy = canvas.height / 2;
  const scale = 0.16; // pixels per mm — tune for your room size

  ctx.fillStyle = "black";
  ctx.fillRect(0, 0, canvas.width, canvas.height);

  ctx.fillStyle = "lime";
  for (let i = 0; i < angles.length; i++) {
    const rad = angles[i] * Math.PI / 180;
    const r = distances[i] * scale;
    const x = cx + r * Math.cos(rad);
    const y = cy + r * Math.sin(rad);
    ctx.fillRect(x, y, 2, 2);
  }

  // Draw the robot at center
  ctx.fillStyle = "red";
  ctx.fillRect(cx - 3, cy - 3, 3, 3);
}