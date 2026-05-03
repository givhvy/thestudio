import { useState, useEffect } from 'react';

export default function TitleBar({ projectName, onMenuClick }) {
  const [maximized, setMaximized] = useState(false);

  useEffect(() => {
    if (window.electronAPI) {
      window.electronAPI.onMenuAction?.(onMenuClick);
    }
  }, [onMenuClick]);

  const handleMinimize = () => window.electronAPI?.minimize?.();
  const handleMaximize = () => {
    window.electronAPI?.maximize?.();
    setMaximized(v => !v);
  };
  const handleClose = () => window.electronAPI?.close?.();

  return (
    <div
      style={{
        height: 32,
        background: 'linear-gradient(180deg, #27272a 0%, #18181b 100%)',
        borderBottom: '1px solid #000',
        display: 'flex',
        alignItems: 'center',
        paddingLeft: 10,
        paddingRight: 8,
        gap: 12,
        WebkitAppRegion: 'drag', // makes the bar draggable
        userSelect: 'none',
        flexShrink: 0,
        zIndex: 2000,
      }}
    >
      {/* App logo / name */}
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          gap: 8,
        }}
      >
        <div
          style={{
            width: 14,
            height: 14,
            borderRadius: '50%',
            background: 'linear-gradient(135deg, #f97316 0%, #c2410c 100%)',
            boxShadow: '0 0 8px rgba(249,115,22,.6), inset 0 1px 1px rgba(255,255,255,.3)',
            flexShrink: 0,
          }}
        />
        <span
          style={{
            color: '#fafafa',
            fontSize: 11,
            fontWeight: 700,
            letterSpacing: '0.18em',
            textTransform: 'uppercase',
            textShadow: '0 1px 0 rgba(0,0,0,.8)',
          }}
        >
          Stratum
        </span>
        <span
          style={{
            color: '#52525b',
            fontSize: 10,
            marginLeft: 4,
          }}
        >
          {projectName}
        </span>
      </div>

      <div style={{ flex: 1 }} />

      {/* Window controls */}
      <div
        style={{
          display: 'flex',
          gap: 8,
          WebkitAppRegion: 'no-drag', // buttons must be clickable
        }}
      >
        <button
          onClick={handleMinimize}
          title="Minimize"
          style={{
            width: 14,
            height: 14,
            borderRadius: 3,
            background: 'transparent',
            border: 'none',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            color: '#a1a1aa',
            transition: 'color .15s, background .15s',
          }}
          onMouseEnter={(e) => { e.target.style.background = 'rgba(255,255,255,.08)'; e.target.style.color = '#fafafa'; }}
          onMouseLeave={(e) => { e.target.style.background = 'transparent'; e.target.style.color = '#a1a1aa'; }}
        >
          <svg width={10} height={10} viewBox="0 0 10 10" fill="none">
            <rect x={1} y={5} width={8} height={1} fill="currentColor" />
          </svg>
        </button>

        <button
          onClick={handleMaximize}
          title={maximized ? 'Restore' : 'Maximize'}
          style={{
            width: 14,
            height: 14,
            borderRadius: 3,
            background: 'transparent',
            border: 'none',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            color: '#a1a1aa',
            transition: 'color .15s, background .15s',
          }}
          onMouseEnter={(e) => { e.target.style.background = 'rgba(255,255,255,.08)'; e.target.style.color = '#fafafa'; }}
          onMouseLeave={(e) => { e.target.style.background = 'transparent'; e.target.style.color = '#a1a1aa'; }}
        >
          {maximized ? (
            <svg width={10} height={10} viewBox="0 0 10 10" fill="none">
              <rect x={2} y={2} width={6} height={6} fill="none" stroke="currentColor" strokeWidth={1.2} />
            </svg>
          ) : (
            <svg width={10} height={10} viewBox="0 0 10 10" fill="none">
              <rect x={1} y={1} width={8} height={8} fill="currentColor" />
            </svg>
          )}
        </button>

        <button
          onClick={handleClose}
          title="Close"
          style={{
            width: 14,
            height: 14,
            borderRadius: 3,
            background: 'transparent',
            border: 'none',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            color: '#a1a1aa',
            transition: 'color .15s, background .15s',
          }}
          onMouseEnter={(e) => { e.target.style.background = '#ef4444'; e.target.style.color = '#fff'; }}
          onMouseLeave={(e) => { e.target.style.background = 'transparent'; e.target.style.color = '#a1a1aa'; }}
        >
          <svg width={10} height={10} viewBox="0 0 10 10" fill="none">
            <path d="M2 2L8 8M8 2L2 8" stroke="currentColor" strokeWidth={1.4} strokeLinecap="round" />
          </svg>
        </button>
      </div>
    </div>
  );
}
