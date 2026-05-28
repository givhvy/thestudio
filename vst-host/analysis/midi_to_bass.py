"""Parse Chordify (or any) MIDI into bass-root notes on a 16th grid."""

from __future__ import annotations

import struct
from pathlib import Path
from typing import Any


def _read_var_len(data: bytes, pos: int) -> tuple[int, int]:
    value = 0
    while pos < len(data):
        b = data[pos]
        pos += 1
        value = (value << 7) | (b & 0x7F)
        if not (b & 0x80):
            break
    return value, pos


def _parse_midi_tracks(path: Path) -> list[list[tuple[int, int, int, int]]]:
    """Return list of tracks; each track is (abs_tick, pitch, vel, dur_ticks)."""
    data = path.read_bytes()
    if data[:4] != b"MThd":
        raise ValueError("Not a MIDI file")

    pos = 8
    fmt = struct.unpack(">HHH", data[pos : pos + 6])
    _format, num_tracks, division = fmt
    pos += 6
    if division & 0x8000:
        raise ValueError("SMPTE MIDI timing not supported")

    tracks_notes: list[list[tuple[int, int, int, int]]] = []
    for _ in range(num_tracks):
        if data[pos : pos + 4] != b"MTrk":
            break
        track_len = struct.unpack(">I", data[pos + 4 : pos + 8])[0]
        pos += 8
        track_end = pos + track_len
        tick = 0
        notes_on: dict[tuple[int, int], tuple[int, int]] = {}
        track: list[tuple[int, int, int, int]] = []
        running = 0

        while pos < track_end:
            delta, pos = _read_var_len(data, pos)
            tick += delta
            status = data[pos]
            if status < 0x80:
                status = running
                pos -= 1
            else:
                pos += 1
                if status < 0xF0:
                    running = status

            if status == 0xFF:
                _meta, pos = _read_var_len(data, pos)
                ln, pos = _read_var_len(data, pos)
                pos += ln
                continue
            if status == 0xF0 or status == 0xF7:
                ln, pos = _read_var_len(data, pos)
                pos += ln
                continue

            cmd = status & 0xF0
            ch = status & 0x0F
            if cmd == 0x90:
                n, pos = _read_var_len(data, pos)
                v, pos = _read_var_len(data, pos)
                if v > 0:
                    notes_on[(ch, n)] = (tick, v)
                else:
                    key = (ch, n)
                    if key in notes_on:
                        st, vel = notes_on.pop(key)
                        track.append((st, n, vel, tick - st))
            elif cmd == 0x80:
                n, pos = _read_var_len(data, pos)
                _v, pos = _read_var_len(data, pos)
                key = (ch, n)
                if key in notes_on:
                    st, vel = notes_on.pop(key)
                    track.append((st, n, vel, tick - st))
            elif cmd in (0xA0, 0xB0, 0xE0):
                _, pos = _read_var_len(data, pos)
                _, pos = _read_var_len(data, pos)
            elif cmd in (0xC0, 0xD0):
                _, pos = _read_var_len(data, pos)
            else:
                break

        tracks_notes.append(track)
        pos = track_end

    return tracks_notes


def _pick_bass_track(tracks: list[list[tuple[int, int, int, int]]]) -> list[tuple[int, int, int, int]]:
    if not tracks:
        return []
    if len(tracks) == 1:
        return tracks[0]

    def avg_pitch(tr: list[tuple[int, int, int, int]]) -> float:
        if not tr:
            return 127.0
        return sum(n[1] for n in tr) / len(tr)

    return min(tracks, key=avg_pitch)


def midi_file_to_bass_notes(midi_path: Path, bpm: float, max_steps: int | None = None) -> list[dict[str, int]]:
    from chord_to_midi import fold_pitch_to_c4_c6, seconds_to_step

    tracks = _parse_midi_tracks(midi_path)
    raw = _pick_bass_track(tracks)
    if not raw:
        return []

    # Assume 480 ticks/quarter unless we parse tempo — Chordify filenames often include BPM.
    ticks_per_beat = 480.0
    sec_per_tick = 60.0 / (bpm * ticks_per_beat)

    notes: list[dict[str, int]] = []
    previous: int | None = None
    min_steps = 2

    for start_tick, pitch, vel, dur_ticks in sorted(raw, key=lambda x: x[0]):
        start_sec = start_tick * sec_per_tick
        end_sec = (start_tick + max(dur_ticks, ticks_per_beat // 4)) * sec_per_tick
        start_step = max(0, seconds_to_step(start_sec, bpm))
        end_step = max(start_step + 1, seconds_to_step(end_sec, bpm))
        length = max(min_steps, end_step - start_step)

        if max_steps is not None and start_step >= max_steps:
            break
        if max_steps is not None and start_step + length > max_steps:
            length = max(min_steps, max_steps - start_step)

        p = fold_pitch_to_c4_c6(int(pitch), previous)
        previous = p

        if notes and notes[-1]["pitch"] == p:
            notes[-1]["lengthSteps"] += length
            continue

        notes.append(
            {
                "pitch": p,
                "startStep": start_step,
                "lengthSteps": length,
                "velocity": max(70, min(115, int(vel))),
            }
        )

    return notes


def midi_file_to_all_notes(midi_path: Path, bpm: float, max_steps: int | None = None) -> list[dict[str, int]]:
    """Import every note from every MIDI track (Chordify chord voicings)."""
    from chord_to_midi import seconds_to_step

    tracks = _parse_midi_tracks(midi_path)
    raw: list[tuple[int, int, int, int]] = []
    for tr in tracks:
        raw.extend(tr)
    if not raw:
        return []

    ticks_per_beat = 480.0
    sec_per_tick = 60.0 / (bpm * ticks_per_beat)
    min_steps = 1
    notes: list[dict[str, int]] = []

    for start_tick, pitch, vel, dur_ticks in sorted(raw, key=lambda x: (x[0], x[1])):
        start_sec = start_tick * sec_per_tick
        end_sec = (start_tick + max(dur_ticks, ticks_per_beat // 8)) * sec_per_tick
        start_step = max(0, seconds_to_step(start_sec, bpm))
        end_step = max(start_step + 1, seconds_to_step(end_sec, bpm))
        length = max(min_steps, end_step - start_step)

        if max_steps is not None and start_step >= max_steps:
            break
        if max_steps is not None and start_step + length > max_steps:
            length = max(min_steps, max_steps - start_step)

        notes.append(
            {
                "pitch": max(36, min(96, int(pitch))),
                "startStep": start_step,
                "lengthSteps": length,
                "velocity": max(60, min(127, int(vel))),
            }
        )

    return notes
