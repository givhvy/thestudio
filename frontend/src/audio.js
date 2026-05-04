import { playSynthVoice, createChannelEffects, createReverb } from './dsp.js';

let ctx = null;
let masterGain = null;
let compressor = null;
let analyser = null;
let masterReverb = null;        // Sprint 2: master reverb bus
const sampleBuffers = new Map();
const activeSourceNodes = new Set(); // track all playing BufferSourceNodes

// Per-channel audio routing: id -> { input, fx, gain, pan, mute, dispose }
// Lets us route each channel through its own node chain → master, so vol/pan/mute
// actually shape the audio (Sprint 1: beat-making essentials).
// Each strip also has an effects chain (filter + delay) inserted before the amp.
const channelStrips = new Map();

export function initAudio() {
  if (ctx) return;
  ctx = new (window.AudioContext || window.webkitAudioContext)();
  compressor = ctx.createDynamicsCompressor();
  compressor.threshold.value = -20;
  compressor.knee.value = 10;
  compressor.ratio.value = 4;
  masterGain = ctx.createGain();
  masterGain.gain.value = 0.8;
  analyser = ctx.createAnalyser();
  analyser.fftSize = 64;
  // Master reverb bus runs in parallel with the dry master path
  masterReverb = createReverb(ctx, { duration: 2.5, decay: 2.0 });
  masterGain.connect(compressor);
  compressor.connect(analyser);
  analyser.connect(ctx.destination);
  // wet path: reverb output → destination (post-master)
  masterReverb.output.connect(ctx.destination);
}

// --- Master reverb ---
export function setMasterReverbMix(mix) { if (masterReverb) masterReverb.setMix(mix); }
export function setMasterReverbWet(wet) { if (masterReverb) masterReverb.setWet(wet); }

export function resumeAudio() {
  if (!ctx) return;
  if (ctx.state === 'suspended') ctx.resume();
}

// Aggressively unlock audio — needed in JUCE WebView and some browsers
export async function unlockAudio() {
  if (!ctx) initAudio();
  if (ctx.state !== 'running') {
    try { await ctx.resume(); } catch (_) {}
  }
  // Play a silent buffer to force unlock
  const buf = ctx.createBuffer(1, 1, ctx.sampleRate);
  const src = ctx.createBufferSource();
  src.buffer = buf;
  src.connect(ctx.destination);
  src.start(0);
}

export function stopAllAudio() {
  // Stop all tracked active sources immediately
  activeSourceNodes.forEach(src => {
    try { src.stop(); } catch (_) {}
  });
  activeSourceNodes.clear();
  // Also silence master briefly to cut any oscillator/synth tails
  if (masterGain) {
    masterGain.gain.cancelScheduledValues(ctx.currentTime);
    masterGain.gain.setValueAtTime(0, ctx.currentTime);
    masterGain.gain.setValueAtTime(masterGain.gain.value || 0.8, ctx.currentTime + 0.03);
  }
}

export function destroyAudio() {
  if (ctx) {
    try { ctx.close(); } catch(e) {}
    ctx = null;
    masterGain = null;
    compressor = null;
    analyser = null;
    masterReverb = null;
    channelStrips.clear();
  }
}

export function now() { return ctx ? ctx.currentTime : 0; }

export function setMasterVolume(v) {
  if (masterGain) masterGain.gain.value = v;
}

export function getMeterData() {
  if (!analyser) return 0;
  const data = new Uint8Array(analyser.frequencyBinCount);
  analyser.getByteFrequencyData(data);
  let max = 0;
  for (let i = 0; i < data.length; i++) if (data[i] > max) max = data[i];
  return max / 255;
}

export function getChannelMeterLevel(id) {
  const strip = channelStrips.get(id);
  if (!strip?.analyser) return 0;
  const data = new Uint8Array(strip.analyser.frequencyBinCount);
  strip.analyser.getByteTimeDomainData(data);
  let peak = 0;
  for (let i = 0; i < data.length; i++) {
    const v = Math.abs((data[i] - 128) / 128);
    if (v > peak) peak = v;
  }
  return Math.min(1, peak * 2);
}

