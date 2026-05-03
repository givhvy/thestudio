import { useEffect } from 'react';

export default function Mixer({ tracks, onVolChange, onPanChange, onMute, onSolo, onClose }) {
  useEffect(() => {
    const interval = setInterval(() => {
      tracks.forEach((t, i) => {
        const el = document.getElementById(`meter${i}`);
        if (el) {
          const decayed = t.meter * 0.9;
          el.style.height = `${decayed}%`;
        }
      });
    }, 50);
    return () => clearInterval(interval);
  }, [tracks]);

  return (
    <div className="mixer" style={{height:'100%',display:'flex',flexDirection:'column'}}>
      <div className="mixer-header">
        <span style={{color:'#ff8c00',fontWeight:'bold'}}>Mixer</span>
        <div style={{flex:1}}></div>
        <span style={{fontSize:11,color:'#888'}}>{tracks.length} tracks</span>
        {onClose && (
          <button className="tool-btn" onClick={onClose} title="Close">✕</button>
        )}
      </div>
      <div className="mixer-strips" style={{flex:1,display:'flex',overflowX:'auto',padding:8,gap:4}}>
        {tracks.map((tr, i) => (
          <div key={i} className="mixer-strip">
            <div className="strip-name" style={{color:i<8?'#aaa':'#666'}}>{tr.name}</div>
            <div className="strip-meter">
              <div className="meter-fill" id={`meter${i}`}></div>
            </div>
            <input className="strip-fader" type="range" min="0" max="100" value={tr.vol}
              orient="vertical" onChange={e=>onVolChange(i,parseInt(e.target.value))}
              style={{writingMode:'vertical-lr',height:80,width:20}} />
            <input className="strip-pan" type="range" min="-100" max="100" value={tr.pan}
              onChange={e=>onPanChange(i,parseInt(e.target.value))} />
            <div className={`strip-mute ${tr.mute?'active':''}`} onClick={()=>onMute(i)}>M</div>
            <div className={`strip-solo ${tr.solo?'active':''}`} onClick={()=>onSolo(i)}>S</div>
          </div>
        ))}
      </div>
    </div>
  );
}
