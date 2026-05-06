import { useEffect, useRef, useState } from 'react';
import { getChannelMeterLevel, getMeterData } from '../audio.js';

const FADER_H = 120; // px height of fader track
const STRIP_W = 52;

function PanKnob({ value, onChange }) {
  const ref = useRef(null);
  const drag = useRef(null);
  const angle = (value / 100) * 135; // -135..135 deg

  const onMouseDown = (e) => {
    e.preventDefault();
    drag.current = { startY: e.clientY, startVal: value };
    window.addEventListener('mousemove', onMove);
    window.addEventListener('mouseup', onUp);
  };
  const onMove = (e) => {
    if (!drag.current) return;
    const delta = (drag.current.startY - e.clientY) * 1.5;
    const v = Math.max(-100, Math.min(100, Math.round(drag.current.startVal + delta)));
    onChange(v);
  };
  const onUp = () => { drag.current = null; window.removeEventListener('mousemove', onMove); window.removeEventListener('mouseup', onUp); };

  return (
    <div ref={ref} onMouseDown={onMouseDown} title={`Pan: ${value > 0 ? 'R' : value < 0 ? 'L' : 'C'}${Math.abs(value) || ''}`}
      style={{ width:28, height:28, borderRadius:'50%', background:'radial-gradient(circle at 40% 35%,#3a3a3e,#1a1a1e)', border:'1px solid #3f3f46', cursor:'ns-resize', position:'relative', flexShrink:0, margin:'0 auto' }}>
      <div style={{
        position:'absolute', top:'50%', left:'50%', width:2, height:10,
        background: value === 0 ? '#3b82f6' : '#f97316',
        transformOrigin:'50% 0', borderRadius:1,
        transform:`translate(-50%,-100%) rotate(${angle}deg)`,
      }} />
    </div>
  );
}