// --- Channel strips (per-channel routing) ---

/**
 * Create or update a per-channel audio strip.
 * Each strip: input → gain (vol) → panner → muteGain → masterGain
 */
export function ensureChannelStrip(id, opts = {}) {
  const {
    vol = 0.8, pan = 0, mute = false,
    hp, lp, filterQ,
    delayTime, delayMix, delayFeedback,
    reverbSend,
  } = opts;
  if (!ctx) initAudio();
  let strip = channelStrips.get(id);
  if (!strip) {
    const input = ctx.createGain();
    input.gain.value = 1;
    // Sprint 2: per-channel effects chain (filters + delay) inserted before amp.
    const fx = createChannelEffects(ctx);
    const gainNode = ctx.createGain();
    gainNode.gain.value = vol;
    const panNode = ctx.createStereoPanner ? ctx.createStereoPanner() : null;
    if (panNode) panNode.pan.value = pan;
    const muteNode = ctx.createGain();
    muteNode.gain.value = mute ? 0 : 1;
    // Per-channel master-reverb send
    const reverbSendGain = ctx.createGain();
    reverbSendGain.gain.value = 0;

    input.connect(fx.input);
    fx.output.connect(gainNode);
    if (panNode) {
      gainNode.connect(panNode);
      panNode.connect(muteNode);
    } else {
      gainNode.connect(muteNode);
    }
    muteNode.connect(masterGain);
    // tap post-mute into reverb bus
    muteNode.connect(reverbSendGain);
    if (masterReverb) reverbSendGain.connect(masterReverb.send);

    // Per-channel analyser for VU meter
    const stripAnalyser = ctx.createAnalyser();
    stripAnalyser.fftSize = 256;
    muteNode.connect(stripAnalyser);

    strip = { input, fx, gain: gainNode, pan: panNode, mute: muteNode, reverbSend: reverbSendGain, analyser: stripAnalyser };
    channelStrips.set(id, strip);
  } else {
    strip.gain.gain.value = vol;
    if (strip.pan) strip.pan.pan.value = pan;
    strip.mute.gain.value = mute ? 0 : 1;
  }
  // apply optional fx parameters
  if (hp != null) strip.fx.setHp(hp);
  if (lp != null) strip.fx.setLp(lp);
  if (filterQ != null) strip.fx.setFilterQ(filterQ);
  if (delayTime != null) strip.fx.setDelayTime(delayTime);
  if (delayMix != null) strip.fx.setDelayMix(delayMix);
  if (delayFeedback != null) strip.fx.setDelayFeedback(delayFeedback);
  if (reverbSend != null) strip.reverbSend.gain.value = Math.max(0, Math.min(1, reverbSend));
  return strip.input;
}

// Public: tweak a single FX param without rebuilding the strip
export function setChannelFx(id, key, value) {
  const strip = channelStrips.get(id);
  if (!strip) return;
  switch (key) {
    case 'hp': strip.fx.setHp(value); break;
    case 'lp': strip.fx.setLp(value); break;
    case 'filterQ': strip.fx.setFilterQ(value); break;
    case 'delayTime': strip.fx.setDelayTime(value); break;
    case 'delayMix': strip.fx.setDelayMix(value); break;
    case 'delayFeedback': strip.fx.setDelayFeedback(value); break;
    case 'reverbSend': strip.reverbSend.gain.value = Math.max(0, Math.min(1, value)); break;
  }
}

export function getChannelInput(id) {
  const strip = channelStrips.get(id);
  return strip ? strip.input : masterGain;
}

export function disposeChannelStrip(id) {
  const strip = channelStrips.get(id);
  if (!strip) return;
  try {
    strip.input.disconnect();
    strip.gain.disconnect();
    if (strip.pan) strip.pan.disconnect();
    strip.mute.disconnect();
  } catch {}
  channelStrips.delete(id);
}

