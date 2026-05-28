import { useState, useRef, useEffect } from 'react';

const API = 'http://localhost:3002/api';

const QUICK_COMMANDS = [
  { label: 'Trap Beat', cmd: { type: 'set_pattern', bpm: 140, channels: [
    { name: 'Kick',  steps: [1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0] },
    { name: 'Snare', steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0] },
    { name: 'Hihat Closed', steps: [1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1] },
    { name: 'Clap',  steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0] },
  ]}},
  { label: 'Boom Bap', cmd: { type: 'set_pattern', bpm: 90, channels: [
    { name: 'Kick',  steps: [1,0,0,1,0,0,1,0,1,0,0,0,0,1,0,0] },
    { name: 'Snare', steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0] },
    { name: 'Hihat Closed', steps: [1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0] },
  ]}},
  { label: 'House', cmd: { type: 'set_pattern', bpm: 128, channels: [
    { name: 'Kick',  steps: [1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0] },
    { name: 'Snare', steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0] },
    { name: 'Hihat Closed', steps: [0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0] },
  ]}},
  { label: 'Drill', cmd: { type: 'set_pattern', bpm: 145, channels: [
    { name: 'Kick',  steps: [1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,0] },
    { name: 'Snare', steps: [0,0,0,0,1,0,0,1,0,0,0,0,1,0,1,0] },
    { name: 'Hihat Closed', steps: [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1] },
  ]}},
  { label: 'Play', cmd: { type: 'play' } },
  { label: 'Stop', cmd: { type: 'stop' } },
];

async function sendCmd(cmd) {
  const res = await fetch(`${API}/daw/command`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(cmd),
  });
  return res.json();
}

function parseUserIntent(text) {
  const t = text.toLowerCase().trim();

  // BPM
  const bpmMatch = t.match(/(?:set\s+)?bpm\s+(?:to\s+)?(\d+)/);
  if (bpmMatch) return { type: 'set_bpm', bpm: parseInt(bpmMatch[1]) };

  // play / stop
  if (/^(play|start|go)/.test(t)) return { type: 'play' };
  if (/^(stop|pause|halt)/.test(t)) return { type: 'stop' };

  // beat styles
  if (t.includes('trap')) return QUICK_COMMANDS[0].cmd;
  if (t.includes('boom bap') || t.includes('boombap')) return QUICK_COMMANDS[1].cmd;
  if (t.includes('house')) return QUICK_COMMANDS[2].cmd;
  if (t.includes('drill')) return QUICK_COMMANDS[3].cmd;
  if (t.includes('hiphop') || t.includes('hip hop') || t.includes('hip-hop')) return {
    type: 'set_pattern', bpm: 90, channels: [
      { name: 'Kick',  steps: [1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0] },
      { name: 'Snare', steps: [0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0] },
      { name: 'Hihat Closed', steps: [1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0] },
      { name: 'Clap',  steps: [0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0] },
    ]
  };

  // channel step e.g. "set kick step 1 on"
  const stepMatch = t.match(/set\s+(\w+)\s+step\s+(\d+)\s+(on|off)/);
  if (stepMatch) {
    const chNames = ['kick','snare','hihat closed','hihat open','clap','bass','lead','pad'];
    const idx = chNames.findIndex(n => n.includes(stepMatch[1]));
    if (idx >= 0) return { type: 'set_step', channelIndex: idx, step: parseInt(stepMatch[2]) - 1, on: stepMatch[3] === 'on' };
  }

  return null;
}

