#!/usr/bin/env node
/**
 * Stratum DAW — MCP Server
 *
 * Exposes the DAW's core actions as MCP tools so any AI assistant
 * (Claude Desktop, Cursor, Windsurf, etc.) can control it over stdio.
 *
 * Usage — add to your MCP config:
 *   {
 *     "mcpServers": {
 *       "stratum-daw": {
 *         "command": "node",
 *         "args": ["f:/PlaygroundTest/flstudioclonee/backend/src/mcp-server.js"]
 *       }
 *     }
 *   }
 *
 * The server talks to the Express REST API at http://localhost:3002.
 * Start the REST server first: `npm run dev` inside /backend.
 */

import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { z } from 'zod';

const API = process.env.STRATUM_API ?? 'http://localhost:3002/api';

async function api(method, path, body) {
  const res = await fetch(`${API}${path}`, {
    method,
    headers: { 'Content-Type': 'application/json' },
    body: body != null ? JSON.stringify(body) : undefined,
  });
  if (!res.ok) {
    const text = await res.text();
    throw new Error(`API ${method} ${path} → ${res.status}: ${text}`);
  }
  return res.json();
}

const server = new McpServer({
  name: 'stratum-daw',
  version: '1.0.0',
});

// ===== PROJECT TOOLS =====

server.tool(
  'list_projects',
  'List all projects saved in the DAW',
  {},
  async () => {
    const projects = await api('GET', '/projects');
    return {
      content: [{
        type: 'text',
        text: projects.map(p => `${p.id} | ${p.name} | BPM: ${p.bpm}`).join('\n') || 'No projects yet.',
      }],
    };
  }
);

server.tool(
  'create_project',
  'Create a new DAW project',
  { name: z.string().describe('Project name'), bpm: z.number().int().min(20).max(300).optional().describe('Tempo in BPM') },
  async ({ name, bpm = 120 }) => {
    const p = await api('POST', '/projects', { name, bpm });
    return { content: [{ type: 'text', text: `Created project "${p.name}" (id: ${p.id}, BPM: ${p.bpm})` }] };
  }
);

server.tool(
  'get_project',
  'Load a project by id and return its full data',
  { id: z.string().describe('Project id') },
  async ({ id }) => {
    const p = await api('GET', `/projects/${id}`);
    return { content: [{ type: 'text', text: JSON.stringify(p, null, 2) }] };
  }
);

server.tool(
  'set_project_bpm',
  'Change the BPM of a project',
  { id: z.string(), bpm: z.number().int().min(20).max(300) },
  async ({ id, bpm }) => {
    const p = await api('PUT', `/projects/${id}`, { bpm });
    return { content: [{ type: 'text', text: `BPM set to ${p.bpm} for "${p.name}"` }] };
  }
);

server.tool(
  'save_project',
  'Save the full project state (patterns, channels, playlist) to the database',
  {
    id: z.string().describe('Project id'),
    name: z.string().optional(),
    bpm: z.number().int().optional(),
    patterns: z.array(z.object({
      name: z.string(),
      channels: z.array(z.object({
        name: z.string(),
        type: z.string(),
        steps: z.array(z.number()),
        vol: z.number().optional(),
        pan: z.number().optional(),
        mute: z.boolean().optional(),
      })).optional(),
    })).describe('All patterns with their channels and steps'),
    playlistBlocks: z.array(z.array(z.object({
      pattern: z.number(),
      start: z.number(),
      length: z.number(),
      track: z.number().optional(),
    }))).optional(),
  },
  async (args) => {
    await api('POST', `/projects/${args.id}/save`, args);
    return { content: [{ type: 'text', text: 'Project saved.' }] };
  }
);

// ===== BEAT / PATTERN TOOLS =====

server.tool(
  'create_pattern',
  'Add a new pattern to a project',
  { projectId: z.string(), name: z.string(), index: z.number().int().optional() },
  async ({ projectId, name, index = 0 }) => {
    const p = await api('POST', '/patterns', { projectId, name, index });
    return { content: [{ type: 'text', text: `Created pattern "${p.name}" (id: ${p.id})` }] };
  }
);