// --- Sample loading & playback ---

// Pure JS WAV Decoder to bypass Chromium C++ segfault bug
async function decodeWav(arrayBuffer) {
  const view = new DataView(arrayBuffer);
  if (String.fromCharCode(...new Uint8Array(arrayBuffer, 0, 4)) !== 'RIFF') {
    throw new Error('Not a valid WAV file');
  }

  const numChannels = view.getUint16(22, true);
  const sampleRate = view.getUint32(24, true);
  const bitDepth = view.getUint16(34, true);

  let offset = 12; // Start after 'WAVE'
  while (offset < view.byteLength) {
    const chunkId = String.fromCharCode(...new Uint8Array(arrayBuffer, offset, 4));
    const chunkSize = view.getUint32(offset + 4, true);

    if (chunkId === 'data') {
      const dataOffset = offset + 8;
      const numSamples = chunkSize / (numChannels * (bitDepth / 8));
      const audioBuffer = ctx.createBuffer(numChannels, numSamples, sampleRate);

      for (let c = 0; c < numChannels; c++) {
        const channelData = audioBuffer.getChannelData(c);
        let readOffset = dataOffset + c * (bitDepth / 8);

        for (let i = 0; i < numSamples; i++) {
          if (bitDepth === 16) {
            channelData[i] = view.getInt16(readOffset, true) / 32768.0;
          } else if (bitDepth === 24) {
            let val = view.getUint8(readOffset) | (view.getUint8(readOffset + 1) << 8) | (view.getInt8(readOffset + 2) << 16);
            channelData[i] = val / 8388608.0;
          } else if (bitDepth === 32) {
            channelData[i] = view.getFloat32(readOffset, true);
          }
          readOffset += numChannels * (bitDepth / 8);
        }
      }
      return audioBuffer;
    }
    offset += 8 + chunkSize;
  }
  throw new Error('No data chunk found in WAV');
}

export async function loadSample(name, arrayBuffer) {
  if (!ctx) initAudio();
  try {
    let buffer;
    if (name.toLowerCase().endsWith('.wav')) {
      buffer = await decodeWav(arrayBuffer);
    } else {
      // Fallback for mp3, etc. (might still crash if the bug hits them, but usually WAV triggers it)
      buffer = await ctx.decodeAudioData(arrayBuffer);
    }
    sampleBuffers.set(name, buffer);
    return buffer;
  } catch (err) {
    console.error('loadSample failed:', err);
    throw err;
  }
}

export function playSample(name, time, gain = 1, rate = 1) {
  if (!ctx) return;
  const buffer = sampleBuffers.get(name);
  if (!buffer) return;
  const src = ctx.createBufferSource();
  src.buffer = buffer;
  src.playbackRate.value = rate;
  const g = ctx.createGain();
  g.gain.value = gain;
  src.connect(g);
  g.connect(masterGain);
  activeSourceNodes.add(src);
  src.onended = () => activeSourceNodes.delete(src);
  src.start(time);
  return src;
}

export function hasSample(name) {
  return sampleBuffers.has(name);
}

// --- Note frequencies ---

const noteFreqs = {};
const notes = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
for (let oct = 0; oct <= 8; oct++) {
  for (let i = 0; i < 12; i++) {
    noteFreqs[notes[i] + oct] = 440 * Math.pow(2, (oct - 4) + (i - 9) / 12);
  }
}

export function freqFromMidi(note) {
  const name = notes[note % 12] + Math.floor(note / 12);
  return noteFreqs[name] || 440;
}

// --- Sound generators (context-agnostic) ---

