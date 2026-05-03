import { useState, useRef } from 'react';
import { loadSample } from '../audio.js';

const DEFAULT_TREE = [
  { label: 'Current project', type: 'folder', selected: true },
  { label: 'Recent files', type: 'folder' },
  { label: 'Plugin database', type: 'folder' },
  { label: 'Plugin presets', type: 'folder' },
  { label: 'Channel presets', type: 'folder' },
  { label: 'Mixer presets', type: 'folder' },
  { label: 'Scores', type: 'folder' },
  { label: 'Backup', type: 'folder' },
  { label: 'Clipboard files', type: 'folder' },
  { label: 'Demo projects', type: 'folder' },
  { label: 'Desktop', type: 'folder' },
  { label: 'Envelopes', type: 'folder' },
  { label: 'IL shared data', type: 'folder' },
  { label: 'Impulses', type: 'folder' },
  { label: 'Loops_Samples', type: 'folder' },
  { label: 'Misc', type: 'folder' },
  { label: 'My projects', type: 'folder' },
  { label: 'Packs', type: 'folder' },
  { label: 'packs', type: 'folder' },
  { label: 'Project bones', type: 'folder' },
  { label: 'Recorded', type: 'folder' },
  { label: 'Rendered', type: 'folder' },
  { label: 'Sliced audio', type: 'folder' },
  { label: 'Soundfonts', type: 'folder' },
  { label: 'Speech', type: 'folder' },
  { label: 'Templates', type: 'folder' },
];

export default function Browser() {
  const [samples, setSamples] = useState([]);
  const [selected, setSelected] = useState('Current project');
  const fileRef = useRef(null);

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
        <span style={{marginLeft:'auto',color:'#7a8588'}}>All ▾</span>
      </div>
      <div className="browser-tree">
        {DEFAULT_TREE.map(item => (
          <div
            key={item.label}
            className={`tree-item ${selected === item.label ? 'selected' : ''}`}
            onClick={() => setSelected(item.label)}
            onDoubleClick={() => fileRef.current?.click()}
          >
            <span className="tree-icon folder">▸</span>
            <span>{item.label}</span>
          </div>
        ))}
        {samples.length > 0 && samples.map((s, i) => (
          <div key={i} className="tree-item" title={s.name}>
            <span className="tree-icon sample">♪</span>
            <span>{s.name}</span>
          </div>
        ))}
      </div>
      <input ref={fileRef} type="file" accept="audio/*" style={{display:'none'}} onChange={handleUpload} />
    </div>
  );
}
