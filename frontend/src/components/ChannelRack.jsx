export default function ChannelRack({ channels, onStepToggle, onMute, onSolo, onVolChange, onPanChange, onAddChannel, playing, stepIndex, currentPattern, patterns, onPatternChange, onNewPattern, onOpenPiano }) {
  return (
    <div className="channel-rack">
      <div className="rack-header">
        <span className="rack-title">Channel Rack</span>
        <div style={{flex:1}} />
        <button className="tool-btn active">Step</button>
        <button className="tool-btn">Graph</button>
        <button className="tool-btn" onClick={onAddChannel}>+ Ch</button>
      </div>
      <div className="rack-body">
        {channels.map((ch, ci) => (
          <div key={ci} className="channel-row">
            <div className="ch-index">{ci + 1}</div>
            <div className={`ch-led ${playing ? 'on' : ''}`} />
            <div className={`ch-mute ${ch.mute?'on':''}`} onClick={()=>onMute(ci)}>M</div>
            <div className={`ch-solo ${ch.solo?'on':''}`} onClick={()=>onSolo(ci)}>S</div>
            <div className="ch-name" onClick={()=>onOpenPiano && onOpenPiano()} title={ch.name}>{ch.name}</div>
            <input className="ch-pan" type="range" min="-100" max="100" value={ch.pan} onChange={e=>onPanChange(ci,parseInt(e.target.value))} />
            <input className="ch-vol" type="range" min="0" max="100" value={ch.vol} onChange={e=>onVolChange(ci,parseInt(e.target.value))} />
            <div className="steps">
              {ch.steps.map((on, si) => (
                <div key={si} className={`step ${on?'active':''} ${playing && stepIndex===si?'current':''}`}
                  onClick={()=>onStepToggle(ci, si)} />
              ))}
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}