function playKickInContext(ac, dest, time, gain = 1) {
  const osc = ac.createOscillator();
  const g = ac.createGain();
  osc.frequency.setValueAtTime(150, time);
  osc.frequency.exponentialRampToValueAtTime(0.01, time + 0.5);
  g.gain.setValueAtTime(gain, time);
  g.gain.exponentialRampToValueAtTime(0.01, time + 0.5);
  osc.connect(g);
  g.connect(dest);
  osc.start(time);
  osc.stop(time + 0.5);
}

function playSnareInContext(ac, dest, time, gain = 1) {
  const noise = ac.createBufferSource();
  const buffer = ac.createBuffer(1, ac.sampleRate * 0.2, ac.sampleRate);
  const data = buffer.getChannelData(0);
  for (let i = 0; i < data.length; i++) data[i] = Math.random() * 2 - 1;
  noise.buffer = buffer;
  const filter = ac.createBiquadFilter();
  filter.type = 'highpass';
  filter.frequency.value = 1000;
  const g = ac.createGain();
  g.gain.setValueAtTime(gain * 0.6, time);
  g.gain.exponentialRampToValueAtTime(0.01, time + 0.2);
  noise.connect(filter);
  filter.connect(g);
  g.connect(dest);
  noise.start(time);
  noise.stop(time + 0.2);

  const osc = ac.createOscillator();
  const og = ac.createGain();
  osc.type = 'triangle';
  osc.frequency.setValueAtTime(200, time);
  og.gain.setValueAtTime(gain * 0.5, time);
  og.gain.exponentialRampToValueAtTime(0.01, time + 0.1);
  osc.connect(og);
  og.connect(dest);
  osc.start(time);
  osc.stop(time + 0.1);
}

function playHihatInContext(ac, dest, time, open = false, gain = 1) {
  const noise = ac.createBufferSource();
  const buffer = ac.createBuffer(1, ac.sampleRate * 0.1, ac.sampleRate);
  const data = buffer.getChannelData(0);
  for (let i = 0; i < data.length; i++) data[i] = Math.random() * 2 - 1;
  noise.buffer = buffer;
  const filter = ac.createBiquadFilter();
  filter.type = 'highpass';
  filter.frequency.value = 8000;
  const g = ac.createGain();
  const dur = open ? 0.3 : 0.05;
  g.gain.setValueAtTime(gain * 0.3, time);
  g.gain.exponentialRampToValueAtTime(0.001, time + dur);
  noise.connect(filter);
  filter.connect(g);
  g.connect(dest);
  noise.start(time);
  noise.stop(time + dur);
}

function playClapInContext(ac, dest, time, gain = 1) {
  const noise = ac.createBufferSource();
  const buffer = ac.createBuffer(1, ac.sampleRate * 0.2, ac.sampleRate);
  const data = buffer.getChannelData(0);
  for (let i = 0; i < data.length; i++) data[i] = Math.random() * 2 - 1;
  noise.buffer = buffer;
  const filter = ac.createBiquadFilter();
  filter.type = 'bandpass';
  filter.frequency.value = 1500;
  filter.Q.value = 1;
  const g = ac.createGain();
  g.gain.setValueAtTime(0, time);
  g.gain.linearRampToValueAtTime(gain * 0.5, time + 0.01);
  g.gain.exponentialRampToValueAtTime(0.01, time + 0.2);
  noise.connect(filter);
  filter.connect(g);
  g.connect(dest);
  noise.start(time);
  noise.stop(time + 0.2);
}

function playToneInContext(ac, dest, freq, time, duration = 0.2, type = 'square', gain = 0.3) {
  if (!freq) return;
  const osc = ac.createOscillator();
  const g = ac.createGain();
  osc.type = type;
  osc.frequency.setValueAtTime(freq, time);
  g.gain.setValueAtTime(gain, time);
  g.gain.exponentialRampToValueAtTime(0.001, time + duration);
  osc.connect(g);
  g.connect(dest);
  osc.start(time);
  osc.stop(time + duration);
}

