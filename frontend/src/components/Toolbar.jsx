export default function Toolbar({ bpm, setBpm, playing, onPlay, onStop, recording, onRecord, currentPattern, patterns, onPatternChange, onNewPattern, onSave, onShowProjects, projectName, activePanel, onTogglePiano, onToggleMixer, onToggleChannelRack, onShowPluginBrowser }) {
  const iconActions = [
    { icon: '⌘', title: 'Song / Pattern Mode', action: () => alert('Song/Pattern mode toggle') },
    { icon: '◀', title: 'Rewind', action: () => alert('Rewind') },
    { icon: '▶', title: 'Forward', action: () => alert('Forward') },
    { icon: '3x', title: '3x Speed', action: () => alert('3x Speed') },
    { icon: '♪', title: 'Metronome', action: () => alert('Metronome toggle') },
    { icon: '✎', title: 'Edit Mode', action: () => alert('Edit mode toggle') },
    { icon: '▦', title: 'Snap to Grid', action: () => alert('Snap to grid toggle') },
    { icon: '↕', title: 'Zoom Vertical', action: () => alert('Zoom vertical') },
    { icon: '↔', title: 'Zoom Horizontal', action: () => alert('Zoom horizontal') },
    { icon: '⌁', title: 'Smart Disable', action: () => alert('Smart disable') },
    { icon: '⚙', title: 'Settings', action: () => alert('Settings') },
    { icon: '?', title: 'Help', action: () => alert('Help') },
  ];

  return (
    <div className="toolbar">
      <div className="fl-panel transport-panel">
        <button className={`transport-led ${playing ? 'lit' : ''}`} title="Song / Pattern" onClick={onToggleChannelRack}>PAT</button>
        <button className="fl-round play-round" onClick={onPlay} title="Play">▶</button>
        <button className="fl-round stop-round" onClick={onStop} title="Stop">■</button>
        <button className={`fl-round rec-round ${recording ? 'lit' : ''}`} onClick={onRecord} title="Record">●</button>
      </div>

      <div className="fl-panel bpm-panel">
        <input
          className="bpm-input"
          value={bpm}
          onChange={e => setBpm(Number(e.target.value) || 1)}
          title="Tempo"
        />
      </div>

      <div className="fl-panel time-panel">
        <div className="time-big">0:00:00</div>
      </div>

      <div className="fl-panel icon-strip">
        {iconActions.map((item, i) => (
          <button key={i} className="fl-icon" title={item.title} onClick={item.action}>{item.icon}</button>
        ))}
      </div>

      <div className="fl-panel pattern-sel">
        <select value={currentPattern} onChange={e => onPatternChange(Number(e.target.value))}>
          {patterns.map((p,i) => <option key={i} value={i}>{p.name}</option>)}
        </select>
        <button className="fl-icon" onClick={onNewPattern} title="New Pattern">+</button>
      </div>

      <div className="fl-panel icon-strip">
        <button className={`fl-icon wide ${activePanel==='pianoRoll'?'active':''}`} onClick={onTogglePiano}>Piano</button>
        <button className={`fl-icon wide ${activePanel==='mixerPanel'?'active':''}`} onClick={onToggleMixer}>Mixer</button>
      </div>

      <div className="project-display">{projectName}</div>

      <div className="fl-panel icon-strip">
        <button className="fl-icon wide" onClick={onSave} title="Save">Save</button>
        <button className="fl-icon wide" onClick={onShowProjects} title="Open">Open</button>
      </div>
    </div>
  );
}
