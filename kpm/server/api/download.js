const fs = require('fs');
const path = require('path');

module.exports = (req, res) => {
  const pkg = req.query.pkg || path.basename(req.url.split('?')[0]);
  const clientIp = req.headers['x-forwarded-for'] || req.socket.remoteAddress || 'Unknown IP';
  const userAgent = req.headers['user-agent'] || 'Unknown Client';

  console.log(`[KPM-LIVE] >>> INCOMING DOWNLOAD: "${pkg}" from IP [${clientIp}]`);
  console.log(`[KPM-LIVE] Client User-Agent: ${userAgent}`);

  if (!pkg) {
    console.error(`[KPM-LIVE] [X] 400 Bad Request: Missing package parameter.`);
    return res.status(400).json({ error: "Missing package name" });
  }

  // Look for package in local packages directory
  const filePath = path.join(process.cwd(), 'packages', pkg);

  if (!fs.existsSync(filePath)) {
    console.error(`[KPM-LIVE] [X] 404 Not Found: Package "${pkg}" does not exist in registry.`);
    return res.status(404).json({ error: `Package ${pkg} not found` });
  }

  try {
    const fileData = fs.readFileSync(filePath);
    console.log(`[KPM-LIVE] [√] 200 OK: Successfully streaming "${pkg}" (${fileData.length} bytes) to [${clientIp}]`);

    res.setHeader('Content-Type', 'application/octet-stream');
    res.setHeader('Content-Disposition', `attachment; filename="${pkg}"`);
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET,OPTIONS');
    res.setHeader('Cache-Control', 'no-cache, no-store, must-revalidate');

    return res.status(200).send(fileData);
  } catch (err) {
    console.error(`[KPM-LIVE] [X] 500 Server Error: ${err.message}`);
    return res.status(500).json({ error: "Internal Server Error" });
  }
};
