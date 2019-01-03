import { Accelerometer } from "accelerometer";
import { Barometer } from "barometer";
import { BodyPresenceSensor } from "body-presence";
import document from "document";
import { Gyroscope } from "gyroscope";
import { HeartRateSensor } from "heart-rate";
import { OrientationSensor } from "orientation";
import * as messaging from "messaging";

const accel = new Accelerometer();
const bar = new Barometer();
const bps = new BodyPresenceSensor();
const gyro = new Gyroscope();
const hrm = new HeartRateSensor();
const orientation = new OrientationSensor();

accel.start();
bar.start();
bps.start();
gyro.start();
hrm.start();
orientation.start();

const accelData = document.getElementById("accel-data");
const barData = document.getElementById("bar-data");
const bpsData = document.getElementById("bps-data");
const gyroData = document.getElementById("gyro-data");
const hrmData = document.getElementById("hrm-data");
const orientationData = document.getElementById("orientation-data");

messaging.peerSocket.onopen = () => {
  console.log("Ready");
}

messaging.peerSocket.onerror = (err) => {
  console.log(`Connection error: ${err.code} - ${err.message}`);
}

messaging.peerSocket.onmessage = (evt) => {
  console.log(JSON.stringify(evt.data));
}

function sendDataToCompanion(data) {
  if (messaging.peerSocket.readyState === messaging.peerSocket.OPEN) {
    messaging.peerSocket.send(data);
  }
}

function refreshData() {
  const data = {
    accel: {
      x: accel.x ? accel.x.toFixed(3) : 0,
      y: accel.y ? accel.y.toFixed(3) : 0,
      z: accel.z ? accel.z.toFixed(3) : 0
    },
    bar: {
      pressure: bar.pressure ? parseInt(bar.pressure) : 0
    },
    bps: {
      presence: bps.present
    },
    gyro: {
      x: gyro.x ? gyro.x.toFixed(3) : 0,
      y: gyro.y ? gyro.y.toFixed(3) : 0,
      z: gyro.z ? gyro.z.toFixed(3) : 0,
    },
    hrm: {
      heartRate: hrm.heartRate ? hrm.heartRate : 0
    },
    orientation: orientation.quaternion ? orientation.quaternion.map(n => n.toFixed(3)) : null,
  }

  accelData.text = JSON.stringify(data.accel);
  barData.text = JSON.stringify(data.bar);
  bpsData.text = JSON.stringify(data.bps);
  gyroData.text = JSON.stringify(data.gyro);
  hrmData.text = JSON.stringify(data.hrm);
  orientationData.text = JSON.stringify(data.orientation);
  
  sendDataToCompanion(data);
}

refreshData();
setInterval(refreshData, 500);
