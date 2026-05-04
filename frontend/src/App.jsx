import { useState, useEffect, useRef, useCallback } from 'react';
import { initAudio, resumeAudio, unlockAudio, stopAllAudio, now, playKick, playSnare, playHihat, playClap, playTone, playSampleAt, playSynth, freqFromMidi, setMasterVolume, renderWAV, ensureChannelStrip, getChannelInput, disposeChannelStrip, loadSample, hasSample, setMasterReverbWet } from './audio.js';
import { useMidiInput } from './hooks/useMidiInput.js';
import { useUndoableState } from './hooks/useUndoableState.js';
import { wamNoteOn, wamNoteOff, getLoadedWams, wamShowGui } from './wam/WamLoader.js';
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
import AiPanel from './components/AiPanel.jsx';

const notes = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];

const MOTIVATION_VIDEOS = [
  { title: 'Lo-Fi Hip Hop Radio', url: 'https://www.youtube.com/embed/jfKfPfyJRdk?autoplay=1' },
  { title: 'Beat Making Chill', url: 'https://www.youtube.com/embed/5qap5aO4i9A?autoplay=1' },
  { title: 'Dark Trap Beats', url: 'https://www.youtube.com/embed/kgx4WGK0oNU?autoplay=1' },
];

function VideoPlayerPanel() {
  const [url, setUrl] = useState(MOTIVATION_VIDEOS[0].url);
  const [customUrl, setCustomUrl] = useState('');
  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', background: '#09090b' }}>
      <div style={{ display: 'flex', gap: 4, padding: '4px 6px', borderBottom: '1px solid #27272a', flexShrink: 0, flexWrap: 'wrap' }}>
        {MOTIVATION_VIDEOS.map(v => (
          <button key={v.url} className="tool-btn" style={{ fontSize: 9 }} onClick={() => setUrl(v.url)}>{v.title}</button>
        ))}
        <input
          placeholder="YouTube embed URL…"
          value={customUrl}
          onChange={e => setCustomUrl(e.target.value)}
          onKeyDown={e => { if (e.key === 'Enter' && customUrl) setUrl(customUrl); }}
          style={{ flex: 1, background: '#18181b', border: '1px solid #3f3f46', color: '#d4d4d8', borderRadius: 4, padding: '2px 6px', fontSize: 10, minWidth: 120 }}
        />
      </div>
      <iframe
        src={url}
        style={{ flex: 1, border: 'none', width: '100%' }}
        allow="autoplay; encrypted-media"
        allowFullScreen
        title="Video Player"
      />
    </div>
  );
}

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
  const arr = Array.from({length:32},()=>[]);
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
  const [showPatternPanel, setShowPatternPanel] = useState(true);
  const [showVideoPlayer, setShowVideoPlayer] = useState(false);

  const [bpm, setBpm] = useState(130);
  const [playing, setPlaying] = useState(false);
  const [recording, setRecording] = useState(false);
  const [currentPattern, setCurrentPattern] = useState(0);
  const [stepIndex, setStepIndex] = useState(0);
  const [transportTime, setTransportTime] = useState(0);
  const [channels, setChannels, { undo: undoChannels, redo: redoChannels }] = useUndoableState(makeDefaultChannels());
  const [patterns, setPatterns] = useState([{ name: 'Pattern 1', channels: makeDefaultChannels().map(c=>({steps:[...c.steps]})) }]);
  const [mixerTracks, setMixerTracks] = useState(makeDefaultMixerTracks());
  const [playlistBlocks, setPlaylistBlocks] = useState(makeDefaultPlaylistBlocks());
  const [pianoNotes, setPianoNotes] = useState([makeDefaultPianoNotes()]);
  const [channelPianoNotes, setChannelPianoNotes] = useState({});
  const [selectedChannelForPiano, setSelectedChannelForPiano] = useState(null);
  const [showChannelPiano, setShowChannelPiano] = useState(false);
  const [zoomPr, setZoomPr] = useState(1);
  const [zoomPl, setZoomPl] = useState(1);
  const [prSnap, setPrSnap] = useState('1/8');
  const [plSnap, setPlSnap] = useState('1/4');
  const [currentFile, setCurrentFile] = useState(null);
  const [projectName, setProjectName] = useState('Untitled');
  const [showAiPanel, setShowAiPanel] = useState(false);
  const [masterVol, setMasterVol] = useState(80);
  const [showPluginBrowser, setShowPluginBrowser] = useState(false);
  const [pluginBrowserTarget, setPluginBrowserTarget] = useState(null); // { trackIdx, slotIdx, plugin }
  const [mixerLoadedPlugins, setMixerLoadedPlugins] = useState({}); // { trackIdx_slotIdx: { name, type, path, slotId } }
  const [activePluginGui, setActivePluginGui] = useState(null); // { slotId, name, type }
  const pluginGuiContainerRef = useRef(null);
  const tapTimesRef = useRef([]);
  const [consoleLogs, setConsoleLogs] = useState([]);
  const [showConsolePanel, setShowConsolePanel] = useState(false);

  // Intercept console.log to capture logs
  useEffect(() => {
    const originalLog = console.log;
    const originalError = console.error;
    const originalWarn = console.warn;

    const addLog = (type, args) => {
      const message = args.map(arg => 
        typeof arg === 'object' ? JSON.stringify(arg, null, 2) : String(arg)
      ).join(' ');
      const timestamp = new Date().toLocaleTimeString();
      setConsoleLogs(prev => [...prev.slice(-49), { type, message, timestamp }]);
    };

    console.log = (...args) => {
      originalLog(...args);
      addLog('log', args);
    };
    console.error = (...args) => {
      originalError(...args);
      addLog('error', args);
    };
    console.warn = (...args) => {
      originalWarn(...args);
      addLog('warn', args);
    };

    return () => {
      console.log = originalLog;
      console.error = originalError;
      console.warn = originalWarn;
    };
  }, []);

  const nextStepTimeRef = useRef(0);
  const stepIdxRef = useRef(0);
  const timerRef = useRef(null);
  const transportTimerRef = useRef(null);
  const transportStartTimeRef = useRef(0);
  const playingRef = useRef(false);
  const channelsRef = useRef(channels);
  const bpmRef = useRef(bpm);
  const mixerTracksRef = useRef(mixerTracks);
  const channelPianoNotesRef = useRef(channelPianoNotes);
  const currentPatternRef = useRef(currentPattern);
  const togglePlayRef = useRef(null);
  const stopPlaybackRef = useRef(null);
  const playlistBlocksRef = useRef(playlistBlocks);
  const patternsRef = useRef(patterns);

  const hasAPI = typeof window !== 'undefined' && window.electronAPI;

  useEffect(() => { channelsRef.current = channels; }, [channels]);
  useEffect(() => { bpmRef.current = bpm; }, [bpm]);
  useEffect(() => { playlistBlocksRef.current = playlistBlocks; }, [playlistBlocks]);
  useEffect(() => { patternsRef.current = patterns; }, [patterns]);
  useEffect(() => { mixerTracksRef.current = mixerTracks; }, [mixerTracks]);
  useEffect(() => { channelPianoNotesRef.current = channelPianoNotes; }, [channelPianoNotes]);
  useEffect(() => { currentPatternRef.current = currentPattern; }, [currentPattern]);

  // Render WAM plugin GUI when activePluginGui changes
  useEffect(() => {
    if (!activePluginGui || activePluginGui.type !== 'wam') return;

    console.log('Attempting to render plugin GUI:', activePluginGui);

    // Check if plugin exists in wamInstances
    const loaded = getLoadedWams();
    const plugin = loaded.find(p => p.slotId === activePluginGui.slotId);
    console.log('Loaded WAMs:', loaded, 'Looking for slot:', activePluginGui.slotId, 'Found:', plugin);

    if (!plugin) {
      console.error('Plugin not found in wamInstances:', activePluginGui.slotId);
      const container = document.getElementById(`plugin-gui-${activePluginGui.slotId}`);
      if (container) {
        container.innerHTML = `<div style="color:#ef4444;padding:20px;text-align:center">
          <div style="font-weight:bold;margin-bottom:10px">Plugin not loaded</div>
          <div style="font-size:11px;color:#71717a">Slot ID: ${activePluginGui.slotId}</div>
          <div style="font-size:10px;color:#52525b;margin-top:10px">Loaded plugins: ${loaded.map(p => p.slotId).join(', ')}</div>
        </div>`;
      }
      return;
    }

    // Use requestAnimationFrame to ensure the DOM is updated
    requestAnimationFrame(() => {
      requestAnimationFrame(() => {
        const containerId = `plugin-gui-${activePluginGui.slotId}`;
        console.log('Looking for container:', containerId);
        const container = document.getElementById(containerId);
        if (!container) {
          console.error('Plugin GUI container not found:', containerId);
          return;
        }
        console.log('Container found, rendering GUI for slotId:', activePluginGui.slotId);
        // Clear previous content
        container.innerHTML = '';
        // Use wamShowGui to render the plugin
        try {
          wamShowGui(activePluginGui.slotId, container);
          console.log('GUI rendered successfully');
        } catch (err) {
          console.error('Failed to render plugin GUI:', err);
          container.innerHTML = `<div style="color:#ef4444;padding:20px;text-align:center">
            <div style="font-weight:bold;margin-bottom:10px">Failed to load plugin GUI</div>
            <div style="font-size:11px;color:#71717a">${err.message}</div>
            <div style="font-size:10px;color:#52525b;margin-top:10px">Slot ID: ${activePluginGui.slotId}</div>
          </div>`;
        }
      });
    });
  }, [activePluginGui]);

  // --- MCP SSE live command listener ---
  useEffect(() => {
    const es = new EventSource('http://localhost:3002/api/daw/events');
    es.onmessage = (e) => {
      try {
        const cmd = JSON.parse(e.data);
        if (cmd.type === 'connected') return;
        if (cmd.type === 'play') { initAudio(); resumeAudio(); if (!playingRef.current) togglePlayRef.current?.(); }
        else if (cmd.type === 'stop') { if (playingRef.current) stopPlaybackRef.current?.(); }
        else if (cmd.type === 'set_bpm') setBpm(cmd.bpm);
        else if (cmd.type === 'set_step') {
          setChannels(prev => prev.map((ch, i) => {
            if (i !== cmd.channelIndex) return ch;
            const steps = [...ch.steps];
            while (steps.length <= cmd.step) steps.push(0);
            steps[cmd.step] = cmd.on ? 1 : 0;
            return { ...ch, steps };
          }));
        } else if (cmd.type === 'set_pattern_steps') {
          setChannels(prev => prev.map((ch, i) => i === cmd.channelIndex ? { ...ch, steps: cmd.steps } : ch));
        } else if (cmd.type === 'set_channel_volume') {
          setChannels(prev => prev.map((ch, i) => i === cmd.channelIndex ? { ...ch, vol: cmd.volume } : ch));
        } else if (cmd.type === 'add_piano_note') {
          const key = `${currentPatternRef.current}_${cmd.channelIndex}`;
          setChannelPianoNotes(prev => ({ ...prev, [key]: [...(prev[key] || []), { note: cmd.note, start: cmd.start, length: cmd.length, vel: cmd.vel ?? 80 }] }));
        } else if (cmd.type === 'clear_piano_notes') {
          const key = `${currentPatternRef.current}_${cmd.channelIndex}`;
          setChannelPianoNotes(prev => ({ ...prev, [key]: [] }));
        } else if (cmd.type === 'set_pattern') {
          if (cmd.bpm) setBpm(cmd.bpm);
          if (cmd.channels) {
            setChannels(prev => {
              const next = [...prev];
              cmd.channels.forEach((cmdCh, i) => {
                const existing = next.findIndex(c => c.name.toLowerCase() === cmdCh.name.toLowerCase());
                if (existing >= 0) {
                  next[existing] = { ...next[existing], steps: cmdCh.steps };
                } else if (i < next.length) {
                  next[i] = { ...next[i], steps: cmdCh.steps };
                }
              });
              return next;
            });
          }
        }
      } catch (_) {}
    };
    es.onerror = () => {
      // EventSource auto-reconnects; log silently
      console.debug('[MCP] SSE disconnected, will auto-reconnect…');
    };
    // Push live state every 3s
    const pushState = () => {
      fetch('http://localhost:3002/api/daw/state', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ bpm: bpmRef.current, playing: playingRef.current, channels: channelsRef.current }),
      }).catch(() => {});
    };
    const stateInterval = setInterval(pushState, 3000);
    pushState();
    return () => { es.close(); clearInterval(stateInterval); };
  }, []);

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

  // --- Prevent Electron navigation on drag/drop (blank screen fix) ---
  useEffect(() => {
    const prevent = (e) => { e.preventDefault(); };
    window.addEventListener('dragover', prevent);
    window.addEventListener('drop', prevent);
    return () => {
      window.removeEventListener('dragover', prevent);
      window.removeEventListener('drop', prevent);
    };
  }, []);

  // --- Keyboard shortcuts (FL Studio style) ---
  useEffect(() => {
    const handler = (e) => {
      const tag = document.activeElement?.tagName;
      const inInput = tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT';

      // Always-on shortcuts (work even in inputs)
      if (e.code === 'Space' && !inInput) { e.preventDefault(); togglePlay(); return; }
      if (e.key === 'Escape') { e.preventDefault(); if (playingRef.current) stopPlaybackRef.current?.(); return; }

      if (inInput) return; // Don't intercept other keys when typing

      // --- Transport ---
      if (e.key === 'Enter') { togglePlay(); }                              // FL: Enter = Play/Pause
      if (e.key === 'L' || e.key === 'l') { togglePlay(); }                // FL: L = Play (alt)

      // --- Panel / View shortcuts (FL uses F5-F9) ---
      if (e.key === 'F5') { e.preventDefault(); setActivePanel('channelRack'); }
      if (e.key === 'F6') { e.preventDefault(); setActivePanel('channelRack'); } // Channel Rack
      if (e.key === 'F7') { e.preventDefault(); setActivePanel('pianoRoll'); }   // Piano Roll
      if (e.key === 'F8') { e.preventDefault(); setShowProjects(true); }         // Project browser
      if (e.key === 'F9') { e.preventDefault(); setActivePanel('mixerPanel'); }  // Mixer
      if (e.key === 'F4') { e.preventDefault(); setShowAiPanel(v => !v); }       // AI panel

      // Number row — match FL Studio quick-panel
      if (e.key === '1' && !e.ctrlKey) setActivePanel('channelRack');
      if (e.key === '2' && !e.ctrlKey) setActivePanel('pianoRoll');
      if (e.key === '4' && !e.ctrlKey) setActivePanel('mixerPanel');

      // --- BPM nudge ---
      if (e.key === '+' || e.key === '=') setBpm(b => Math.min(300, b + (e.shiftKey ? 10 : 1)));
      if (e.key === '-') setBpm(b => Math.max(20, b - (e.shiftKey ? 10 : 1)));

      // --- File / project ---
      if ((e.ctrlKey || e.metaKey) && e.key === 's') { e.preventDefault(); handleSave(); }
      if ((e.ctrlKey || e.metaKey) && e.key === 'o') { e.preventDefault(); setShowProjects(true); }
      if ((e.ctrlKey || e.metaKey) && e.key === 'n') { e.preventDefault(); handleNew(); }
      if ((e.ctrlKey || e.metaKey) && e.key === 'e') { e.preventDefault(); handleExportWAV(); }

      // --- Undo / Redo ---
      if ((e.ctrlKey || e.metaKey) && !e.shiftKey && e.key === 'z') { e.preventDefault(); undoChannels(); }
      if ((e.ctrlKey || e.metaKey) && (e.shiftKey ? e.key === 'Z' : e.key === 'y')) { e.preventDefault(); redoChannels(); }

      // --- Record ---
      if (e.key === 'r' || e.key === 'R') setRecording(v => !v);

      // --- Channel rack pattern cycling ---
      if (e.key === 'ArrowLeft' && e.ctrlKey) setCurrentPattern(p => Math.max(0, p - 1));
      if (e.key === 'ArrowRight' && e.ctrlKey) setCurrentPattern(p => Math.min(patterns.length - 1, p + 1));

      // --- Toggle panels (Ctrl+M, Ctrl+P, Ctrl+T) ---
      if ((e.ctrlKey || e.metaKey) && e.key === 'm') { e.preventDefault(); setActivePanel(p => p === 'mixerPanel' ? 'channelRack' : 'mixerPanel'); }
      if ((e.ctrlKey || e.metaKey) && e.key === 'p') { e.preventDefault(); setActivePanel(p => p === 'pianoRoll' ? 'channelRack' : 'pianoRoll'); }
      if ((e.ctrlKey || e.metaKey) && e.key === 't') { e.preventDefault(); setShowChannelRack(v => !v); }

      // --- Zoom playlist ---
      if (e.key === 'ArrowUp' && e.ctrlKey) { e.preventDefault(); setZoomPl(z => Math.min(4, +(z + 0.25).toFixed(2))); }
      if (e.key === 'ArrowDown' && e.ctrlKey) { e.preventDefault(); setZoomPl(z => Math.max(0.25, +(z - 0.25).toFixed(2))); }
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
      const arr = Array.from({length:32},()=>[]);
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
        const BACKEND = 'http://localhost:4002/api';
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

  // --- Keyboard shortcuts ---
  useEffect(() => {
    const onKey = (e) => {
      // Ignore when typing in inputs
      if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
      if ((e.ctrlKey || e.metaKey) && !e.shiftKey && e.key === 'z') {
        e.preventDefault();
        undoChannels();
      }
      if ((e.ctrlKey || e.metaKey) && (e.key === 'y' || (e.shiftKey && e.key === 'z'))) {
        e.preventDefault();
        redoChannels();
      }
      if (e.key === ' ') {
        e.preventDefault();
        if (playingRef.current) stopPlayback();
        else startPlayback();
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
  togglePlayRef.current = togglePlay;

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

  function scheduleStep(stepNumber, time, patternOverride = null) {
    const baseChs = channelsRef.current;
    let chs = baseChs;
    // If a pattern override is given, merge pattern steps into current channels
    if (patternOverride != null) {
      const pat = patternsRef.current[patternOverride];
      if (pat) {
        chs = baseChs.map((c, i) => ({
          ...c,
          steps: pat.channels[i]?.steps ?? c.steps,
        }));
      }
    }
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

    // Per-channel piano notes: fire notes that land on this step (16th-note grid)
    const spb = 60.0 / bpmRef.current;
    const stepBeat = stepNumber * 0.25; // beat position of this step
    const patIdx = patternOverride ?? currentPatternRef.current;
    const cpn = channelPianoNotesRef.current;
    chs.forEach((ch, ci) => {
      if (ch.mute) return;
      const notes = cpn[`${patIdx}_${ci}`] || [];
      notes.forEach(n => {
        if (n.start >= stepBeat && n.start < stepBeat + 0.25) {
          const noteTime = time + (n.start - stepBeat) * spb;
          const noteDur = (n.length || 0.25) * spb;
          const dest = getChannelInput(`ch${ci}`);
          const vel = (n.vel ?? 80) / 100;
          const mt = mixerTracksRef.current[ch.mixerTrack];
          const mx = mt ? mt.vol / 100 : 1;
          // Play using channel type at the note's pitch
          switch (ch.type) {
            case 'kick': playKick(noteTime, vel * mx, dest); break;
            case 'snare': playSnare(noteTime, vel * mx, dest); break;
            case 'hihat': playHihat(noteTime, false, vel * mx, dest); break;
            case 'hihat_open': playHihat(noteTime, true, vel * mx, dest); break;
            case 'clap': playClap(noteTime, vel * mx, dest); break;
            case 'piano': {
              playSynth(freqFromMidi(n.note), noteTime, {
                waveforms: ['triangle','sine'], detune: [0, 4], gains: [0.45, 0.35],
                attack: 0.002, decay: 0.6, sustain: 0.2, release: 0.4,
                duration: noteDur, velocity: vel * 0.8,
              }, dest);
              break;
            }
            default:
              if (hasSample(ch.type)) {
                playSampleAt(ch.type, noteTime, vel * mx, 1, dest);
              } else {
                playSynth(freqFromMidi(n.note), noteTime, {
                  waveforms: ['sawtooth', 'sine'], detune: [0, 7], gains: [0.4, 0.2],
                  attack: 0.005, decay: 0.1, sustain: 0.5, release: 0.1,
                  duration: noteDur, velocity: vel * 0.7,
                }, dest);
              }
          }
        }
      });
    });
  }

  function nextNote() {
    const secondsPerBeat = 60.0 / bpmRef.current;
    nextStepTimeRef.current += 0.25 * secondsPerBeat;
    const maxSteps = Math.max(16, ...channelsRef.current.map(c => c.steps?.length ?? 16));
    stepIdxRef.current = (stepIdxRef.current + 1) % maxSteps;
    setStepIndex(stepIdxRef.current);
  }

  // Lookahead scheduler — runs every 25 ms, schedules ahead by 100 ms.
  // setInterval keeps running when the tab is backgrounded (rAF doesn't),
  // so playback stays tight even when you tab away.
  function scheduler() {
    while (nextStepTimeRef.current < now() + 0.1) {
      const beat = stepIdxRef.current * 0.25;
      // Always schedule current pattern (channel rack view)
      scheduleStep(stepIdxRef.current, nextStepTimeRef.current);
      // Also schedule any playlist blocks active at this beat
      const blocks = playlistBlocksRef.current;
      blocks.forEach((trackBlocks) => {
        trackBlocks.forEach(blk => {
          if (blk.pattern != null && beat >= blk.start && beat < blk.start + blk.length) {
            const blockStep = Math.floor((beat - blk.start) / 0.25) % 16;
            scheduleStep(blockStep, nextStepTimeRef.current, blk.pattern);
          }
          // Handle audio clip playback
          if (blk.sampleName && !blk.pattern) {
            const clipId = `${blk.sampleName}_${blk.track}_${blk.start}`;
            // Trigger at the start of the clip
            if (beat >= blk.start && beat < blk.start + 0.25 && !scheduledAudioClips.current.has(clipId)) {
              scheduledAudioClips.current.add(clipId);
              initAudio();
              if (hasSample(blk.sampleName)) {
                playSampleAt(blk.sampleName, nextStepTimeRef.current, 0.8, 1);
              } else if (blk.samplePath) {
                // Load during playback if not loaded yet
                window.electronAPI?.invoke('fs:readBinaryFile', blk.samplePath).then(result => {
                  if (result && result.data) {
                    const arrayBuffer = result.data.buffer || result.data;
                    loadSample(blk.sampleName, arrayBuffer).then(() => {
                      playSampleAt(blk.sampleName, nextStepTimeRef.current, 0.8, 1);
                    }).catch(() => {});
                  }
                }).catch(() => {});
              }
            }
            // Clear scheduled clip when we pass it
            if (beat >= blk.start + blk.length) {
              scheduledAudioClips.current.delete(clipId);
            }
          }
        });
      });
      nextNote();
    }
  }

  const scheduledAudioClips = useRef(new Set());

  async function startPlayback() {
    initAudio();
    await unlockAudio(); // aggressively unlock — needed for JUCE WebView
    if (playingRef.current) return;
    channelsRef.current.forEach((ch, i) => {
      ensureChannelStrip(`ch${i}`, {
        vol: (ch.vol / 100) * 0.85,
        pan: (ch.pan || 0) / 100,
        mute: ch.mute,
      });
    });
    scheduledAudioClips.current.clear();
    playingRef.current = true;
    setPlaying(true);
    stepIdxRef.current = 0;
    setStepIndex(0);
    nextStepTimeRef.current = now() + 0.05;
    transportStartTimeRef.current = now(); // AudioContext clock — no drift
    setTransportTime(0);
    if (transportTimerRef.current) clearInterval(transportTimerRef.current);
    transportTimerRef.current = setInterval(() => {
      const elapsed = now() - transportStartTimeRef.current;
      setTransportTime(Math.max(0, elapsed));
      // Schedule any playlist audio clips whose start time is approaching
      const spb = 60.0 / bpmRef.current;
      playlistBlocksRef.current.forEach((blocks) => {
        blocks.forEach((blk) => {
          if (!blk.sampleName) return;
          const clipId = `${blk.sampleName}_${blk.start}`;
          if (scheduledAudioClips.current.has(clipId)) return;
          const clipStartSec = blk.start * spb;
          const audioStartTime = transportStartTimeRef.current + clipStartSec;
          const lookahead = 0.2;
          if (audioStartTime <= now() + lookahead && audioStartTime >= now() - 0.05) {
            scheduledAudioClips.current.add(clipId);
            playSampleAt(blk.sampleName, Math.max(now(), audioStartTime), 0.8, 1);
          }
        });
      });
    }, 50);
    timerRef.current = setInterval(scheduler, 25);
  }

  function stopPlayback() {
    playingRef.current = false;
    setPlaying(false);
    if (timerRef.current) clearInterval(timerRef.current);
    if (transportTimerRef.current) clearInterval(transportTimerRef.current);
    timerRef.current = null;
    transportTimerRef.current = null;
    stepIdxRef.current = 0;
    setStepIndex(0);
    setTransportTime(0);
    stopAllAudio(); // kill all scheduled/playing sounds immediately
    scheduledAudioClips.current.clear(); // Clear scheduled audio clips
  }
  stopPlaybackRef.current = stopPlayback;

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

  function handleFilesDropped(files) {
    if (!files?.length) return;
    initAudio();
    const audioFiles = Array.from(files).filter(f => f.type.startsWith('audio/') || /\.(wav|mp3|ogg|flac|aiff?|m4a)$/i.test(f.name));
    if (!audioFiles.length) return;

    setTimeout(async () => {
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
    }, 50);
  }

  // --- Per-channel piano notes helpers ---
  function getChannelNotes(patIdx, chIdx) {
    return channelPianoNotes[`${patIdx}_${chIdx}`] || [];
  }
  function setChannelNotes(patIdx, chIdx, notes) {
    setChannelPianoNotes(prev => ({ ...prev, [`${patIdx}_${chIdx}`]: notes }));
  }

  // Sync a note list back to the 16/32-step grid for the given channel
  function syncNotesToSteps(ci, notes) {
    const ch = channels[ci];
    if (!ch) return;
    const stepCount = ch.steps?.length ?? 16;
    const newSteps = Array(stepCount).fill(0);
    notes.forEach(n => {
      const si = Math.round(n.start / 0.25);
      if (si >= 0 && si < stepCount) newSteps[si] = 1;
    });
    setChannels(prev => prev.map((c, i) => i === ci ? { ...c, steps: newSteps } : c));
    setPatterns(prev => prev.map((p, pi) => pi === currentPattern
      ? { ...p, channels: p.channels.map((c, i) => i === ci ? { steps: newSteps } : c) }
      : p));
  }

  function handleChannelAddNote(note) {
    if (selectedChannelForPiano === null) return;
    const ci = selectedChannelForPiano;
    const updated = [...getChannelNotes(currentPattern, ci), note];
    setChannelNotes(currentPattern, ci, updated);
    syncNotesToSteps(ci, updated);
  }
  function handleChannelDeleteNote(idx) {
    if (selectedChannelForPiano === null) return;
    const ci = selectedChannelForPiano;
    const updated = getChannelNotes(currentPattern, ci).filter((_, i) => i !== idx);
    setChannelNotes(currentPattern, ci, updated);
    syncNotesToSteps(ci, updated);
  }
  function handleChannelUpdateNote(idx, changes) {
    if (selectedChannelForPiano === null) return;
    const ci = selectedChannelForPiano;
    const updated = getChannelNotes(currentPattern, ci).map((n, i) => i === idx ? { ...n, ...changes } : n);
    setChannelNotes(currentPattern, ci, updated);
    syncNotesToSteps(ci, updated);
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
    const toggleStep = (steps) => {
      const arr = [...steps];
      while (arr.length <= si) arr.push(0);
      arr[si] = arr[si] ? 0 : 1;
      return arr;
    };
    setChannels(prev => {
      const next = prev.map((c, i) => i === ci ? { ...c, steps: toggleStep(c.steps) } : c);
      // Sync piano roll if it's open for this channel and has only step-seeded notes
      if (selectedChannelForPiano === ci) {
        const key = `${currentPattern}_${ci}`;
        const ch = next[ci];
        const existing = channelPianoNotesRef.current[key] || [];
        const hasUserNotes = existing.some(n => !n.id?.startsWith('step_'));
        if (!hasUserNotes) {
          const defaultNote = ch?.midiNote ?? 60;
          const stepNotes = (ch?.steps || []).map((on, s) =>
            on ? { id: `step_${s}`, note: defaultNote, start: s * 0.25, length: 0.25, vel: 80 } : null
          ).filter(Boolean);
          setChannelPianoNotes(p => ({ ...p, [key]: stepNotes }));
        }
      }
      return next;
    });
    setPatterns(prev => prev.map((p, pi) => pi === currentPattern ? {
      ...p, channels: p.channels.map((c, i) => i === ci ? { steps: toggleStep(c.steps) } : c)
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

  async function handleLoadSample(ci, { path, name }) {
    try {
      let arrayBuffer;
      if (window.electronAPI) {
        const result = await window.electronAPI.invoke('fs:readBinaryFile', path);
        if (result?.error) throw new Error(result.error);
        const bytes = result?.data instanceof Uint8Array ? result.data : new Uint8Array(result?.data ?? []);
        arrayBuffer = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
      } else {
        const fileUrl = 'file:///' + encodeURI(path.replaceAll('\\', '/')).replace(/#/g, '%23').replace(/\?/g, '%3F');
        const res = await fetch(fileUrl);
        if (!res.ok) throw new Error('Cannot fetch file: ' + res.statusText);
        arrayBuffer = await res.arrayBuffer();
      }
      await loadSample(name, arrayBuffer);
      setChannels(prev => prev.map((ch, i) => i === ci ? {
        ...ch,
        name: name.replace(/\.[^.]+$/, '').slice(0, 18),
        type: name,
        color: '#f97316',
      } : ch));
    } catch (err) {
      console.error('[App] load sample failed:', err);
      alert('Failed to load sample: ' + (err.message || err));
    }
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

  function handleMixerSlotClick(trackIdx, slotIdx, plugin) {
    setPluginBrowserTarget({ trackIdx, slotIdx, plugin });
    setShowPluginBrowser(true);
  }

  function handlePluginLoad(pluginData) {
    if (!pluginBrowserTarget) return;
    const { trackIdx, slotIdx } = pluginBrowserTarget;
    const key = `${trackIdx}_${slotIdx}`;
    setMixerLoadedPlugins(prev => ({ ...prev, [key]: pluginData }));
    setShowPluginBrowser(false);
    setPluginBrowserTarget(null);

    // Show plugin GUI after loading
    if (pluginData.type === 'wam') {
      setActivePluginGui({ slotId: pluginData.slotId, name: pluginData.name, type: 'wam' });
    } else if (pluginData.type === 'vst') {
      setActivePluginGui({ slotId: pluginData.slotId, name: pluginData.name, type: 'vst' });
      // For VST, tell JUCE to show the editor
      window.electronAPI?.vstCall?.('showEditor', { slotId: pluginData.slotId, show: true });
    }
  }

  function handleMixerPluginRemove(trackIdx, slotIdx) {
    const key = `${trackIdx}_${slotIdx}`;
    setMixerLoadedPlugins(prev => {
      const np = { ...prev };
      delete np[key];
      return np;
    });
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
  function handleAddPlaylistBlock(trackIdx, start, existingBlk) {
    setPlaylistBlocks(prev => prev.map((blocks, i) => i === trackIdx
      ? [...blocks, existingBlk ? { ...existingBlk, start, track: trackIdx } : { pattern: currentPattern, start, length: 4, track: trackIdx }]
      : blocks
    ));
  }

  function handleDeletePlaylistBlock(trackIdx, blockIdx) {
    setPlaylistBlocks(prev => prev.map((blocks, i) => i === trackIdx ? blocks.filter((_, j) => j !== blockIdx) : blocks));
  }

  function handleMovePlaylistBlock(trackIdx, blockIdx, updates) {
    setPlaylistBlocks(prev => prev.map((blocks, i) => i === trackIdx ? blocks.map((b, j) => j === blockIdx ? { ...b, ...updates } : b) : blocks));
  }

  async function handleAddAudioClip(trackIdx, clip) {
    initAudio();
    // Load the sample if it has a path
    if (clip.samplePath) {
      try {
        const result = await window.electronAPI?.invoke('fs:readBinaryFile', clip.samplePath);
        if (result && result.data) {
          // JUCE bridge already converts base64 to bytes (see WebBrowserHost.cpp lines 43-48)
          const arrayBuffer = result.data.buffer || result.data;
          await loadSample(clip.sampleName, arrayBuffer);
        }
      } catch (err) {
        console.error('Failed to load sample:', err);
      }
    }
    setPlaylistBlocks(prev => prev.map((blocks, i) => i === trackIdx
      ? [...blocks, { sampleName: clip.sampleName, samplePath: clip.samplePath, start: clip.start, length: clip.length, track: trackIdx }]
      : blocks
    ));
  }

  // Tap Tempo
  function handleTapTempo() {
    const now_ms = performance.now();
    const taps = tapTimesRef.current;
    taps.push(now_ms);
    if (taps.length > 8) taps.shift();
    if (taps.length >= 2) {
      const gaps = [];
      for (let i = 1; i < taps.length; i++) gaps.push(taps[i] - taps[i - 1]);
      const avgGap = gaps.reduce((a, b) => a + b, 0) / gaps.length;
      const tappedBpm = Math.round(60000 / avgGap);
      setBpm(Math.min(300, Math.max(20, tappedBpm)));
    }
  }

  // Master volume
  function handleMasterVol(val) {
    setMasterVol(val);
    setMasterVolume(val / 100);
  }

  // Export WAV
  async function handleExportWAV() {
    initAudio();
    renderWAV(transportTimeRef.current || 0, bpm);
  }

  function handleOpenDevTools() {
    window.electronAPI?.invoke('openDevTools');
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
      <TitleBar projectName={projectName}>
        <MenuBar
          compact
          hideLogo
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
      </TitleBar>
      {showProjects && (
        <ProjectManager
          onSelect={loadProject}
          onClose={() => setShowProjects(false)}
          onNew={handleNewProject}
        />
      )}


      <Toolbar
        bpm={bpm} setBpm={setBpm}
        transportTime={transportTime}
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
        onExport={handleExportWAV}
        onTapTempo={handleTapTempo}
        onOpenDevTools={handleOpenDevTools}
        onToggleConsole={() => setShowConsolePanel(v => !v)}
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
          <Browser channels={channels} onLoadSample={handleLoadSample} />
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

        <div className="pattern-panel" style={{ width: showPatternPanel ? undefined : 18, minWidth: showPatternPanel ? undefined : 18, overflow: 'hidden', transition: 'width .15s' }}>
          <div className="pattern-panel-toolbar" style={{ justifyContent: 'space-between' }}>
            {showPatternPanel && <>
              <button>▶</button>
              <button>◌</button>
              <span style={{flex:1}}>All</span>
            </>}
            <button
              title={showPatternPanel ? 'Collapse panel' : 'Expand panel'}
              style={{ background: 'none', border: 'none', color: '#71717a', cursor: 'pointer', fontSize: 10, padding: '0 2px' }}
              onClick={() => setShowPatternPanel(v => !v)}
            >{showPatternPanel ? '◀' : '▶'}</button>
          </div>
          {showPatternPanel && <>
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
          </>}
        </div>

        <div className="main-area">
          <Playlist
            playlistBlocks={playlistBlocks}
            patterns={patterns}
            onAddBlock={handleAddPlaylistBlock}
            onDeleteBlock={handleDeletePlaylistBlock}
            onMoveBlock={handleMovePlaylistBlock}
            onAddAudioClip={handleAddAudioClip}
            zoom={zoomPl}
            snap={plSnap}
            playing={playing}
            bpm={bpm}
            transportTime={transportTime}
          />
          <div className="bottom-dock">
            <div className="dock-panel dock-status-panel">
              <div className="dock-title">Session</div>
              <div className="dock-row">
                <span>Project</span>
                <strong>{projectName}</strong>
              </div>
              <div className="dock-row">
                <span>Pattern</span>
                <strong>{patterns[currentPattern]?.name || 'Pattern'}</strong>
              </div>
              <div className="dock-row">
                <span>Status</span>
                <strong className={playing ? 'dock-accent' : ''}>{playing ? 'Playing' : 'Stopped'}</strong>
              </div>
            </div>

            <div className="dock-panel dock-mixer-panel">
              <div className="dock-title">Mixer Preview</div>
              <div className="dock-mini-strips">
                {mixerTracks.slice(0, 8).map((track, i) => (
                  <button
                    key={i}
                    className={`dock-mini-strip ${track.mute ? 'muted' : ''} ${track.solo ? 'solo' : ''}`}
                    onClick={() => setActivePanel('mixerPanel')}
                    title={`${track.name} - open Mixer`}
                  >
                    <span className="dock-meter">
                      <span style={{ height: `${playing ? 18 + ((i * 13) % 58) : 6}%` }} />
                    </span>
                    <span className="dock-strip-name">{track.name}</span>
                  </button>
                ))}
              </div>
            </div>

            <div className="dock-panel dock-tools-panel">
              <div className="dock-title">Quick Tools</div>
              <div className="dock-actions">
                <button onClick={() => setActivePanel('mixerPanel')}>Mixer</button>
                <button onClick={() => setActivePanel('pianoRoll')}>Piano Roll</button>
                <button onClick={() => setShowChannelRack(true)}>Channel Rack</button>
                <button onClick={() => setShowPluginBrowser(v => !v)}>Plugins</button>
                <button onClick={() => setShowVideoPlayer(v => !v)}>📺 Video</button>
                <button onClick={() => setShowAiPanel(v => !v)} style={{ background: showAiPanel ? '#ff8c00' : undefined, color: showAiPanel ? '#000' : undefined }}>🤖 AI</button>
              </div>
            </div>
          </div>

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
                onLoadSample={handleLoadSample}
                playing={playing}
                stepIndex={stepIndex}
                currentPattern={currentPattern}
                patterns={patterns}
                onPatternChange={handlePatternChange}
                onNewPattern={handleNewPattern}
                onAddInstrument={(inst) => {
                  const newCh = {
                    name: inst.name,
                    color: inst.color || `hsl(${(channels.length * 45) % 360},70%,50%)`,
                    type: inst.type,
                    steps: Array(16).fill(0),
                    pan: 0, vol: 70, mute: false, solo: false,
                    mixerTrack: Math.min(channels.length, 15),
                    midiNote: 60,
                  };
                  setChannels(prev => [...prev, newCh]);
                  setPatterns(prev => prev.map(p => ({ ...p, channels: [...p.channels, { steps: Array(16).fill(0) }] })));
                }}
                onOpenPiano={(ci) => {
                  setSelectedChannelForPiano(ci);
                  // Always seed piano notes from steps so they stay in sync
                  const key = `${currentPattern}_${ci}`;
                  const ch = channels[ci];
                  const defaultNote = ch?.midiNote ?? 60;
                  const existing = channelPianoNotes[key] || [];
                  // Only seed if no user-placed notes exist (i.e., all are step-seeded or empty)
                  const hasUserNotes = existing.some(n => !n.id?.startsWith('step_'));
                  if (!hasUserNotes) {
                    const stepNotes = (ch?.steps || []).map((on, si) =>
                      on ? { id: `step_${si}`, note: defaultNote, start: si * 0.25, length: 0.25, vel: 80 } : null
                    ).filter(Boolean);
                    setChannelPianoNotes(prev => ({ ...prev, [key]: stepNotes }));
                  }
                  setShowChannelPiano(true);
                }}
              />
            </DraggableWindow>
          )}
        </div>
      </div>

      {/* Per-channel Piano Roll (FL Studio style — one per channel) */}
      {showChannelPiano && selectedChannelForPiano !== null && (
        <DraggableWindow
          title={`Piano Roll — ${channels[selectedChannelForPiano]?.name || `Ch ${selectedChannelForPiano + 1}`}`}
          defaultPos={{ x: 120, y: 80 }}
          defaultSize={{ w: 900, h: 420 }}
          onClose={() => setShowChannelPiano(false)}
        >
          <PianoRoll
            pianoNotes={getChannelNotes(currentPattern, selectedChannelForPiano)}
            onAddNote={handleChannelAddNote}
            onDeleteNote={handleChannelDeleteNote}
            onUpdateNote={handleChannelUpdateNote}
            zoom={zoomPr}
            snap={prSnap}
            playing={playing}
            bpm={bpm}
            onClose={() => setShowChannelPiano(false)}
            channelMidiNote={channels[selectedChannelForPiano]?.midiNote ?? 60}
            channelType={channels[selectedChannelForPiano]?.type}
          />
        </DraggableWindow>
      )}

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
            channelType={null}
          />
        </div>
      )}

      {/* Video Player — motivation videos while making beats */}
      {showVideoPlayer && (
        <DraggableWindow
          title="Video Player"
          defaultPos={{ x: 300, y: 60 }}
          defaultSize={{ w: 560, h: 360 }}
          onClose={() => setShowVideoPlayer(false)}
        >
          <VideoPlayerPanel />
        </DraggableWindow>
      )}

      {showAiPanel && (
        <DraggableWindow
          title="Stratum AI"
          defaultPos={{ x: 80, y: 80 }}
          defaultSize={{ w: 360, h: 480 }}
          onClose={() => setShowAiPanel(false)}
        >
          <AiPanel />
        </DraggableWindow>
      )}

      {showMixer && (
        <div className="mixer-overlay">
          <Mixer
            tracks={mixerTracks}
            onVolChange={handleMixerVol}
            onPanChange={handleMixerPan}
            onMute={handleMixerMute}
            onSolo={handleMixerSolo}
            onSlotClick={handleMixerSlotClick}
            loadedPlugins={mixerLoadedPlugins}
            onPluginRemove={handleMixerPluginRemove}
            onClose={() => setActivePanel('channelRack')}
          />
        </div>
      )}

      {/* Plugin Browser Modal */}
      {showPluginBrowser && (
        <div style={{
          position:'fixed', inset:0, zIndex:10000,
          display:'flex', alignItems:'center', justifyContent:'center',
          background:'rgba(0,0,0,0.8)', backdropFilter:'blur(4px)',
        }}>
          <div style={{
            width:500, height:600,
            background:'#18181b', border:'1px solid #3f3f46', borderRadius:12,
            display:'flex', flexDirection:'column',
            boxShadow:'0 8px 32px rgba(0,0,0,0.8)',
          }} onClick={e => e.stopPropagation()}>
            <PluginBrowser
              onSlotLoad={handlePluginLoad}
              onClose={() => { setShowPluginBrowser(false); setPluginBrowserTarget(null); }}
            />
          </div>
        </div>
      )}

      {/* Plugin GUI Modal */}
      {activePluginGui && (
        <div
          onClick={() => setActivePluginGui(null)}
          style={{
            position:'fixed', inset:0, zIndex:10001,
            display:'flex', alignItems:'center', justifyContent:'center',
            background:'rgba(0,0,0,0.7)', backdropFilter:'blur(2px)',
          }}
        >
          <div
            onClick={e => e.stopPropagation()}
            style={{
              maxWidth:'95vw', maxHeight:'92vh', overflow:'auto',
              borderRadius:12, boxShadow:'0 8px 32px rgba(0,0,0,0.8)',
              border:'1px solid #3f3f46', background:'#0a0a0b',
            }}
          >
            {/* Header bar */}
            <div style={{
              height:32, display:'flex', alignItems:'center', justifyContent:'space-between',
              padding:'0 12px', background:'#18181b', borderBottom:'1px solid #27272a',
            }}>
              <span style={{ fontSize:10, fontWeight:600, color:'#d4d4d8', letterSpacing:0.5 }}>
                {activePluginGui.name}
              </span>
              <button
                onClick={() => setActivePluginGui(null)}
                style={{
                  width:20, height:20, display:'flex', alignItems:'center', justifyContent:'center',
                  background:'transparent', border:'none', color:'#71717a',
                  cursor:'pointer', fontSize:14, borderRadius:4,
                }}
                title="Close"
              >×</button>
            </div>
            {/* GUI container */}
            <div id={`plugin-gui-${activePluginGui.slotId}`} style={{ minHeight:400, display:'flex', alignItems:'center', justifyContent:'center', color:'#71717a', fontSize:12 }}>
              Loading plugin GUI...
            </div>
          </div>
        </div>
      )}

      {/* Console Panel */}
      {showConsolePanel && (
        <div style={{
          position:'fixed', bottom:0, right:0, width:500, height:300,
          background:'#0a0a0b', border:'1px solid #3f3f46', borderTopLeftRadius:12,
          zIndex:10002, display:'flex', flexDirection:'column',
          boxShadow:'0 -4px 20px rgba(0,0,0,0.8)',
        }}>
          <div style={{
            height:32, display:'flex', alignItems:'center', justifyContent:'space-between',
            padding:'0 12px', background:'#18181b', borderBottom:'1px solid #27272a',
            borderTopLeftRadius:12,
          }}>
            <span style={{ fontSize:10, fontWeight:600, color:'#d4d4d8', letterSpacing:0.5 }}>
              Console Logs
            </span>
            <div style={{ display:'flex', gap:8 }}>
              <button
                onClick={() => {
                  const logText = consoleLogs.map(l => `[${l.timestamp}] [${l.type.toUpperCase()}] ${l.message}`).join('\n');
                  navigator.clipboard.writeText(logText);
                }}
                style={{
                  padding:'2px 8px', fontSize:9, background:'#27272a', border:'1px solid #3f3f46',
                  color:'#a1a1aa', borderRadius:4, cursor:'pointer',
                }}
              >Copy</button>
              <button
                onClick={() => setConsoleLogs([])}
                style={{
                  padding:'2px 8px', fontSize:9, background:'#27272a', border:'1px solid #3f3f46',
                  color:'#a1a1aa', borderRadius:4, cursor:'pointer',
                }}
              >Clear</button>
            </div>
            <button
              onClick={() => setShowConsolePanel(false)}
              style={{
                width:20, height:20, display:'flex', alignItems:'center', justifyContent:'center',
                background:'transparent', border:'none', color:'#71717a',
                cursor:'pointer', fontSize:14, borderRadius:4,
              }}
            >×</button>
          </div>
          <div style={{ flex:1, overflow:'auto', padding:8, fontFamily:'monospace', fontSize:10 }}>
            {consoleLogs.length === 0 && (
              <div style={{ color:'#52525b', textAlign:'center', marginTop:40 }}>
                No logs yet
              </div>
            )}
            {consoleLogs.map((log, i) => (
              <div key={i} style={{ marginBottom:4, color: log.type === 'error' ? '#ef4444' : log.type === 'warn' ? '#f97316' : '#a1a1aa' }}>
                <span style={{ color:'#52525b', marginRight:8 }}>[{log.timestamp}]</span>
                <span>{log.message}</span>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
}
