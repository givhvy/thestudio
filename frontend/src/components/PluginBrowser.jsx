import { useState, useRef, useEffect } from 'react';
import { WAM_REGISTRY, loadWam, wamNoteOn, wamNoteOff, wamShowGui, unloadWam, getLoadedWams } from '../wam/WamLoader.js';
import { initAudio } from '../audio.js';

const TAB = { WAM: 'wam', VST: 'vst' };

export default function PluginBrowser({ masterNode, onChannelAdd, onSlotLoad, onClose }) {
  const [tab, setTab] = useState(TAB.WAM);
  const [loadedWams, setLoadedWams] = useState([]);
  const [vstPlugins, setVstPlugins] = useState([]);
  const [vstStatus, setVstStatus] = useState('');
  const [loading, setLoading] = useState(null);
  const [activeGui, setActiveGui] = useState(null);
  const guiContainerRef = useRef(null);
  const modalGuiRef = useRef(null);

  useEffect(() => {
    if (!activeGui || !modalGuiRef.current) return;
    wamShowGui(activeGui.slotId, modalGuiRef.current);
  }, [activeGui]);

  async function handleLoadWam(entry) {
    setLoading(entry.id);
    try {
      initAudio();
      const { slotId, name } = await loadWam(
        window.__audioCtx || (window.__audioCtx = new AudioContext()),
        entry.id,
        masterNode || window.__audioCtx.destination
      );
      const updated = getLoadedWams();
      setLoadedWams(updated);

      // If loading into a mixer slot, call onSlotLoad with the loaded plugin info
      if (onSlotLoad) {
        onSlotLoad({ name, type: 'wam', slotId: slotId, path: entry.id });
      } else {
        // Otherwise, add as a channel (old behavior)
        onChannelAdd?.({
          name: name.slice(0, 18),
          color: '#60a5fa',
          type: `wam:${slotId}`,
          steps: Array(16).fill(0),
          vol: 80, pan: 0, mute: false, solo: false, mixerTrack: 0,
          wamSlotId: slotId,
        });
      }
    } catch (err) {
      console.error('WAM load failed', err);
      alert('Could not load WAM plugin: ' + (err.message || err));
    }
    setLoading(null);
  }

  async function handleShowGui(slotId) {
    const plugin = loadedWams.find(w => w.slotId === slotId);
    setActiveGui({ slotId, name: plugin?.name ?? 'Plugin' });
  }

  async function handleScanVst() {
    if (!window.electronAPI?.vstScanFolder) {
      setVstStatus('VST host not compiled yet. See vst-host/BUILD.md');
      return;
    }
    setVstStatus('Scanning...');
    const res = await window.electronAPI.vstScanFolder();
    if (res.error) { setVstStatus(res.error); return; }
    if (res.canceled) { setVstStatus('Cancelled.'); return; }
    setVstPlugins(res.plugins || []);
    setVstStatus(`Found ${res.plugins?.length ?? 0} plugin(s)`);
  }

  async function handleLoadVst(p) {
    if (!window.electronAPI?.vstCall) return;
    setLoading(p.fileOrIdentifier);
    const res = await window.electronAPI.vstCall('loadPlugin', { fileOrIdentifier: p.fileOrIdentifier });
    if (res.error) { alert(res.error); setLoading(null); return; }
    const { slotId } = res.result;

    // If loading into a mixer slot, call onSlotLoad
    if (onSlotLoad) {
      onSlotLoad({ name: p.name, type: 'vst', slotId: slotId, path: p.fileOrIdentifier });
    } else {
      // Otherwise, add as a channel (old behavior)
      onChannelAdd?.({
        name: p.name.slice(0, 18),
        color: '#a78bfa',
        type: `vst:${slotId}`,
        steps: Array(16).fill(0),
        vol: 80, pan: 0, mute: false, solo: false, mixerTrack: 0,
        vstSlotId: slotId,
      });
    }
    setLoading(null);
  }

  async function handleShowVstGui(vstSlotId) {
    await window.electronAPI?.vstCall('showEditor', { slotId: vstSlotId, show: true });
  }

  return (
    <div className="flex flex-col h-full bg-panel border border-zinc-700 rounded-lg overflow-hidden text-zinc-300 text-[11px]">
      {/* Tabs */}
      <div className="flex h-8 bg-[#18181b] border-b border-zinc-900">
        {[TAB.WAM, TAB.VST].map(t => (
          <button
            key={t}
            onClick={() => setTab(t)}
            className={`flex-1 h-full tracking-widest uppercase text-[9px] font-bold transition-colors
              ${tab === t
                ? 'bg-panel-orange text-orange-950 shadow-glow-orange'
                : 'text-zinc-500 hover:text-zinc-200'
              }`}
          >
            {t === TAB.WAM ? 'WASM Plugins' : 'VST / DLL'}
          </button>
        ))}
      </div>

      {/* WAM tab */}
      {tab === TAB.WAM && (
        <div className="flex flex-col flex-1 overflow-hidden">
          <div className="px-3 py-2 text-[9px] text-zinc-500 tracking-widest uppercase border-b border-zinc-900">
            Free WASM instruments — no install needed
          </div>
          <div className="flex-1 overflow-y-auto">
            {WAM_REGISTRY.map(entry => {
              const loaded = loadedWams.find(w => w.name === entry.name);
              return (
                <div key={entry.id}
                  className="flex items-center gap-2 px-3 py-2 border-b border-zinc-900 hover:bg-white/[0.025]">
                  <span className={`w-2 h-2 rounded-full flex-shrink-0 ${entry.isInstrument ? 'bg-accent' : 'bg-blue-400'}`} />
                  <span className="flex-1 truncate text-zinc-300">{entry.name}</span>
                  {loaded ? (
                    <button
                      onClick={() => handleShowGui(loaded.slotId)}
                      className="px-2 py-0.5 bg-zinc-700 rounded text-[9px] text-zinc-300 hover:bg-zinc-600"
                    >GUI</button>
                  ) : (
                    <button
                      onClick={() => handleLoadWam(entry)}
                      disabled={loading === entry.id}
                      className="px-2 py-0.5 bg-panel-orange rounded text-[9px] text-orange-950 font-bold hover:brightness-110 disabled:opacity-50"
                    >{loading === entry.id ? '...' : 'Load'}</button>
                  )}
                </div>
              );
            })}
          </div>
          {/* GUI mount target */}
          <div
            ref={guiContainerRef}
            className="border-t border-zinc-900 min-h-0 overflow-auto"
            style={{ maxHeight: 280 }}
          />
        </div>
      )}

      {/* VST tab */}
      {tab === TAB.VST && (
        <div className="flex flex-col flex-1 overflow-hidden">
          <div className="px-3 py-2 flex items-center gap-2 border-b border-zinc-900">
            <button
              onClick={handleScanVst}
              className="px-3 py-1 bg-panel-raised rounded text-zinc-300 text-[9px] uppercase tracking-widest hover:text-white font-bold"
            >Scan Folder…</button>
            <span className="text-zinc-500 text-[9px]">{vstStatus}</span>
          </div>
          {vstPlugins.length === 0 ? (
            <div className="flex-1 flex flex-col items-center justify-center gap-3 px-6 text-center">
              <span className="text-zinc-600 text-[10px] leading-relaxed">
                Click <strong className="text-zinc-400">Scan Folder</strong> to load your VST3/.dll plugins.
                <br/>
                Requires <code className="text-accent">StratumVSTHost.exe</code> to be compiled.
                <br/>
                See <code className="text-blue-400">vst-host/BUILD.md</code> for instructions.
              </span>
            </div>
          ) : (
            <div className="flex-1 overflow-y-auto">
              {vstPlugins.map((p, i) => (
                <div key={i}
                  className="flex items-center gap-2 px-3 py-2 border-b border-zinc-900 hover:bg-white/[0.025]">
                  <span className={`w-2 h-2 rounded-full flex-shrink-0 ${p.isInstrument ? 'bg-purple-400' : 'bg-blue-400'}`} />
                  <div className="flex-1 min-w-0">
                    <div className="truncate text-zinc-200">{p.name}</div>
                    <div className="text-zinc-600 text-[9px]">{p.pluginFormatName} · {p.manufacturer}</div>
                  </div>
                  <button
                    onClick={() => handleLoadVst(p)}
                    disabled={loading === p.fileOrIdentifier}
                    className="px-2 py-0.5 bg-purple-800 rounded text-[9px] text-purple-200 hover:bg-purple-700 disabled:opacity-50 font-bold"
                  >{loading === p.fileOrIdentifier ? '...' : 'Load'}</button>
                </div>
              ))}
            </div>
          )}
        </div>
      )}
      {activeGui && (
        <div className="fixed inset-0 z-[5000] flex items-center justify-center bg-black/70 backdrop-blur-sm p-8">
          <div className="relative max-w-[95vw] max-h-[92vh] overflow-auto rounded-2xl shadow-2xl border border-zinc-800 bg-black">
            <div className="sticky top-0 z-10 h-8 px-3 flex items-center justify-between bg-[#18181b] border-b border-zinc-900">
              <span className="text-[10px] uppercase tracking-[0.24em] text-zinc-500">{activeGui.name}</span>
              <button
                onClick={() => setActiveGui(null)}
                className="w-6 h-6 rounded text-zinc-500 hover:text-white hover:bg-red-500/80"
                title="Close Plugin UI"
              >
                ×
              </button>
            </div>
            <div ref={modalGuiRef} />
          </div>
        </div>
      )}
    </div>
  );
}
