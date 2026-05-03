/**
 * Stratum DAW — VST Host bridge (Electron main process)
 *
 * Spawns vst-host.exe as a child process and exposes a
 * JSON-RPC client to the rest of the Electron main process.
 *
 * Usage:
 *   const vst = require('./vstBridge');
 *   await vst.connect();
 *   const plugins = await vst.call('scanPlugins', { path: 'C:/VST3' });
 *   const { slotId } = await vst.call('loadPlugin', { fileOrIdentifier: 'Serum.vst3' });
 *   await vst.call('noteOn',  { slotId, channel: 1, note: 60, velocity: 100 });
 *   await vst.call('noteOff', { slotId, channel: 1, note: 60 });
 */

const { spawn } = require('child_process');
const net = require('net');
const path = require('path');

const VST_HOST_EXE = path.join(__dirname, '../vst-host/build/Release/StratumVSTHost.exe');
const VST_PORT = parseInt(process.env.STRATUM_VST_PORT ?? '9001', 10);

let hostProcess = null;
let rpcSocket   = null;
let connected   = false;
let pendingCalls = new Map();   // id → { resolve, reject }
let nextId = 1;
let lineBuffer = '';

function startHostProcess() {
  if (hostProcess) return;
  hostProcess = spawn(VST_HOST_EXE, [], {
    env: { ...process.env, STRATUM_VST_PORT: String(VST_PORT) },
    stdio: 'pipe',
  });
  hostProcess.stdout.on('data', d => process.stdout.write('[vst-host] ' + d));
  hostProcess.stderr.on('data', d => process.stderr.write('[vst-host] ' + d));
  hostProcess.on('exit', (code) => {
    console.log('[vstBridge] host exited with code', code);
    hostProcess = null;
    rpcSocket   = null;
    connected   = false;
  });
}

function waitForHost(retries = 20, delayMs = 300) {
  return new Promise((resolve, reject) => {
    let tries = 0;
    const attempt = () => {
      const sock = net.connect(VST_PORT, '127.0.0.1', () => {
        sock.destroy();
        resolve();
      });
      sock.on('error', () => {
        if (++tries >= retries) return reject(new Error('VST host did not start in time'));
        setTimeout(attempt, delayMs);
      });
    };
    attempt();
  });
}

function openRpcSocket() {
  return new Promise((resolve, reject) => {
    rpcSocket = net.connect(VST_PORT, '127.0.0.1');
    rpcSocket.setEncoding('utf8');
    rpcSocket.on('connect', () => { connected = true; resolve(); });
    rpcSocket.on('error', reject);
    rpcSocket.on('data', (data) => {
      lineBuffer += data;
      let nl;
      while ((nl = lineBuffer.indexOf('\n')) >= 0) {
        const line = lineBuffer.slice(0, nl).trim();
        lineBuffer  = lineBuffer.slice(nl + 1);
        if (!line) continue;
        try {
          const msg = JSON.parse(line);
          if (msg.event) {
            // broadcast to renderer via IPC event (handled in main.js)
            module.exports.onEvent?.(msg.event, msg.data);
          } else if (msg.id != null) {
            const pending = pendingCalls.get(msg.id);
            if (pending) {
              pendingCalls.delete(msg.id);
              if (msg.error) pending.reject(new Error(msg.error));
              else pending.resolve(msg.result);
            }
          }
        } catch {}
      }
    });
    rpcSocket.on('close', () => { connected = false; });
  });
}

async function connect() {
  if (connected) return;
  startHostProcess();
  await waitForHost();
  await openRpcSocket();
  console.log('[vstBridge] connected to VST host on port', VST_PORT);
}

function call(method, params = {}) {
  if (!connected) return Promise.reject(new Error('VST host not connected'));
  return new Promise((resolve, reject) => {
    const id = nextId++;
    pendingCalls.set(id, { resolve, reject });
    const msg = JSON.stringify({ id, method, params }) + '\n';
    rpcSocket.write(msg);
    setTimeout(() => {
      if (pendingCalls.has(id)) {
        pendingCalls.delete(id);
        reject(new Error(`Timeout waiting for response to "${method}"`));
      }
    }, 10000);
  });
}

function stop() {
  if (hostProcess) hostProcess.kill();
}

module.exports = { connect, call, stop, onEvent: null };
