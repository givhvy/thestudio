export default function Toolbar({ bpm, setBpm, playing, onPlay, onStop, recording, onRecord, currentPattern, patterns, onPatternChange, onNewPattern, onSave, onShowProjects, projectName, activePanel, onTogglePiano, onToggleMixer }) {
  return (
    <div className="toolbar">
      <div className="fl-panel transport-panel">
        <button className={`transport-led ${playing ? 'lit' : ''}`} title="Song / Pattern">PAT</button>
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
        {['⌘','◀','▶','3x','♪','✎','▦','↕','↔','⌁','⚙','?'].map((x, i) => (
          <button key={i} className="fl-icon">{x}</button>
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
