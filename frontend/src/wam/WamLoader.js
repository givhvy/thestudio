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
  wamInstances.set(slotId, {
    engine,
    name,
    params: {
      gain: 40,
      output: 210,
      mix: 0,
      sliders: [20, 60, 35, 10],
      power: true,
      bypass: false,
      engage: true,
      satUp: false,
    },
  });

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

export async function wamSetParam() {}
export async function wamGetParams() { return []; }

function escapeHtml(value) {
  return String(value)
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#039;');
}

function renderObsidianGui(slotId, slot) {
  const safeId = slotId.replace(/[^a-zA-Z0-9_-]/g, '_');
  const name = escapeHtml(slot?.name ?? slotId);
  const params = slot?.params ?? {};
  const sliders = params.sliders ?? [20, 60, 35, 10];
  const gain = params.gain ?? 40;
  const output = params.output ?? 210;
  const mix = params.mix ?? 0;
  const powerRight = params.power === false ? 'auto' : '4px';
  const powerLeft = params.power === false ? '4px' : 'auto';
  const powerBg = params.power === false ? 'radial-gradient(circle at 30% 30%, #444, #222)' : 'radial-gradient(circle at 30% 30%, #ff8a00, #c24100)';
  const bypassClass = params.bypass ? 'on red' : '';
  const engageClass = params.engage === false ? '' : 'on blue';
  const satTop = params.satUp ? '4px' : 'auto';
  const satBottom = params.satUp ? 'auto' : '4px';

  return `
    <style>
      #${safeId}.obsidian-plugin{font-family:Inter,system-ui,sans-serif;color:#d4d4d8;padding:12px;background:#050505;overflow:hidden}
      #${safeId} .jb{font-family:'JetBrains Mono','Consolas',monospace}
      #${safeId} .chassis{position:relative;border-radius:18px;background:linear-gradient(165deg,#2a2a2a 0%,#171717 42%,#0a0a0a 100%);box-shadow:rgba(255,255,255,.15) 0 2px 1px -1px inset,rgba(0,0,0,.8) 0 -3px 4px -1px inset,rgba(255,255,255,.05) 1px 0 2px -1px inset,rgba(0,0,0,.5) -1px 0 2px -1px inset,#000 0 24px 48px -18px,#000 0 0 0 1px;min-width:520px}
      #${safeId} .grain{position:absolute;inset:0;border-radius:18px;opacity:.14;mix-blend-mode:overlay;pointer-events:none;background-image:url('data:image/svg+xml,%3Csvg viewBox=%220 0 200 200%22 xmlns=%22http://www.w3.org/2000/svg%22%3E%3Cfilter id=%22n%22%3E%3CfeTurbulence type=%22fractalNoise%22 baseFrequency=%220.85%22 numOctaves=%223%22 stitchTiles=%22stitch%22/%3E%3C/filter%3E%3Crect width=%22100%25%22 height=%22100%25%22 filter=%22url(%23n)%22/%3E%3C/svg%3E')}
      #${safeId} .screw{position:absolute;width:14px;height:14px;border-radius:999px;display:flex;align-items:center;justify-content:center;background:linear-gradient(135deg,#444,#111);box-shadow:inset 0 1px 1px rgba(255,255,255,.3),0 1px 2px rgba(0,0,0,.8)}
      #${safeId} .screw:after{content:'';width:8px;height:8px;border-radius:999px;background:#171717;box-shadow:inset 0 1px 2px rgba(0,0,0,.8),0 1px 0 rgba(255,255,255,.2)}
      #${safeId} .top{position:relative;z-index:2;display:flex;align-items:center;justify-content:space-between;gap:18px;padding:18px 28px;border-bottom:1px solid rgba(23,23,23,.85);box-shadow:0 4px 6px -1px rgba(0,0,0,.3)}
      #${safeId} .eyebrow{display:block;font-size:9px;line-height:1;text-transform:uppercase;letter-spacing:.28em;color:#737373;margin-bottom:5px}
      #${safeId} .title{font-size:18px;line-height:1;text-transform:uppercase;letter-spacing:.32em;color:#d4d4d8;font-weight:700;text-shadow:0 1px 2px rgba(0,0,0,.8)}
      #${safeId} .model{font-size:10px;color:#525252;letter-spacing:.1em;white-space:nowrap}
      #${safeId} .vents{display:flex;gap:5px;opacity:.58}
      #${safeId} .vent{width:26px;height:6px;border-radius:999px;background:#000;box-shadow:inset 0 1px 2px #000,0 1px 0 rgba(255,255,255,.1)}
      #${safeId} .power-wrap{display:flex;align-items:center;gap:10px}
      #${safeId} .label{font-size:10px;text-transform:uppercase;letter-spacing:.18em;color:#737373;font-weight:700}
      #${safeId} .switch{position:relative;width:44px;height:22px;border-radius:999px;padding:4px;cursor:pointer;background:#0f0f0f;box-shadow:inset 0 2px 4px rgba(0,0,0,.8),0 1px 0 rgba(255,255,255,.1)}
      #${safeId} .thumb{position:absolute;top:4px;width:14px;height:14px;border-radius:999px;transition:all .22s;background:${powerBg};right:${powerRight};left:${powerLeft};box-shadow:0 1px 3px rgba(0,0,0,.5),inset 0 -1px 2px rgba(0,0,0,.4)}
      #${safeId} .main{position:relative;z-index:2;display:grid;grid-template-columns:150px 1fr 130px;gap:24px;padding:24px 30px}
      #${safeId} .section{display:flex;flex-direction:column;align-items:center;justify-content:space-between;gap:22px}
      #${safeId} .border-r{border-right:1px solid rgba(38,38,38,.75);padding-right:22px;box-shadow:1px 0 0 rgba(255,255,255,.03)}
      #${safeId} .knob-block{display:flex;flex-direction:column;align-items:center;gap:12px}
      #${safeId} .knob-ring{position:relative;border-radius:999px;display:flex;align-items:center;justify-content:center;background:#1a1a1a;box-shadow:inset 0 2px 4px rgba(0,0,0,.6),0 1px 0 rgba(255,255,255,.05)}
      #${safeId} .knob-ring:before{content:'';position:absolute;inset:8px;border-radius:999px;border:1px dashed rgba(63,63,70,.75)}
      #${safeId} .knob{position:relative;border-radius:999px;display:flex;align-items:center;justify-content:center;cursor:grab;background:linear-gradient(135deg,#444 0%,#222 50%,#111 100%);box-shadow:0 12px 15px -3px rgba(0,0,0,.9),inset 0 1px 1px rgba(255,255,255,.3),inset 0 -2px 2px rgba(0,0,0,.5)}
      #${safeId} .knob:active{cursor:grabbing}
      #${safeId} .knob:before{content:'';position:absolute;inset:2px;border-radius:999px;opacity:.3;background:repeating-conic-gradient(#000 0deg 2deg,transparent 2deg 4deg)}
      #${safeId} .knob-inner{position:relative;border-radius:999px;pointer-events:none;background:radial-gradient(circle at 50% 0%,#3a3a3a 0%,#1a1a1a 80%);box-shadow:inset 0 2px 2px rgba(255,255,255,.15),inset 0 -2px 4px rgba(0,0,0,.6)}
      #${safeId} .knob-dot{position:absolute;top:6px;left:50%;transform:translateX(-50%);width:6px;height:6px;border-radius:999px;background:#fff;box-shadow:0 0 5px rgba(255,255,255,.8)}
      #${safeId} .plate{font-size:10px;font-weight:700;letter-spacing:.16em;text-transform:uppercase;color:#a3a3a3;background:rgba(23,23,23,.58);padding:5px 10px;border-radius:6px;border:1px solid #262626;box-shadow:inset 0 1px 2px rgba(0,0,0,.8);white-space:nowrap}
      #${safeId} .toggle-box{width:44px;height:58px;border-radius:7px;background:#171717;display:flex;align-items:center;justify-content:center;padding:4px;box-shadow:inset 0 2px 5px #000,0 1px 0 rgba(255,255,255,.1)}
      #${safeId} .toggle-slot{position:relative;width:24px;height:46px;background:#090909;border-radius:4px;box-shadow:inset 0 2px 4px #000;cursor:pointer}
      #${safeId} .toggle-bat{position:absolute;left:50%;transform:translateX(-50%);width:16px;height:22px;border-radius:3px;background:linear-gradient(90deg,#555 0%,#aaa 50%,#333 100%);box-shadow:0 4px 6px rgba(0,0,0,.8),inset 0 1px 1px rgba(255,255,255,.5);border-bottom:2px solid #111;transition:all .18s;top:${satTop};bottom:${satBottom}}
      #${safeId} .eq-head{display:flex;align-items:center;justify-content:space-between;width:100%;margin-bottom:18px}
      #${safeId} .led-wrap{width:15px;height:15px;border-radius:999px;display:flex;align-items:center;justify-content:center;background:linear-gradient(180deg,#333,#111);box-shadow:inset 0 1px 1px rgba(255,255,255,.1),0 1px 2px rgba(0,0,0,.8)}
      #${safeId} .led{width:8px;height:8px;border-radius:999px;background:#27272a;box-shadow:inset 0 1px 3px rgba(0,0,0,.9)}
      #${safeId} .led.on{animation:${safeId}-pulse 2s ease-in-out infinite}
      #${safeId} .led.orange{background:#f97316;box-shadow:0 0 10px 2px rgba(249,115,22,.6),inset 0 1px 2px rgba(255,255,255,.8)}
      #${safeId} .led.red{background:#ef4444;box-shadow:0 0 12px 3px rgba(239,68,68,.75),inset 0 1px 2px rgba(255,255,255,.85)}
      #${safeId} .led.blue{background:#3b82f6;box-shadow:0 0 12px 3px rgba(59,130,246,.75),inset 0 1px 2px rgba(255,255,255,.85)}
      #${safeId} .slider-row{display:flex;justify-content:space-around;width:100%;gap:18px}
      #${safeId} .slider-col{display:flex;flex-direction:column;align-items:center;gap:12px}
      #${safeId} .slider-track{position:relative;width:30px;height:150px;display:flex;justify-content:center}
      #${safeId} .slider-track:before{content:'';position:absolute;inset:0 auto;width:8px;border-radius:999px;background:#080808;box-shadow:inset 0 2px 5px #000,0 1px 0 rgba(255,255,255,.05)}
      #${safeId} .slider-track:after{content:'';position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);width:22px;height:1px;background:#525252;opacity:.45}
      #${safeId} .slider-cap{position:absolute;left:50%;transform:translateX(-50%);width:38px;height:22px;border-radius:5px;display:flex;align-items:center;justify-content:center;cursor:ns-resize;user-select:none;touch-action:none;background:linear-gradient(180deg,#4a4a4a 0%,#2a2a2a 100%);box-shadow:0 8px 10px -2px rgba(0,0,0,.8),inset 0 1px 1px rgba(255,255,255,.2),inset 0 -1px 2px rgba(0,0,0,.8);border:1px solid #111;z-index:3}
      #${safeId} .slider-cap:hover{filter:brightness(1.25)}
      #${safeId} .slider-groove{width:2px;height:100%;background:#0a0a0a}
      #${safeId} .bottom{position:relative;z-index:2;display:flex;align-items:center;justify-content:space-between;padding:22px 46px;border-top:1px solid rgba(23,23,23,.95);background:rgba(23,23,23,.35);border-radius:0 0 18px 18px;box-shadow:inset 0 1px 0 rgba(255,255,255,.03)}
      #${safeId} .stomp{display:flex;flex-direction:column;align-items:center;gap:14px}
      #${safeId} .stomp-btn{width:62px;height:62px;border-radius:999px;display:flex;align-items:center;justify-content:center;cursor:pointer;background:#111;box-shadow:0 4px 6px rgba(0,0,0,.6),inset 0 1px 0 rgba(255,255,255,.1)}
      #${safeId} .stomp-outer{width:50px;height:50px;border-radius:999px;display:flex;align-items:center;justify-content:center;background:conic-gradient(from 0deg,#555,#999,#555,#999,#555)}
      #${safeId} .stomp-inner{width:32px;height:32px;border-radius:999px;background:radial-gradient(circle at 50% 20%,#e5e5e5 0%,#888 100%);box-shadow:0 5px 8px rgba(0,0,0,.8),inset 0 2px 2px #fff,inset 0 -2px 4px rgba(0,0,0,.4);transition:transform .07s}
      #${safeId} .stomp-btn:active .stomp-inner{transform:scale(.95) translateY(1px)}
      #${safeId} .logo-mark{font-size:30px;color:#737373;opacity:.34;letter-spacing:.12em}
      @keyframes ${safeId}-pulse{0%,100%{opacity:1}50%{opacity:.6}}
    </style>
    <div id="${safeId}" class="obsidian-plugin">
      <div class="chassis">
        <div class="grain"></div>
        <div class="screw" style="top:14px;left:14px"></div>
        <div class="screw" style="top:14px;right:14px;transform:rotate(45deg)"></div>
        <div class="screw" style="bottom:14px;left:14px;transform:rotate(-12deg)"></div>
        <div class="screw" style="bottom:14px;right:14px;transform:rotate(90deg)"></div>
        <div class="top">
          <div style="display:flex;align-items:center;gap:14px;min-width:0">
            <div style="min-width:0">
              <span class="eyebrow">${slot?.engine?.inputNode ? 'Analog Processor' : 'Software Instrument'}</span>
              <div class="title">${name}</div>
            </div>
            <div style="height:24px;width:1px;background:#262626;box-shadow:1px 0 0 rgba(255,255,255,.05)"></div>
            <span class="jb model">MOD-X7</span>
          </div>
          <div class="vents"><span class="vent"></span><span class="vent"></span><span class="vent"></span><span class="vent"></span></div>
          <div class="power-wrap">
            <span class="label">Power</span>
            <div class="switch" data-power><div class="thumb" data-power-thumb></div></div>
          </div>
        </div>
        <div class="main">
          <div class="section border-r">
            <div class="knob-block">
              <div class="knob-ring" style="width:104px;height:104px">
                <div class="knob" data-knob="gain" data-angle="${gain}" style="width:74px;height:74px">
                  <div class="knob-inner" data-knob-inner="gain" style="width:54px;height:54px;transform:rotate(${gain}deg)"><span class="knob-dot"></span></div>
                </div>
              </div>
              <span class="plate">Input Gain</span>
            </div>
            <div class="knob-block">
              <div class="toggle-box"><div class="toggle-slot" data-sat><div class="toggle-bat" data-sat-bat></div></div></div>
              <span class="label">Sat. Type</span>
            </div>
          </div>
          <div>
            <div class="eq-head">
              <div style="display:flex;align-items:center;gap:8px"><span class="led-wrap"><span class="led orange on"></span></span><span class="label">EQ Active</span></div>
              <span class="jb model">4-BAND PRECISION</span>
            </div>
            <div class="slider-row">
              ${['80Hz', '400Hz', '2kHz', '8kHz'].map((label, i) => `
                <div class="slider-col">
                  <div class="slider-track"><div class="slider-cap" data-slider="${i}" style="top:${sliders[i]}%"><span class="slider-groove"></span></div></div>
                  <span class="jb model">${label}</span>
                </div>
              `).join('')}
            </div>
          </div>
          <div class="section">
            <div class="knob-block">
              <div class="knob-ring" style="width:92px;height:92px">
                <div class="knob" data-knob="output" data-angle="${output}" style="width:64px;height:64px">
                  <div class="knob-inner" data-knob-inner="output" style="width:48px;height:48px;transform:rotate(${output}deg)"><span class="knob-dot"></span></div>
                </div>
              </div>
              <span class="plate">Output</span>
            </div>
            <div class="knob-block">
              <div class="knob-ring" style="width:78px;height:78px">
                <div class="knob" data-knob="mix" data-angle="${mix}" style="width:54px;height:54px">
                  <div class="knob-inner" data-knob-inner="mix" style="width:40px;height:40px;transform:rotate(${mix}deg)"><span class="knob-dot" style="width:4px;height:6px"></span></div>
                </div>
              </div>
              <span class="label">Wet / Dry</span>
            </div>
          </div>
        </div>
        <div class="bottom">
          <div class="stomp">
            <span class="led-wrap"><span class="led ${bypassClass}" data-bypass-led></span></span>
            <div class="stomp-btn" data-bypass><span class="stomp-outer"><span class="stomp-inner"></span></span></div>
            <span class="label">Bypass</span>
          </div>
          <div class="logo-mark">◉</div>
          <div class="stomp">
            <span class="led-wrap"><span class="led ${engageClass}" data-engage-led></span></span>
            <div class="stomp-btn" data-engage><span class="stomp-outer"><span class="stomp-inner"></span></span></div>
            <span class="label">Engage</span>
          </div>
        </div>
      </div>
    </div>
  `;
}

