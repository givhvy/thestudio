import { useState, useEffect, useRef, useCallback } from 'react';
import { initAudio, resumeAudio, now, playKick, playSnare, playHihat, playClap, playTone, playSampleAt, playSynth, freqFromMidi, setMasterVolume, renderWAV, ensureChannelStrip, getChannelInput, disposeChannelStrip, loadSample, setMasterReverbWet } from './audio.js';
import { useMidiInput } from './hooks/useMidiInput.js';
import { useUndoableState } from './hooks/useUndoableState.js';
import { wamNoteOn, wamNoteOff } from './wam/WamLoader.js';
import PluginBrowser from './components/PluginBrowser.jsx';
import ProjectManager from './components/ProjectManager.jsx';
import Toolbar from './components/Toolbar.jsx';
import ChannelRack from './components/ChannelRack.jsx';
import DraggableWindow from './components/DraggableWindow.jsx';
import MenuBar from './components/MenuBar.jsx';
import TitleBar from './components/TitleBar.jsx';
import PianoRoll from './components/PianoRoll.jsx';
import Playlist from './components/Playlist.jsx';
import Mixer from './components/Mixer.jsx';
import Browser from './components/Browser.jsx';

const notes = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];

const bassNotes = [36, 36, 39, 36, 41, 36, 39, 36, 36, 36, 39, 36, 41, 43, 39, 36];
const leadNotes = [60, 0, 63, 65, 67, 0, 65, 63, 60, 0, 58, 60, 63, 0, 60, 0];
const padNotes = [48, 51, 55, 48, 43, 46, 50, 48];

function makeDefaultChannels() {
  return [
    { name: 'Kick', color: '#ff8c00', type: 'kick', steps: [1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0], pan: 0, vol: 80, mute: false, solo: false, mixerTrack: 0 },
    { name: 'Snare', color: '#4caf50', type: 'snare', steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0], pan: 0, vol: 70, mute: false, solo: false, mixerTrack: 1 },
    { name: 'Hihat Closed', color: '#2196f3', type: 'hihat', steps: [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1], pan: 10, vol: 50, mute: false, solo: false, mixerTrack: 2 },
    { name: 'Hihat Open', color: '#9c27b0', type: 'hihat_open', steps: Array(16).fill(0), pan: -10, vol: 45, mute: false, solo: false, mixerTrack: 3 },
    { name: 'Clap', color: '#e91e63', type: 'clap', steps: [0,0,0,0,1,0,0,1,0,0,0,0,1,0,0,0], pan: 0, vol: 60, mute: false, solo: false, mixerTrack: 4 },
    { name: 'Bass', color: '#00bcd4', type: 'bass', steps: Array(16).fill(0), pan: 0, vol: 70, mute: false, solo: false, mixerTrack: 5 },
    { name: 'Lead', color: '#ffeb3b', type: 'lead', steps: Array(16).fill(0), pan: 5, vol: 60, mute: false, solo: false, mixerTrack: 6 },
    { name: 'Pad', color: '#795548', type: 'pad', steps: Array(16).fill(0), pan: -5, vol: 55, mute: false, solo: false, mixerTrack: 7 },
  ];
}

function makeDefaultMixerTracks() {
  const names = ['Kick','Snare','Hihat','Clap','Perc','Bass','Lead','Pad','','','','','','','Reverb','Master'];
  return Array.from({length:16},(_,i)=>({ name: names[i] || `Track ${i+1}`, vol: 80, pan: 0, mute: false, solo: false, meter: 0 }));
}

function makeDefaultPlaylistBlocks() {
  const arr = Array.from({length:12},()=>[]);
  arr[0].push({ pattern: 0, start: 0, length: 4, track: 0 });
  return arr;
}

function makeDefaultPianoNotes() {
  return [
    { note: 48, start: 0, length: 0.5, vel: 80 },
    { note: 51, start: 0.5, length: 0.5, vel: 80 },
    { note: 55, start: 1, length: 0.5, vel: 80 },
    { note: 60, start: 1.5, length: 0.5, vel: 80 },
    { note: 63, start: 2, length: 0.5, vel: 80 },
    { note: 67, start: 2.5, length: 0.5, vel: 80 },
    { note: 72, start: 3, length: 0.5, vel: 80 },
    { note: 67, start: 3.5, length: 0.5, vel: 80 },
  ];
}

