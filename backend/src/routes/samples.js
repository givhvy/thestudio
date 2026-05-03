import { Router } from 'express';
import multer from 'multer';
import path from 'path';
import fs from 'fs';
import { db } from '../db.js';

const UPLOAD_DIR = path.resolve('uploads');
if (!fs.existsSync(UPLOAD_DIR)) fs.mkdirSync(UPLOAD_DIR, { recursive: true });

const upload = multer({ dest: UPLOAD_DIR });
const r = Router();

r.get('/', async (req, res) => {
  const samples = await db.sample.findMany({ orderBy: { createdAt: 'desc' } });
  res.json(samples);
});

r.post('/upload', upload.single('audio'), async (req, res) => {
  const file = req.file;
  if (!file) return res.status(400).json({ error: 'No file' });
  const name = req.body.name || file.originalname;
  const finalPath = path.join(UPLOAD_DIR, name);
  fs.renameSync(file.path, finalPath);
  const sample = await db.sample.create({
    data: { name, filePath: finalPath, size: file.size, folderId: req.body.folderId || null },
  });
  res.status(201).json(sample);
});

r.delete('/:id', async (req, res) => {
  await db.sample.delete({ where: { id: req.params.id } });
  res.json({ ok: true });
});

export default r;
