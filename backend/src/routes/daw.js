import { Router } from 'express';

const r = Router();

// SSE clients waiting for live commands
const clients = new Set();

// Latest state pushed from the frontend
let latestState = null;

// POST /api/daw/command  — send a live command to the open DAW UI
r.post('/command', (req, res) => {
  const cmd = req.body;
  if (!cmd || !cmd.type) return res.status(400).json({ error: 'Missing command type' });
  const payload = `data: ${JSON.stringify(cmd)}\n\n`;
  for (const client of clients) {
    try { client.write(payload); } catch (_) { clients.delete(client); }
  }
  res.json({ ok: true, delivered: clients.size });
});

// POST /api/daw/state  — frontend pushes its state here
r.post('/state', (req, res) => {
  latestState = req.body;
  res.json({ ok: true });
});

// GET /api/daw/state  — MCP queries the latest state
r.get('/state', (req, res) => {
  if (!latestState) return res.status(503).json({ error: 'DAW not connected' });
  res.json(latestState);
});

// GET /api/daw/events  — SSE stream for the frontend
r.get('/events', (req, res) => {
  res.setHeader('Content-Type', 'text/event-stream');
  res.setHeader('Cache-Control', 'no-cache');
  res.setHeader('Connection', 'keep-alive');
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.flushHeaders();

  res.write('data: {"type":"connected"}\n\n');
  clients.add(res);

  req.on('close', () => {
    clients.delete(res);
  });
});

export default r;