export default function App() {
  const [showProjects, setShowProjects] = useState(false);
  const [activePanel, setActivePanel] = useState('channelRack');
  const [showChannelRack, setShowChannelRack] = useState(true);

  const [bpm, setBpm] = useState(130);
  const [playing, setPlaying] = useState(false);
  const [recording, setRecording] = useState(false);
  const [currentPattern, setCurrentPattern] = useState(0);
  const [stepIndex, setStepIndex] = useState(0);
  const [channels, setChannels, { undo: undoChannels, redo: redoChannels }] = useUndoableState(makeDefaultChannels());
  const [patterns, setPatterns] = useState([{ name: 'Pattern 1', channels: makeDefaultChannels().map(c=>({steps:[...c.steps]})) }]);
  const [mixerTracks, setMixerTracks] = useState(makeDefaultMixerTracks());
  const [playlistBlocks, setPlaylistBlocks] = useState(makeDefaultPlaylistBlocks());
  const [pianoNotes, setPianoNotes] = useState([makeDefaultPianoNotes()]);
  const [zoomPr, setZoomPr] = useState(1);
  const [zoomPl, setZoomPl] = useState(1);
  const [prSnap, setPrSnap] = useState('1/8');
  const [plSnap, setPlSnap] = useState('1/4');
  const [currentFile, setCurrentFile] = useState(null);
  const [projectName, setProjectName] = useState('Untitled');

  const nextStepTimeRef = useRef(0);
  const stepIdxRef = useRef(0);
  const timerRef = useRef(null);
  const playingRef = useRef(false);
  const channelsRef = useRef(channels);
  const bpmRef = useRef(bpm);
  const mixerTracksRef = useRef(mixerTracks);

  const hasAPI = typeof window !== 'undefined' && window.electronAPI;

  useEffect(() => { channelsRef.current = channels; }, [channels]);
  useEffect(() => { bpmRef.current = bpm; }, [bpm]);
  useEffect(() => { mixerTracksRef.current = mixerTracks; }, [mixerTracks]);

  // --- Electron menu listeners ---
  useEffect(() => {
    if (!hasAPI) return;
    window.electronAPI.onMenuAction((action) => {
      if (action === 'new') handleNew();
    });
    window.electronAPI.onOpenAction(() => setShowProjects(true));
    window.electronAPI.onSaveAction(() => handleSave());
    window.electronAPI.onExportAction(() => handleExport());
    window.electronAPI.onPlayAction(() => togglePlay());
    window.electronAPI.onStopAction(() => stopPlayback());
    window.electronAPI.onRecordAction(() => setRecording(r => !r));
    window.electronAPI.onUndoAction(() => {});
    window.electronAPI.onPanelAction((panel) => setActivePanel(panel));
    return () => { if (hasAPI) window.electronAPI.removeAllListeners(); };
  }, [hasAPI, channels, patterns, mixerTracks, playlistBlocks, pianoNotes, bpm, currentFile, projectName]);

  // --- Keyboard shortcuts ---
  useEffect(() => {
    const handler = (e) => {
      if (e.code === 'Space') { e.preventDefault(); togglePlay(); }
      if (e.key === '1') setActivePanel('channelRack');
      if (e.key === '2') setActivePanel('pianoRoll');
      if (e.key === '3') setActivePanel('playlist');
      if (e.key === '4') setActivePanel('mixerPanel');
      if ((e.ctrlKey || e.metaKey) && e.key === 's') { e.preventDefault(); handleSave(); }
      if ((e.ctrlKey || e.metaKey) && e.key === 'o') { e.preventDefault(); setShowProjects(true); }
      if ((e.ctrlKey || e.metaKey) && e.key === 'n') { e.preventDefault(); handleNew(); }
    };
    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, [playing, channels, patterns, mixerTracks, playlistBlocks, pianoNotes, bpm, currentFile, projectName]);

  function handleNew() {
    setProjectName('Untitled');
    setCurrentFile(null);
    setBpm(130);
    setChannels(makeDefaultChannels());
    setPatterns([{ name: 'Pattern 1', channels: makeDefaultChannels().map(c=>({steps:[...c.steps]})) }]);
    setMixerTracks(makeDefaultMixerTracks());
    setPlaylistBlocks(makeDefaultPlaylistBlocks());
    setPianoNotes([makeDefaultPianoNotes()]);
    setCurrentPattern(0);
    setStepIndex(0);
    stopPlayback();
  }

  async function handleSave() {
    const data = {
      project: { name: projectName, bpm, master_volume: 0.8, current_pattern: currentPattern },
      channels: channels.map(c => ({ ...c, steps: [...c.steps] })),
      patterns: patterns.map(p => ({ ...p, channels: p.channels.map(c => ({ steps: [...c.steps] })) })),
      mixerTracks: mixerTracks.map(t => ({ ...t })),
      playlistBlocks: playlistBlocks.flat().map(b => ({ ...b })),
      pianoNotes: pianoNotes.map(p => [...p])
    };

    let filePath = currentFile;
    if (!filePath) {
      if (!hasAPI) {
        const blob = new Blob([JSON.stringify(data, null, 2)], {type:'application/json'});
        const a = document.createElement('a');
        a.href = URL.createObjectURL(blob);
        a.download = `${projectName}.flc`;
        a.click();
        return;
      }
      filePath = await window.electronAPI.saveFile(`${projectName}.flc`);
      if (!filePath) return;
      setCurrentFile(filePath);
    }

    if (hasAPI) {
      const result = await window.electronAPI.writeFile(filePath, data);
      if (result.error) { alert('Save failed: ' + result.error); return; }
    }
    // Flash the project name briefly
    const orig = projectName;
    setProjectName(orig + ' - Saved');
    setTimeout(() => setProjectName(orig), 1000);
  }

  async function handleExport() {
    try {
      setProjectName(prev => prev + ' - Exporting...');
      const blob = await renderWAV(bpm, playlistBlocks, patterns, channels, pianoNotes, mixerTracks);
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `${projectName}.wav`;
      a.click();
      URL.revokeObjectURL(url);
      setProjectName(prev => prev.replace(' - Exporting...', ''));
    } catch (e) {
      alert('Export failed: ' + e.message);
      setProjectName(prev => prev.replace(' - Exporting...', ''));
    }
  }

  function loadProject(data) {
    if (!data) return;
    if (data.project) {
      setBpm(data.project.bpm || 130);
      setCurrentPattern(data.project.current_pattern || 0);
      setProjectName(data.project.name || 'Untitled');
    }
    if (data.channels) {
      setChannels(data.channels.map(c => ({
        ...c,
        steps: Array.isArray(c.steps) ? c.steps : JSON.parse(c.steps || '[]'),
        mute: !!c.mute, solo: !!c.solo
      })));
    }
    if (data.patterns) {
      setPatterns(data.patterns.map(p => ({
        ...p,
        channels: p.channels?.map(c => ({
          steps: Array.isArray(c.steps) ? [...c.steps] : JSON.parse(c.steps || '[]')
        })) || p.data?.channels?.map(c => ({
          steps: Array.isArray(c.steps) ? [...c.steps] : JSON.parse(c.steps || '[]')
        })) || []
      })));
    }
    if (data.mixerTracks) {
      setMixerTracks(data.mixerTracks.map(t => ({ ...t, mute: !!t.mute, solo: !!t.solo })));
    }
    if (data.playlistBlocks) {
      const arr = Array.from({length:12},()=>[]);
      data.playlistBlocks.forEach(b => { if (b.track >= 0 && b.track < arr.length) arr[b.track].push({ ...b }); });
      setPlaylistBlocks(arr);
    }
    if (data.pianoNotes) {
      if (Array.isArray(data.pianoNotes[0])) {
        setPianoNotes(data.pianoNotes.map(p => [...p]));
      } else {
        // backward compat: flat array becomes pattern 0
        setPianoNotes([data.pianoNotes.map(n => ({ ...n }))]);
      }
    }
    stopPlayback();
  }

  function handleNewProject(name) {
    handleNew();
    setProjectName(name);
  }

  // --- Auto-save to Prisma backend (debounced, 3 s) ---
  const autoSaveTimerRef = useRef(null);
  const projectIdRef = useRef(null);

  useEffect(() => {
    clearTimeout(autoSaveTimerRef.current);
    autoSaveTimerRef.current = setTimeout(async () => {
      try {
        const BACKEND = 'http://localhost:3002/api';
        // Create or reuse project record
        if (!projectIdRef.current) {
          const res = await fetch(`${BACKEND}/projects`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ name: projectName, bpm }),
          });
          if (!res.ok) return;
          const proj = await res.json();
          projectIdRef.current = proj.id;
        }
        // Save full state
        await fetch(`${BACKEND}/projects/${projectIdRef.current}/save`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            name: projectName,
            bpm,
            patterns: patterns.map(p => ({
              name: p.name,
              channels: channels.map((ch, i) => ({
                name: ch.name,
                type: ch.type,
                steps: ch.steps,
                vol: ch.vol,
                pan: ch.pan,
                mute: ch.mute,
              })),
            })),
            playlistBlocks,
          }),
        });
      } catch (err) {
        // Backend may not be running — silent fail in dev
      }
    }, 3000);
    return () => clearTimeout(autoSaveTimerRef.current);
  }, [projectName, bpm, patterns, channels, playlistBlocks]);

  // --- Undo / Redo keyboard shortcuts ---
  useEffect(() => {
    const onKey = (e) => {
      if ((e.ctrlKey || e.metaKey) && !e.shiftKey && e.key === 'z') {
        e.preventDefault();
        undoChannels();
      }
      if ((e.ctrlKey || e.metaKey) && (e.key === 'y' || (e.shiftKey && e.key === 'z'))) {
        e.preventDefault();
        redoChannels();
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [undoChannels, redoChannels]);

  // --- AUDIO ---
  useEffect(() => { playingRef.current = playing; }, [playing]);

  function togglePlay() {
    if (playingRef.current) stopPlayback();
    else startPlayback();
  }

  // Sync per-channel Web Audio strips with channel state (vol/pan/mute)
  // so that volume/pan/mute actually shape the audio instead of just being
  // post-multiplied at schedule time.
  useEffect(() => {
    if (!playing && !channelsRef.current.length) return;
    initAudio();
    channels.forEach((ch, i) => {
      ensureChannelStrip(`ch${i}`, {
        vol: (ch.vol / 100) * 0.85,
        pan: (ch.pan || 0) / 100,
        mute: ch.mute,
      });
    });
  }, [channels]);

  function scheduleStep(stepNumber, time) {
    const chs = channelsRef.current;
    const mts = mixerTracksRef.current;
    const anySolo = chs.some(c => c.solo);
    chs.forEach((ch, i) => {
      if (ch.mute) return;
      if (anySolo && !ch.solo) return;
      if (!ch.steps[stepNumber]) return;
      const mt = mts[ch.mixerTrack];
      if (mt && mt.mute) return;
      if (mts.some(t => t.solo) && mt && !mt.solo) return;
      // Route through this channel's strip so vol/pan/mute apply natively.
      const dest = getChannelInput(`ch${i}`);
      // Mixer gain still applied at schedule time (until mixer routing exists).
      const mixerScale = (mt ? mt.vol / 100 : 1);
      switch (ch.type) {
        case 'kick': playKick(time, mixerScale, dest); break;
        case 'snare': playSnare(time, mixerScale, dest); break;
        case 'hihat': playHihat(time, false, mixerScale, dest); break;
        case 'hihat_open': playHihat(time, true, mixerScale, dest); break;
        case 'clap': playClap(time, mixerScale, dest); break;
        case 'bass': {
          const bn = bassNotes[stepNumber];
          playSynth(freqFromMidi(bn), time, {
            waveforms: ['sawtooth','square'], detune: [0, -7], gains: [0.45, 0.25],
            attack: 0.005, decay: 0.18, sustain: 0.25, release: 0.1,
            filterCutoff: 600, filterQ: 6, filterEnv: 0.8,
            duration: 0.22, velocity: mixerScale * 0.6,
          }, dest);
          break;
        }
        case 'lead': {
          const ln = leadNotes[stepNumber];
          if (ln) playSynth(freqFromMidi(ln), time, {
            waveforms: ['square','sawtooth','sine'], detune: [0, 7, -7], gains: [0.4, 0.25, 0.15],
            attack: 0.01, decay: 0.15, sustain: 0.5, release: 0.2,
            filterCutoff: 3500, filterQ: 3, filterEnv: 0.4,
            duration: 0.18, velocity: mixerScale * 0.4,
          }, dest);
          break;
        }
        case 'pad': {
          const pn = padNotes[Math.floor(stepNumber / 2) % padNotes.length];
          playSynth(freqFromMidi(pn), time, {
            waveforms: ['sine','sawtooth','sine'], detune: [0, 12, -12], gains: [0.4, 0.15, 0.3],
            attack: 0.08, decay: 0.3, sustain: 0.6, release: 0.4,
            filterCutoff: 1800, filterQ: 1, filterEnv: 0.2,
            duration: 0.55, velocity: mixerScale * 0.3,
          }, dest);
          break;
        }
        default: {
          if (ch.type?.startsWith('wam:')) {
            // WAM WASM plugin — send MIDI note via WAM API
            // We send a 16th-note (250 ms at 120 BPM) note-on;
            // note-off is scheduled via setTimeout to avoid stuck notes.
            const slotId = ch.type.slice(4);
            const midiNote = ch.midiNote ?? 60;
            wamNoteOn(slotId, midiNote, Math.round(mixerScale * 100));
            setTimeout(() => wamNoteOff(slotId, midiNote), 200);
          } else if (ch.type?.startsWith('vst:')) {
            // VST plugin — forward MIDI via Electron IPC to the C++ host
            const vstSlotId = parseInt(ch.type.slice(4));
            window.electronAPI?.vstCall?.('noteOn', {
              slotId: vstSlotId, channel: 1,
              note: ch.midiNote ?? 60,
              velocity: Math.round(mixerScale * 100),
            });
            setTimeout(() => {
              window.electronAPI?.vstCall?.('noteOff', {
                slotId: vstSlotId, channel: 1, note: ch.midiNote ?? 60,
              });
            }, 200);
          } else {
            // user-loaded sample (drag-dropped)
            playSampleAt(ch.type, time, mixerScale, ch.rate || 1, dest);
          }
          break;
        }
      }
    });
  }

  function nextNote() {
    const secondsPerBeat = 60.0 / bpmRef.current;
    nextStepTimeRef.current += 0.25 * secondsPerBeat;
    stepIdxRef.current = (stepIdxRef.current + 1) % 16;
    setStepIndex(stepIdxRef.current);
  }

  // Lookahead scheduler — runs every 25 ms, schedules ahead by 100 ms.
  // setInterval keeps running when the tab is backgrounded (rAF doesn't),
  // so playback stays tight even when you tab away.
  function scheduler() {
    while (nextStepTimeRef.current < now() + 0.1) {
      scheduleStep(stepIdxRef.current, nextStepTimeRef.current);
      nextNote();
    }
  }

  function startPlayback() {
    initAudio();
    resumeAudio();
    if (playingRef.current) return;
    // make sure all strips exist before scheduling
    channelsRef.current.forEach((ch, i) => {
      ensureChannelStrip(`ch${i}`, {
        vol: (ch.vol / 100) * 0.85,
        pan: (ch.pan || 0) / 100,
        mute: ch.mute,
      });
    });
    playingRef.current = true;
    setPlaying(true);
    stepIdxRef.current = 0;
    setStepIndex(0);
    nextStepTimeRef.current = now() + 0.05;
    timerRef.current = setInterval(scheduler, 25);
  }

  function stopPlayback() {
    playingRef.current = false;
    setPlaying(false);
    if (timerRef.current) clearInterval(timerRef.current);
    timerRef.current = null;
    stepIdxRef.current = 0;
    setStepIndex(0);
  }

  // ============================================================
  // Sprint 3: MIDI input — play synth via external keyboard
  // ============================================================
  useMidiInput(useCallback(({ note, velocity, on }) => {
    if (!on) return;
    initAudio();
    resumeAudio();
    const dest = getChannelInput('ch_midi');
    ensureChannelStrip('ch_midi', { vol: 0.85, pan: 0, mute: false, reverbSend: 0.15 });
    playSynth(freqFromMidi(note), now() + 0.001, {
      waveforms: ['sawtooth','square','sine'],
      detune: [0, 7, -7],
      gains: [0.4, 0.25, 0.15],
      attack: 0.01, decay: 0.15, sustain: 0.6, release: 0.25,
      filterCutoff: 3000, filterQ: 2, filterEnv: 0.4,
      duration: 0.4, velocity: velocity * 0.7,
    }, dest);
  }, []));

  // ============================================================
  // Sprint 1: Drag-and-drop audio import
  // ============================================================
  const [dropping, setDropping] = useState(false);

  async function handleFilesDropped(files) {
    if (!files?.length) return;
    initAudio();
    const audioFiles = Array.from(files).filter(f => f.type.startsWith('audio/') || /\.(wav|mp3|ogg|flac|aiff?|m4a)$/i.test(f.name));
    if (!audioFiles.length) return;

    const newChannels = [];
    for (const file of audioFiles) {
      try {
        const buf = await file.arrayBuffer();
        await loadSample(file.name, buf);
        newChannels.push({
          name: file.name.replace(/\.[^.]+$/, '').slice(0, 18),
          color: '#f97316',
          type: file.name,            // sample lookup key
          steps: Array(16).fill(0),
          pan: 0, vol: 80, mute: false, solo: false,
          mixerTrack: Math.min(channelsRef.current.length, 15),
          rate: 1,
        });
      } catch (err) {
        console.error('Drop import failed for', file.name, err);
      }
    }
    if (newChannels.length) {
      setChannels(prev => [...prev, ...newChannels]);
      // also add to active pattern
      setPatterns(prev => prev.map((p, idx) => idx === currentPattern
        ? { ...p, channels: [...p.channels, ...newChannels.map(c => ({ steps: [...c.steps] }))] }
        : p
      ));
    }
  }

  // --- PATTERN / CHANNEL ---
  function handlePatternChange(idx) {
    setCurrentPattern(idx);
    const pat = patterns[idx];
    if (pat) {
      setChannels(prev => prev.map((c, i) => ({
        ...c, steps: pat.channels[i]?.steps ? [...pat.channels[i].steps] : [...c.steps]
      })));
    }
    // ensure pianoNotes array has entry for this pattern
    setPianoNotes(prev => {
      if (prev[idx]) return prev;
      const next = [...prev];
      while (next.length <= idx) next.push([]);
      return next;
    });
  }

  function handleNewPattern() {
    const newPat = { name: `Pattern ${patterns.length + 1}`, channels: channels.map(c => ({ steps: [...c.steps] })) };
    setPatterns([...patterns, newPat]);
    setPianoNotes(prev => [...prev, []]);
    setCurrentPattern(patterns.length);
  }

  function handleStepToggle(ci, si) {
    setChannels(prev => {
      const next = prev.map((c, i) => i === ci ? { ...c, steps: c.steps.map((s, j) => j === si ? (s ? 0 : 1) : s) } : c);
      return next;
    });
    setPatterns(prev => prev.map((p, pi) => pi === currentPattern ? {
      ...p, channels: p.channels.map((c, i) => i === ci ? { steps: c.steps.map((s, j) => j === si ? (s ? 0 : 1) : s) } : c)
    } : p));
  }

  function handleMute(ci) {
    setChannels(prev => prev.map((c, i) => i === ci ? { ...c, mute: !c.mute } : c));
  }

  function handleSolo(ci) {
    setChannels(prev => prev.map((c, i) => i === ci ? { ...c, solo: !c.solo } : c));
  }

  function handleVolChange(ci, val) {
    setChannels(prev => prev.map((c, i) => i === ci ? { ...c, vol: val } : c));
    setMixerTracks(prev => prev.map((t, i) => i === channels[ci].mixerTrack ? { ...t, vol: val } : t));
  }

  function handlePanChange(ci, val) {
    setChannels(prev => prev.map((c, i) => i === ci ? { ...c, pan: val } : c));
    setMixerTracks(prev => prev.map((t, i) => i === channels[ci].mixerTrack ? { ...t, pan: val } : t));
  }

  function handleAddChannel() {
    const types = ['kick','snare','hihat','hihat_open','clap','bass','lead','pad'];
    const names = {kick:'Kick',snare:'Snare',hihat:'Hihat',hihat_open:'Open Hat',clap:'Clap',bass:'Bass',lead:'Lead',pad:'Pad'};
    const type = types[channels.length % types.length];
    const newCh = {
      name: names[type] + ' ' + (channels.length + 1),
      color: `hsl(${(channels.length * 45) % 360},70%,50%)`,
      type, steps: Array(16).fill(0), pan: 0, vol: 70,
      mute: false, solo: false, mixerTrack: Math.min(channels.length, 15)
    };
    setChannels([...channels, newCh]);
    setPatterns(prev => prev.map(p => ({ ...p, channels: [...p.channels, { steps: Array(16).fill(0) }] })));
  }

  // --- MIXER ---
  function handleMixerVol(i, val) {
    setMixerTracks(prev => prev.map((t, j) => j === i ? { ...t, vol: val } : t));
  }

  function handleMixerPan(i, val) {
    setMixerTracks(prev => prev.map((t, j) => j === i ? { ...t, pan: val } : t));
  }

  function handleMixerMute(i) {
    setMixerTracks(prev => prev.map((t, j) => j === i ? { ...t, mute: !t.mute } : t));
  }

  function handleMixerSolo(i) {
    setMixerTracks(prev => prev.map((t, j) => j === i ? { ...t, solo: !t.solo } : t));
  }

  // --- PIANO ROLL ---
  function handleAddPianoNote(note) {
    initAudio();
    resumeAudio();
    setPianoNotes(prev => prev.map((notes, pi) => pi === currentPattern ? [...notes, note] : notes));
    const freq = freqFromMidi(note.note);
    if (freq) playTone(freq, now(), 0.2, 'square', 0.2);
  }

  function handleDeletePianoNote(idx) {
    setPianoNotes(prev => prev.map((notes, pi) => pi === currentPattern ? notes.filter((_, i) => i !== idx) : notes));
  }

  function handleUpdatePianoNote(idx, updated) {
    setPianoNotes(prev => prev.map((notes, pi) => pi === currentPattern ? notes.map((n, i) => i === idx ? { ...n, ...updated } : n) : notes));
  }

  // --- PLAYLIST ---
  function handleAddPlaylistBlock(trackIdx, start) {
    setPlaylistBlocks(prev => prev.map((blocks, i) => i === trackIdx ? [...blocks, { pattern: currentPattern, start, length: 4, track: trackIdx }] : blocks));
  }

  function handleDeletePlaylistBlock(trackIdx, blockIdx) {
    setPlaylistBlocks(prev => prev.map((blocks, i) => i === trackIdx ? blocks.filter((_, j) => j !== blockIdx) : blocks));
  }

  function handleMovePlaylistBlock(trackIdx, blockIdx, updates) {
    setPlaylistBlocks(prev => prev.map((blocks, i) => i === trackIdx ? blocks.map((b, j) => j === blockIdx ? { ...b, ...updates } : b) : blocks));
  }

  const showPiano = activePanel === 'pianoRoll';
  const showMixer = activePanel === 'mixerPanel';

  function handleDeleteChannel(ci) {
    setChannels(prev => prev.filter((_, i) => i !== ci));
    setPatterns(prev => prev.map(p => ({
      ...p,
      channels: p.channels.filter((_, i) => i !== ci),
    })));
  }

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100vh', background: '#1a1a1a', position: 'relative' }}>
      <TitleBar projectName={projectName} />
      <MenuBar
        onNew={handleNew}
        onSave={handleSave}
        onOpen={() => setShowProjects(true)}
        onUndo={undoChannels}
        onRedo={redoChannels}
        onToggleChannelRack={() => setShowChannelRack(v => !v)}
        onTogglePiano={() => setActivePanel(p => p === 'pianoRoll' ? 'channelRack' : 'pianoRoll')}
        onToggleMixer={() => setActivePanel(p => p === 'mixerPanel' ? 'channelRack' : 'mixerPanel')}
        onTogglePlay={togglePlay}
        onStop={stopPlayback}
        bpm={bpm}
        setBpm={setBpm}
        playing={playing}
        onAddChannel={handleAddChannel}
        onNewPattern={handleNewPattern}
        onShowPluginBrowser={() => setShowPluginBrowser(v => !v)}
      />
      {showProjects && (
        <ProjectManager
          onSelect={loadProject}
          onClose={() => setShowProjects(false)}
          onNew={handleNewProject}
        />
      )}


      <Toolbar
        bpm={bpm} setBpm={setBpm}
        playing={playing} onPlay={togglePlay} onStop={stopPlayback}
        recording={recording} onRecord={() => setRecording(!recording)}
        currentPattern={currentPattern} patterns={patterns}
        onPatternChange={handlePatternChange} onNewPattern={handleNewPattern}
        onSave={handleSave} onShowProjects={() => setShowProjects(true)}
        projectName={projectName}
        activePanel={activePanel}
        onTogglePiano={() => setActivePanel(p => p === 'pianoRoll' ? 'channelRack' : 'pianoRoll')}
        onToggleMixer={() => setActivePanel(p => p === 'mixerPanel' ? 'channelRack' : 'mixerPanel')}
        onToggleChannelRack={() => setShowChannelRack(v => !v)}
        onShowPluginBrowser={() => setShowPluginBrowser(v => !v)}
      />

      <div
        className={`workspace ${dropping ? 'drop-active' : ''}`}
        onDragOver={(e) => { e.preventDefault(); setDropping(true); }}
        onDragLeave={(e) => { if (e.currentTarget === e.target) setDropping(false); }}
        onDrop={(e) => {
          e.preventDefault();
          setDropping(false);
          handleFilesDropped(e.dataTransfer.files);
        }}
      >
        <div className="browser-sidebar">
          <Browser />
          <PluginBrowser
            onChannelAdd={(newCh) => {
              setChannels(prev => [...prev, newCh], { commit: true });
              setPatterns(prev => prev.map((p, idx) => idx === currentPattern
                ? { ...p, channels: [...p.channels, { steps: [...newCh.steps] }] }
                : p
              ));
            }}
          />
        </div>

        <div className="pattern-panel">
          <div className="pattern-panel-toolbar">
            <button>▶</button>
            <button>◌</button>
            <span>All</span>
          </div>
          <div className="pattern-list">
            {patterns.map((pattern, index) => (
              <div
                key={index}
                className={`pattern-list-item ${index === currentPattern ? 'active' : ''}`}
                onClick={() => handlePatternChange(index)}
              >
                <span className="pattern-arrow">▶</span>
                {pattern.name}
              </div>
            ))}
          </div>
          <button className="pattern-add" onClick={handleNewPattern}>+</button>
        </div>

        <div className="main-area">
          <Playlist
            playlistBlocks={playlistBlocks}
            patterns={patterns}
            onAddBlock={handleAddPlaylistBlock}
            onDeleteBlock={handleDeletePlaylistBlock}
            onMoveBlock={handleMovePlaylistBlock}
            zoom={zoomPl}
            snap={plSnap}
            playing={playing}
            bpm={bpm}
          />

          {showChannelRack && (
            <DraggableWindow
              title="Channel Rack"
              defaultPos={{ x: 32, y: 200 }}
              defaultSize={{ w: 860, h: 220 }}
              onClose={() => setShowChannelRack(false)}
            >
              <ChannelRack
                channels={channels}
                onStepToggle={handleStepToggle}
                onMute={handleMute}
                onSolo={handleSolo}
                onVolChange={handleVolChange}
                onPanChange={handlePanChange}
                onAddChannel={handleAddChannel}
                onDeleteChannel={handleDeleteChannel}
                playing={playing}
                stepIndex={stepIndex}
                currentPattern={currentPattern}
                patterns={patterns}
                onPatternChange={handlePatternChange}
                onNewPattern={handleNewPattern}
                onOpenPiano={() => setActivePanel('pianoRoll')}
              />
            </DraggableWindow>
          )}
        </div>

        <div className="right-sidebar">
          <Mixer
            channels={channels}
            onVolChange={handleVolChange}
            onPanChange={handlePanChange}
            onMute={handleMute}
            onSolo={handleSolo}
          />
        </div>
      </div>

      {showPiano && (
        <div className="piano-roll-overlay">
          <PianoRoll
            pianoNotes={pianoNotes[currentPattern] || []}
            onAddNote={handleAddPianoNote}
            onDeleteNote={handleDeletePianoNote}
            onUpdateNote={handleUpdatePianoNote}
            zoom={zoomPr}
            snap={prSnap}
            playing={playing}
            bpm={bpm}
            onClose={() => setActivePanel('channelRack')}
          />
        </div>
      )}

      {showMixer && (
        <div className="mixer-overlay">
          <Mixer
            tracks={mixerTracks}
            onVolChange={handleMixerVol}
            onPanChange={handleMixerPan}
            onMute={handleMixerMute}
            onSolo={handleMixerSolo}
            onClose={() => setActivePanel('channelRack')}
          />
        </div>
      )}
    </div>
  );
}
