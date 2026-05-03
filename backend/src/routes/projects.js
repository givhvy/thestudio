import { Router } from 'express';
import { db } from '../db.js';

const r = Router();

r.get('/', async (req, res) => {
  const projects = await db.project.findMany({
    orderBy: { openedAt: 'desc' },
    include: { workspace: true },
  });
  res.json(projects);
});

r.get('/:id', async (req, res) => {
  const p = await db.project.findUnique({
    where: { id: req.params.id },
    include: {
      patterns: { include: { channels: true } },
      playlist: true,
    },
  });
  if (!p) return res.status(404).json({ error: 'Not found' });
  res.json(p);
});

r.post('/', async (req, res) => {
  const { name = 'Untitled', bpm = 120, workspaceId } = req.body;
  const p = await db.project.create({ data: { name, bpm, workspaceId } });
  res.status(201).json(p);
});

r.put('/:id', async (req, res) => {
  const { name, bpm, timeSignature } = req.body;
  const p = await db.project.update({
    where: { id: req.params.id },
    data: { name, bpm, timeSignature, openedAt: new Date() },
  });
  res.json(p);
});

// Full project save — upserts patterns + channels + playlist in one call
r.post('/:id/save', async (req, res) => {
  const { name, bpm, patterns, playlistBlocks } = req.body;
  const projectId = req.params.id;
  await db.$transaction(async (tx) => {
    await tx.project.update({
      where: { id: projectId },
      data: { name, bpm, openedAt: new Date() },
    });
    // delete old patterns and rebuild
    await tx.pattern.deleteMany({ where: { projectId } });
    for (const [idx, pat] of patterns.entries()) {
      const created = await tx.pattern.create({
        data: { name: pat.name, index: idx, projectId },
      });
      for (const [ci, ch] of (pat.channels || []).entries()) {
        await tx.channel.create({
          data: {
            patternId: created.id,
            name: ch.name || `Ch ${ci + 1}`,
            color: ch.color || '#f97316',
            vol: ch.vol ?? 80,
            pan: ch.pan ?? 0,
            mute: !!ch.mute,
            solo: !!ch.solo,
            steps: JSON.stringify(ch.steps || []),
            notes: JSON.stringify(ch.notes || []),
            index: ci,
          },
        });
      }
    }
    // replace playlist blocks
    await tx.playlistBlock.deleteMany({ where: { projectId } });
    for (const blk of (playlistBlocks || [])) {
      for (const b of blk) {
        await tx.playlistBlock.create({
          data: {
            projectId,
            trackIndex: b.track ?? 0,
            startBeat: b.start ?? 0,
            lengthBeats: b.length ?? 4,
            patternRef: b.pattern ?? 0,
          },
        });
      }
    }
  });
  res.json({ ok: true });
});

r.delete('/:id', async (req, res) => {
  await db.project.delete({ where: { id: req.params.id } });
  res.json({ ok: true });
});

export default r;