export default function AiPanel() {
  const [messages, setMessages] = useState([
    { role: 'ai', text: 'Hi! I\'m the Stratum AI. Tell me what beat to make, or use a quick command below. I can also be controlled via MCP from Claude/Windsurf.' }
  ]);
  const [input, setInput] = useState('');
  const [busy, setBusy] = useState(false);
  const bottomRef = useRef(null);

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [messages]);

  async function handleSend() {
    const text = input.trim();
    if (!text) return;
    setInput('');
    setMessages(m => [...m, { role: 'user', text }]);
    setBusy(true);

    const cmd = parseUserIntent(text);
    if (cmd) {
      try {
        const result = await sendCmd(cmd);
        const delivered = result.delivered ?? 0;
        let reply = `Done! Command "${cmd.type}" sent.`;
        if (delivered === 0) reply += ' (No DAW window connected — open the app first)';
        setMessages(m => [...m, { role: 'ai', text: reply }]);
      } catch (e) {
        setMessages(m => [...m, { role: 'ai', text: `Error: ${e.message}` }]);
      }
    } else {
      setMessages(m => [...m, {
        role: 'ai',
        text: `I didn't understand "${text}". Try: "trap beat", "set bpm to 140", "play", "stop", "boom bap", "house", "drill", or "hip hop".`
      }]);
    }
    setBusy(false);
  }

  async function handleQuick(cmd) {
    setBusy(true);
    try {
      const result = await sendCmd(cmd);
      const delivered = result.delivered ?? 0;
      let reply = `Sent command "${cmd.type}".`;
      if (delivered === 0) reply += ' (DAW window not connected)';
      setMessages(m => [...m, { role: 'ai', text: reply }]);
    } catch (e) {
      setMessages(m => [...m, { role: 'ai', text: `Error: ${e.message}` }]);
    }
    setBusy(false);
  }

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', background: '#0d0d0f', color: '#d4d4d8', fontSize: 12 }}>
      {/* Header */}
      <div style={{ padding: '6px 10px', borderBottom: '1px solid #27272a', background: '#18181b', flexShrink: 0 }}>
        <span style={{ color: '#ff8c00', fontWeight: 'bold', fontSize: 13 }}>Stratum AI</span>
        <span style={{ marginLeft: 8, color: '#52525b', fontSize: 10 }}>MCP-powered DAW control</span>
      </div>

      {/* Messages */}
      <div style={{ flex: 1, overflowY: 'auto', padding: '8px 10px', display: 'flex', flexDirection: 'column', gap: 6 }}>
        {messages.map((m, i) => (
          <div key={i} style={{
            alignSelf: m.role === 'user' ? 'flex-end' : 'flex-start',
            maxWidth: '85%',
            background: m.role === 'user' ? '#1d4ed8' : '#27272a',
            borderRadius: 8,
            padding: '6px 10px',
            lineHeight: 1.5,
          }}>
            {m.text}
          </div>
        ))}
        {busy && (
          <div style={{ alignSelf: 'flex-start', color: '#ff8c00', fontSize: 11 }}>thinking…</div>
        )}
        <div ref={bottomRef} />
      </div>

      {/* Quick commands */}
      <div style={{ padding: '4px 8px', borderTop: '1px solid #27272a', display: 'flex', flexWrap: 'wrap', gap: 4, flexShrink: 0 }}>
        {QUICK_COMMANDS.map(q => (
          <button key={q.label} className="tool-btn" style={{ fontSize: 10, padding: '2px 7px' }}
            onClick={() => handleQuick(q.cmd)} disabled={busy}>
            {q.label}
          </button>
        ))}
      </div>

      {/* Input */}
      <div style={{ display: 'flex', gap: 6, padding: '6px 8px', borderTop: '1px solid #27272a', flexShrink: 0 }}>
        <input
          value={input}
          onChange={e => setInput(e.target.value)}
          onKeyDown={e => { if (e.key === 'Enter') handleSend(); }}
          placeholder="Ask AI to make a beat, set BPM, play…"
          disabled={busy}
          style={{
            flex: 1, background: '#18181b', border: '1px solid #3f3f46',
            color: '#d4d4d8', borderRadius: 6, padding: '5px 10px', fontSize: 12,
            outline: 'none',
          }}
        />
        <button className="tool-btn" onClick={handleSend} disabled={busy || !input.trim()}
          style={{ background: '#ff8c00', color: '#000', fontWeight: 'bold', padding: '4px 12px', borderRadius: 6 }}>
          Send
        </button>
      </div>
    </div>
  );
}