function bindObsidianGui(slotId, container) {
  const slot = wamInstances.get(slotId);
  const root = container.querySelector('.obsidian-plugin');
  if (!slot || !root) return;
  const params = slot.params;
  const clamp = (v, min, max) => Math.max(min, Math.min(max, v));

  root.querySelectorAll('[data-slider]').forEach(cap => {
    let dragging = false;
    let startY = 0;
    let startTop = 0;
    const capHeight = 22;
    const parentHeight = () => cap.parentElement.getBoundingClientRect().height;
    const start = y => {
      dragging = true;
      startY = y;
      startTop = (parseFloat(cap.style.top) || 0) / 100 * parentHeight();
      document.body.style.userSelect = 'none';
    };
    const move = y => {
      if (!dragging) return;
      const height = parentHeight();
      const next = clamp(startTop + y - startY, 0, height - capHeight);
      const pct = Number((next / height * 100).toFixed(1));
      cap.style.top = `${pct}%`;
      params.sliders[Number(cap.dataset.slider)] = pct;
    };
    const stop = () => {
      dragging = false;
      document.body.style.userSelect = '';
    };
    cap.addEventListener('pointerdown', e => {
      e.preventDefault();
      cap.setPointerCapture(e.pointerId);
      start(e.clientY);
    });
    cap.addEventListener('pointermove', e => move(e.clientY));
    cap.addEventListener('pointerup', stop);
    cap.addEventListener('pointercancel', stop);
  });

  root.querySelectorAll('[data-knob]').forEach(knob => {
    const name = knob.dataset.knob;
    const inner = root.querySelector(`[data-knob-inner="${name}"]`);
    let dragging = false;
    let lastY = 0;
    let angle = Number(knob.dataset.angle) || 0;
    const update = () => {
      if (inner) inner.style.transform = `rotate(${angle}deg)`;
      params[name] = angle;
    };
    knob.addEventListener('pointerdown', e => {
      e.preventDefault();
      knob.setPointerCapture(e.pointerId);
      dragging = true;
      lastY = e.clientY;
      document.body.style.userSelect = 'none';
    });
    knob.addEventListener('pointermove', e => {
      if (!dragging) return;
      angle = (angle + (lastY - e.clientY) * 1.5) % 360;
      if (angle < 0) angle += 360;
      lastY = e.clientY;
      update();
    });
    const stop = () => {
      dragging = false;
      document.body.style.userSelect = '';
    };
    knob.addEventListener('pointerup', stop);
    knob.addEventListener('pointercancel', stop);
  });

  root.querySelector('[data-power]')?.addEventListener('click', () => {
    const thumb = root.querySelector('[data-power-thumb]');
    params.power = !params.power;
    if (!thumb) return;
    thumb.style.right = params.power ? '4px' : 'auto';
    thumb.style.left = params.power ? 'auto' : '4px';
    thumb.style.background = params.power
      ? 'radial-gradient(circle at 30% 30%, #ff8a00, #c24100)'
      : 'radial-gradient(circle at 30% 30%, #444, #222)';
  });

  root.querySelector('[data-sat]')?.addEventListener('click', () => {
    const bat = root.querySelector('[data-sat-bat]');
    params.satUp = !params.satUp;
    if (!bat) return;
    bat.style.bottom = params.satUp ? 'auto' : '4px';
    bat.style.top = params.satUp ? '4px' : 'auto';
  });

  root.querySelector('[data-bypass]')?.addEventListener('click', () => {
    const led = root.querySelector('[data-bypass-led]');
    params.bypass = !params.bypass;
    led?.classList.toggle('on', params.bypass);
    led?.classList.toggle('red', params.bypass);
  });

  root.querySelector('[data-engage]')?.addEventListener('click', () => {
    const led = root.querySelector('[data-engage-led]');
    params.engage = !params.engage;
    led?.classList.toggle('on', params.engage);
    led?.classList.toggle('blue', params.engage);
  });
}

export async function wamShowGui(slotId, container) {
  if (!container) return;
  const slot = wamInstances.get(slotId);
  container.innerHTML = renderObsidianGui(slotId, slot);
  bindObsidianGui(slotId, container);
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