export default function Mixer({ tracks, onVolChange, onPanChange, onMute, onSolo, onClose, onSlotClick, loadedPlugins, onPluginRemove, onReverbSendChange }) {
  const peakHoldRef = useRef(tracks.map(() => ({ level: 0, frames: 0 })));
  const [selectedStrip, setSelectedStrip] = useState(tracks.length - 1);

  useEffect(() => {
    let raf;
    const tick = () => {
      tracks.forEach((t, i) => {
        const level = i === tracks.length - 1 ? getMeterData() : getChannelMeterLevel(`ch${i}`);
        const ph = peakHoldRef.current[i] || { level: 0, frames: 0 };
        if (level >= ph.level) { ph.level = level; ph.frames = 60; }
        else if (ph.frames > 0) ph.frames--;
        else ph.level = Math.max(0, ph.level - 0.008);
        peakHoldRef.current[i] = ph;

        const fill = document.getElementById(`mfill-${i}`);
        const peak = document.getElementById(`mpeak-${i}`);
        if (fill) fill.style.height = `${Math.min(100, level * 100)}%`;
        if (peak) {
          const pct = Math.min(100, ph.level * 100);
          peak.style.bottom = `${pct}%`;
          peak.style.background = ph.level > 0.85 ? '#ef4444' : ph.level > 0.6 ? '#eab308' : '#22c55e';
        }
      });
      raf = requestAnimationFrame(tick);
    };
    raf = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(raf);
  }, [tracks.length]);

  const sel = tracks[selectedStrip];

  return (
    <div style={{ display:'flex', flexDirection:'column', height:'100%', background:'#131315', color:'#d4d4d8', fontSize:10, userSelect:'none' }}>
      {/* Mixer header */}
      <div style={{ display:'flex', alignItems:'center', height:26, background:'#0d0d0f', borderBottom:'1px solid #27272a', padding:'0 8px', gap:6, flexShrink:0 }}>
        <span style={{ fontWeight:700, fontSize:11, letterSpacing:1, color:'#a1a1aa' }}>MIXER</span>
        <div style={{flex:1}}/>
        <button style={{ fontSize:9, padding:'1px 6px', background:'#27272a', border:'1px solid #3f3f46', borderRadius:2, color:'#a1a1aa', cursor:'pointer' }}>Wide</button>
        <button style={{ fontSize:9, padding:'1px 6px', background:'#27272a', border:'1px solid #3f3f46', borderRadius:2, color:'#a1a1aa', cursor:'pointer' }}>Route</button>
        {onClose && <button onClick={onClose} style={{ fontSize:10, padding:'1px 6px', background:'transparent', border:'none', color:'#71717a', cursor:'pointer' }}>✕</button>}
      </div>

      {/* Strips + detail panel */}
      <div style={{ display:'flex', flex:1, overflow:'hidden' }}>

        {/* Channel strips */}
        <div style={{ display:'flex', overflowX:'auto', overflowY:'hidden', flex:1, borderRight:'1px solid #27272a' }}>
          {tracks.map((tr, i) => {
            const isMaster = i === tracks.length - 1;
            const isSelected = i === selectedStrip;
            const faderPct = tr.vol; // 0–100

            return (
              <div key={i}
                onClick={() => setSelectedStrip(i)}
                style={{
                  width: STRIP_W, flexShrink:0, display:'flex', flexDirection:'column', alignItems:'center',
                  background: isSelected ? '#1e1e22' : isMaster ? '#16161a' : '#131315',
                  borderRight:'1px solid #1e1e22',
                  borderTop: isSelected ? '2px solid #f97316' : '2px solid transparent',
                  cursor:'pointer', padding:'4px 0',
                }}>

                {/* Strip number */}
                <div style={{ fontSize:9, color: isMaster ? '#f97316' : '#52525b', marginBottom:2 }}>
                  {isMaster ? 'M' : i + 1}
                </div>

                {/* Name */}
                <div style={{ fontSize:8, color: isSelected ? '#f97316' : '#71717a', maxWidth:STRIP_W-4, overflow:'hidden', textOverflow:'ellipsis', whiteSpace:'nowrap', marginBottom:4 }}>
                  {tr.name || (isMaster ? 'Master' : `Insert ${i + 1}`)}
                </div>

                {/* Pan knob */}
                <PanKnob value={tr.pan} onChange={v => onPanChange(i, v)} />
                <div style={{ fontSize:8, color:'#52525b', marginTop:2, marginBottom:4 }}>
                  {tr.pan > 0 ? `R${tr.pan}` : tr.pan < 0 ? `L${Math.abs(tr.pan)}` : 'C'}
                </div>

                {/* VU meter + fader */}
                <div style={{ display:'flex', gap:2, alignItems:'flex-end', height:FADER_H, marginBottom:4 }}>
                  {/* VU meter (stereo appearance with 2 thin bars) */}
                  <div style={{ display:'flex', gap:1 }}>
                    {[0,1].map(ch => (
                      <div key={ch} style={{ width:4, height:FADER_H, background:'#0d0d0f', borderRadius:2, position:'relative', overflow:'hidden' }}>
                        <div id={ch === 0 ? `mfill-${i}` : undefined} style={{
                          position:'absolute', bottom:0, left:0, right:0, height:'0%',
                          background:'linear-gradient(to top,#22c55e 0%,#86efac 60%,#eab308 80%,#ef4444 100%)',
                          borderRadius:2, transition:'height 0.04s',
                        }} />
                        <div id={ch === 0 ? `mpeak-${i}` : undefined} style={{
                          position:'absolute', left:0, right:0, height:2, bottom:'0%', background:'#22c55e',
                        }} />
                      </div>
                    ))}
                  </div>

                  {/* Fader */}
                  <div style={{ position:'relative', height:FADER_H, width:16, display:'flex', flexDirection:'column', alignItems:'center' }}>
                    <input
                      type="range" min="0" max="100" value={tr.vol}
                      onChange={e => onVolChange(i, parseInt(e.target.value))}
                      title={`Volume: ${tr.vol}`}
                      style={{
                        writingMode:'vertical-lr', direction:'rtl',
                        appearance:'slider-vertical', WebkitAppearance:'slider-vertical',
                        width:16, height:FADER_H,
                        accentColor: isMaster ? '#f97316' : '#3b82f6',
                        cursor:'ns-resize',
                      }}
                    />
                  </div>
                </div>

                {/* Reverb send */}
                <div style={{ marginBottom:4 }}>
                  <input
                    type="range" min="0" max="100" value={tr.reverbSend || 0}
                    onChange={e => onReverbSendChange && onReverbSendChange(i, parseInt(e.target.value))}
                    title={`Reverb Send: ${tr.reverbSend || 0}%`}
                    style={{
                      width:30, height:4,
                      appearance:'slider-horizontal', WebkitAppearance:'slider-horizontal',
                      accentColor: '#8b5cf6',
                      cursor:'ew-resize',
                    }}
                  />
                  <div style={{ fontSize:7, color:'#52525b', marginTop:1 }}>Rvb: {tr.reverbSend || 0}%</div>
                </div>

                {/* Vol readout */}
                <div style={{ fontSize:8, color:'#52525b', marginBottom:4 }}>{tr.vol}%</div>

                {/* Mute / Solo */}
                <div style={{ display:'flex', gap:2 }}>
                  <button
                    onClick={e => { e.stopPropagation(); onMute(i); }}
                    style={{
                      width:20, height:16, fontSize:8, border:'1px solid #3f3f46',
                      background: tr.mute ? '#eab308' : '#27272a',
                      color: tr.mute ? '#000' : '#71717a',
                      borderRadius:2, cursor:'pointer',
                    }}>M</button>
                  <button
                    onClick={e => { e.stopPropagation(); onSolo(i); }}
                    style={{
                      width:20, height:16, fontSize:8, border:'1px solid #3f3f46',
                      background: tr.solo ? '#22c55e' : '#27272a',
                      color: tr.solo ? '#000' : '#71717a',
                      borderRadius:2, cursor:'pointer',
                    }}>S</button>
                </div>
              </div>
            );
          })}
        </div>

        {/* Right detail panel — FX inserts for selected strip */}
        <div style={{ width:180, flexShrink:0, display:'flex', flexDirection:'column', background:'#0f0f11' }}>
          <div style={{ padding:'6px 8px', borderBottom:'1px solid #27272a', fontSize:10, color:'#a1a1aa', fontWeight:600 }}>
            {sel ? (sel.name || (selectedStrip === tracks.length - 1 ? 'Master' : `Insert ${selectedStrip + 1}`)) : 'Mixer'}
          </div>
          <div style={{ padding:'4px 8px', borderBottom:'1px solid #1e1e22', fontSize:9, color:'#52525b' }}>
            FX Chain
          </div>
          <div style={{ flex:1, overflowY:'auto' }}>
            {Array.from({ length: 8 }, (_, si) => {
              const key = `${selectedStrip}_${si}`;
              const plugin = loadedPlugins[key];
              return (
                <div
                  key={si}
                  onClick={() => onSlotClick && onSlotClick(selectedStrip, si, plugin)}
                  style={{
                    display:'flex', alignItems:'center', padding:'4px 8px',
                    borderBottom:'1px solid #1a1a1c',
                    background: plugin ? '#1a2a1a' : 'transparent',
                    cursor:'pointer',
                  }}
                  title={plugin ? plugin.name : 'Click to add plugin'}
                >
                  <span style={{ flex:1, fontSize:9, color: plugin ? '#86efac' : '#3f3f46' }}>
                    {plugin ? `▸ ${plugin.name}` : `▸ Slot ${si + 1} (empty)`}
                  </span>
                  {plugin && (
                    <button
                      onClick={(e) => { e.stopPropagation(); onPluginRemove && onPluginRemove(selectedStrip, si); }}
                      style={{ width:14, height:14, fontSize:9, border:'1px solid #3f3f46', background:'#ef4444', color:'#fff', borderRadius:2, cursor:'pointer' }}
                      title="Remove plugin"
                    >×</button>
                  )}
                  <div style={{ width:8, height:8, borderRadius:'50%', background: plugin ? '#22c55e' : '#27272a', border:'1px solid #3f3f46' }} />
                </div>
              );
            })}
          </div>
          <div style={{ padding:'6px 8px', borderTop:'1px solid #27272a', fontSize:9, color:'#52525b' }}>
            Out 1 — Out 2
          </div>
        </div>
      </div>
    </div>
  );
}
