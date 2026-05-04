import { useRef } from 'react';

function BpmWidget({ bpm, setBpm, onTapTempo }) {
  const dragRef = useRef(null);
  const isDragging = useRef(false);

  const onMouseDown = (e) => {
    // Don't hijack if clicking directly on the input
    isDragging.current = false;
    dragRef.current = { startY: e.clientY, startBpm: bpm };
    window.addEventListener('mousemove', onMouseMove);
    window.addEventListener('mouseup', onMouseUp);
  };

  const onMouseMove = (e) => {
    if (!dragRef.current) return;
    const delta = dragRef.current.startY - e.clientY;
    if (Math.abs(delta) > 2) isDragging.current = true;
    const newBpm = Math.min(300, Math.max(20, Math.round(dragRef.current.startBpm + delta * 0.5)));
    setBpm(newBpm);
  };

  const onMouseUp = () => {
    window.removeEventListener('mousemove', onMouseMove);
    window.removeEventListener('mouseup', onMouseUp);
    if (!isDragging.current) onTapTempo();
    dragRef.current = null;
    isDragging.current = false;
  };

  return (
    <input
      className="bpm-input"
      type="number"
      min="20" max="300"
      value={bpm}
      title="BPM — drag up/down or scroll to change, click to tap tempo"
      onChange={e => setBpm(Math.min(300, Math.max(20, Number(e.target.value) || 120)))}
      onWheel={e => { e.preventDefault(); setBpm(b => Math.min(300, Math.max(20, b + (e.deltaY < 0 ? 1 : -1)))); }}
      onMouseDown={onMouseDown}
      style={{ cursor: 'ns-resize' }}
    />
  );
}

export default function Toolbar({ bpm, setBpm, transportTime = 0, playing, onPlay, onStop, recording, onRecord, currentPattern, patterns, onPatternChange, onNewPattern, onSave, onShowProjects, projectName, activePanel, onTogglePiano, onToggleMixer, onToggleChannelRack, onShowPluginBrowser, onExport, onTapTempo }) {
  const minutes = Math.floor(transportTime / 60);
  const seconds = Math.floor(transportTime % 60);
  const frames = Math.floor((transportTime % 1) * 100);
  const timeDisplay = `${minutes}:${String(seconds).padStart(2, '0')}:${String(frames).padStart(2, '0')}`;

  return (
    <div className="toolbar">
      {/* Transport */}
      <div className="fl-panel transport-panel">
        <button className={`transport-led ${playing ? 'lit' : ''}`} title="Song / Pattern" onClick={onToggleChannelRack}>PAT</button>
        <button className="fl-round play-round" onClick={onPlay} title="Play (Space)">▶</button>
        <button className="fl-round stop-round" onClick={onStop} title="Stop (Space)">■</button>
        <button className={`fl-round rec-round ${recording ? 'lit' : ''}`} onClick={onRecord} title="Record">●</button>
      </div>

      {/* BPM widget */}
      <div className="fl-panel" style={{ padding: '0 6px' }}>
        <BpmWidget bpm={bpm} setBpm={setBpm} onTapTempo={onTapTempo} />
      </div>

      {/* Time display */}
      <div className="fl-panel time-panel">
        <div className="time-big">{timeDisplay}</div>
      </div>

      {/* Pattern selector */}
      <div className="fl-panel pattern-sel">
        <select value={currentPattern} onChange={e => onPatternChange(Number(e.target.value))}>
          {patterns.map((p,i) => <option key={i} value={i}>{p.name}</option>)}
        </select>
        <button className="fl-icon" onClick={onNewPattern} title="New Pattern">+</button>
      </div>

      {/* View buttons */}
      <div className="fl-panel icon-strip">
        <button className={`fl-icon wide ${activePanel==='pianoRoll'?'active':''}`} onClick={onTogglePiano}>PIANO</button>
        <button className={`fl-icon wide ${activePanel==='mixerPanel'?'active':''}`} onClick={onToggleMixer}>MIXER</button>
      </div>

      <div className="project-display">{projectName}</div>

      {/* Actions */}
      <div className="fl-panel icon-strip">
        <button className="fl-icon wide" onClick={onSave}>SAVE</button>
        <button className="fl-icon wide" onClick={onShowProjects}>OPEN</button>
        <button className="fl-icon wide" onClick={onExport} style={{ background:'#1e3050', color:'#93c5fd', borderColor:'#2563eb' }}>EXPORT</button>
      </div>
    </div>
  );
}
