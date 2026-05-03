import { useRef, useEffect, useState } from 'react';

const basePxPerBeat = 40;
const trackHeight = 40;

export default function Playlist({ playlistBlocks, patterns, onAddBlock, onDeleteBlock, onMoveBlock, zoom, snap, playing, bpm }) {
  const areaRef = useRef(null);
  const [playheadPos, setPlayheadPos] = useState(0);
  const pxPerBeat = basePxPerBeat * zoom;

  // Drag state
  const dragRef = useRef(null); // { track, idx, startX, origStart }

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
    return 0.5;
  };

  const snapVal = parseSnap(snap);

  const handleClick = (e) => {
    if (dragRef.current) return;
    if (!areaRef.current) return;
    const rect = areaRef.current.getBoundingClientRect();
    const x = e.clientX - rect.left + areaRef.current.scrollLeft;
    const y = e.clientY - rect.top + areaRef.current.scrollTop;
    const trackIdx = Math.floor(y / trackHeight);
    if (trackIdx < 0 || trackIdx >= playlistBlocks.length) return;
    const beat = x / pxPerBeat;
    const snapped = Math.floor(beat / snapVal) * snapVal;
    onAddBlock(trackIdx, snapped);
  };

  const handleBlockMouseDown = (e, trackIdx, blockIdx, block) => {
    e.stopPropagation();
    dragRef.current = {
      track: trackIdx,
      idx: blockIdx,
      startX: e.clientX,
      origStart: block.start
    };
    window.addEventListener('mousemove', handleMouseMove);
    window.addEventListener('mouseup', handleMouseUp);
  };

  const handleMouseMove = (e) => {
    if (!dragRef.current || !areaRef.current) return;
    const { track, idx, startX, origStart } = dragRef.current;
    const deltaX = (e.clientX - startX) / pxPerBeat;
    const newStart = Math.max(0, Math.floor((origStart + deltaX) / snapVal) * snapVal);
    if (onMoveBlock) onMoveBlock(track, idx, { start: newStart });
  };

  const handleMouseUp = () => {
    dragRef.current = null;
    window.removeEventListener('mousemove', handleMouseMove);
    window.removeEventListener('mouseup', handleMouseUp);
  };

  return (
    <div className="arrangement">
      <div className="arr-header">
        <span className="arr-title">Playlist</span>
        <div style={{flex:1}} />
        <span style={{fontSize:10,color:'#888'}}>Snap: {snap}</span>
      </div>
      <div className="arr-timeline">
        <div className="arr-grid" style={{minHeight:playlistBlocks.length*32+10}}>
          <div className="playlist-ruler">
            {Array.from({length:18},(_,i)=>(
              <div key={i} className="playlist-ruler-mark" style={{left:i*pxPerBeat*4}}>
                {i + 1}
              </div>
            ))}
          </div>
          <div className="arr-track-header" style={{minHeight:playlistBlocks.length*32}}>
            {playlistBlocks.map((_, ti) => (
              <div key={ti} className="arr-track-name">Track {ti + 1}</div>
            ))}
          </div>
          <div className="arr-track-area" ref={areaRef} onClick={handleClick}
            style={{minHeight:playlistBlocks.length*32}}>
            {playlistBlocks.map((blocks, ti) => (
              <div key={ti} className="arr-track-row">
                {blocks.map((blk, bi) => (
                  <div key={bi} className="arr-block"
                    style={{left:blk.start*pxPerBeat,width:blk.length*pxPerBeat}}
                    onMouseDown={e => handleBlockMouseDown(e, ti, bi, blk)}
                    onClick={e => { e.stopPropagation(); if (!dragRef.current) onDeleteBlock(ti, bi); }}>
                    {patterns[blk.pattern]?.name || 'Pattern'}
                  </div>
                ))}
              </div>
            ))}
            {Array.from({length:65},(_,i)=>{
              return (
                <div key={`bl-${i}`} className="arr-bar-line"
                  style={{left:i*pxPerBeat}}></div>
              );
            })}
            <div className="arr-playhead" style={{
              display: playing ? 'block' : 'none',
              left: playheadPos
            }}></div>
          </div>
        </div>
      </div>
    </div>
  );
}
