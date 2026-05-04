import { useRef, useEffect, useState } from 'react';
import { playSynth, freqFromMidi, now, initAudio, resumeAudio, playKick, playSnare, playHihat, playClap, hasSample, playSampleAt } from '../audio.js';

const notes = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
const noteHeight = 28;
const basePxPerBeat = 80;

const NOTE_COLORS = [
  '#ef4444','#f97316','#eab308','#22c55e','#14b8a6',
  '#3b82f6','#8b5cf6','#ec4899','#f43f5e','#06b6d4',
];

function getNoteColor(midiNote) {
  const oct = Math.floor(midiNote / 12);
  return NOTE_COLORS[oct % NOTE_COLORS.length];
}

export default function PianoRoll({ pianoNotes, onAddNote, onDeleteNote, onUpdateNote, zoom: zoomProp, snap, playing, bpm, onClose, channelMidiNote, channelType }) {
  const areaRef = useRef(null);
  const scrollWrapRef = useRef(null);
  const rulerRef = useRef(null);
  const [playheadPos, setPlayheadPos] = useState(0);
  const [zoom, setZoom] = useState(zoomProp ?? 1);
  const [selectedNotes, setSelectedNotes] = useState(new Set());
  const hasScrolled = useRef(false);
  const pxPerBeat = basePxPerBeat * zoom;

  // Scroll to show notes (or C5 default) — runs after layout
  useEffect(() => {
    const doScroll = () => {
      if (!scrollWrapRef.current) return;
      // Find the topmost note to scroll to, fallback to channelMidiNote or C5 (60)
      let targetNote = channelMidiNote ?? 60;
      if (pianoNotes && pianoNotes.length > 0) {
        targetNote = Math.round(pianoNotes.reduce((s, n) => s + n.note, 0) / pianoNotes.length);
      }
      const targetTop = (96 - targetNote) * noteHeight;
      const wrapH = scrollWrapRef.current.clientHeight;
      scrollWrapRef.current.scrollTop = Math.max(0, targetTop - wrapH / 2);
    };
    // First open: always scroll
    if (!hasScrolled.current) {
      hasScrolled.current = true;
      requestAnimationFrame(() => requestAnimationFrame(doScroll));
    }
  }, [pianoNotes, channelMidiNote]);

  // Drag state
  const dragRef = useRef(null); // { idx, mode: 'move'|'resize', startX, startY, origNote }

  useEffect(() => {
    if (!playing) { setPlayheadPos(0); return; }
    let raf;
    const startTime = performance.now();
    const beatDur = 60000 / bpm;
    const animate = () => {
      const elapsed = performance.now() - startTime;
      const beats = elapsed / beatDur;
      setPlayheadPos(beats * pxPerBeat);
      raf = requestAnimationFrame(animate);
    };
    raf = requestAnimationFrame(animate);
    return () => cancelAnimationFrame(raf);
  }, [playing, bpm, pxPerBeat]);

  const parseSnap = (s) => {
    if (s === 'Bar') return 4;
    if (s === '1/2') return 2;
    if (s === '1/4') return 1;
    if (s === '1/8') return 0.5;
    if (s === '1/16') return 0.25;
    if (s === '1/32') return 0.125;
    return 0.5;
  };

  const snapVal = parseSnap(snap);

  const handleAreaClick = (e) => {
    if (dragRef.current) return; // don't add note if we just finished a drag
    if (!areaRef.current) return;
    const rect = areaRef.current.getBoundingClientRect();
    const x = e.clientX - rect.left + areaRef.current.scrollLeft;
    const y = e.clientY - rect.top + areaRef.current.scrollTop;
    const beat = x / pxPerBeat;
    const noteVal = 96 - Math.floor(y / noteHeight);
    const snappedBeat = Math.floor(beat / snapVal) * snapVal;
    
    // Play preview sound based on channel type
    initAudio();
    resumeAudio();
    
    if (channelType === 'kick') {
      playKick(now(), 0.8);
    } else if (channelType === 'snare') {
      playSnare(now(), 0.8);
    } else if (channelType === 'hihat') {
      playHihat(now(), false, 0.8);
    } else if (channelType === 'hihat_open') {
      playHihat(now(), true, 0.8);
    } else if (channelType === 'clap') {
      playClap(now(), 0.8);
    } else if (channelType === 'piano') {
      const freq = freqFromMidi(noteVal);
      if (freq) {
        playSynth(freq, now(), {
          waveforms: ['triangle','sine'], detune: [0, 4], gains: [0.45, 0.35],
          attack: 0.002, decay: 0.6, sustain: 0.2, release: 0.4,
          duration: 0.2, velocity: 0.8,
        });
      }
    } else if (hasSample(channelType)) {
      playSampleAt(channelType, now(), 0.8, 1);
    } else {
      // Default synth for other types
      const freq = freqFromMidi(noteVal);
      if (freq) {
        playSynth(freq, now(), {
          waveforms: ['sawtooth', 'sine'],
          detune: [0, 7],
          gains: [0.4, 0.2],
          attack: 0.005,
          decay: 0.1,
          sustain: 0.5,
          release: 0.1,
          duration: 0.2,
          velocity: 0.7,
        });
      }
    }
    
    onAddNote({ note: noteVal, start: snappedBeat, length: snapVal, vel: 80 });
  };

  const handleNoteMouseDown = (e, idx, note) => {
    e.stopPropagation();
    const rect = areaRef.current.getBoundingClientRect();
    const x = e.clientX - rect.left + areaRef.current.scrollLeft;
    const isResize = x > (note.start + note.length) * pxPerBeat - 6;
    dragRef.current = {
      idx,
      mode: isResize ? 'resize' : 'move',
      startX: e.clientX,
      startY: e.clientY,
      origNote: { ...note }
    };
    window.addEventListener('mousemove', handleMouseMove);
    window.addEventListener('mouseup', handleMouseUp);
  };

  const handleMouseMove = (e) => {
    if (!dragRef.current || !areaRef.current) return;
    const rect = areaRef.current.getBoundingClientRect();
    const { idx, mode, startX, startY, origNote } = dragRef.current;

    if (mode === 'move') {
      const deltaX = (e.clientX - startX) / pxPerBeat;
      const deltaY = (e.clientY - startY) / noteHeight;
      const newStart = Math.max(0, Math.floor((origNote.start + deltaX) / snapVal) * snapVal);
      const newNote = Math.min(96, Math.max(36, Math.round(96 - ((96 - origNote.note) + deltaY))));
      onUpdateNote(idx, { start: newStart, note: newNote });
    } else if (mode === 'resize') {
      const deltaX = (e.clientX - startX) / pxPerBeat;
      const newLen = Math.max(snapVal, Math.round((origNote.length + deltaX) / snapVal) * snapVal);
      onUpdateNote(idx, { length: newLen });
    }
  };

  const handleMouseUp = () => {
    dragRef.current = null;
    window.removeEventListener('mousemove', handleMouseMove);
    window.removeEventListener('mouseup', handleMouseUp);
  };

  // Keyboard shortcuts
  useEffect(() => {
    const onKey = (e) => {
      if (e.target.tagName === 'INPUT') return;
      if ((e.ctrlKey || e.metaKey) && e.key === 'a') {
        e.preventDefault();
        setSelectedNotes(new Set(pianoNotes.map((_, i) => i)));
      }
      if ((e.key === 'Delete' || e.key === 'Backspace') && selectedNotes.size > 0) {
        e.preventDefault();
        const idxs = [...selectedNotes].sort((a, b) => b - a);
        idxs.forEach(i => onDeleteNote(i));
        setSelectedNotes(new Set());
      }
      if (e.key === 'Escape') setSelectedNotes(new Set());
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [selectedNotes, pianoNotes, onDeleteNote]);

  // Sync ruler scroll with grid scroll
  const handleGridScroll = (e) => {
    if (rulerRef.current) rulerRef.current.scrollLeft = e.currentTarget.scrollLeft;
  };

  const totalBars = 16;

  return (
    <div className="piano-roll" style={{height:'100%',display:'flex',flexDirection:'column'}}>
      <div className="pr-toolbar">
        <span style={{color:'#ff8c00',fontWeight:'bold'}}>Piano Roll</span>
        <div style={{flex:1}}></div>
        <span style={{fontSize:10,color:'#888'}}>Snap: {snap}</span>
        <div style={{width:1,background:'#3f3f46',margin:'0 6px',alignSelf:'stretch'}}/>
        <button className="tool-btn" onClick={() => setSelectedNotes(new Set(pianoNotes.map((_,i)=>i)))} title="Select All (Ctrl+A)">Sel All</button>
        {selectedNotes.size > 0 && (
          <button className="tool-btn" style={{color:'#ef4444'}}
            onClick={() => { [...selectedNotes].sort((a,b)=>b-a).forEach(i=>onDeleteNote(i)); setSelectedNotes(new Set()); }}
            title="Delete selected notes (Delete key)"
          >Del {selectedNotes.size}</button>
        )}
        <div style={{width:1,background:'#3f3f46',margin:'0 6px',alignSelf:'stretch'}}/>
        <span style={{fontSize:10,color:'#888'}}>Zoom:</span>
        <button className="tool-btn" onClick={() => setZoom(z => Math.max(0.25, +(z - 0.25).toFixed(2)))} title="Zoom Out">−</button>
        <span style={{fontSize:10,color:'#ccc',minWidth:32,textAlign:'center'}}>{Math.round(zoom * 100)}%</span>
        <button className="tool-btn" onClick={() => setZoom(z => Math.min(4, +(z + 0.25).toFixed(2)))} title="Zoom In">+</button>
        {onClose && (
          <button className="tool-btn" onClick={onClose} title="Close" style={{marginLeft:6}}>✕</button>
        )}
      </div>

      {/* Bar ruler */}
      <div style={{display:'flex',overflow:'hidden',flexShrink:0,borderBottom:'1px solid #333'}}>
        <div style={{width:60,flexShrink:0,background:'#1a1a1a',borderRight:'1px solid #444'}}/>
        <div ref={rulerRef} style={{flex:1,overflowX:'hidden',position:'relative',height:20,background:'#18181b'}}>
          {Array.from({length:totalBars+1},(_,i)=>(
            <div key={i} style={{
              position:'absolute',left:i*pxPerBeat*4,top:0,bottom:0,
              borderLeft:'1px solid #444',paddingLeft:3,
              fontSize:9,color:i%4===0?'#ff8c00':'#555',lineHeight:'20px',
              pointerEvents:'none',userSelect:'none',
            }}>{i>0?`${i}`:''}</div>
          ))}
        </div>
      </div>

      <div className="pr-grid-wrap" ref={scrollWrapRef} onScroll={handleGridScroll} style={{flex:1,overflow:'auto'}}>
        <div className="pr-grid" style={{minWidth:2000,minHeight:61*noteHeight,position:'relative'}}>
          <div className="pr-keys" style={{position:'absolute',left:0,top:0,width:60,height:61*noteHeight,zIndex:2,borderRight:'1px solid #444',background:'#2a2a2a'}}>
            {Array.from({length:61},(_,i)=>{
              const n = 96 - i;
              const isBlack = [1,3,6,8,10].includes(n%12);
              const name = notes[n%12]+Math.floor(n/12);
              return (
                <div key={n} className={`pr-key ${isBlack?'black':''}`}
                  style={{top:i*noteHeight,height:noteHeight,position:'absolute',left:0,right:0}}>
                  {name}
                </div>
              );
            })}
          </div>
          <div className="pr-notes-area" ref={areaRef} onClick={handleAreaClick}
            style={{marginLeft:60,position:'relative',height:61*noteHeight,cursor:'crosshair'}}>
            {Array.from({length:61},(_,i)=>{
              const n = 96 - i;
              return (
                <div key={`gl-${n}`} className="pr-grid-line"
                  style={{top:i*noteHeight+15,position:'absolute',left:0,right:0}}></div>
              );
            })}
            {Array.from({length:33},(_,i)=>{
              return (
                <div key={`bl-${i}`} className={i%4===0?'pr-bar-line':'pr-beat-line'}
                  style={{left:i*pxPerBeat,position:'absolute',top:0,bottom:0}}></div>
              );
            })}
            {pianoNotes.map((pn, idx) => {
              const col = getNoteColor(pn.note);
              const w = Math.max(8, pn.length * pxPerBeat - 2);
              return (
                <div key={idx} className="pr-note"
                  style={{
                    left: pn.start * pxPerBeat,
                    top: (96 - pn.note) * noteHeight,
                    width: w,
                    height: noteHeight - 2,
                    background: col,
                    boxShadow: selectedNotes.has(idx) ? `0 0 0 2px #fff, 0 0 8px ${col}` : `0 0 6px ${col}66`,
                    borderRadius: 3,
                    display: 'flex', alignItems: 'center', overflow: 'hidden',
                    outline: selectedNotes.has(idx) ? '2px solid #fff' : 'none',
                  }}
                  onMouseDown={e => handleNoteMouseDown(e, idx, pn)}
                  onClick={e => {
                    e.stopPropagation();
                    if (dragRef.current) return;
                    if (e.shiftKey || e.ctrlKey || e.metaKey) {
                      setSelectedNotes(prev => { const n = new Set(prev); n.has(idx) ? n.delete(idx) : n.add(idx); return n; });
                    } else {
                      onDeleteNote(idx);
                    }
                  }}
                  onContextMenu={e => { e.preventDefault(); e.stopPropagation(); onDeleteNote(idx); }}
                >
                  <span style={{fontSize:9,color:'rgba(0,0,0,0.7)',fontWeight:'bold',paddingLeft:3,pointerEvents:'none',userSelect:'none',overflow:'hidden',whiteSpace:'nowrap'}}>
                    {notes[pn.note % 12]}{Math.floor(pn.note/12)}
                  </span>
                  <div style={{position:'absolute',right:0,top:0,bottom:0,width:6,cursor:'ew-resize',background:'rgba(0,0,0,0.25)'}}/>
                </div>
              );
            })}
            <div className="pr-playhead" style={{
              display: playing ? 'block' : 'none',
              left: playheadPos
            }}></div>
          </div>
        </div>
      </div>
    </div>
  );
}
