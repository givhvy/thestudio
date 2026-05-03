// ============================================================
// Stratum DSP module
// Reusable synth voice + per-channel effects + master reverb.
// Pure helpers around an AudioContext — no React, no globals.
// ============================================================

// ---- Synth voice (3-osc, ADSR, filter) ----
//
// playSynthVoice(ac, dest, freq, time, opts) schedules one note.
// Designed to be called from the lookahead scheduler with sample-accurate `time`.
//
// opts:
//   waveforms: ['sawtooth','square','sine'] (length 1-3)
//   detune:    [0, +7, -7] cents per oscillator
//   gains:     [0.3, 0.3, 0.3] mix per oscillator
//   attack, decay, sustain, release  (seconds, 0-1, 0-1, seconds)
//   filterCutoff: 2000   (Hz)
//   filterQ:     1
//   filterEnv:   0.6     (how much envelope opens the filter, 0..1)
//   duration:    0.4     (note length in seconds, before release)
//   velocity:    0.9     (0..1)
export function playSynthVoice(ac, dest, freq, time, opts = {}) {
  const {
    waveforms = ['sawtooth', 'square', 'sine'],
    detune = [0, 7, -7],
    gains = [0.5, 0.3, 0.2],
    attack = 0.005,
    decay = 0.1,
    sustain = 0.7,
    release = 0.15,
    filterCutoff = 2200,
    filterQ = 1,
    filterEnv = 0.5,
    duration = 0.3,
    velocity = 0.9,
  } = opts;

  // Voice bus (filter -> amp -> dest)
  const filter = ac.createBiquadFilter();
  filter.type = 'lowpass';
  filter.Q.value = filterQ;
  // filter envelope: opens to cutoff, closes back over decay
  const peak = filterCutoff * (1 + filterEnv * 4);
  filter.frequency.setValueAtTime(filterCutoff, time);
  filter.frequency.exponentialRampToValueAtTime(Math.max(50, peak), time + 0.005);
  filter.frequency.exponentialRampToValueAtTime(Math.max(50, filterCutoff * (1 - filterEnv * 0.4)), time + 0.005 + decay);

  const amp = ac.createGain();
  amp.gain.setValueAtTime(0.0001, time);
  amp.gain.exponentialRampToValueAtTime(velocity, time + attack);
  amp.gain.exponentialRampToValueAtTime(Math.max(0.0001, velocity * sustain), time + attack + decay);
  amp.gain.setValueAtTime(Math.max(0.0001, velocity * sustain), time + duration);
  amp.gain.exponentialRampToValueAtTime(0.0001, time + duration + release);

  filter.connect(amp);
  amp.connect(dest);

  // Oscillators
  const oscs = [];
  for (let i = 0; i < waveforms.length; i++) {
    const osc = ac.createOscillator();
    osc.type = waveforms[i];
    osc.frequency.value = freq;
    osc.detune.value = detune[i] || 0;
    const og = ac.createGain();
    og.gain.value = gains[i] || 0.3;
    osc.connect(og);
    og.connect(filter);
    osc.start(time);
    osc.stop(time + duration + release + 0.05);
    oscs.push(osc);
  }
}

// ---- Per-channel effects chain ----
//
// Build a configurable insert chain: input -> [hp -> lp -> delay] -> output.
// Returns { input, output, params: { setHp, setLp, setQ, setDelayTime, setDelayMix, setDelayFeedback } }
// Connect strip's pre-amp into chain.input, chain.output into amp/master.
export function createChannelEffects(ac) {
  const input = ac.createGain();
  const output = ac.createGain();

  // High-pass
  const hp = ac.createBiquadFilter();
  hp.type = 'highpass';
  hp.frequency.value = 20;
  hp.Q.value = 0.7;

  // Low-pass
  const lp = ac.createBiquadFilter();
  lp.type = 'lowpass';
  lp.frequency.value = 18000;
  lp.Q.value = 0.7;

  // Delay (parallel send + dry mix)
  const dry = ac.createGain();
  dry.gain.value = 1;
  const delaySend = ac.createGain();
  delaySend.gain.value = 0;        // mix amount
  const delay = ac.createDelay(2.0);
  delay.delayTime.value = 0.25;
  const fb = ac.createGain();
  fb.gain.value = 0.35;

  // Wiring
  input.connect(hp);
  hp.connect(lp);
  // dry path
  lp.connect(dry);
  dry.connect(output);
  // delay path
  lp.connect(delaySend);
  delaySend.connect(delay);
  delay.connect(fb);
  fb.connect(delay);              // feedback loop
  delay.connect(output);          // delay tail to output

  return {
    input, output,
    setHp: (hz) => hp.frequency.value = Math.max(20, Math.min(20000, hz)),
    setLp: (hz) => lp.frequency.value = Math.max(20, Math.min(20000, hz)),
    setFilterQ: (q) => { hp.Q.value = q; lp.Q.value = q; },
    setDelayTime: (s) => delay.delayTime.value = Math.max(0.001, Math.min(2, s)),
    setDelayMix: (m) => delaySend.gain.value = Math.max(0, Math.min(1, m)),
    setDelayFeedback: (f) => fb.gain.value = Math.max(0, Math.min(0.95, f)),
    dispose: () => {
      try {
        [input, output, hp, lp, dry, delaySend, delay, fb].forEach(n => n.disconnect());
      } catch {}
    },
  };
}

// ---- Master reverb (convolution with synthesized impulse) ----
//
// Build once per AudioContext and connect a send into it.
// Returns { send, output, setMix }.
// Place between master compressor and destination, or as a parallel master send.
export function createReverb(ac, { duration = 2.5, decay = 2.0 } = {}) {
  const convolver = ac.createConvolver();
  convolver.normalize = true;

  // Generate exponentially decaying noise impulse
  const sr = ac.sampleRate;
  const len = Math.max(1, Math.floor(sr * duration));
  const impulse = ac.createBuffer(2, len, sr);
  for (let ch = 0; ch < 2; ch++) {
    const data = impulse.getChannelData(ch);
    for (let i = 0; i < len; i++) {
      const t = i / len;
      data[i] = (Math.random() * 2 - 1) * Math.pow(1 - t, decay);
    }
  }
  convolver.buffer = impulse;

  const send = ac.createGain();
  send.gain.value = 0.0;            // master reverb send level
  const wet = ac.createGain();
  wet.gain.value = 0.6;
  const output = ac.createGain();
  output.gain.value = 1;

  send.connect(convolver);
  convolver.connect(wet);
  wet.connect(output);

  return {
    send, output,
    setMix: (m) => send.gain.value = Math.max(0, Math.min(1, m)),
    setWet: (w) => wet.gain.value = Math.max(0, Math.min(1, w)),
  };
}
