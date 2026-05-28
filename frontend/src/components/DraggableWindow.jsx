import { useRef, useState, useCallback } from 'react';

/**
 * A floating, draggable, resizable window shell.
 * Drag by the title bar. Minimise/maximise/close buttons included.
 *
 * Props:
 *   title       — window title string
 *   defaultPos  — { x, y }  (px from top-left of parent)
 *   defaultSize — { w, h }  (px)
 *   onClose     — called when X is clicked
 *   children    — window body
 *   className   — extra class on the outer shell
 *   headerRight — extra JSX rendered to the right of the title in the header
 */
export default function DraggableWindow({
  title = 'Window',
  defaultPos  = { x: 40, y: 200 },
  defaultSize = { w: 860, h: 220 },
  onClose,
  children,
  className = '',
  headerRight,
}) {
  const [pos,  setPos]  = useState(defaultPos);
  const [size, setSize] = useState(defaultSize);
  const [minimised, setMinimised] = useState(false);

  const dragRef   = useRef(null);
  const resizeRef = useRef(null);
  const windowRef = useRef(null);

  // ── Drag title-bar ────────────────────────────────────────────────────────
  const onHeaderMouseDown = useCallback((e) => {
    if (e.target.closest('button')) return;   // don't drag when clicking buttons
    e.preventDefault();
    dragRef.current = { startX: e.clientX - pos.x, startY: e.clientY - pos.y };

    const onMove = (ev) => {
      if (!dragRef.current) return;
      setPos({
        x: ev.clientX - dragRef.current.startX,
        y: ev.clientY - dragRef.current.startY,
      });
    };
    const onUp = () => {
      dragRef.current = null;
      window.removeEventListener('mousemove', onMove);
      window.removeEventListener('mouseup', onUp);
    };
    window.addEventListener('mousemove', onMove);
    window.addEventListener('mouseup', onUp);
  }, [pos]);

  // ── Resize handle (bottom-right corner) ───────────────────────────────────
  const onResizeMouseDown = useCallback((e) => {
    e.preventDefault();
    e.stopPropagation();
    resizeRef.current = { startX: e.clientX, startY: e.clientY, w: size.w, h: size.h };

    const onMove = (ev) => {
      if (!resizeRef.current) return;
      setSize({
        w: Math.max(400, resizeRef.current.w + ev.clientX - resizeRef.current.startX),
        h: Math.max(120, resizeRef.current.h + ev.clientY - resizeRef.current.startY),
      });
    };
    const onUp = () => {
      resizeRef.current = null;
      window.removeEventListener('mousemove', onMove);
      window.removeEventListener('mouseup', onUp);
    };
    window.addEventListener('mousemove', onMove);
    window.addEventListener('mouseup', onUp);
  }, [size]);

  return (
    <div
      ref={windowRef}
      className={`draggable-window ${className}`}
      style={{
        position: 'absolute',
        left: pos.x,
        top: pos.y,
        width: size.w,
        height: minimised ? 36 : size.h,
        zIndex: 20,
        display: 'flex',
        flexDirection: 'column',
        background: 'linear-gradient(135deg,#2a2a2e 0%,#121214 100%)',
        border: '1px solid #3f3f46',
        borderRadius: 14,
        boxShadow: '0 8px 40px rgba(0,0,0,.85), inset 1px 1px 0 rgba(255,255,255,.07)',
        overflow: 'hidden',
        minWidth: 400,
      }}
    >
      {/* ── Title bar ─────────────────────────────────────────────────────── */}
      <div
        className="dw-titlebar"
        onMouseDown={onHeaderMouseDown}
        style={{
          height: 34,
          flexShrink: 0,
          display: 'flex',
          alignItems: 'center',
          padding: '0 10px',
          gap: 8,
          background: 'linear-gradient(180deg,#2f2f33 0%,#1a1a1d 100%)',
          borderBottom: '1px solid #000',
          cursor: 'move',
          userSelect: 'none',
        }}
      >
        {/* Traffic-light buttons */}
        <button
          onClick={onClose}
          title="Close"
          style={trafficBtn('#ef4444')}
        />
        <button
          onClick={() => setMinimised(m => !m)}
          title={minimised ? 'Restore' : 'Minimise'}
          style={trafficBtn('#eab308')}
        />
        <button
          title="Maximise"
          style={trafficBtn('#22c55e')}
        />

        {/* Orange dot + title */}
        <span style={{
          marginLeft: 6,
          width: 6, height: 6, borderRadius: '50%',
          background: '#f97316',
          boxShadow: '0 0 6px rgba(249,115,22,.8)',
          flexShrink: 0,
        }} />
        <span style={{
          color: '#fafafa', fontSize: 10,
          letterSpacing: '0.18em', textTransform: 'uppercase',
          fontWeight: 700, textShadow: '0 1px 0 rgba(0,0,0,.8)',
        }}>{title}</span>

        <div style={{ flex: 1 }} />
        {headerRight}
      </div>

      {/* ── Body ──────────────────────────────────────────────────────────── */}
      {!minimised && (
        <div style={{ flex: 1, overflow: 'auto', position: 'relative' }}>
          {children}
        </div>
      )}

      {/* ── Resize handle ─────────────────────────────────────────────────── */}
      {!minimised && (
        <div
          onMouseDown={onResizeMouseDown}
          style={{
            position: 'absolute', bottom: 0, right: 0,
            width: 16, height: 16, cursor: 'nwse-resize',
            background: 'linear-gradient(135deg,transparent 50%,#52525b 50%)',
            borderRadius: '0 0 13px 0',
            zIndex: 30,
          }}
        />
      )}
    </div>
  );
}

function trafficBtn(color) {
  return {
    width: 12, height: 12, borderRadius: '50%',
    background: color, border: 'none', cursor: 'pointer',
    flexShrink: 0, padding: 0,
    boxShadow: `0 1px 3px rgba(0,0,0,.6), inset 0 1px 1px rgba(255,255,255,.3)`,
  };
}
