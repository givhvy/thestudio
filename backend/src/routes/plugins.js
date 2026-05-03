import { Router } from 'express';
import { db } from '../db.js';

const r = Router();

r.get('/', async (_, res) => {
  res.json(await db.plugin.findMany({ orderBy: { name: 'asc' } }));
});

r.post('/scan', async (req, res) => {
  // Receives an array of {name,format,filePath,manufacturer,version,isInstrument}
  // from Electron's native scanner and upserts them.
  const { plugins = [] } = req.body;
  const results = [];
  for (const p of plugins) {
    const upserted = await db.plugin.upsert({
      where: { id: p.id || '' },
      update: { name: p.name, format: p.format, filePath: p.filePath, manufacturer: p.manufacturer, version: p.version, isInstrument: p.isInstrument ?? true, scannedAt: new Date() },
      create: { name: p.name, format: p.format || 'vst3', filePath: p.filePath, manufacturer: p.manufacturer, version: p.version, isInstrument: p.isInstrument ?? true },
    });
    results.push(upserted);
  }
  res.json(results);
});

export default r;
