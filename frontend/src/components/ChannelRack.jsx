import { useState, useRef } from 'react';

const BUILT_IN_INSTRUMENTS = [
  { label: 'Kick Drum',    type: 'kick' },
  { label: 'Snare',        type: 'snare' },
  { label: 'Hi-Hat',       type: 'hihat' },
  { label: 'Open Hi-Hat',  type: 'hihat_open' },
  { label: 'Clap',         type: 'clap' },
  { label: 'Bass Synth',   type: 'bass' },
  { label: 'Lead Synth',   type: 'lead' },
  { label: 'Pad',          type: 'pad' },
  { label: 'Sample…',      type: '__sample__' },
];

export default function ChannelRack({
  channels, onStepToggle, onMute, onSolo, onVolChange, onPanChange,
  onAddChannel, playing, stepIndex, currentPattern, patterns,
  onPatternChange, onNewPattern, onOpenPiano, onDeleteChannel,
  onLoadSample, onAddInstrument, onStepCountChange,
}) {
  const [view, setView] = useState('step'); // 'step' | 'graph'
  const [selectedCh, setSelectedCh] = useState(null);
  const [showAddMenu, setShowAddMenu] = useState(false);
  const [stepCount, setStepCount] = useState(16);
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
        <div style={{ width: 1, background: '#3f3f46', margin: '0 4px', alignSelf: 'stretch' }} />
        {[16, 32].map(n => (
          <button key={n} className={`tool-btn ${stepCount === n ? 'active' : ''}`}
            onClick={() => setStepCount(n)} title={`${n} steps`}>{n}</button>
        ))}
      </div>

      {/* ── Channel rows ──────────────────────────────────────────────────── */}
      <div className="rack-body" style={{ flex: 1, overflowY: 'auto' }}>
        {channels.map((ch, ci) => (
          <div 
            key={ci} 
            className={`channel-row ${selectedCh === ci ? 'selected' : ''}`}
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
              e.stopPropagation(); // Stop it from adding a duplicate channel in the workspace drop handler
              dragCounters.current[ci] = 0;
              e.currentTarget.classList.remove('drag-over');
              
              let data = window.__draggedSample;
              if (!data && e.dataTransfer.files && e.dataTransfer.files.length > 0) {
                const file = e.dataTransfer.files[0];
                // Check if it's an audio file
                if (/\.(wav|mp3|ogg|flac|aiff?|m4a)$/i.test(file.name)) {
                  data = { path: file.path, name: file.name };
                }
              }
              
              window.__draggedSample = null;
              if (data && data.path && onLoadSample) onLoadSample(ci, data);
            }}
          >
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
{(() => {
                const displaySteps = ch.steps.length >= stepCount
                  ? ch.steps.slice(0, stepCount)
                  : [...ch.steps, ...Array(stepCount - ch.steps.length).fill(0)];
                return view === 'step' ? (
                  <div className="steps" style={{ display: 'flex', flexWrap: 'nowrap', gap: 1 }}>
                    {displaySteps.map((on, si) => (
                      <div
                        key={si}
                        className={`step ${on ? 'active' : ''} ${playing && stepIndex === si ? 'current' : ''} ${si === 4 || si === 8 || si === 12 || si === 16 || si === 20 || si === 24 || si === 28 ? 'beat-sep' : ''}`}
                        onClick={() => onStepToggle(ci, si)}
                        title={`Step ${si + 1}`}
                        style={{ width: stepCount === 32 ? 11 : undefined, marginLeft: (si === 4 || si === 8 || si === 12 || si === 16 || si === 20 || si === 24 || si === 28) ? 4 : undefined }}
                      />
                    ))}
                  </div>
                ) : (
                  <div className="steps" style={{ alignItems: 'flex-end', gap: 1 }}>
                    {displaySteps.map((on, si) => (
                      <div
                        key={si}
                        title={`Step ${si + 1}`}
                        onClick={() => onStepToggle(ci, si)}
                        style={{
                          width: stepCount === 32 ? 9 : 14, cursor: 'pointer',
                          height: on ? 18 : 4,
                          background: on
                            ? (playing && stepIndex === si ? '#fff' : '#f97316')
                            : '#27272a',
                          borderRadius: 2,
                          transition: 'height .1s',
                          marginLeft: (si === 4 || si === 8 || si === 12 || si === 16 || si === 20 || si === 24 || si === 28) ? 3 : undefined,
                        }}
                      />
                    ))}
                  </div>
                );
              })()}

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

      {/* ── Add Instrument button (FL Studio style) ──────────────────────── */}
      <div style={{ flexShrink: 0, padding: '6px 8px', borderTop: '1px solid #27272a', position: 'relative' }}>
        <button
          className="tool-btn"
          style={{
            width: '100%', padding: '5px 0', fontSize: 13, letterSpacing: 1,
            border: '1px solid #f97316', color: '#f97316', background: 'transparent',
            borderRadius: 4, cursor: 'pointer',
          }}
          onClick={() => setShowAddMenu(v => !v)}
        >
          +
        </button>
        {showAddMenu && (
          <div style={{
            position: 'absolute', bottom: '100%', left: 8, right: 8,
            background: '#18181b', border: '1px solid #3f3f46', borderRadius: 6,
            zIndex: 9999, boxShadow: '0 4px 24px #000a',
            maxHeight: 320, overflowY: 'auto',
          }}>
            <div style={{ padding: '6px 10px', fontSize: 9, color: '#71717a', textTransform: 'uppercase', letterSpacing: 1 }}>
              Built-in Instruments
            </div>
            {BUILT_IN_INSTRUMENTS.map(inst => (
              <div
                key={inst.type}
                style={{ padding: '6px 14px', cursor: 'pointer', fontSize: 12, color: '#d4d4d8' }}
                onMouseEnter={e => e.currentTarget.style.background = '#27272a'}
                onMouseLeave={e => e.currentTarget.style.background = 'transparent'}
                onClick={() => {
                  setShowAddMenu(false);
                  if (inst.type === '__sample__') {
                    // open file dialog to pick a sample
                    window.electronAPI?.invoke('dialog:openFile', 'Select audio sample')
                      .then(path => {
                        if (path) {
                          const name = path.split(/[\\/]/).pop().replace(/\.[^.]+$/, '');
                          onAddInstrument && onAddInstrument({ name, type: path, color: '#f97316' });
                        }
                      });
                  } else {
                    onAddInstrument && onAddInstrument({ name: inst.label, type: inst.type });
                  }
                }}
              >
                {inst.label}
              </div>
            ))}
            <div style={{ padding: '6px 10px', fontSize: 9, color: '#71717a', textTransform: 'uppercase', letterSpacing: 1, borderTop: '1px solid #27272a', marginTop: 4 }}>
              VST / DLL Plugins
            </div>
            <div
              style={{ padding: '6px 14px', cursor: 'pointer', fontSize: 12, color: '#d4d4d8' }}
              onMouseEnter={e => e.currentTarget.style.background = '#27272a'}
              onMouseLeave={e => e.currentTarget.style.background = 'transparent'}
              onClick={() => {
                setShowAddMenu(false);
                window.electronAPI?.invoke('dialog:openFile', 'Select VST3 Plugin')
                  .then(path => {
                    if (path) {
                      const name = path.split(/[\\/]/).pop().replace(/\.[^.]+$/, '');
                      onAddInstrument && onAddInstrument({ name, type: `vst:${path}`, color: '#3b82f6' });
                    }
                  });
              }}
            >
              Load VST3 Plugin (.vst3)…
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
