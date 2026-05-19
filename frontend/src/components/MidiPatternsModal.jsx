import { useState, useRef, useCallback } from 'react';

// ── Genre-based MIDI step patterns ──────────────────────────────────────────
// Each pattern is a 16-step array (1 = on, 0 = off)
const GENRE_PATTERNS = {
  trap: {
    label: 'Trap',
    emoji: '🔥',
    color: '#ef4444',
    instruments: {
      kick:       { name: 'Trap Kick',       steps: [1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0] },
      snare:      { name: 'Trap Snare',      steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0] },
      hihat:      { name: 'Trap Hi-Hat',      steps: [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1] },
      hihat_open: { name: 'Trap Open HH',     steps: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0] },
      clap:       { name: 'Trap Clap',        steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0] },
    },
  },
  'trap-hard': {
    label: 'Trap (Hard)',
    emoji: '💀',
    color: '#dc2626',
    instruments: {
      kick:       { name: 'Hard Kick',        steps: [1,0,0,1,0,0,1,0,0,0,1,0,0,1,0,0] },
      snare:      { name: 'Hard Snare',       steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,1] },
      hihat:      { name: 'Hard Hi-Hat Roll',  steps: [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1] },
      hihat_open: { name: 'Hard Open HH',     steps: [0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1] },
      clap:       { name: 'Hard Clap',        steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0] },
    },
  },
  hiphop: {
    label: 'Hip-Hop',
    emoji: '🎤',
    color: '#f59e0b',
    instruments: {
      kick:       { name: 'Boom Bap Kick',    steps: [1,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0] },
      snare:      { name: 'Boom Bap Snare',   steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0] },
      hihat:      { name: 'Boom Bap HH',      steps: [1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0] },
      hihat_open: { name: 'Boom Bap Open HH', steps: [0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1] },
      clap:       { name: 'Boom Bap Clap',    steps: [0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0] },
    },
  },
  drill: {
    label: 'Drill',
    emoji: '🇬🇧',
    color: '#8b5cf6',
    instruments: {
      kick:       { name: 'Drill Kick',       steps: [1,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0] },
      snare:      { name: 'Drill Snare',      steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0] },
      hihat:      { name: 'Drill Hi-Hat',      steps: [1,0,1,1,0,1,1,0,1,0,1,1,0,1,1,0] },
      hihat_open: { name: 'Drill Open HH',    steps: [0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1] },
      clap:       { name: 'Drill Clap',       steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0] },
    },
  },
  house: {
    label: 'House',
    emoji: '🏠',
    color: '#06b6d4',
    instruments: {
      kick:       { name: 'Four on Floor',    steps: [1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0] },
      snare:      { name: 'House Snare',      steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0] },
      hihat:      { name: 'House Hi-Hat',      steps: [0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0] },
      hihat_open: { name: 'House Open HH',    steps: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0] },
      clap:       { name: 'House Clap',       steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0] },
    },
  },
  rnb: {
    label: 'R&B',
    emoji: '💜',
    color: '#ec4899',
    instruments: {
      kick:       { name: 'R&B Kick',         steps: [1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,0] },
      snare:      { name: 'R&B Snare',        steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0] },
      hihat:      { name: 'R&B Hi-Hat',        steps: [1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0] },
      hihat_open: { name: 'R&B Open HH',      steps: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1] },
      clap:       { name: 'R&B Clap',         steps: [0,0,0,0,1,0,0,1,0,0,0,0,1,0,0,1] },
    },
  },
  reggaeton: {
    label: 'Reggaeton',
    emoji: '🌴',
    color: '#22c55e',
    instruments: {
      kick:       { name: 'Dembow Kick',      steps: [1,0,0,1,0,0,1,0,0,0,1,0,0,1,0,0] },
      snare:      { name: 'Dembow Snare',     steps: [0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1] },
      hihat:      { name: 'Dembow HH',        steps: [1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0] },
      hihat_open: { name: 'Dembow Open HH',   steps: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0] },
      clap:       { name: 'Dembow Clap',      steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0] },
    },
  },
  lofi: {
    label: 'Lo-Fi',
    emoji: '🌧️',
    color: '#a78bfa',
    instruments: {
      kick:       { name: 'Lo-Fi Kick',       steps: [1,0,0,0,0,0,0,1,0,0,1,0,0,0,0,0] },
      snare:      { name: 'Lo-Fi Snare',      steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0] },
      hihat:      { name: 'Lo-Fi Hi-Hat',      steps: [0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1] },
      hihat_open: { name: 'Lo-Fi Open HH',    steps: [0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0] },
      clap:       { name: 'Lo-Fi Clap',       steps: [0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0] },
    },
  },
  afrobeats: {
    label: 'Afrobeats',
    emoji: '🌍',
    color: '#f97316',
    instruments: {
      kick:       { name: 'Afro Kick',        steps: [1,0,0,0,0,1,0,0,1,0,0,0,0,1,0,0] },
      snare:      { name: 'Afro Snare',       steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0] },
      hihat:      { name: 'Afro HH',          steps: [1,0,1,1,0,1,0,1,1,0,1,1,0,1,0,1] },
      hihat_open: { name: 'Afro Open HH',     steps: [0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0] },
      clap:       { name: 'Afro Clap',        steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,1] },
    },
  },
  phonk: {
    label: 'Phonk',
    emoji: '🚗',
    color: '#b91c1c',
    instruments: {
      kick:       { name: 'Phonk Kick',       steps: [1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0] },
      snare:      { name: 'Phonk Snare',      steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0] },
      hihat:      { name: 'Phonk Hi-Hat',      steps: [1,1,0,1,1,0,1,1,1,1,0,1,1,0,1,1] },
      hihat_open: { name: 'Phonk Cowbell',     steps: [0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0] },
      clap:       { name: 'Phonk Clap',       steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0] },
    },
  },
};

