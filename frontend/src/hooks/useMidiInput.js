import { useEffect, useState } from 'react';

// useMidiInput — subscribe to Web MIDI input (note on/off).
// onNote: ({ note, velocity, on }) => void
// Returns { devices, enabled, error }.
export function useMidiInput(onNote) {
  const [devices, setDevices] = useState([]);
  const [enabled, setEnabled] = useState(false);
  const [error, setError] = useState(null);

  useEffect(() => {
    if (!navigator.requestMIDIAccess) {
      setError('Web MIDI not supported in this environment');
      return;
    }
    let access;
    let cleanup = () => {};

    navigator.requestMIDIAccess({ sysex: false })
      .then((midi) => {
        access = midi;
        const inputs = Array.from(midi.inputs.values());
        setDevices(inputs.map(i => ({ id: i.id, name: i.name, manufacturer: i.manufacturer })));
        setEnabled(true);

        const handler = (event) => {
          const [status, dataA, dataB = 0] = event.data;
          const cmd = status & 0xf0;
          if (cmd === 0x90 && dataB > 0) {
            onNote?.({ note: dataA, velocity: dataB / 127, on: true });
          } else if (cmd === 0x80 || (cmd === 0x90 && dataB === 0)) {
            onNote?.({ note: dataA, velocity: 0, on: false });
          }
        };

        inputs.forEach(i => i.addEventListener('midimessage', handler));
        midi.onstatechange = () => {
          const list = Array.from(midi.inputs.values());
          setDevices(list.map(i => ({ id: i.id, name: i.name, manufacturer: i.manufacturer })));
          list.forEach(i => {
            i.removeEventListener('midimessage', handler);
            i.addEventListener('midimessage', handler);
          });
        };

        cleanup = () => {
          inputs.forEach(i => i.removeEventListener('midimessage', handler));
        };
      })
      .catch(err => setError(err.message || String(err)));

    return () => cleanup();
  }, [onNote]);

  return { devices, enabled, error };
}
