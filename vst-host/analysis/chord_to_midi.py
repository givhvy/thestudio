"""Map chord timelines to bass-root MIDI notes on a 16th-note grid."""

from __future__ import annotations

import re
from typing import Any

ROOT_TO_PC = {
    "C": 0,
    "B#": 0,
    "C#": 1,
    "DB": 1,
    "D": 2,
    "D#": 3,
    "EB": 3,
    "E": 4,
    "FB": 4,
    "F": 5,
    "E#": 5,
    "F#": 6,
    "GB": 6,
    "G": 7,
    "G#": 8,
    "AB": 8,
    "A": 9,
    "A#": 10,
    "BB": 10,
    "B": 11,
    "CB": 11,
}

CHORD_ROOT_RE = re.compile(
    r"^(?P<root>[A-G](?:#|b)?)(?P<quality>.*)$",
    re.IGNORECASE,
)

MIN_QUALITY_SUFFIXES = ("maj7", "min7", "m7b5", "m7", "dim7", "sus4", "sus2", "7", "m", "dim", "aug")


def parse_chord_root_pc(chord_name: str) -> int | None:
    name = (chord_name or "").strip()
    if not name or name.upper() in {"N", "NC", "X", "NOCHORD"}:
        return None

    if "/" in name:
        name = name.split("/", 1)[0].strip()

    # Normalise common spellings: Cmin7, Cmin, Cmajor, etc.
    upper = name.upper()
    for suffix in MIN_QUALITY_SUFFIXES:
        if upper.endswith(suffix.upper()) and len(name) > len(suffix):
            name = name[: -len(suffix)]
            break

    match = CHORD_ROOT_RE.match(name)
    if not match:
        return None

    root = match.group("root").upper()
    return ROOT_TO_PC.get(root)


def fold_pitch_to_c4_c6(pitch: int, previous: int | None = None) -> int:
    pitch = int(pitch)
    while pitch < 60:
        pitch += 12
    while pitch > 84:
        pitch -= 12
    pitch = max(60, min(84, pitch))

    if previous is not None and 60 <= previous <= 84:
        best = pitch
        best_dist = abs(pitch - previous)
        for candidate in (pitch - 12, pitch, pitch + 12):
            if 60 <= candidate <= 84:
                dist = abs(candidate - previous)
                if dist < best_dist:
                    best = candidate
                    best_dist = dist
        return best
    return pitch


def seconds_to_step(seconds: float, bpm: float) -> int:
    if bpm <= 0:
        bpm = 120.0
    return int(round((float(seconds) / 60.0) * float(bpm) * 4.0))


def chords_to_bass_notes(
    chords: list[dict[str, Any]],
    bpm: float,
    max_steps: int | None = None,
) -> list[dict[str, int]]:
    notes: list[dict[str, int]] = []
    previous_pitch: int | None = None
    min_steps = 2

    for seg in chords:
        chord = str(seg.get("chord", "")).strip()
        pc = parse_chord_root_pc(chord)
        if pc is None:
            continue

        start_sec = float(seg.get("time", 0.0))
        end_sec = float(seg.get("end", start_sec + 0.5))
        if end_sec <= start_sec:
            end_sec = start_sec + 0.25

        start_step = max(0, seconds_to_step(start_sec, bpm))
        end_step = max(start_step + 1, seconds_to_step(end_sec, bpm))
        length_steps = max(min_steps, end_step - start_step)

        if notes and notes[-1]["pitch"] == fold_pitch_to_c4_c6(60 + pc, previous_pitch):
            notes[-1]["lengthSteps"] += length_steps
            previous_pitch = notes[-1]["pitch"]
            continue

        pitch = fold_pitch_to_c4_c6(60 + pc, previous_pitch)
        previous_pitch = pitch

        if max_steps is not None and start_step >= max_steps:
            break
        if max_steps is not None and start_step + length_steps > max_steps:
            length_steps = max(min_steps, max_steps - start_step)

        duration_sec = end_sec - start_sec
        velocity = int(round(78 + min(24.0, duration_sec * 8.0)))

        notes.append(
            {
                "pitch": pitch,
                "startStep": start_step,
                "lengthSteps": length_steps,
                "velocity": max(70, min(115, velocity)),
            }
        )

    return notes
