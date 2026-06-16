const express = require('express');
const multer = require('multer');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 8080;
const HOST = process.env.HOST || '192.168.1.238'; // ip lan của máy: ipconfig để check
const uploadDir = path.join(__dirname, 'uploads');

if (!fs.existsSync(uploadDir)) {
  fs.mkdirSync(uploadDir, { recursive: true });
}

const storage = multer.diskStorage({
  destination: (_req, _file, cb) => cb(null, uploadDir),
  filename: (_req, _file, cb) => cb(null, 'firmware.bin'),
});

const upload = multer({ storage });

app.use('/files', express.static(uploadDir));

app.post('/api/ota/upload', upload.single('firmware'), (req, res) => {
  if (!req.file) {
    return res.status(400).json({ error: 'Thieu file firmware' });
  }

  // Tinh MD5 de dashboard gui kem trong payload MQTT: {"url":"...","md5":"..."}
  const fileBuffer = fs.readFileSync(req.file.path);
  const md5 = crypto.createHash('md5').update(fileBuffer).digest('hex');
  const url = `http://${HOST}:${PORT}/files/firmware.bin`;

  return res.json({
    message: 'Upload firmware thanh cong',
    url,
    md5,
  });
});

app.listen(PORT, () => {
  console.log(`OTA mock server running at http://${HOST}:${PORT}`);
  console.log('POST /api/ota/upload (form-data key: firmware)');
  console.log(`Firmware URL: http://${HOST}:${PORT}/files/firmware.bin`);
});
