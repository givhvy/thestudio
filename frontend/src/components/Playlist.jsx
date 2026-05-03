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
    <div className="playlist" style={{height:'100%',display:'flex',flexDirection:'column'}}>
      <div className="pl-toolbar">
        <span style={{color:'#ff8c00',fontWeight:'bold'}}>Playlist</span>
        <div style={{flex:1}}></div>
        <span style={{fontSize:11,color:'#888'}}>Snap:</span>
        <span style={{fontSize:11,color:'#ccc'}}>{snap}</span>
      </div>
      <div className="pl-timeline" style={{flex:1,overflow:'auto'}}>
        <div className="pl-grid" style={{minWidth:3000,minHeight:playlistBlocks.length*trackHeight,position:'relative'}}>
          <div className="pl-track-header" style={{position:'absolute',left:0,top:0,width:120,zIndex:2,borderRight:'1px solid #444',background:'#2a2a2a'}}>
            {playlistBlocks.map((_, ti) => (
              <div key={ti} className="pl-track-name">Track {ti + 1}</div>
            ))}
          </div>
          <div className="pl-track-area" ref={areaRef} onClick={handleClick}
            style={{marginLeft:120,position:'relative',minHeight:playlistBlocks.length*trackHeight,cursor:'crosshair'}}>
            {playlistBlocks.map((blocks, ti) => (
              <div key={ti} className="pl-track-row" style={{height:trackHeight,position:'relative'}}>
                {blocks.map((blk, bi) => (
                  <div key={bi} className="pl-block"
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
                <div key={`bl-${i}`} className="pl-bar-line"
                  style={{left:i*pxPerBeat,position:'absolute',top:0,bottom:0}}></div>
              );
            })}
            <div className="pl-playhead" style={{
              display: playing ? 'block' : 'none',
              left: playheadPos
            }}></div>
          </div>
        </div>
      </div>
    </div>
  );
}
