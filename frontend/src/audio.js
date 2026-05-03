let ctx = null;
let masterGain = null;
let compressor = null;
let analyser = null;
const sampleBuffers = new Map();

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
  masterGain.connect(compressor);
  compressor.connect(analyser);
  analyser.connect(ctx.destination);
}

export function resumeAudio() {
  if (ctx && ctx.state === 'suspended') ctx.resume();
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

// --- Sample loading & playback ---

export async function loadSample(name, arrayBuffer) {
  if (!ctx) initAudio();
  const buffer = await ctx.decodeAudioData(arrayBuffer.slice(0));
  sampleBuffers.set(name, buffer);
  return buffer;
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
  src.start(time);
}

// --- Public real-time API ---

export function playKick(time, gain = 1) { if (ctx) playKickInContext(ctx, masterGain, time, gain); }
export function playSnare(time, gain = 1) { if (ctx) playSnareInContext(ctx, masterGain, time, gain); }
export function playHihat(time, open = false, gain = 1) { if (ctx) playHihatInContext(ctx, masterGain, time, open, gain); }
export function playClap(time, gain = 1) { if (ctx) playClapInContext(ctx, masterGain, time, gain); }
export function playTone(freq, time, duration = 0.2, type = 'square', gain = 0.3) { if (ctx) playToneInContext(ctx, masterGain, freq, time, duration, type, gain); }

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
