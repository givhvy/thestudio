import { useState, useRef, useEffect } from 'react';

const S = {
  bar: {
    display: 'flex', alignItems: 'center',
    height: 22, background: '#18181b',
    borderBottom: '1px solid #000',
    paddingLeft: 8, gap: 0,
    fontSize: 11, userSelect: 'none',
    flexShrink: 0, zIndex: 100,
    position: 'relative',
  },
  logo: {
    color: '#f97316', fontWeight: 800, fontSize: 11,
    letterSpacing: '0.2em', padding: '0 12px 0 4px',
    textShadow: '0 0 8px rgba(249,115,22,.5)',
  },
  item: (open) => ({
    padding: '0 10px', height: '100%',
    display: 'flex', alignItems: 'center',
    cursor: 'pointer', position: 'relative',
    color: open ? '#f97316' : '#a1a1aa',
    background: open ? 'rgba(249,115,22,.1)' : 'transparent',
    fontSize: 11, fontWeight: 500,
    transition: 'color .1s, background .1s',
  }),
  dropdown: {
    position: 'absolute', top: '100%', left: 0,
    background: '#1c1c1f', border: '1px solid #3f3f46',
    borderRadius: 6, minWidth: 180,
    boxShadow: '0 8px 24px rgba(0,0,0,.8)',
    zIndex: 999, overflow: 'hidden',
  },
  entry: (disabled) => ({
    padding: '5px 16px', cursor: disabled ? 'default' : 'pointer',
    color: disabled ? '#52525b' : '#d4d4d8',
    fontSize: 11, display: 'flex',
    justifyContent: 'space-between', gap: 24,
    transition: 'background .1s',
  }),
  sep: {
    height: 1, background: '#27272a', margin: '3px 0',
  },
};

function MenuItem({ label, items, onClose }) {
  const [open, setOpen] = useState(false);
  const ref = useRef(null);

  useEffect(() => {
    if (!open) return;
    const handler = (e) => { if (ref.current && !ref.current.contains(e.target)) setOpen(false); };
    window.addEventListener('mousedown', handler);
    return () => window.removeEventListener('mousedown', handler);
  }, [open]);

  return (
    <div ref={ref} style={S.item(open)} onClick={() => setOpen(v => !v)}>
      {label}
      {open && (
        <div style={S.dropdown}>
          {items.map((it, i) =>
            it === '---' ? (
              <div key={i} style={S.sep} />
            ) : (
              <div
                key={i}
                style={S.entry(it.disabled)}
                onMouseEnter={e => { if (!it.disabled) e.currentTarget.style.background = 'rgba(249,115,22,.12)'; }}
                onMouseLeave={e => { e.currentTarget.style.background = 'transparent'; }}
                onClick={(e) => {
                  if (it.disabled) return;
                  e.stopPropagation();
                  it.action?.();
                  setOpen(false);
                  onClose?.();
                }}
              >
                <span>{it.label}</span>
                {it.shortcut && <span style={{ color: '#52525b', fontSize: 10 }}>{it.shortcut}</span>}
              </div>
            )
          )}
        </div>
      )}
    </div>
  );
}

export default function MenuBar({
  onNew, onSave, onOpen,
  onUndo, onRedo,
  onToggleChannelRack, onTogglePiano, onToggleMixer,
  onTogglePlay, onStop,
  bpm, setBpm, playing,
  onAddChannel, onNewPattern, onShowPluginBrowser,
}) {
  const menus = [
    {
      label: 'File',
      items: [
        { label: 'New Project',      shortcut: 'Ctrl+N', action: onNew },
        { label: 'Open Project…',    shortcut: 'Ctrl+O', action: onOpen },
        '---',
        { label: 'Save',             shortcut: 'Ctrl+S', action: onSave },
        { label: 'Save As…',         shortcut: 'Ctrl+Shift+S', action: onSave },
        '---',
        { label: 'Export WAV',       shortcut: 'Ctrl+E', disabled: false, action: () => alert('Export: use the Render button in the Toolbar') },
        '---',
        { label: 'Quit',             shortcut: 'Alt+F4', action: () => window.electronAPI?.close?.() },
      ],
    },
    {
      label: 'Edit',
      items: [
        { label: 'Undo',             shortcut: 'Ctrl+Z',   action: onUndo },
        { label: 'Redo',             shortcut: 'Ctrl+Y',   action: onRedo },
        '---',
        { label: 'Select All',       shortcut: 'Ctrl+A',   disabled: true },
        { label: 'Delete',           shortcut: 'Del',      disabled: true },
      ],
    },
    {
      label: 'Add',
      items: [
        { label: 'Audio Track',      action: onAddChannel },
        { label: 'Instrument Track', action: onAddChannel },
        { label: 'FX Track',         action: onAddChannel },
        '---',
        { label: 'New Pattern',      action: onNewPattern },
      ],
    },
    {
      label: 'Channels',
      items: [
        { label: 'Channel Rack',         shortcut: 'F6',   action: onToggleChannelRack },
        { label: 'Piano Roll',           shortcut: 'F7',   action: onTogglePiano },
        { label: 'Mixer',                shortcut: 'F9',   action: onToggleMixer },
        '---',
        { label: 'Add Channel',          action: onAddChannel },
        { label: 'Group Selected',       disabled: true },
      ],
    },
    {
      label: 'View',
      items: [
        { label: 'Channel Rack',   shortcut: 'F6',  action: onToggleChannelRack },
        { label: 'Piano Roll',     shortcut: 'F7',  action: onTogglePiano },
        { label: 'Playlist',       shortcut: 'F5',  disabled: true },
        { label: 'Mixer',          shortcut: 'F9',  action: onToggleMixer },
        '---',
        { label: 'Browser',        shortcut: 'F8',  disabled: true },
        { label: 'Plugin Browser', action: onShowPluginBrowser },
      ],
    },
    {
      label: 'Options',
      items: [
        { label: 'Audio Settings…',  action: () => alert('Audio settings: currently using default output via Web Audio API.') },
        { label: 'MIDI Settings…',   action: () => alert('MIDI: connect a MIDI device and it will be detected automatically.') },
        '---',
        { label: 'Metronome',        disabled: true },
        { label: 'Snap to Grid',     disabled: true },
      ],
    },
    {
      label: 'Tools',
      items: [
        { label: 'Macro Controller',   disabled: true },
        { label: 'Script Editor',      disabled: true },
        '---',
        { label: 'MCP Server Info',    action: () => alert('MCP server runs on port 9001 via StratumVSTHost.exe and port 3002 (Express backend).') },
      ],
    },
    {
      label: 'Help',
      items: [
        { label: 'About Stratum DAW',  action: () => alert('Stratum DAW — built with Electron + React + JUCE + Web Audio API') },
        { label: 'GitHub Repo',        action: () => window.open?.('https://github.com/givhvy/thestudio') },
        { label: 'Build Guide (VST)',  action: () => alert('See vst-host/BUILD.md for instructions to compile the VST3 host.') },
      ],
    },
  ];

  return (
    <div style={S.bar}>
      <span style={S.logo}>STRATUM</span>
      {menus.map(m => (
        <MenuItem key={m.label} label={m.label} items={m.items} />
      ))}
      <div style={{ flex: 1 }} />
      <span style={{ color: '#52525b', fontSize: 10, paddingRight: 10 }}>
        {playing ? '▶ PLAYING' : '■ STOPPED'} · {bpm} BPM
      </span>
    </div>
  );
}
