import { useState } from 'react';
import { loadSample } from '../audio.js';

export default function Browser() {
  const [sampleList, setSampleList] = useState([]);

  const handleUpload = async (e) => {
    const file = e.target.files[0];
    if (!file) return;
    try {
      const buf = await file.arrayBuffer();
      await loadSample(file.name, buf);
      setSampleList(prev => [...prev, { name: file.name, size: file.size }]);
    } catch (err) {
      alert('Failed to load sample: ' + err.message);
    }
  };

  return (
    <div className="browser" style={{width:220,background:'#2a2a2a',borderRight:'1px solid #1a1a1a',display:'flex',flexDirection:'column'}}>
      <div className="browser-header">Browser</div>
      <div className="browser-tree" style={{flex:1,overflowY:'auto'}}>
        <div className="tree-item selected"><span className="tree-icon">📁</span> Packs</div>
        <div className="tree-item" style={{paddingLeft:20}}><span className="tree-icon">📂</span> Drums</div>
        <div className="tree-item" style={{paddingLeft:40}}><span className="tree-icon">🔊</span> Kick</div>
        <div className="tree-item" style={{paddingLeft:40}}><span className="tree-icon">🔊</span> Snare</div>
        <div className="tree-item" style={{paddingLeft:40}}><span className="tree-icon">🔊</span> Hihat</div>
        <div className="tree-item" style={{paddingLeft:40}}><span className="tree-icon">🔊</span> Clap</div>
        <div className="tree-item" style={{paddingLeft:20}}><span className="tree-icon">📂</span> Instruments</div>
        <div className="tree-item" style={{paddingLeft:40}}><span className="tree-icon">🎹</span> 3xOsc</div>
        <div className="tree-item" style={{paddingLeft:40}}><span className="tree-icon">🎹</span> Sampler</div>
        <div className="tree-item" style={{paddingLeft:40}}><span className="tree-icon">🎹</span> Sytrus</div>
        <div className="tree-item"><span className="tree-icon">📁</span> Current Project</div>
        <div className="tree-item" style={{paddingLeft:20}}><span className="tree-icon">🎵</span> Patterns</div>
        <div className="tree-item" style={{paddingLeft:20}}><span className="tree-icon">🎛</span> Mixer</div>
        <div className="tree-item"><span className="tree-icon">📁</span> Plugin Database</div>
        <div className="tree-item"><span className="tree-icon">📁</span> Recent Files</div>

        {sampleList.length > 0 && (
          <>
            <div className="tree-item" style={{paddingLeft:10,marginTop:8,borderTop:'1px solid #444',paddingTop:8}}>
              <span className="tree-icon">📁</span> <strong style={{color:'#ff8c00'}}>My Samples</strong>
            </div>
            {sampleList.map((s, idx) => (
              <div key={idx} className="tree-item" style={{paddingLeft:30}}>
                <span className="tree-icon">🔊</span> {s.name}
              </div>
            ))}
          </>
        )}
      </div>
      <div style={{padding:8,borderTop:'1px solid #1a1a1a'}}>
        <label style={{fontSize:11,color:'#888',cursor:'pointer',display:'block',textAlign:'center',padding:4,background:'#3a3a3a',borderRadius:3}}>
          + Add Sample
          <input type="file" accept="audio/*" style={{display:'none'}} onChange={handleUpload} />
        </label>
      </div>
    </div>
  );
}
