export default function ChannelRack({ channels, onStepToggle, onMute, onSolo, onVolChange, onPanChange, onAddChannel, playing, stepIndex }) {
  return (
    <div className="channel-rack" style={{height:'100%',display:'flex',flexDirection:'column'}}>
      <div className="rack-header">
        <span style={{color:'#ff8c00',fontWeight:'bold'}}>Channel Rack</span>
        <div style={{flex:1}}></div>
        <button className="tool-btn active">Step</button>
        <button className="tool-btn">Graph</button>
        <button className="tool-btn" onClick={onAddChannel}>+ Channel</button>
      </div>
      <div className="rack-body" style={{flex:1,overflow:'auto'}}>
        {channels.map((ch, ci) => (
          <div key={ci} className="channel-row">
            <div className={`ch-mute ${ch.mute?'active':''}`} onClick={()=>onMute(ci)}>M</div>
            <div className={`ch-solo ${ch.solo?'active':''}`} onClick={()=>onSolo(ci)}>S</div>
            <div className="ch-name" style={{color:ch.color}}>{ch.name}</div>
            <input className="ch-pan" type="range" min="-100" max="100" value={ch.pan} onChange={e=>onPanChange(ci,parseInt(e.target.value))} />
            <input className="ch-vol" type="range" min="0" max="100" value={ch.vol} onChange={e=>onVolChange(ci,parseInt(e.target.value))} />
            <div className="steps">
              {ch.steps.map((on, si) => (
                <div key={si} className={`step ${on?'active':''} ${playing && stepIndex===si?'current':''}`}
                  onClick={()=>onStepToggle(ci, si)}></div>
              ))}
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}
