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
    <div className="mixer">
      <div className="mixer-header">
        <span className="mixer-title">Mixer</span>
        <div className="mixer-toolbar">
          <button>Wide</button>
          <button>Route</button>
          <button>FX</button>
        </div>
        <span className="mixer-track-count">{tracks.length} tracks</span>
        {onClose && (
          <button className="tool-btn" onClick={onClose} title="Close">✕</button>
        )}
      </div>

      <div className="mixer-body">
        <div className="mixer-strips">
          {tracks.map((tr, i) => {
            const isMaster = i === tracks.length - 1;
            return (
              <div key={i} className={`mixer-strip ${isMaster ? 'master' : ''}`}>
                <div className="strip-index">{isMaster ? 'M' : i + 1}</div>
                <div className="strip-name" title={tr.name}>{tr.name || `Insert ${i + 1}`}</div>
                <div className="strip-slot-led" />
                <input
                  className="strip-pan"
                  type="range"
                  min="-100"
                  max="100"
                  value={tr.pan}
                  onChange={e => onPanChange(i, parseInt(e.target.value))}
                  title="Pan"
                />
                <div className="strip-pan-label">{tr.pan > 0 ? `R${tr.pan}` : tr.pan < 0 ? `L${Math.abs(tr.pan)}` : 'C'}</div>
                <div className="strip-meter-fader">
                  <div className="strip-meter">
                    <div className="meter-fill" id={`meter${i}`} style={{ height: `${tr.meter || 0}%` }} />
                  </div>
                  <input
                    className="strip-fader"
                    type="range"
                    min="0"
                    max="100"
                    value={tr.vol}
                    onChange={e => onVolChange(i, parseInt(e.target.value))}
                    title="Volume"
                  />
                </div>
                <div className="strip-volume-readout">{tr.vol}</div>
                <div className="strip-buttons">
                  <button className={`strip-mute ${tr.mute ? 'on' : ''}`} onClick={() => onMute(i)}>M</button>
                  <button className={`strip-solo ${tr.solo ? 'on' : ''}`} onClick={() => onSolo(i)}>S</button>
                </div>
                <div className="strip-route">OUT</div>
              </div>
            );
          })}
        </div>

        <div className="mixer-detail">
          <div className="mixer-detail-title">Mixer - Master</div>
          <div className="mixer-fx-header">(none)</div>
          <div className="mixer-fx-list">
            {Array.from({ length: 10 }, (_, i) => (
              <div key={i} className={`mixer-fx-slot ${i === 5 ? 'enabled' : ''}`}>
                <span>▸ Slot {i + 1}</span>
                <button />
              </div>
            ))}
          </div>
          <div className="mixer-eq">
            <div className="mixer-eq-display">
              <span />
            </div>
            <div className="mixer-eq-label">Equalizer</div>
            <div className="mixer-eq-knobs">
              {Array.from({ length: 6 }, (_, i) => <button key={i} />)}
            </div>
          </div>
          <div className="mixer-out">Out 1 - Out 2</div>
        </div>
      </div>
    </div>
  );
}