server.tool(
  'set_step',
  'Toggle a single step on/off in a channel (updates the pattern data)',
  {
    projectId: z.string(),
    patternName: z.string().describe('Pattern name'),
    channelName: z.string().describe('Channel name e.g. "Kick"'),
    step: z.number().int().min(0).max(63).describe('0-indexed step number'),
    on: z.boolean(),
  },
  async ({ projectId, patternName, channelName, step, on }) => {
    const p = await api('GET', `/projects/${projectId}`);
    const pat = p.patterns.find(x => x.name.toLowerCase() === patternName.toLowerCase());
    if (!pat) throw new Error(`Pattern "${patternName}" not found`);
    const ch = pat.channels.find(x => x.name.toLowerCase() === channelName.toLowerCase());
    if (!ch) throw new Error(`Channel "${channelName}" not found in "${patternName}"`);
    const steps = JSON.parse(ch.steps || '[]');
    while (steps.length <= step) steps.push(0);
    steps[step] = on ? 1 : 0;
    ch.steps = steps;
    // rebuild patterns array for save
    const patterns = p.patterns.map(pt => ({
      name: pt.name,
      channels: pt.channels.map(c => ({
        ...c,
        steps: c.id === ch.id ? steps : JSON.parse(c.steps || '[]'),
      })),
    }));
    await api('POST', `/projects/${projectId}/save`, { name: p.name, bpm: p.bpm, patterns });
    return { content: [{ type: 'text', text: `Step ${step} in ${channelName} set to ${on ? 'ON' : 'OFF'}` }] };
  }
);

server.tool(
  'make_beat',
  'Generate a complete beat pattern with kick/snare/hihat/clap steps and save it',
  {
    projectId: z.string(),
    style: z.enum(['trap', 'hiphop', 'house', 'drill', 'boom_bap']).describe('Beat style preset'),
    patternName: z.string().optional().default('Generated Beat'),
  },
  async ({ projectId, style, patternName }) => {
    const PRESETS = {
      trap: {
        Kick:  [1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0],
        Snare: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0],
        Hihat: [1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1],
        Clap:  [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0],
      },
      hiphop: {
        Kick:  [1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0],
        Snare: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0],
        Hihat: [1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0],
        Clap:  [0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0],
      },
      house: {
        Kick:  [1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0],
        Snare: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0],
        Hihat: [0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0],
        Clap:  [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0],
      },
      drill: {
        Kick:  [1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,0],
        Snare: [0,0,0,0,1,0,0,1,0,0,0,0,1,0,1,0],
        Hihat: [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
        Clap:  [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0],
      },
      boom_bap: {
        Kick:  [1,0,0,1,0,0,1,0,1,0,0,0,0,1,0,0],
        Snare: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0],
        Hihat: [1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0],
        Clap:  [0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0],
      },
    };
    const preset = PRESETS[style];
    const p = await api('GET', `/projects/${projectId}`);
    const channels = Object.entries(preset).map(([name, steps], i) => ({
      name, type: name.toLowerCase(), steps, vol: 80, pan: 0, mute: false, solo: false,
    }));
    const patterns = [
      ...p.patterns,
      { name: patternName, channels },
    ];
    await api('POST', `/projects/${projectId}/save`, { name: p.name, bpm: p.bpm, patterns });
    return {
      content: [{
        type: 'text',
        text: `Created ${style} beat pattern "${patternName}" with ${Object.keys(preset).join(', ')} channels.`,
      }],
    };
  }
);

// ===== SAMPLE TOOLS =====

server.tool(
  'list_samples',
  'List all samples in the library',
  {},
  async () => {
    const samples = await api('GET', '/samples');
    return {
      content: [{
        type: 'text',
        text: samples.map(s => `${s.id} | ${s.name} | ${s.filePath}`).join('\n') || 'No samples.',
      }],
    };
  }
);

// ===== PLUGIN TOOLS =====

server.tool(
  'list_plugins',
  'List all plugins scanned into the DAW',
  {},
  async () => {
    const plugins = await api('GET', '/plugins');
    return {
      content: [{
        type: 'text',
        text: plugins.map(p => `${p.id} | ${p.name} | ${p.format} | ${p.manufacturer || 'unknown'}`).join('\n') || 'No plugins scanned.',
      }],
    };
  }
);

// ===== WORKSPACE TOOLS =====

server.tool(
  'list_workspaces',
  'List all workspace folders',
  {},
  async () => {
    const ws = await api('GET', '/workspaces');
    return {
      content: [{
        type: 'text',
        text: ws.map(w => `${w.id} | ${w.name} | ${w.diskPath || 'no path'} (${w.projects?.length || 0} projects)`).join('\n') || 'No workspaces.',
      }],
    };
  }
);

server.tool(
  'create_workspace',
  'Create a new workspace folder',
  { name: z.string(), diskPath: z.string().optional() },
  async ({ name, diskPath }) => {
    const w = await api('POST', '/workspaces', { name, diskPath });
    return { content: [{ type: 'text', text: `Created workspace "${w.name}" (id: ${w.id})` }] };
  }
);

// ===== START =====
const transport = new StdioServerTransport();
await server.connect(transport);
