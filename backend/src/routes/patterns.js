import { Router } from 'express';
import { db } from '../db.js';

const r = Router();

r.get('/project/:projectId', async (req, res) => {
  const patterns = await db.pattern.findMany({
    where: { projectId: req.params.projectId },
    orderBy: { index: 'asc' },
    include: { channels: { orderBy: { index: 'asc' } } },
  });
  res.json(patterns);
});

r.post('/', async (req, res) => {
  const { name, index, projectId } = req.body;
  const p = await db.pattern.create({ data: { name, index, projectId } });
  res.status(201).json(p);
});

r.delete('/:id', async (req, res) => {
  await db.pattern.delete({ where: { id: req.params.id } });
  res.json({ ok: true });
});

export default r;
