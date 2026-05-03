import { useState, useRef } from 'react';

export default function ChannelRack({
  channels, onStepToggle, onMute, onSolo, onVolChange, onPanChange,
  onAddChannel, playing, stepIndex, currentPattern, patterns,
  onPatternChange, onNewPattern, onOpenPiano, onDeleteChannel,
  onLoadSample,
}) {
  const [view, setView] = useState('step'); // 'step' | 'graph'
  const [selectedCh, setSelectedCh] = useState(null);
  const dragCounters = useRef({}); // per-channel drag-enter counter

  function handleChNameClick(ci) {
    setSelectedCh(ci === selectedCh ? null : ci);
    onOpenPiano && onOpenPiano(ci);
  }

  function handleFillAll(ci) {
    for (let si = 0; si < (channels[ci]?.steps.length ?? 16); si++) {
      if (!channels[ci].steps[si]) onStepToggle(ci, si);
    }
  }
  function handleClearAll(ci) {
    for (let si = 0; si < (channels[ci]?.steps.length ?? 16); si++) {
      if (channels[ci].steps[si]) onStepToggle(ci, si);
    }
  }
  function handleEveryOther(ci) {
    channels[ci]?.steps.forEach((on, si) => {
      const want = si % 2 === 0;
      if (on !== want) onStepToggle(ci, si);
    });
  }

  return (
    <div className="channel-rack" style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      {/* ── Sub-toolbar ───────────────────────────────────────────────────── */}
      <div className="rack-header" style={{ display: 'flex', alignItems: 'center', gap: 4, flexShrink: 0 }}>
        <span className="rack-title">Channel Rack</span>
        <div style={{ flex: 1 }} />

        {/* Pattern selector */}
        <select
          value={currentPattern}
          onChange={e => onPatternChange(Number(e.target.value))}
          style={{ background: '#18181b', color: '#d4d4d8', border: '1px solid #3f3f46', borderRadius: 4, fontSize: 10, padding: '1px 4px' }}
        >
          {patterns?.map((p, i) => <option key={i} value={i}>{p.name}</option>)}
        </select>
        <button className="tool-btn" onClick={onNewPattern} title="New Pattern">+</button>

        <div style={{ width: 1, background: '#3f3f46', margin: '0 4px', alignSelf: 'stretch' }} />

        {/* View toggle */}
        <button className={`tool-btn ${view==='step'?'active':''}`} onClick={() => setView('step')}>Step</button>
        <button className={`tool-btn ${view==='graph'?'active':''}`} onClick={() => setView('graph')}>Graph</button>

        <div style={{ width: 1, background: '#3f3f46', margin: '0 4px', alignSelf: 'stretch' }} />

        <button className="tool-btn" onClick={onAddChannel} title="Add Channel">+ Ch</button>
      </div>

      {/* ── Channel rows ──────────────────────────────────────────────────── */}
      <div className="rack-body" style={{ flex: 1, overflowY: 'auto' }}>
        {channels.map((ch, ci) => (
          <div key={ci} className={`channel-row ${selectedCh === ci ? 'selected' : ''}`}>
            <div className="ch-index">{ci + 1}</div>

            {/* Activity LED — lights on current step */}
            <div className={`ch-led ${playing && stepIndex !== undefined && ch.steps[stepIndex] ? 'on' : ''}`} />

            {/* Mute */}
            <div className={`ch-mute ${ch.mute ? 'on' : ''}`} onClick={() => onMute(ci)} title="Mute">M</div>

            {/* Solo */}
            <div className={`ch-solo ${ch.solo ? 'on' : ''}`} onClick={() => onSolo(ci)} title="Solo">S</div>

            {/* Channel name — click opens piano roll, drop loads sample */}
            <div
              className="ch-name"
              onClick={() => handleChNameClick(ci)}
              onDragOver={(e) => { e.preventDefault(); e.dataTransfer.dropEffect = 'copy'; }}
              onDragEnter={(e) => {
                dragCounters.current[ci] = (dragCounters.current[ci] || 0) + 1;
                e.currentTarget.classList.add('drag-over');
              }}
              onDragLeave={(e) => {
                dragCounters.current[ci] = Math.max(0, (dragCounters.current[ci] || 0) - 1);
                if (!dragCounters.current[ci]) e.currentTarget.classList.remove('drag-over');
              }}
              onDrop={(e) => {
                e.preventDefault();
                dragCounters.current[ci] = 0;
                e.currentTarget.classList.remove('drag-over');
                let raw = '';
                try { raw = e.dataTransfer.getData('text/plain'); } catch {}
                if (!raw) return;
                try {
                  const data = JSON.parse(raw);
                  if (data.path && onLoadSample) onLoadSample(ci, data);
                } catch (err) { console.error('[ChannelRack] drop parse failed:', err); }
              }}
              title={`${ch.name} — click to open Piano Roll, drop sample here`}
              style={{ cursor: 'pointer', color: selectedCh === ci ? '#f97316' : undefined }}
            >
              {ch.name}
            </div>

            {/* Pan knob */}
            <input
              className="ch-pan"
              type="range" min="-100" max="100" value={ch.pan}
              onChange={e => onPanChange(ci, parseInt(e.target.value))}
              title={`Pan: ${ch.pan}`}
            />

            {/* Volume knob */}
            <input
              className="ch-vol"
              type="range" min="0" max="100" value={ch.vol}
              onChange={e => onVolChange(ci, parseInt(e.target.value))}
              title={`Volume: ${ch.vol}`}
            />

            {/* Steps or Graph */}
            {view === 'step' ? (
              <div className="steps">
                {ch.steps.map((on, si) => (
                  <div
                    key={si}
                    className={`step ${on ? 'active' : ''} ${playing && stepIndex === si ? 'current' : ''}`}
                    onClick={() => onStepToggle(ci, si)}
                    title={`Step ${si + 1}`}
                  />
                ))}
              </div>
            ) : (
              /* Graph view — velocity bars */
              <div className="steps" style={{ alignItems: 'flex-end', gap: 1 }}>
                {ch.steps.map((on, si) => (
                  <div
                    key={si}
                    title={`Step ${si + 1}`}
                    onClick={() => onStepToggle(ci, si)}
                    style={{
                      width: 14, cursor: 'pointer',
                      height: on ? 18 : 4,
                      background: on
                        ? (playing && stepIndex === si ? '#fff' : '#f97316')
                        : '#27272a',
                      borderRadius: 2,
                      transition: 'height .1s',
                    }}
                  />
                ))}
              </div>
            )}

            {/* Quick-fill context buttons (shown when channel selected) */}
            {selectedCh === ci && (
              <div style={{ display: 'flex', gap: 3, marginLeft: 6, flexShrink: 0 }}>
                <button className="tool-btn" style={{ fontSize: 8, padding: '0 5px' }} onClick={() => handleFillAll(ci)} title="Fill all steps">Fill</button>
                <button className="tool-btn" style={{ fontSize: 8, padding: '0 5px' }} onClick={() => handleEveryOther(ci)} title="Every other step">1/2</button>
                <button className="tool-btn" style={{ fontSize: 8, padding: '0 5px' }} onClick={() => handleClearAll(ci)} title="Clear all steps">Clr</button>
                {onDeleteChannel && (
                  <button
                    className="tool-btn"
                    style={{ fontSize: 8, padding: '0 5px', color: '#ef4444' }}
                    onClick={() => { onDeleteChannel(ci); setSelectedCh(null); }}
                    title="Delete channel"
                  >Del</button>
                )}
              </div>
            )}
          </div>
        ))}
      </div>
    </div>
  );
}
