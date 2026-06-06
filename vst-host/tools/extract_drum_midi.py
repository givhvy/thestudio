#!/usr/bin/env python3
"""Extract drum onsets from audio into GM drum MIDI for Stratum DAW.

Usage:
  python extract_drum_midi.py song.wav --out Drake/ICEMAN/what-did-i-miss.mid --bpm 126
  python extract_drum_midi.py song.wav --out popular-song-midi/Drake/ICEMAN/what-did-i-miss.mid

Requires: pip install librosa mido numpy soundfile
Optional (better separation): pip install demucs torch
"""

from __future__ import annotations

import argparse
from pathlib import Path

import librosa
import mido
import numpy as np


GM = {
    "kick": 36,
    "snare": 38,
    "hat": 42,
    "perc": 39,
}


def load_mono(path: Path, sr: int = 22050) -> tuple[np.ndarray, int]:
    y, sr = librosa.load(path, sr=sr, mono=True)
    return y, sr


def maybe_isolate_drums(y: np.ndarray, sr: int) -> np.ndarray:
    try:
        import torch
        from demucs.pretrained import get_model
        from demucs.apply import apply_model
    except Exception:
        return y

    wav = torch.from_numpy(y[None, None, :].astype(np.float32))
    model = get_model("htdemucs")
    model.eval()
    with torch.no_grad():
        sources = apply_model(model, wav, split=True, overlap=0.25, progress=False)[0]
    names = model.sources
    if "drums" in names:
        idx = names.index("drums")
        drum = sources[idx, 0].numpy()
        return drum
    return y


def classify_onset(y: np.ndarray, sr: int, onset: int, window: int = 512) -> str:
    start = max(0, onset - window // 2)
    end = min(len(y), onset + window // 2)
    chunk = y[start:end]
    if chunk.size == 0:
        return "perc"

    spec = np.abs(np.fft.rfft(chunk * np.hanning(len(chunk))))
    freqs = np.fft.rfftfreq(len(chunk), 1 / sr)
    low = spec[freqs < 180].sum()
    mid = spec[(freqs >= 180) & (freqs < 2500)].sum()
    high = spec[freqs >= 2500].sum()
    total = low + mid + high + 1e-9

    if low / total > 0.55:
        return "kick"
    if high / total > 0.45:
        return "hat"
    if mid / total > 0.35:
        return "snare"
    return "perc"


def write_midi(events: list[tuple[int, str, int]], bpm: int, out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    mid = mido.MidiFile(ticks_per_beat=480)
    track = mido.MidiTrack()
    mid.tracks.append(track)
    track.append(mido.MetaMessage("set_tempo", tempo=mido.bpm2tempo(bpm), time=0))

    ticks_per_16th = 480 // 4
    last_tick = 0
    for tick, kind, velocity in sorted(events, key=lambda e: e[0]):
        delta = max(0, tick - last_tick)
        track.append(
            mido.Message(
                "note_on",
                note=GM[kind],
                velocity=int(velocity),
                time=delta,
            )
        )
        track.append(mido.Message("note_off", note=GM[kind], velocity=0, time=ticks_per_16th // 2))
        last_tick = tick + ticks_per_16th // 2

    mid.save(out_path)


def extract(audio: Path, out: Path, bpm: int, use_demucs: bool) -> None:
    y, sr = load_mono(audio)
    if use_demucs:
        y = maybe_isolate_drums(y, sr)

    onsets = librosa.onset.onset_detect(y=y, sr=sr, units="samples", backtrack=True)
    events: list[tuple[int, str, int]] = []
    ticks_per_16th = 480 // 4
    seconds_per_16th = 60.0 / bpm / 4.0

    for onset in onsets:
        t = onset / sr
        tick = int(round(t / seconds_per_16th)) * ticks_per_16th
        kind = classify_onset(y, sr, onset)
        rms = float(np.sqrt(np.mean(y[max(0, onset - 256) : onset + 256] ** 2)))
        velocity = int(np.clip(40 + rms * 400, 40, 120))
        events.append((tick, kind, velocity))

    write_midi(events, bpm, out)
    print(f"Wrote {out} ({len(events)} hits @ {bpm} BPM)")


def main() -> None:
    parser = argparse.ArgumentParser(description="Extract drum MIDI from audio")
    parser.add_argument("audio", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--bpm", type=int, default=120)
    parser.add_argument("--demucs", action="store_true", help="Try Demucs drum separation first")
    args = parser.parse_args()

    out = args.out
    if not out.is_absolute() and not str(out).startswith("popular-song-midi"):
        out = Path("data/popular-song-midi") / out

    extract(args.audio, out, args.bpm, args.demucs)


if __name__ == "__main__":
    main()
