const fs = require('fs');
const path = require('path');

const valid = value => typeof value === 'string' && /^[a-z0-9_-]{1,48}$/.test(value);
const validVersion = value => typeof value === 'string' && /^[0-9]+\.[0-9]+\.[0-9]+(?:[-+][a-z0-9.-]+)?$/.test(value);

module.exports = (req, res) => {
  if (req.method === 'OPTIONS') return res.status(204).end();
  if (req.method !== 'GET') return res.status(405).json({ error: 'Method not allowed' });
  let id = String(req.query.pkg || '').toLowerCase();
  if (id.endsWith('.kea')) id = id.slice(0, -4);
  if (!valid(id)) return res.status(400).json({ error: 'Invalid package id' });
  try {
    const index = JSON.parse(fs.readFileSync(path.join(process.cwd(), 'index-v2.json'), 'utf8'));
    const candidates = index.packages.filter(item => item.id === id);
    const version = req.query.version || (candidates.length ? candidates[candidates.length - 1].version : '');
    if (!validVersion(version)) return res.status(404).json({ error: 'Package not found' });
    const metadata = candidates.find(item => item.version === version);
    if (!metadata) return res.status(404).json({ error: 'Package not found' });
    const filePath = path.join(process.cwd(), 'packages', id, version, `${id}.kea`);
    const root = path.join(process.cwd(), 'packages') + path.sep;
    if (!filePath.startsWith(root) || !fs.existsSync(filePath)) return res.status(404).json({ error: 'Package not found' });
    const stat = fs.statSync(filePath);
    if (stat.size !== metadata.size) return res.status(500).json({ error: 'Repository package size mismatch' });
    res.setHeader('Content-Type', 'application/octet-stream');
    res.setHeader('Content-Disposition', `attachment; filename="${id}-${version}.kea"`);
    res.setHeader('Content-Length', stat.size);
    res.setHeader('ETag', `"sha256-${metadata.sha256}"`);
    res.setHeader('Cache-Control', 'public, max-age=31536000, immutable');
    return fs.createReadStream(filePath).pipe(res);
  } catch (error) {
    return res.status(500).json({ error: 'Repository unavailable' });
  }
};
