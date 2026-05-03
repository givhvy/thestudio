import { useEffect, useMemo, useRef, useState } from 'react';
import { loadSample } from '../audio.js';

const DEFAULT_PACKS = [
  {
    id: 'drum-kit',
    name: 'Drum Kit',
    path: 'E:\\!Storage\\1500 THE DRUMS LORD COLLECTION',
  },
];

const AUDIO_EXTENSIONS = new Set(['.wav', '.mp3', '.aif', '.aiff', '.ogg', '.flac', '.m4a']);

export default function Browser({ channels = [], onLoadSample }) {
  const [samples, setSamples] = useState([]);
  const [packs, setPacks] = useState(() => {
    try {
      const saved = localStorage.getItem('stratum.samplePacks');
      return saved ? JSON.parse(saved) : DEFAULT_PACKS;
    } catch {
      return DEFAULT_PACKS;
    }
  });
  const [selected, setSelected] = useState(DEFAULT_PACKS[0].id);
  const [expanded, setExpanded] = useState({ [DEFAULT_PACKS[0].id]: true });
  const [packEntries, setPackEntries] = useState({});
  const [directoryEntries, setDirectoryEntries] = useState({});
  const [previewing, setPreviewing] = useState(null);
  const [lastSample, setLastSample] = useState(null);
  const audioRef = useRef(null);
  const fileInputRef = useRef(null);

  const selectedPack = useMemo(
    () => packs.find(pack => pack.id === selected),
    [packs, selected],
  );

  useEffect(() => {
    localStorage.setItem('stratum.samplePacks', JSON.stringify(packs));
  }, [packs]);

  // (no context menus — inline buttons instead)

  useEffect(() => {
    packs.forEach(pack => {
      if (expanded[pack.id]) loadPackEntries(pack);
    });
  }, [expanded, packs]);

  async function loadPackEntries(pack) {
    if (!window.electronAPI?.listDirectory) {
      setPackEntries(prev => ({
        ...prev,
        [pack.id]: { error: 'Electron folder access is not available.', entries: [] },
      }));
      return;
    }
    const result = await window.electronAPI.listDirectory(pack.path);
    setPackEntries(prev => ({
      ...prev,
      [pack.id]: {
        error: result?.error,
        entries: (result?.entries || []).filter(entry => entry.isDirectory || AUDIO_EXTENSIONS.has(entry.ext)),
      },
    }));
  }

  async function loadDirectoryEntries(dirPath) {
    if (!window.electronAPI?.listDirectory) {
      setDirectoryEntries(prev => ({
        ...prev,
        [dirPath]: { error: 'Electron folder access is not available.', entries: [] },
      }));
      return;
    }
    const result = await window.electronAPI.listDirectory(dirPath);
    setDirectoryEntries(prev => ({
      ...prev,
      [dirPath]: {
        error: result?.error,
        entries: (result?.entries || []).filter(entry => entry.isDirectory || AUDIO_EXTENSIONS.has(entry.ext)),
      },
    }));
  }

  async function handleAddPack() {
    const name = window.prompt('Pack name', 'New Pack');
    if (!name) return;
    const defaultPath = await window.electronAPI?.openDirectory?.();
    const path = window.prompt('Pack folder path', defaultPath || '');
    if (!path) return;
    const id = `${Date.now()}_${Math.random().toString(36).slice(2)}`;
    setPacks(prev => [...prev, { id, name, path }]);
    setExpanded(prev => ({ ...prev, [id]: true }));
    setSelected(id);
  }

  function togglePack(pack) {
    setSelected(pack.id);
    setExpanded(prev => ({ ...prev, [pack.id]: !prev[pack.id] }));
  }

  function stopPreview() {
    if (audioRef.current) {
      audioRef.current.pause();
      audioRef.current.src = '';
      audioRef.current = null;
    }
    setPreviewing(null);
  }

  function audition(entry) {
    stopPreview();
    setPreviewing(entry.name);
    const fileUrl = 'file:///' + encodeURI(entry.path.replaceAll('\\', '/')).replace(/#/g, '%23').replace(/\?/g, '%3F');
    const audio = new Audio(fileUrl);
    audioRef.current = audio;
    audio.onended = () => { setPreviewing(null); audioRef.current = null; };
    audio.onerror = (e) => {
      console.error('[Browser] audio error:', e, 'url:', fileUrl);
      setPreviewing(null);
      audioRef.current = null;
    };
    audio.play().catch(err => {
      console.error('[Browser] play() failed:', err, 'url:', fileUrl);
      setPreviewing(null);
      audioRef.current = null;
    });
  }

  function handleSampleClick(entry) {
    setSelected(entry.path);
    setLastSample(entry);
    audition(entry);
    setSamples(prev =>
      prev.some(s => s.path === entry.path)
        ? prev
        : [...prev, { name: entry.name, path: entry.path }]
    );
  }

  function toggleDirectory(entry) {
    setSelected(entry.path);
    setExpanded(prev => {
      const nextOpen = !prev[entry.path];
      if (nextOpen && !directoryEntries[entry.path]) loadDirectoryEntries(entry.path);
      return { ...prev, [entry.path]: nextOpen };
    });
  }

  function renderEntry(entry, depth = 1) {
    const isOpen = !!expanded[entry.path];
    const data = directoryEntries[entry.path];
    const isSelected = selected === entry.path;
    const paddingLeft = 12 + depth * 14;

    return (
      <div key={entry.path}>
        <div
          className={`tree-item ${isSelected ? 'selected' : ''} ${entry.isFile ? 'sample-entry' : ''} ${previewing === entry.name ? 'previewing' : ''}`}
          title={entry.path}
          onClick={() => entry.isDirectory ? toggleDirectory(entry) : handleSampleClick(entry)}
          draggable={entry.isFile ? "true" : "false"}
          style={{ paddingLeft, cursor: entry.isFile ? 'grab' : 'pointer' }}
          onDragStart={(e) => {
            if (entry.isFile) {
              window.__draggedSample = { path: entry.path, name: entry.name };
              e.dataTransfer.setData('text/plain', 'internal-sample');
              e.dataTransfer.effectAllowed = 'copy';
            }
          }}
        >
          <span className={`tree-icon ${entry.isFile ? 'sample' : 'folder'}`}>
            {entry.isFile ? (previewing === entry.name ? '♫' : '♪') : isOpen ? '▾' : '▸'}
          </span>
          <span className="tree-label">{entry.name}</span>
          {entry.isFile && (
            <div className="sample-ch-buttons" style={{marginLeft:'auto',display:'flex',gap:'2px',flexShrink:0}}>
              {channels.slice(0, 8).map((ch, ci) => (
                <button
                  key={ci}
                  className="ch-mini-btn"
                  title={`Load to ${ch.name}`}
                  onClick={(e) => { e.stopPropagation(); onLoadSample && onLoadSample(ci, { path: entry.path, name: entry.name }); }}
                >{ci + 1}</button>
              ))}
              <button
                className="ch-mini-btn new"
                title="Load to new channel"
                onClick={(e) => { e.stopPropagation(); onLoadSample && onLoadSample(channels.length, { path: entry.path, name: entry.name }); }}
              >+</button>
            </div>
          )}
        </div>
        {entry.isDirectory && isOpen && (
          <div>
            {data?.error && (
              <div className="tree-item tree-error" title={data.error} style={{ paddingLeft: paddingLeft + 14 }}>
                <span className="tree-icon">!</span>
                <span>Path unavailable</span>
              </div>
            )}
            {!data && (
              <div className="tree-item" style={{ paddingLeft: paddingLeft + 14 }}>
                <span className="tree-icon">…</span>
                <span>Loading...</span>
              </div>
            )}
            {!data?.error && (data?.entries || []).map(child => renderEntry(child, depth + 1))}
          </div>
        )}
      </div>
    );
  }

  const handleUpload = async (e) => {
    const file = e.target.files?.[0];
    if (!file) return;
    try {
      const buf = await file.arrayBuffer();
      await loadSample(file.name, buf);
      setSamples(prev => [...prev, { name: file.name }]);
    } catch (err) {
      alert('Failed to load sample: ' + err.message);
    }
    e.target.value = '';
  };

  return (
    <div className="browser">
      <div className="browser-user">
        <span>[ADMIN]</span>
        <span className="browser-user-arrow">▼</span>
      </div>
      <div className="browser-header">
        <span className="browser-header-icon">▲</span>
        <span className="browser-header-icon">▼</span>
        <span>Browser</span>
        <div style={{marginLeft:'auto',display:'flex',gap:'4px',alignItems:'center'}}>
          <button
            className="browser-audition-btn"
            onClick={() => lastSample && audition(lastSample)}
            disabled={!lastSample}
            title="Audition selected sample"
          >▶</button>
          <button
            className="browser-stop-btn"
            onClick={stopPreview}
            title="Stop preview"
          >■</button>
          <span style={{color:'#7a8588'}}>All ▾</span>
        </div>
      </div>
      <div className="browser-tree">
        {packs.map(pack => (
          <div key={pack.id}>
            <div
              className={`tree-item ${selected === pack.id ? 'selected' : ''}`}
              onClick={() => togglePack(pack)}
              title={pack.path}
            >
              <span className="tree-icon folder">{expanded[pack.id] ? '▾' : '▸'}</span>
              <span>{pack.name}</span>
            </div>
            {expanded[pack.id] && (
              <div className="tree-children">
                {packEntries[pack.id]?.error && (
                  <div className="tree-item tree-error" title={packEntries[pack.id].error}>
                    <span className="tree-icon">!</span>
                    <span>Path unavailable</span>
                  </div>
                )}
                {!packEntries[pack.id]?.error && (packEntries[pack.id]?.entries || []).map(entry => renderEntry(entry, 1))}
              </div>
            )}
          </div>
        ))}
        {samples.length > 0 && (
          <div className="tree-section-label">Loaded samples</div>
        )}
        {samples.map((s, i) => (
          <div
            key={`${s.path}_${i}`}
            className="tree-item sample-entry"
            title={s.path || s.name}
            draggable="true"
            style={{ cursor: 'grab' }}
            onDragStart={(e) => {
              window.__draggedSample = { path: s.path, name: s.name };
              e.dataTransfer.setData('text/plain', 'internal-sample');
              e.dataTransfer.effectAllowed = 'copy';
            }}
          >
            <span className="tree-icon sample">✓</span>
            <span>{s.name}</span>
          </div>
        ))}
        <div className="browser-footer" style={{padding:'6px 12px',borderTop:'1px solid #18181b',display:'flex',gap:'6px'}}>
          <button className="browser-footer-btn" onClick={handleAddPack}>+ Pack</button>
          <button className="browser-footer-btn" onClick={() => selectedPack && loadPackEntries(selectedPack)} disabled={!selectedPack}>↻ Refresh</button>
        </div>
      </div>
      <input ref={fileInputRef} type="file" accept="audio/*" style={{display:'none'}} onChange={handleUpload} />
    </div>
  );
}
