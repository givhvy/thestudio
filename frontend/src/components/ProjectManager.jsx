import { useState, useEffect } from 'react';

export default function ProjectManager({ onSelect, onClose, onNew }) {
  const [projectList, setProjectList] = useState([]);
  const [newName, setNewName] = useState('');
  const [selected, setSelected] = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    loadProjects();
  }, []);

  const loadProjects = async () => {
    try {
      const files = await window.electronAPI.listProjects();
      setProjectList(files || []);
      if (files && files.length > 0) setSelected(files[0]);
    } catch (e) { console.error(e); }
    finally { setLoading(false); }
  };

  const createProject = () => {
    if (!newName.trim()) return;
    onNew(newName.trim());
    onClose();
  };

  const openProject = async () => {
    if (!selected) return;
    try {
      const data = await window.electronAPI.readFile(selected.path);
      if (data.error) { alert(data.error); return; }
      onSelect(data);
      onClose();
    } catch (e) { alert(e.message); }
  };

  const deleteProject = async () => {
    if (!selected) return;
    if (!confirm(`Delete "${selected.name}"?`)) return;
    try {
      await window.electronAPI.deleteFile(selected.path);
      await loadProjects();
    } catch (e) { alert(e.message); }
  };

  const openFileDialog = async () => {
    try {
      const path = await window.electronAPI.openFile();
      if (!path) return;
      const data = await window.electronAPI.readFile(path);
      if (data.error) { alert(data.error); return; }
      onSelect(data);
      onClose();
    } catch (e) { alert(e.message); }
  };

  return (
    <div className="project-manager">
      <div className="pm-panel">
        <div className="pm-header">
          <span>Project Manager</span>
          <span style={{fontSize:11,color:'#888'}}>FL Studio Clone</span>
        </div>
        <div className="pm-list">
          {loading ? <div style={{padding:20,textAlign:'center',color:'#888',fontSize:12}}>Loading...</div> :
            projectList.length === 0 ? <div style={{padding:20,textAlign:'center',color:'#888',fontSize:12}}>No projects yet</div> :
            projectList.map(p => (
              <div key={p.path} className={`pm-item ${selected?.path===p.path?'active':''}`} onClick={()=>setSelected(p)}>
                <span>{p.name}</span>
                <span style={{fontSize:10,color:'#666'}}>{new Date(p.modified).toLocaleDateString()}</span>
              </div>
            ))
          }
        </div>
        <div style={{padding:'8px 12px',borderTop:'1px solid #1a1a1a',display:'flex',gap:8}}>
          <input className="modal-input" placeholder="New project name" value={newName} onChange={e=>setNewName(e.target.value)} style={{flex:1,margin:0}} />
          <button className="modal-btn primary" onClick={createProject}>New</button>
        </div>
        <div className="pm-actions">
          <button className="modal-btn" onClick={openFileDialog}>Browse...</button>
          <div style={{flex:1}}></div>
          <button className="modal-btn" onClick={onClose}>Cancel</button>
          <button className="modal-btn" onClick={deleteProject} disabled={!selected}>Delete</button>
          <button className="modal-btn primary" onClick={openProject} disabled={!selected}>Open</button>
        </div>
      </div>
    </div>
  );
}
