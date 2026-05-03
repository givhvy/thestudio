// ============================================================
// Software Instrument Plugin System
// Built-in synths using Web Audio API — works offline, no CDN,
// no WASM compile step, no external SDK required.
// Each "plugin" is a JS synth engine backed by the Web Audio API.
// ============================================================

export const WAM_REGISTRY = [
  { id: 'poly-saw',   name: 'PolySaw (Analog Lead)',   isInstrument: true  },
  { id: 'fm-synth',   name: 'FM Synth (DX-style)',     isInstrument: true  },
  { id: 'sub-bass',   name: 'Sub Bass',                isInstrument: true  },
  { id: 'pad-synth',  name: 'Lush Pad',                isInstrument: true  },
  { id: 'pluck',      name: 'Pluck / Karplus-Strong',  isInstrument: true  },
  { id: 'sw-delay',   name: 'Stereo Delay (FX)',       isInstrument: false },
  { id: 'sw-reverb',  name: 'Hall Reverb (FX)',        isInstrument: false },
];

// ---------------------------------------------------------------------------
// Built-in synth engines — pure Web Audio, no external dependencies.
// Each factory(audioContext, destNode) returns { noteOn, noteOff, destroy }
// ---------------------------------------------------------------------------

const SYNTH_FACTORIES = {
  'poly-saw': (ctx, dest) => {
    const voices = new Map();
    const gain = ctx.createGain(); gain.gain.value = 0.3; gain.connect(dest);
    const filter = ctx.createBiquadFilter();
    filter.type = 'lowpass'; filter.frequency.value = 2000; filter.Q.value = 4;
    filter.connect(gain);
    return {
      noteOn(note, vel) {
        const f = 440 * Math.pow(2, (note - 69) / 12);
        const v = vel / 127;
        const env = ctx.createGain(); env.gain.setValueAtTime(0, ctx.currentTime);
        env.gain.linearRampToValueAtTime(v, ctx.currentTime + 0.01);
        env.connect(filter);
        const oscs = [ctx.createOscillator(), ctx.createOscillator()];
        oscs[0].type = 'sawtooth'; oscs[0].frequency.value = f;
        oscs[1].type = 'sawtooth'; oscs[1].frequency.value = f * 1.005;
        oscs.forEach(o => { o.connect(env); o.start(); });
        voices.set(note, { oscs, env });
      },
      noteOff(note) {
        const v = voices.get(note); if (!v) return;
        v.env.gain.linearRampToValueAtTime(0, ctx.currentTime + 0.15);
        v.oscs.forEach(o => o.stop(ctx.currentTime + 0.15));
        voices.delete(note);
      },
      destroy() { gain.disconnect(); },
    };
  },

  'fm-synth': (ctx, dest) => {
    const voices = new Map();
    const masterGain = ctx.createGain(); masterGain.gain.value = 0.35; masterGain.connect(dest);
    return {
      noteOn(note, vel) {
        const f = 440 * Math.pow(2, (note - 69) / 12);
        const carrier = ctx.createOscillator();
        const modulator = ctx.createOscillator();
        const modGain = ctx.createGain();
        const envGain = ctx.createGain();
        modulator.frequency.value = f * 2;
        modGain.gain.value = f * 3;
        modulator.connect(modGain); modGain.connect(carrier.frequency);
        carrier.frequency.value = f; carrier.type = 'sine';
        envGain.gain.setValueAtTime(0, ctx.currentTime);
        envGain.gain.linearRampToValueAtTime(vel / 127 * 0.5, ctx.currentTime + 0.005);
        envGain.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + 1.5);
        carrier.connect(envGain); envGain.connect(masterGain);
        carrier.start(); modulator.start();
        voices.set(note, { carrier, modulator, envGain });
      },
      noteOff(note) {
        const v = voices.get(note); if (!v) return;
        v.envGain.gain.cancelScheduledValues(ctx.currentTime);
        v.envGain.gain.linearRampToValueAtTime(0, ctx.currentTime + 0.1);
        v.carrier.stop(ctx.currentTime + 0.1);
        v.modulator.stop(ctx.currentTime + 0.1);
        voices.delete(note);
      },
      destroy() { masterGain.disconnect(); },
    };
  },

  'sub-bass': (ctx, dest) => {
    const voices = new Map();
    const masterGain = ctx.createGain(); masterGain.gain.value = 0.5; masterGain.connect(dest);
    return {
      noteOn(note, vel) {
        const f = 440 * Math.pow(2, (note - 69) / 12);
        const osc = ctx.createOscillator(); osc.type = 'sine'; osc.frequency.value = f;
        const env = ctx.createGain();
        env.gain.setValueAtTime(0, ctx.currentTime);
        env.gain.linearRampToValueAtTime(vel / 127, ctx.currentTime + 0.02);
        osc.connect(env); env.connect(masterGain); osc.start();
        voices.set(note, { osc, env });
      },
      noteOff(note) {
        const v = voices.get(note); if (!v) return;
        v.env.gain.linearRampToValueAtTime(0, ctx.currentTime + 0.2);
        v.osc.stop(ctx.currentTime + 0.2);
        voices.delete(note);
      },
      destroy() { masterGain.disconnect(); },
    };
  },

  'pad-synth': (ctx, dest) => {
    const voices = new Map();
    const masterGain = ctx.createGain(); masterGain.gain.value = 0.2;
    const reverb = ctx.createConvolver();
    // Simple impulse response for reverb
    const ir = ctx.createBuffer(2, ctx.sampleRate * 2, ctx.sampleRate);
    for (let ch = 0; ch < 2; ch++) {
      const d = ir.getChannelData(ch);
      for (let i = 0; i < d.length; i++) d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / d.length, 2);
    }
    reverb.buffer = ir;
    masterGain.connect(reverb); reverb.connect(dest); masterGain.connect(dest);
    return {
      noteOn(note, vel) {
        const f = 440 * Math.pow(2, (note - 69) / 12);
        const types = ['sawtooth', 'triangle', 'sine'];
        const env = ctx.createGain();
        env.gain.setValueAtTime(0, ctx.currentTime);
        env.gain.linearRampToValueAtTime(vel / 127 * 0.4, ctx.currentTime + 0.4);
        env.connect(masterGain);
        const oscs = types.map((t, i) => {
          const o = ctx.createOscillator(); o.type = t;
          o.frequency.value = f * [1, 1.003, 0.5][i];
          o.connect(env); o.start(); return o;
        });
        voices.set(note, { oscs, env });
      },
      noteOff(note) {
        const v = voices.get(note); if (!v) return;
        v.env.gain.linearRampToValueAtTime(0, ctx.currentTime + 0.6);
        v.oscs.forEach(o => o.stop(ctx.currentTime + 0.6));
        voices.delete(note);
      },
      destroy() { masterGain.disconnect(); reverb.disconnect(); },
    };
  },

  'pluck': (ctx, dest) => {
    const masterGain = ctx.createGain(); masterGain.gain.value = 0.4; masterGain.connect(dest);
    return {
      noteOn(note, vel) {
        // Karplus-Strong style via noise burst + filter
        const f = 440 * Math.pow(2, (note - 69) / 12);
        const bufSize = Math.round(ctx.sampleRate / f);
        const buf = ctx.createBuffer(1, bufSize, ctx.sampleRate);
        const d = buf.getChannelData(0);
        for (let i = 0; i < bufSize; i++) d[i] = Math.random() * 2 - 1;
        const src = ctx.createBufferSource(); src.buffer = buf; src.loop = true;
        const filt = ctx.createBiquadFilter(); filt.type = 'lowpass';
        filt.frequency.value = f * 3;
        const env = ctx.createGain();
        env.gain.setValueAtTime(vel / 127 * 0.5, ctx.currentTime);
        env.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + 1.2);
        src.connect(filt); filt.connect(env); env.connect(masterGain);
        src.start(); src.stop(ctx.currentTime + 1.2);
      },
      noteOff() {},
      destroy() { masterGain.disconnect(); },
    };
  },

  'sw-delay': (ctx, dest) => {
    const input = ctx.createGain();
    const delay = ctx.createDelay(1.0); delay.delayTime.value = 0.375;
    const feedback = ctx.createGain(); feedback.gain.value = 0.45;
    const wet = ctx.createGain(); wet.gain.value = 0.5;
    input.connect(dest); input.connect(delay);
    delay.connect(feedback); feedback.connect(delay);
    delay.connect(wet); wet.connect(dest);
    return {
      noteOn() {}, noteOff() {},
      destroy() { input.disconnect(); wet.disconnect(); },
      inputNode: input,
    };
  },

  'sw-reverb': (ctx, dest) => {
    const input = ctx.createGain();
    const conv = ctx.createConvolver();
    const ir = ctx.createBuffer(2, ctx.sampleRate * 3, ctx.sampleRate);
    for (let ch = 0; ch < 2; ch++) {
      const d = ir.getChannelData(ch);
      for (let i = 0; i < d.length; i++) d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / d.length, 1.5);
    }
    conv.buffer = ir;
    const wet = ctx.createGain(); wet.gain.value = 0.4;
    input.connect(dest); input.connect(conv); conv.connect(wet); wet.connect(dest);
    return {
      noteOn() {}, noteOff() {},
      destroy() { input.disconnect(); conv.disconnect(); wet.disconnect(); },
      inputNode: input,
    };
  },
};

