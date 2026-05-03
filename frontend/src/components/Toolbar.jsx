export default function Toolbar({ bpm, setBpm, playing, onPlay, onStop, recording, onRecord, currentPattern, patterns, onPatternChange, onNewPattern, onSave, onShowProjects, projectName }) {
  return (
    <div className="toolbar">
      <button className={`tb-btn play-btn ${playing?'active':''}`} onClick={onPlay}>▶ Play</button>
      <button className="tb-btn stop-btn" onClick={onStop}>■ Stop</button>
      <button className={`tb-btn rec-btn ${recording?'active':''}`} onClick={onRecord}>● Rec</button>
      <div className="tb-sep"></div>
      <div style={{display:'flex',flexDirection:'column',alignItems:'center',gap:2}}>
        <span style={{fontSize:9,color:'#888'}}>BPM</span>
        <input className="tb-display" value={bpm} onChange={e=>setBpm(parseInt(e.target.value)||130)} style={{width:50,color:'#4caf50'}}/>
      </div>
      <div style={{display:'flex',flexDirection:'column',alignItems:'center',gap:2}}>
        <span style={{fontSize:9,color:'#888'}}>PAT</span>
        <div className="tb-display">{currentPattern+1}</div>
      </div>
      <div className="tb-sep"></div>
      <div className="pattern-sel">
        <button className="tb-btn" onClick={onNewPattern}>+ Pattern</button>
        <select value={currentPattern} onChange={e=>onPatternChange(parseInt(e.target.value))}>
          {patterns.map((p,i)=>(<option key={i} value={i}>{p.name}</option>))}
        </select>
      </div>
      <div className="tb-sep"></div>
      <button className="tb-btn" onClick={onShowProjects}>📂 {projectName || 'Projects'}</button>
      <button className="tb-btn" onClick={onSave}>💾 Save</button>
    </div>
  );
}
