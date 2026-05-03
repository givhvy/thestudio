const { app, BrowserWindow, ipcMain, dialog, Menu } = require('electron');
const path = require('path');
const fs = require('fs');
const os = require('os');

const isDev = process.env.NODE_ENV === 'development' || !app.isPackaged;
const projectsDir = path.join(os.homedir(), 'Documents', 'FLStudioClone', 'Projects');

function ensureDir(dir) {
  if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
}
ensureDir(projectsDir);

let mainWindow;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1600,
    height: 1000,
    minWidth: 1200,
    minHeight: 700,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: false
    },
    titleBarStyle: 'hiddenInset',
    backgroundColor: '#1a1a1a',
    icon: path.join(__dirname, '..', 'frontend', 'public', 'icon.ico')
  });

  if (isDev) {
    mainWindow.loadURL('http://localhost:3001');
    mainWindow.webContents.openDevTools();
  } else {
    mainWindow.loadFile(path.join(__dirname, '..', 'frontend', 'dist', 'index.html'));
  }

  mainWindow.on('closed', () => { mainWindow = null; });
}

app.whenReady().then(() => {
  createWindow();
  setupMenu();
  app.on('activate', () => { if (BrowserWindow.getAllWindows().length === 0) createWindow(); });
});

app.on('window-all-closed', () => { if (process.platform !== 'darwin') app.quit(); });

// IPC Handlers
ipcMain.handle('dialog:openFile', async () => {
  const { canceled, filePaths } = await dialog.showOpenDialog(mainWindow, {
    properties: ['openFile'],
    filters: [{ name: 'FL Project', extensions: ['flc'] }]
  });
  if (canceled || filePaths.length === 0) return null;
  return filePaths[0];
});

ipcMain.handle('dialog:saveFile', async (_, defaultPath) => {
  const { canceled, filePath } = await dialog.showSaveDialog(mainWindow, {
    defaultPath: defaultPath || path.join(projectsDir, 'untitled.flc'),
    filters: [{ name: 'FL Project', extensions: ['flc'] }]
  });
  if (canceled) return null;
  return filePath;
});

ipcMain.handle('dialog:openDirectory', async () => {
  const { canceled, filePaths } = await dialog.showOpenDialog(mainWindow, {
    properties: ['openDirectory']
  });
  if (canceled || filePaths.length === 0) return null;
  return filePaths[0];
});

ipcMain.handle('fs:readFile', async (_, filePath) => {
  try {
    const data = fs.readFileSync(filePath, 'utf8');
    return JSON.parse(data);
  } catch (e) { return { error: e.message }; }
});

ipcMain.handle('fs:writeFile', async (_, filePath, data) => {
  try {
    ensureDir(path.dirname(filePath));
    fs.writeFileSync(filePath, JSON.stringify(data, null, 2), 'utf8');
    return { success: true };
  } catch (e) { return { error: e.message }; }
});

ipcMain.handle('fs:listProjects', async () => {
  try {
    const files = fs.readdirSync(projectsDir)
      .filter(f => f.endsWith('.flc'))
      .map(f => {
        const fp = path.join(projectsDir, f);
        const stat = fs.statSync(fp);
        return { name: f.replace('.flc', ''), path: fp, modified: stat.mtime };
      })
      .sort((a, b) => b.modified - a.modified);
    return files;
  } catch (e) { return []; }
});

ipcMain.handle('fs:deleteFile', async (_, filePath) => {
  try { fs.unlinkSync(filePath); return { success: true }; }
  catch (e) { return { error: e.message }; }
});

ipcMain.handle('fs:getProjectsDir', () => projectsDir);

ipcMain.handle('app:minimize', () => { if (mainWindow) mainWindow.minimize(); });
ipcMain.handle('app:maximize', () => { if (mainWindow) mainWindow.isMaximized() ? mainWindow.unmaximize() : mainWindow.maximize(); });
ipcMain.handle('app:close', () => { if (mainWindow) mainWindow.close(); });

function setupMenu() {
  const template = [
    {
      label: 'File',
      submenu: [
        { label: 'New Project', accelerator: 'CmdOrCtrl+N', click: () => mainWindow?.webContents.send('menu-new') },
        { label: 'Open Project', accelerator: 'CmdOrCtrl+O', click: () => mainWindow?.webContents.send('menu-open') },
        { label: 'Save Project', accelerator: 'CmdOrCtrl+S', click: () => mainWindow?.webContents.send('menu-save') },
        { type: 'separator' },
        { label: 'Export WAV', accelerator: 'CmdOrCtrl+E', click: () => mainWindow?.webContents.send('menu-export') },
        { type: 'separator' },
        { label: 'Quit', accelerator: process.platform === 'darwin' ? 'Cmd+Q' : 'Ctrl+Q', click: () => app.quit() }
      ]
    },
    {
      label: 'Edit',
      submenu: [
        { label: 'Undo', accelerator: 'CmdOrCtrl+Z', click: () => mainWindow?.webContents.send('menu-undo') },
        { label: 'Cut', accelerator: 'CmdOrCtrl+X', role: 'cut' },
        { label: 'Copy', accelerator: 'CmdOrCtrl+C', role: 'copy' },
        { label: 'Paste', accelerator: 'CmdOrCtrl+V', role: 'paste' }
      ]
    },
    {
      label: 'View',
      submenu: [
        { label: 'Channel Rack', accelerator: '1', click: () => mainWindow?.webContents.send('menu-panel', 'channelRack') },
        { label: 'Piano Roll', accelerator: '2', click: () => mainWindow?.webContents.send('menu-panel', 'pianoRoll') },
        { label: 'Playlist', accelerator: '3', click: () => mainWindow?.webContents.send('menu-panel', 'playlist') },
        { label: 'Mixer', accelerator: '4', click: () => mainWindow?.webContents.send('menu-panel', 'mixerPanel') },
        { type: 'separator' },
        { label: 'Toggle Fullscreen', accelerator: 'F11', click: () => mainWindow?.setFullScreen(!mainWindow.isFullScreen()) }
      ]
    },
    {
      label: 'Transport',
      submenu: [
        { label: 'Play / Pause', accelerator: 'Space', click: () => mainWindow?.webContents.send('menu-play') },
        { label: 'Stop', accelerator: 'Esc', click: () => mainWindow?.webContents.send('menu-stop') },
        { label: 'Record', accelerator: 'R', click: () => mainWindow?.webContents.send('menu-record') }
      ]
    },
    {
      label: 'Help',
      submenu: [
        { label: 'About', click: () => dialog.showMessageBox(mainWindow, { message: 'FL Studio Clone v1.0\nA desktop DAW built with Electron + React.', title: 'About' }) }
      ]
    }
  ];
  const menu = Menu.buildFromTemplate(template);
  Menu.setApplicationMenu(menu);
}
