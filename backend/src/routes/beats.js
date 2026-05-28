import { Router } from 'express';
import multer from 'multer';
import path from 'path';
import fs from 'fs';

const BEATS_DIR = path.resolve('uploads/beats');
if (!fs.existsSync(BEATS_DIR)) fs.mkdirSync(BEATS_DIR, { recursive: true });

const storage = multer.diskStorage({
  destination: (req, _file, cb) => {
    const safeName = (req.body.name || 'Untitled').replace(/[^\w\- ]+/g, '-').trim() || 'Untitled';
    const dir = path.join(BEATS_DIR, safeName);
    fs.mkdirSync(dir, { recursive: true });
    cb(null, dir);
  },
  filename: (req, file, cb) => {
    const safeName = (req.body.name || 'Untitled').replace(/[^\w\- ]+/g, '-').trim() || 'Untitled';
    cb(null, file.fieldname === 'audio' ? `${safeName}.wav` : 'project.stratum');
  },
});

const upload = multer({ storage });
const r = Router();

r.post('/upload', upload.fields([
  { name: 'audio', maxCount: 1 },
  { name: 'project', maxCount: 1 },
]), (req, res) => {
  const name = (req.body.name || 'Untitled').trim();
  const bpm = req.body.bpm ? Number(req.body.bpm) : null;
  const files = req.files || {};

  res.status(201).json({
    ok: true,
    name,
    bpm,
    audio: files.audio?.[0]?.path || null,
    project: files.project?.[0]?.path || null,
    uploadedAt: new Date().toISOString(),
  });
});

export default r;
