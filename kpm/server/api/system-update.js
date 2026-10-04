const fs = require('fs');
const path = require('path');

module.exports = (req, res) => {
  if (req.method === 'OPTIONS') return res.status(204).end();
  if (req.method !== 'GET') return res.status(405).json({ error: 'Method not allowed' });
  const filePath = path.join(process.cwd(), 'system', 'current.ksu');
  if (!fs.existsSync(filePath)) return res.status(404).json({ error: 'No system update published' });
  const stat = fs.statSync(filePath);
  res.setHeader('Content-Type', 'application/octet-stream');
  res.setHeader('Content-Length', stat.size);
  res.setHeader('Cache-Control', 'no-cache');
  return fs.createReadStream(filePath).pipe(res);
};