function playSampleInContext(ac, dest, name, time, gain = 1, rate = 1) {
  const buffer = sampleBuffers.get(name);
  if (!buffer) return;
  const src = ac.createBufferSource();
  src.buffer = buffer;
  src.playbackRate.value = rate;
  const g = ac.createGain();
  g.gain.value = gain;
  src.connect(g);
  g.connect(dest);
  activeSourceNodes.add(src);
  src.onended = () => activeSourceNodes.delete(src);
  src.start(time);
}

// --- Public real-time API ---

export function playKick(time, gain = 1, dest) { if (ctx) playKickInContext(ctx, dest || masterGain, time, gain); }
export function playSnare(time, gain = 1, dest) { if (ctx) playSnareInContext(ctx, dest || masterGain, time, gain); }
export function playHihat(time, open = false, gain = 1, dest) { if (ctx) playHihatInContext(ctx, dest || masterGain, time, open, gain); }
export function playClap(time, gain = 1, dest) { if (ctx) playClapInContext(ctx, dest || masterGain, time, gain); }
export function playTone(freq, time, duration = 0.2, type = 'square', gain = 0.3, dest) { if (ctx) playToneInContext(ctx, dest || masterGain, freq, time, duration, type, gain); }
export function playSampleAt(name, time, gain = 1, rate = 1, dest) { if (ctx) playSampleInContext(ctx, dest || masterGain, name, time, gain, rate); }

// Sprint 2: real synth voice (3-osc, ADSR, filter)
export function playSynth(freq, time, opts = {}, dest) {
  if (!ctx) return;
  playSynthVoice(ctx, dest || masterGain, freq, time, opts);
}

// --- Offline rendering / WAV export ---

const bassNotes = [36, 36, 39, 36, 41, 36, 39, 36, 36, 36, 39, 36, 41, 43, 39, 36];
const leadNotes = [60, 0, 63, 65, 67, 0, 65, 63, 60, 0, 58, 60, 63, 0, 60, 0];
const padNotes = [48, 51, 55, 48, 43, 46, 50, 48];

function scheduleChannelSound(ac, dest, ch, step, time, gainScale) {
  const g = (ch.vol / 100) * 0.8 * gainScale;
  switch (ch.type) {
    case 'kick': playKickInContext(ac, dest, time, g); break;
    case 'snare': playSnareInContext(ac, dest, time, g); break;
    case 'hihat': playHihatInContext(ac, dest, time, false, g); break;
    case 'hihat_open': playHihatInContext(ac, dest, time, true, g); break;
    case 'clap': playClapInContext(ac, dest, time, g); break;
    case 'bass': {
      const bn = bassNotes[step % 16];
      if (bn) playToneInContext(ac, dest, freqFromMidi(bn), time, 0.25, 'sawtooth', g * 0.4);
      break;
    }
    case 'lead': {
      const ln = leadNotes[step % 16];
      if (ln) playToneInContext(ac, dest, freqFromMidi(ln), time, 0.2, 'square', g * 0.2);
      break;
    }
    case 'pad': {
      const pn = padNotes[Math.floor(step / 2) % padNotes.length];
      if (pn) playToneInContext(ac, dest, freqFromMidi(pn), time, 0.5, 'sine', g * 0.15);
      break;
    }
    default: {
      if (sampleBuffers.has(ch.type)) playSampleInContext(ac, dest, ch.type, time, g);
      break;
    }
  }
}

