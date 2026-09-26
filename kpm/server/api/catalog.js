const fs = require('fs');
const path = require('path');

module.exports = (req, res) => {
  const clientIp = req.headers['x-forwarded-for'] || req.socket.remoteAddress || 'Unknown IP';
  const userAgent = req.headers['user-agent'] || 'Unknown Client';

  console.log(`[KPM-LIVE] >>> INCOMING CATALOG: packages.json requested from IP [${clientIp}]`);
  console.log(`[KPM-LIVE] Client User-Agent: ${userAgent}`);

  const filePath = path.join(process.cwd(), 'packages.json');
  if (fs.existsSync(filePath)) {
    try {
      const data = JSON.parse(fs.readFileSync(filePath, 'utf-8'));
      const count = data.packages ? data.packages.length : 0;
      console.log(`[KPM-LIVE] [√] 200 OK: Sent catalog with ${count} available packages.`);

      res.setHeader('Content-Type', 'application/json');
      res.setHeader('Access-Control-Allow-Origin', '*');
      res.setHeader('Access-Control-Allow-Methods', 'GET,OPTIONS');
      return res.status(200).json(data);
    } catch (e) {
      console.error(`[KPM-LIVE] [X] JSON Parse Error: ${e.message}`);
    }
  }

  console.error(`[KPM-LIVE] [X] packages.json not found.`);
  return res.status(404).json({ error: "Catalog not found" });
};
