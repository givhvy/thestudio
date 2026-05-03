const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  // File system
  readFile: (path) => ipcRenderer.invoke('fs:readFile', path),
  writeFile: (path, data) => ipcRenderer.invoke('fs:writeFile', path, data),
  readBinaryFile: (path) => ipcRenderer.invoke('fs:readBinaryFile', path),
  listProjects: () => ipcRenderer.invoke('fs:listProjects'),
  deleteFile: (path) => ipcRenderer.invoke('fs:deleteFile', path),
  getProjectsDir: () => ipcRenderer.invoke('fs:getProjectsDir'),
  listDirectory: (path) => ipcRenderer.invoke('fs:listDirectory', path),

  // Dialogs
  openFile: () => ipcRenderer.invoke('dialog:openFile'),
  saveFile: (defaultPath) => ipcRenderer.invoke('dialog:saveFile', defaultPath),
  openDirectory: () => ipcRenderer.invoke('dialog:openDirectory'),

  // Window
  minimize: () => ipcRenderer.invoke('app:minimize'),
  maximize: () => ipcRenderer.invoke('app:maximize'),
  close: () => ipcRenderer.invoke('app:close'),

  // Menu events
  onMenuAction: (callback) => ipcRenderer.on('menu-new', () => callback('new')),
  onOpenAction: (callback) => ipcRenderer.on('menu-open', () => callback('open')),
  onSaveAction: (callback) => ipcRenderer.on('menu-save', () => callback('save')),
  onExportAction: (callback) => ipcRenderer.on('menu-export', () => callback('export')),
  onPlayAction: (callback) => ipcRenderer.on('menu-play', () => callback('play')),
  onStopAction: (callback) => ipcRenderer.on('menu-stop', () => callback('stop')),
  onRecordAction: (callback) => ipcRenderer.on('menu-record', () => callback('record')),
  onUndoAction: (callback) => ipcRenderer.on('menu-undo', () => callback('undo')),
  onPanelAction: (callback) => ipcRenderer.on('menu-panel', (_, panel) => callback(panel)),

  removeAllListeners: () => {
    ipcRenderer.removeAllListeners('menu-new');
    ipcRenderer.removeAllListeners('menu-open');
    ipcRenderer.removeAllListeners('menu-save');
    ipcRenderer.removeAllListeners('menu-export');
    ipcRenderer.removeAllListeners('menu-play');
    ipcRenderer.removeAllListeners('menu-stop');
    ipcRenderer.removeAllListeners('menu-record');
    ipcRenderer.removeAllListeners('menu-undo');
    ipcRenderer.removeAllListeners('menu-panel');
  },

  // VST Host bridge (requires StratumVSTHost.exe to be compiled)
  vstConnect:    ()             => ipcRenderer.invoke('vst:connect'),
  vstCall:       (method, p)   => ipcRenderer.invoke('vst:call', method, p),
  vstScanFolder: ()             => ipcRenderer.invoke('vst:scanFolder'),
  onVstEvent:    (cb)          => ipcRenderer.on('vst:event', (_, event, data) => cb(event, data)),
});
