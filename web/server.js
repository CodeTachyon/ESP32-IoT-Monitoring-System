const cors = require('cors');
const express = require('express');
const bodyParser = require('body-parser');
const webpush = require('web-push');
require('dotenv').config();

const app = express();

// ======== MIDDLEWARE ========
app.use(cors({
  origin: 'https://sensor-bb765.web.app',  // Replace with your frontend domain
  methods: ['GET', 'POST'],
  credentials: true
}));
app.use(express.static('public'));
app.use(bodyParser.json());

// ======== DATA STORAGE ========
const deviceData = {};           // { deviceId: { temp, hum, gas_1, ... } }
const subscriptions = {};        // { deviceId: [subscription1, subscription2] }

// ======== VAPID KEYS ========
const vapidKeys = {
  publicKey: process.env.VAPID_PUBLIC_KEY || 'BEJ8pCj0Ed391KdFxZaPmU5JuDSzMg-lEsvB437xhEVf7OEfO1JWNH75m4myWQ6g699ioLUlgeHkelZN8CB1XdE',
  privateKey: process.env.VAPID_PRIVATE_KEY || 'wFfBFb5BgdYO0l9hYymqbcwTQyGhjUgPtsX675YfHuk'
};

webpush.setVapidDetails(
  'mailto:example@sppu.edu',
  vapidKeys.publicKey,
  vapidKeys.privateKey
);

// ======== ROUTES ========

// --- Get public VAPID key
app.get('/vapidPublicKey', (req, res) => {
  res.json({ publicKey: vapidKeys.publicKey });
});

// --- Store push subscription (per device)
app.post('/subscribe', (req, res) => {
  const { deviceId, subscription } = req.body;

  if (!deviceId || !subscription) {
    return res.status(400).json({ error: 'Missing deviceId or subscription' });
  }

  if (!subscriptions[deviceId]) {
    subscriptions[deviceId] = [];
  }

  const alreadySubscribed = subscriptions[deviceId].some(
    (sub) => sub.endpoint === subscription.endpoint
  );

  if (!alreadySubscribed) {
    subscriptions[deviceId].push(subscription);
    console.log(`✅ New subscriber for ${deviceId}`);
  } else {
    console.log(`ℹ️ Duplicate subscription ignored for ${deviceId}`);
  }

  res.status(201).json({ message: 'Subscribed' });
});

// --- Trigger push notification (to specific deviceId)
app.post('/notify', (req, res) => {
  const { title, body, deviceId } = req.body;

  if (!deviceId || !subscriptions[deviceId] || subscriptions[deviceId].length === 0) {
    console.log(`⚠️ No subscribers for deviceId: ${deviceId}`);
    return res.status(200).send('No subscribers.');
  }

  subscriptions[deviceId].forEach((sub, index) => {
    webpush.sendNotification(sub, JSON.stringify({ title, body }))
      .then(() => console.log(`🔔 Notification sent to ${deviceId} subscriber #${index + 1}`))
      .catch(err => {
        console.error('❌ Push error:', err);
        // Optional: Remove failed subscriptions
      });
  });

  res.sendStatus(200);
});

// --- ESP sends sensor data
app.post('/data', (req, res) => {
  const { deviceName ,deviceId, temp, hum, gas_1, gas_2, flame } = req.body;

  if (!deviceId) {
    return res.status(400).json({ error: 'Missing deviceId' });
  }

  deviceData[deviceId] = {deviceName, temp, hum, gas_1, gas_2, flame };
  console.log(`📥 Data received from ${deviceId}:`, deviceData[deviceId]);

  res.sendStatus(200);
});

// --- Frontend fetches sensor data
app.get('/data', (req, res) => {
  const deviceId = req.query.deviceId;

  if (!deviceId || !deviceData[deviceId]) {
    return res.status(404).json({
      temp: 0,
      hum: 0,
      gas_1: "No Data",
      gas_2: "No Data",
      flame: "No Data"
    });
  }

  res.json(deviceData[deviceId]);
});

// ======== SERVER LISTENING ========
const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`🚀 Server running on port ${PORT}`);
});