export async function renderWAV(bpm, playlistBlocks, patterns, channels, pianoNotes, mixerTracks) {
  // Determine total duration from playlist blocks
  let totalBeats = 16; // default minimum
  playlistBlocks.forEach(blocks => {
    blocks.forEach(b => {
      totalBeats = Math.max(totalBeats, b.start + b.length);
    });
  });
  const spb = 60 / bpm;
  const duration = totalBeats * spb;
  const sampleRate = 44100;
  const totalSamples = Math.ceil(sampleRate * duration);

  const offlineCtx = new OfflineAudioContext(2, totalSamples, sampleRate);
  const offMaster = offlineCtx.createGain();
  offMaster.gain.value = 0.8;
  const offCompressor = offlineCtx.createDynamicsCompressor();
  offCompressor.threshold.value = -20;
  offCompressor.knee.value = 10;
  offCompressor.ratio.value = 4;
  offMaster.connect(offCompressor);
  offCompressor.connect(offlineCtx.destination);

  const stepDur = spb * 0.25; // 16th note

  // Schedule playlist blocks
  playlistBlocks.forEach(blocks => {
    blocks.forEach(block => {
      const patIdx = block.pattern;
      const pat = patterns[patIdx];
      const offsetSec = block.start * spb;
      const stepsInBlock = Math.floor(block.length / 0.25);

      const anySolo = (pat && pat.channels) ? pat.channels.some((_, ci) => channels[ci]?.solo) : false;

      for (let step = 0; step < stepsInBlock; step++) {
        const stepTime = offsetSec + step * stepDur;
        if (stepTime >= duration) continue;

        if (pat && pat.channels) {
          pat.channels.forEach((chData, ci) => {
            const ch = channels[ci];
            if (!ch) return;
            if (ch.mute) return;
            if (anySolo && !ch.solo) return;
            if (!chData.steps || !chData.steps[step % 16]) return;
            // apply mixer track mute/solo
            const mt = mixerTracks[ch.mixerTrack];
            if (mt && mt.mute) return;
            if (mixerTracks.some(t => t.solo) && mt && !mt.solo) return;
            scheduleChannelSound(offlineCtx, offMaster, ch, step % 16, stepTime, (mt ? mt.vol / 100 : 1));
          });
        }
      }

      // Schedule piano notes for this pattern
      const pNotes = pianoNotes[patIdx] || [];
      pNotes.forEach(note => {
        const noteTime = offsetSec + note.start * spb;
        const noteDur = note.length * spb;
        if (noteTime >= duration) return;
        const freq = freqFromMidi(note.note);
        const vel = (note.vel ?? 80) / 100;
        playToneInContext(offlineCtx, offMaster, freq, noteTime, noteDur, 'sawtooth', vel * 0.25);
      });
    });
  });

  const rendered = await offlineCtx.startRendering();
  return audioBufferToWav(rendered);
}

function audioBufferToWav(buffer) {
  const numOfChan = buffer.numberOfChannels;
  const length = buffer.length * numOfChan * 2 + 44;
  const ab = new ArrayBuffer(length);
  const view = new DataView(ab);
  const channels = [];
  for (let i = 0; i < numOfChan; i++) channels.push(buffer.getChannelData(i));

  writeString(view, 0, 'RIFF');
  view.setUint32(4, 36 + buffer.length * numOfChan * 2, true);
  writeString(view, 8, 'WAVE');
  writeString(view, 12, 'fmt ');
  view.setUint32(16, 16, true);
  view.setUint16(20, 1, true);
  view.setUint16(22, numOfChan, true);
  view.setUint32(24, buffer.sampleRate, true);
  view.setUint32(28, buffer.sampleRate * 2 * numOfChan, true);
  view.setUint16(32, numOfChan * 2, true);
  view.setUint16(34, 16, true);
  writeString(view, 36, 'data');
  view.setUint32(40, buffer.length * numOfChan * 2, true);

  let offset = 44;
  for (let i = 0; i < buffer.length; i++) {
    for (let ch = 0; ch < numOfChan; ch++) {
      let sample = Math.max(-1, Math.min(1, channels[ch][i]));
      sample = sample < 0 ? sample * 0x8000 : sample * 0x7FFF;
      view.setInt16(offset, sample, true);
      offset += 2;
    }
  }
  return new Blob([ab], { type: 'audio/wav' });
}

function writeString(view, offset, string) {
  for (let i = 0; i < string.length; i++) view.setUint8(offset + i, string.charCodeAt(i));
}
