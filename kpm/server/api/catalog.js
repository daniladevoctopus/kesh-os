const fs = require('fs');
const path = require('path');

module.exports = (req, res) => {
  if (req.method === 'OPTIONS') return res.status(204).end();
  if (req.method !== 'GET') return res.status(405).json({ error: 'Method not allowed' });
  try {
    const index = JSON.parse(fs.readFileSync(path.join(process.cwd(), 'index-v2.json'), 'utf8'));
    res.setHeader('Content-Type', 'application/json; charset=utf-8');
    res.setHeader('Cache-Control', 'public, max-age=60, stale-while-revalidate=300');
    return res.status(200).json(index);
  } catch (error) {
    return res.status(500).json({ error: 'Repository index unavailable' });
  }
};
