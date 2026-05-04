import { useRef, useState } from 'react';
import { loadSample, initAudio } from '../audio.js';

const basePxPerBeat = 40;
const trackHeight = 42; // matches .arr-track-name height from App.css

export default function Playlist({ playlistBlocks, patterns, onAddBlock, onDeleteBlock, onMoveBlock, zoom, snap, playing, bpm, onAddAudioClip, transportTime }) {
  const areaRef = useRef(null);
  const [dropTarget, setDropTarget] = useState(null);
  const dragCounters = useRef({});
  const blockDrag = useRef(null);
  const pxPerBeat = basePxPerBeat * zoom;

  // Playhead uses AudioContext-based transportTime
  const playheadPos = playing ? (transportTime / (60 / Math.max(1, bpm))) * pxPerBeat : 0;

  const parseSnap = (s) => {
    if (s === 'Bar') return 4; if (s === '1/2') return 2; if (s === '1/4') return 1;
    if (s === '1/8') return 0.5; if (s === '1/16') return 0.25; return 0.5;
  };
  const snapVal = parseSnap(snap);

  // --- Grid click to add block ---
  const handleAreaClick = (e) => {
    if (blockDrag.current) return;
    if (!areaRef.current) return;
    const rect = areaRef.current.getBoundingClientRect();
    const x = e.clientX - rect.left + areaRef.current.scrollLeft;
    const y = e.clientY - rect.top + areaRef.current.scrollTop;
    const ti = Math.floor(y / trackHeight);
    if (ti < 0 || ti >= playlistBlocks.length) return;
    const beat = Math.max(0, Math.floor((x / pxPerBeat) / snapVal) * snapVal);
    onAddBlock(ti, beat);
  };

  // --- Block drag (move + cross-track + resize) ---
  const startBlockDrag = (e, ti, bi, blk, isResize) => {
    e.stopPropagation();
    blockDrag.current = { ti, bi, isResize, startX: e.clientX, startY: e.clientY, origStart: blk.start, origLength: blk.length, origTrack: ti, curTrack: ti };
    window.addEventListener('mousemove', onBlockMove);
    window.addEventListener('mouseup', onBlockUp);
  };

  const onBlockMove = (e) => {
    if (!blockDrag.current) return;
    const d = blockDrag.current;
    const deltaX = (e.clientX - d.startX) / pxPerBeat;
    if (d.isResize) {
      const newLen = Math.max(snapVal, Math.round((d.origLength + deltaX) / snapVal) * snapVal);
      onMoveBlock(d.ti, d.bi, { length: newLen });
    } else {
      const newStart = Math.max(0, Math.floor((d.origStart + deltaX) / snapVal) * snapVal);
      const deltaY = e.clientY - d.startY;
      const newTrack = Math.max(0, Math.min(playlistBlocks.length - 1, d.origTrack + Math.round(deltaY / trackHeight)));
      if (newTrack !== d.curTrack) {
        const blk = playlistBlocks[d.ti][d.bi];
        onDeleteBlock(d.ti, d.bi);
        onAddBlock(newTrack, newStart, blk);
        d.ti = newTrack; d.bi = playlistBlocks[newTrack]?.length ?? 0; d.curTrack = newTrack;
      } else {
        onMoveBlock(d.ti, d.bi, { start: newStart });
      }
    }
  };

  const onBlockUp = () => {
    blockDrag.current = null;
    window.removeEventListener('mousemove', onBlockMove);
    window.removeEventListener('mouseup', onBlockUp);
  };

  // --- Audio file drop ---
  const handleDragOver = (e, ti) => { if (e.dataTransfer.types.includes('Files')) { e.preventDefault(); e.dataTransfer.dropEffect = 'copy'; } };
  const handleDragEnter = (e, ti) => { if (!e.dataTransfer.types.includes('Files')) return; dragCounters.current[ti] = (dragCounters.current[ti]||0)+1; setDropTarget(ti); };
  const handleDragLeave = (e, ti) => { dragCounters.current[ti] = Math.max(0,(dragCounters.current[ti]||0)-1); if (!dragCounters.current[ti]) setDropTarget(p=>p===ti?null:p); };
  const handleDrop = async (e, ti) => {
    e.preventDefault(); e.stopPropagation();
    dragCounters.current[ti] = 0; setDropTarget(null);
    const files = Array.from(e.dataTransfer.files).filter(f => /\.(wav|mp3|ogg|flac|aiff?|m4a)$/i.test(f.name));
    if (!files.length) return;
    const rect = e.currentTarget.getBoundingClientRect();
    const x = e.clientX - rect.left + (areaRef.current?.scrollLeft || 0);
    const beat = Math.max(0, Math.floor((x / pxPerBeat) / snapVal) * snapVal);
    for (const file of files) {
      try {
        initAudio();
        const buf = await file.arrayBuffer();
        const decoded = await loadSample(file.name, buf);
        const duration = decoded?.duration ?? 2;
        const length = Math.max(snapVal, Math.ceil((duration / (60 / Math.max(1, bpm))) / snapVal) * snapVal);
        onAddAudioClip?.(ti, { sampleName: file.name, start: beat, length });
      } catch (err) { console.error('[Playlist] drop failed:', err); }
    }
  };

  return (
    <div className="arrangement">
      <div className="arr-header">
        <span className="arr-title">Playlist</span>
        <div style={{flex:1}}/>
        <span style={{fontSize:10,color:'#52525b'}}>Snap: {snap}</span>
      </div>

      <div className="arr-timeline" ref={areaRef} onClick={handleAreaClick}>
        <div className="arr-grid" style={{minHeight: playlistBlocks.length * trackHeight + 10}}>

          {/* Ruler */}
          <div className="playlist-ruler">
            {Array.from({length:18},(_,i)=>(
              <div key={i} className="playlist-ruler-mark" style={{left: i * pxPerBeat * 4}}>
                {i+1}
              </div>
            ))}
          </div>

          {/* Track name headers */}
          <div className="arr-track-header" style={{minHeight: playlistBlocks.length * trackHeight}}>
            {playlistBlocks.map((_, ti) => (
              <div key={ti} className="arr-track-name">Track {ti+1}</div>
            ))}
          </div>

          {/* Track rows + blocks */}
          <div className="arr-track-area" style={{minHeight: playlistBlocks.length * trackHeight}}>
            {playlistBlocks.map((blocks, ti) => (
              <div key={ti} className="arr-track-row"
                style={{ background: dropTarget===ti ? 'rgba(249,115,22,0.06)' : undefined, outline: dropTarget===ti ? '1px dashed #f97316' : undefined }}
                onDragOver={e=>handleDragOver(e,ti)}
                onDragEnter={e=>handleDragEnter(e,ti)}
                onDragLeave={e=>handleDragLeave(e,ti)}
                onDrop={e=>handleDrop(e,ti)}
              >
                {blocks.map((blk, bi) => {
                  const isAudio = !!blk.sampleName;
                  return (
                    <div key={bi} className="arr-block"
                      style={{
                        left: blk.start * pxPerBeat,
                        width: Math.max(20, blk.length * pxPerBeat),
                        background: isAudio ? '#1a3352' : undefined,
                        borderLeft: isAudio ? '2px solid #3b82f6' : undefined,
                        overflow: 'hidden', position: 'absolute',
                      }}
                      onMouseDown={e => startBlockDrag(e, ti, bi, blk, false)}
                      onContextMenu={e => { e.preventDefault(); e.stopPropagation(); onDeleteBlock(ti, bi); }}
                    >
                      <span style={{fontSize:9, color: isAudio?'#93c5fd':undefined, paddingLeft:3, whiteSpace:'nowrap', overflow:'hidden', display:'block'}}>
                        {isAudio ? blk.sampleName.replace(/\.[^.]+$/,'') : (patterns[blk.pattern]?.name||'Pattern')}
                      </span>
                      <div style={{position:'absolute',right:0,top:0,bottom:0,width:6,cursor:'ew-resize',background:'rgba(255,255,255,0.06)'}}
                        onMouseDown={e=>{e.stopPropagation(); startBlockDrag(e,ti,bi,blk,true);}} />
                    </div>
                  );
                })}
              </div>
            ))}

            {/* Grid bar lines */}
            {Array.from({length:65},(_,i)=>(
              <div key={`bl-${i}`} className="arr-bar-line" style={{left:i*pxPerBeat}}/>
            ))}

            {/* Playhead */}
            <div className="arr-playhead" style={{display:playing?'block':'none', left:playheadPos}}/>
          </div>
        </div>
      </div>
    </div>
  );
}