// ---------------------------------------------------------------------------
// Public API — matches the shape that PluginBrowser.jsx + App.jsx expect
// ---------------------------------------------------------------------------

const wamInstances = new Map();

export function initWamHost() { return Promise.resolve(); }

/**
 * "Load" a built-in synth engine and wire it to destNode.
 */
export async function loadWam(audioContext, idOrUrl, destNode) {
  const entry = WAM_REGISTRY.find(r => r.id === idOrUrl);
  const id = entry?.id ?? idOrUrl;
  const name = entry?.name ?? idOrUrl;

  const factory = SYNTH_FACTORIES[id];
  if (!factory) throw new Error(`Unknown built-in plugin: ${id}`);

  const engine = factory(audioContext, destNode);
  const slotId = `wam_${Date.now()}_${Math.random().toString(36).slice(2)}`;
  wamInstances.set(slotId, { engine, name });

  return { slotId, name };
}

/** Trigger a note on a loaded synth */
export function wamNoteOn(slotId, note, velocity = 100) {
  wamInstances.get(slotId)?.engine?.noteOn(note, velocity);
}

/** Release a note */
export function wamNoteOff(slotId, note) {
  wamInstances.get(slotId)?.engine?.noteOff(note);
}

/** No-op stubs kept for API compatibility */
export async function wamSetParam() {}
export async function wamGetParams() { return []; }
export async function wamShowGui(slotId, container) {
  if (!container) return;
  const slot = wamInstances.get(slotId);
  container.innerHTML = `<div style="padding:12px;color:#a1a1aa;font-size:11px">
    <b style="color:#f97316">${slot?.name ?? slotId}</b><br/>
    Built-in software synth — no external GUI.<br/>
    Use the Channel Rack to sequence notes.
  </div>`;
}

/** Disconnect and destroy */
export function unloadWam(slotId) {
  const slot = wamInstances.get(slotId);
  if (!slot) return;
  try { slot.engine.destroy(); } catch {}
  wamInstances.delete(slotId);
}

export function getLoadedWams() {
  return Array.from(wamInstances.entries()).map(([id, s]) => ({ slotId: id, name: s.name }));
}
