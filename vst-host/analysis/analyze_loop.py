#!/usr/bin/env python3
"""
Stratum loop analysis — BTC chord recognition + bass-root MIDI export.
"""

from __future__ import annotations

import argparse
import json
import sys
import traceback
from pathlib import Path

ANALYSIS_DIR = Path(__file__).resolve().parent
LIVECHORD_BACKEND = ANALYSIS_DIR / "third_party" / "LiveChord" / "backend"


def _detect_bpm(audio_path: Path, hint: float | None) -> float:
    if hint and 40.0 <= hint <= 240.0:
        return float(hint)

    try:
        import librosa

        y, sr = librosa.load(str(audio_path), sr=22050, mono=True, duration=45.0)
        if y.size < sr:
            return 120.0
        tempo, _ = librosa.beat.beat_track(y=y, sr=sr, units="time")
        if hasattr(tempo, "__len__"):
            tempo = float(tempo[0]) if len(tempo) else 120.0
        tempo = float(tempo)
        if tempo < 70.0:
            tempo *= 2.0
        if tempo > 180.0:
            tempo *= 0.5
        if 40.0 <= tempo <= 240.0:
            return tempo
    except Exception:
        pass
    return 120.0


def _postprocess_chords(
    audio_path: Path,
    chords: list[dict],
    bpm_hint: float | None,
) -> tuple[float, list[dict]]:
    """Beat snap + ghost-chord filter. Prefer DAW/library BPM when close to detected."""
    processed = [dict(c) for c in chords]
    bpm = float(bpm_hint) if bpm_hint and 40.0 <= bpm_hint <= 240.0 else 120.0

    if not processed or not LIVECHORD_BACKEND.is_dir():
        return bpm, processed

    sys.path.insert(0, str(LIVECHORD_BACKEND))
    try:
        from beat_snap import analyze_and_snap_dynamic  # type: ignore

        snap = analyze_and_snap_dynamic(str(audio_path), processed)
        detected = snap.get("bpm")
        if detected and 40.0 <= float(detected) <= 240.0:
            detected_f = float(detected)
            if bpm_hint and abs(detected_f - float(bpm_hint)) / float(bpm_hint) <= 0.06:
                bpm = float(bpm_hint)
            else:
                bpm = detected_f

        downbeats = snap.get("downbeats") or []
        if downbeats:
            from stratum_btc import filter_ghost_boundary_chords  # type: ignore

            processed, _ = filter_ghost_boundary_chords(processed, downbeats, bpm)
    except Exception:
        pass

    return bpm, processed


def _analyze_with_btc(audio_path: Path, max_steps: int | None) -> tuple[list[dict], str]:
    from stratum_btc import detect_chords_and_key

    min_dur = 0.25 if max_steps and max_steps <= 32 else 0.35
    chords, key = detect_chords_and_key(str(audio_path), min_dur=min_dur)
    return chords, key


def analyze_loop(
    audio_path: Path,
    bpm_hint: float | None = None,
    max_steps: int | None = None,
) -> dict:
    if not audio_path.is_file():
        raise FileNotFoundError(f"Audio not found: {audio_path}")

    chords, key = _analyze_with_btc(audio_path, max_steps)
    if not chords:
        raise RuntimeError("BTC found no chords in this loop")

    bpm, chords = _postprocess_chords(audio_path, chords, bpm_hint)

    from chord_to_midi import chords_to_bass_notes

    notes = chords_to_bass_notes(chords, bpm, max_steps=max_steps)
    if not notes:
        raise RuntimeError("Could not map detected chords to bass MIDI notes")

    return {
        "ok": True,
        "engine": "btc-large-voca",
        "bpm": bpm,
        "key": key,
        "chords": chords,
        "notes": notes,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Analyze loop -> bass-root MIDI JSON")
    parser.add_argument("--input", required=True, help="Path to WAV/MP3/FLAC loop")
    parser.add_argument("--bpm", type=float, default=0.0, help="Optional BPM hint")
    parser.add_argument("--max-steps", type=int, default=0, help="Clip length in 16th steps")
    parser.add_argument("--json", action="store_true", help="Print JSON to stdout")
    parser.add_argument("--output", help="Optional JSON output file")
    args = parser.parse_args()

    audio_path = Path(args.input).resolve()
    bpm_hint = args.bpm if args.bpm > 0 else None
    max_steps = args.max_steps if args.max_steps > 0 else None

    try:
        result = analyze_loop(audio_path, bpm_hint=bpm_hint, max_steps=max_steps)
        payload = json.dumps(result, ensure_ascii=False)
        if args.output:
            Path(args.output).write_text(payload, encoding="utf-8")
        if args.json or not args.output:
            sys.stdout.buffer.write(payload.encode("utf-8"))
            sys.stdout.buffer.write(b"\n")
        return 0
    except Exception as exc:
        err = {
            "ok": False,
            "error": str(exc),
            "traceback": traceback.format_exc(),
        }
        payload = json.dumps(err, ensure_ascii=False)
        sys.stderr.buffer.write(payload.encode("utf-8"))
        sys.stderr.buffer.write(b"\n")
        if args.output:
            Path(args.output).write_text(payload, encoding="utf-8")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
