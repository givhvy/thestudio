import { useRef, useEffect, useState } from 'react';

const notes = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
const noteHeight = 16;
const basePxPerBeat = 40;

export default function PianoRoll({ pianoNotes, onAddNote, onDeleteNote, onUpdateNote, zoom, snap, playing, bpm, onClose }) {
  const areaRef = useRef(null);
  const [playheadPos, setPlayheadPos] = useState(0);
  const pxPerBeat = basePxPerBeat * zoom;

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

  return (
    <div className="piano-roll" style={{height:'100%',display:'flex',flexDirection:'column'}}>
      <div className="pr-toolbar">
        <span style={{color:'#ff8c00',fontWeight:'bold'}}>Piano Roll</span>
        <div style={{flex:1}}></div>
        <span style={{fontSize:11,color:'#888'}}>Snap:</span>
        <span style={{fontSize:11,color:'#ccc'}}>{snap}</span>
        {onClose && (
          <button className="tool-btn" onClick={onClose} title="Close">✕</button>
        )}
      </div>
      <div className="pr-grid-wrap" style={{flex:1,overflow:'auto'}}>
        <div className="pr-grid" style={{minWidth:2000,minHeight:600,position:'relative'}}>
          <div className="pr-keys" style={{position:'absolute',left:0,top:0,width:60,height:960,zIndex:2,borderRight:'1px solid #444',background:'#2a2a2a'}}>
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
            style={{marginLeft:60,position:'relative',height:960,cursor:'crosshair'}}>
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
            {pianoNotes.map((pn, idx) => (
              <div key={idx} className="pr-note"
                style={{
                  left: pn.start * pxPerBeat,
                  top: (96 - pn.note) * noteHeight,
                  width: Math.max(4, pn.length * pxPerBeat - 2)
                }}
                onMouseDown={e => handleNoteMouseDown(e, idx, pn)}
                onClick={e => { e.stopPropagation(); if (!dragRef.current) onDeleteNote(idx); }}
              ></div>
            ))}
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
