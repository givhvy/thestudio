import express from 'express';
import cors from 'cors';
import path from 'path';
import { createServer } from 'http';
import { db } from './db.js';
import projectsRouter from './routes/projects.js';
import patternsRouter from './routes/patterns.js';
import samplesRouter from './routes/samples.js';
import pluginsRouter from './routes/plugins.js';
import workspacesRouter from './routes/workspaces.js';

const app = express();
const PORT = process.env.PORT || 3002;

app.use(cors({ origin: '*' }));
app.use(express.json({ limit: '50mb' }));

// REST routes
app.use('/api/projects',   projectsRouter);
app.use('/api/patterns',   patternsRouter);
app.use('/api/samples',    samplesRouter);
app.use('/api/plugins',    pluginsRouter);
app.use('/api/workspaces', workspacesRouter);

// Health check
app.get('/api/health', (_, res) => res.json({ ok: true, ts: Date.now() }));

const server = createServer(app);
server.listen(PORT, () => {
  console.log(`[stratum-backend] listening on http://localhost:${PORT}`);
});

process.on('SIGINT', async () => { await db.$disconnect(); process.exit(0); });
process.on('SIGTERM', async () => { await db.$disconnect(); process.exit(0); });