// Map channel types to instrument keys
function getInstrumentKey(channelType) {
  const type = (channelType || '').toLowerCase();
  if (type.includes('kick')) return 'kick';
  if (type.includes('snare')) return 'snare';
  if (type.includes('hihat') || type.includes('hi-hat') || type.includes('hi hat')) {
    if (type.includes('open')) return 'hihat_open';
    return 'hihat';
  }
  if (type.includes('clap')) return 'clap';
  return null;
}

export default function MidiPatternsModal({ channel, channelIndex, onApplyPattern, onClose }) {
  const [selectedGenre, setSelectedGenre] = useState(null);
  const [customPatterns, setCustomPatterns] = useState([]); // user-dropped MIDI patterns
  const [isDragging, setIsDragging] = useState(false);
  const [previewPattern, setPreviewPattern] = useState(null);
  const dropRef = useRef(null);
  const dragCounter = useRef(0);

  const instrumentKey = getInstrumentKey(channel.type);
  const currentSteps = channel.steps || Array(16).fill(0);

  // Parse a MIDI file (basic: extract note-on events and quantize to 16 steps)
  const parseMidiToSteps = useCallback((arrayBuffer) => {
    try {
      const view = new DataView(arrayBuffer);
      // Basic MIDI header check
      const header = String.fromCharCode(view.getUint8(0), view.getUint8(1), view.getUint8(2), view.getUint8(3));
      if (header !== 'MThd') {
        // Not a MIDI file — try to interpret as raw pattern data
        return null;
      }
      
      // Simple MIDI parser — extract note-on events from track 1
      const steps = Array(16).fill(0);
      let offset = 14; // Skip header chunk
      
      // Find MTrk
      while (offset < arrayBuffer.byteLength - 8) {
        const chunkType = String.fromCharCode(
          view.getUint8(offset), view.getUint8(offset+1),
          view.getUint8(offset+2), view.getUint8(offset+3)
        );
        const chunkSize = view.getUint32(offset + 4);
        
        if (chunkType === 'MTrk') {
          // Parse track events
          let trackOffset = offset + 8;
          const trackEnd = trackOffset + chunkSize;
          let totalTicks = 0;
          const ppq = view.getUint16(12); // ticks per quarter note
          const ticksPerStep = (ppq * 4) / 16; // assuming 4/4, 16 steps per bar
          
          while (trackOffset < trackEnd && trackOffset < arrayBuffer.byteLength) {
            // Read variable-length delta time
            let delta = 0;
            let byte;
            do {
              byte = view.getUint8(trackOffset++);
              delta = (delta << 7) | (byte & 0x7F);
            } while (byte & 0x80 && trackOffset < trackEnd);
            
            totalTicks += delta;
            
            // Read event
            if (trackOffset >= trackEnd) break;
            const status = view.getUint8(trackOffset);
            
            if ((status & 0xF0) === 0x90) {
              // Note On
              trackOffset++;
              const note = view.getUint8(trackOffset++);
              const velocity = view.getUint8(trackOffset++);
              if (velocity > 0) {
                const stepIdx = Math.round(totalTicks / ticksPerStep) % 16;
                steps[stepIdx] = 1;
              }
            } else if ((status & 0xF0) === 0x80) {
              trackOffset += 3; // Note Off: status + note + velocity
            } else if ((status & 0xF0) === 0xA0) {
              trackOffset += 3;
            } else if ((status & 0xF0) === 0xB0) {
              trackOffset += 3;
            } else if ((status & 0xF0) === 0xC0) {
              trackOffset += 2;
            } else if ((status & 0xF0) === 0xD0) {
              trackOffset += 2;
            } else if ((status & 0xF0) === 0xE0) {
              trackOffset += 3;
            } else if (status === 0xFF) {
              // Meta event
              trackOffset++;
              if (trackOffset >= trackEnd) break;
              const metaType = view.getUint8(trackOffset++);
              let metaLen = 0;
              do {
                if (trackOffset >= trackEnd) break;
                byte = view.getUint8(trackOffset++);
                metaLen = (metaLen << 7) | (byte & 0x7F);
              } while (byte & 0x80);
              trackOffset += metaLen;
              if (metaType === 0x2F) break; // End of track
            } else if (status === 0xF0 || status === 0xF7) {
              // SysEx
              trackOffset++;
              let sysLen = 0;
              do {
                if (trackOffset >= trackEnd) break;
                byte = view.getUint8(trackOffset++);
                sysLen = (sysLen << 7) | (byte & 0x7F);
              } while (byte & 0x80);
              trackOffset += sysLen;
            } else {
              trackOffset++;
            }
          }
          break;
        }
        offset += 8 + chunkSize;
      }
      
      return steps;
    } catch (e) {
      console.warn('MIDI parse error:', e);
      return null;
    }
  }, []);

  const handleDragEnter = (e) => {
    e.preventDefault();
    dragCounter.current++;
    setIsDragging(true);
  };

  const handleDragLeave = (e) => {
    e.preventDefault();
    dragCounter.current = Math.max(0, dragCounter.current - 1);
    if (dragCounter.current === 0) setIsDragging(false);
  };

  const handleDrop = async (e) => {
    e.preventDefault();
    e.stopPropagation();
    dragCounter.current = 0;
    setIsDragging(false);
    
    const files = e.dataTransfer.files;
    if (!files || files.length === 0) return;
    
    for (const file of files) {
      const name = file.name.replace(/\.[^.]+$/, '');
      
      if (/\.mid$/i.test(file.name)) {
        // Parse MIDI file
        const buf = await file.arrayBuffer();
        const steps = parseMidiToSteps(buf);
        if (steps) {
          setCustomPatterns(prev => [...prev, { name, steps, source: 'midi' }]);
        }
      } else {
        // For non-MIDI files, create a basic pattern from the filename
        // (e.g., user might drop audio — we just add it as a pattern template)
        setCustomPatterns(prev => [...prev, { 
          name, 
          steps: [1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0], // default every-other
          source: 'audio'
        }]);
      }
    }
  };

  const handleApply = (steps, patternName) => {
    onApplyPattern(channelIndex, steps, patternName);
  };

  const genreKeys = Object.keys(GENRE_PATTERNS);
  const activeGenre = selectedGenre ? GENRE_PATTERNS[selectedGenre] : null;
  const availablePattern = activeGenre && instrumentKey ? activeGenre.instruments[instrumentKey] : null;
  
  // Get all patterns for the selected genre (for showing all instruments)
  const allGenrePatterns = activeGenre ? Object.entries(activeGenre.instruments) : [];

  return (
    <div className="midi-modal-backdrop" onClick={onClose}>
      <div className="midi-modal" onClick={e => e.stopPropagation()}>
        {/* Header */}
        <div className="midi-modal-header">
          <div className="midi-modal-title">
            <span className="midi-modal-icon">🎹</span>
            <div>
              <h2>MIDI Patterns</h2>
              <span className="midi-modal-subtitle">
                {channel.name} — {instrumentKey ? instrumentKey.replace('_', ' ').toUpperCase() : 'CUSTOM'}
              </span>
            </div>
          </div>
          <button className="midi-modal-close" onClick={onClose}>✕</button>
        </div>

        {/* Current Pattern Preview */}
        <div className="midi-current-section">
          <div className="midi-section-label">CURRENT PATTERN</div>
          <div className="midi-step-preview">
            {currentSteps.slice(0, 16).map((on, i) => (
              <div 
                key={i} 
                className={`midi-preview-step ${on ? 'active' : ''}`}
                style={{ animationDelay: `${i * 30}ms` }}
              />
            ))}
          </div>
          <div className="midi-step-count">
            {currentSteps.filter(s => s).length} / {currentSteps.length} steps active
          </div>
        </div>

        {/* Genre Selector */}
        <div className="midi-section-label" style={{ marginTop: 16 }}>SELECT GENRE</div>
        <div className="midi-genre-grid">
          {genreKeys.map(key => {
            const g = GENRE_PATTERNS[key];
            const isActive = selectedGenre === key;
            const hasPattern = instrumentKey && g.instruments[instrumentKey];
            return (
              <button
                key={key}
                className={`midi-genre-btn ${isActive ? 'active' : ''} ${!hasPattern ? 'no-match' : ''}`}
                onClick={() => setSelectedGenre(isActive ? null : key)}
                style={{
                  '--genre-color': g.color,
                  borderColor: isActive ? g.color : undefined,
                  background: isActive ? `${g.color}18` : undefined,
                }}
              >
                <span className="midi-genre-emoji">{g.emoji}</span>
                <span className="midi-genre-label">{g.label}</span>
                {hasPattern && <span className="midi-genre-dot" style={{ background: g.color }} />}
              </button>
            );
          })}
        </div>

        {/* Genre Patterns */}
        {activeGenre && (
          <div className="midi-patterns-section">
            <div className="midi-section-label" style={{ color: activeGenre.color }}>
              {activeGenre.emoji} {activeGenre.label.toUpperCase()} PATTERNS
            </div>
            <div className="midi-patterns-list">
              {allGenrePatterns.map(([instKey, pattern]) => {
                const isMatch = instKey === instrumentKey;
                const isHovered = previewPattern === `${selectedGenre}_${instKey}`;
                return (
                  <div 
                    key={instKey}
                    className={`midi-pattern-card ${isMatch ? 'match' : ''} ${isHovered ? 'hovered' : ''}`}
                    onMouseEnter={() => setPreviewPattern(`${selectedGenre}_${instKey}`)}
                    onMouseLeave={() => setPreviewPattern(null)}
                    style={{ '--card-color': activeGenre.color }}
                  >
                    <div className="midi-pattern-info">
                      <span className="midi-pattern-name">{pattern.name}</span>
                      <span className="midi-pattern-type">{instKey.replace('_', ' ')}</span>
                      {isMatch && <span className="midi-pattern-badge">MATCH</span>}
                    </div>
                    <div className="midi-step-preview small">
                      {pattern.steps.map((on, i) => (
                        <div 
                          key={i} 
                          className={`midi-preview-step ${on ? 'active' : ''}`}
                          style={{ 
                            background: on ? activeGenre.color : undefined,
                            boxShadow: on ? `0 0 6px ${activeGenre.color}60` : undefined,
                          }}
                        />
                      ))}
                    </div>
                    <button 
                      className="midi-apply-btn"
                      onClick={() => handleApply(pattern.steps, pattern.name)}
                      style={{ background: activeGenre.color }}
                    >
                      Apply
                    </button>
                  </div>
                );
              })}
            </div>
          </div>
        )}

        {/* Custom MIDI Drop Zone */}
        <div className="midi-drop-section">
          <div className="midi-section-label">DROP YOUR OWN MIDI</div>
          <div 
            ref={dropRef}
            className={`midi-drop-zone ${isDragging ? 'dragging' : ''}`}
            onDragEnter={handleDragEnter}
            onDragOver={(e) => { e.preventDefault(); e.dataTransfer.dropEffect = 'copy'; }}
            onDragLeave={handleDragLeave}
            onDrop={handleDrop}
          >
            <div className="midi-drop-icon">{isDragging ? '📥' : '🎵'}</div>
            <div className="midi-drop-text">
              {isDragging ? 'Drop MIDI file here!' : 'Drag & drop a .mid file here'}
            </div>
            <div className="midi-drop-hint">Patterns will be quantized to 16 steps</div>
          </div>

          {/* Custom uploaded patterns */}
          {customPatterns.length > 0 && (
            <div className="midi-custom-list">
              {customPatterns.map((pat, i) => (
                <div key={i} className="midi-pattern-card custom">
                  <div className="midi-pattern-info">
                    <span className="midi-pattern-name">{pat.name}</span>
                    <span className="midi-pattern-type">{pat.source === 'midi' ? 'MIDI Import' : 'Custom'}</span>
                  </div>
                  <div className="midi-step-preview small">
                    {pat.steps.map((on, j) => (
                      <div 
                        key={j} 
                        className={`midi-preview-step ${on ? 'active' : ''}`}
                        style={{ background: on ? '#f97316' : undefined }}
                      />
                    ))}
                  </div>
                  <button 
                    className="midi-apply-btn"
                    onClick={() => handleApply(pat.steps, pat.name)}
                    style={{ background: '#f97316' }}
                  >
                    Apply
                  </button>
                </div>
              ))}
            </div>
          )}
        </div>

        {/* Quick actions */}
        <div className="midi-footer">
          <button className="midi-footer-btn" onClick={() => handleApply(Array(16).fill(0), 'Clear')}>
            Clear All
          </button>
          <button className="midi-footer-btn" onClick={() => {
            const random = Array(16).fill(0).map(() => Math.random() > 0.55 ? 1 : 0);
            handleApply(random, 'Random');
          }}>
            🎲 Randomize
          </button>
          <button className="midi-footer-btn accent" onClick={onClose}>
            Done
          </button>
        </div>
      </div>
    </div>
  );
}
