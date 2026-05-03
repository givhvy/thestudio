import { Router } from 'express';
import { db } from '../db.js';

const r = Router();

r.get('/', async (_, res) => {
  res.json(await db.workspace.findMany({ where: { parentId: null }, include: { children: true, projects: true } }));
});

r.post('/', async (req, res) => {
  const { name, diskPath, parentId } = req.body;
  const w = await db.workspace.create({ data: { name, diskPath, parentId } });
  res.status(201).json(w);
});

r.delete('/:id', async (req, res) => {
  await db.workspace.delete({ where: { id: req.params.id } });
  res.json({ ok: true });
});

export default r;
