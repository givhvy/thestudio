# Stratum Piano Samples

Put piano sample files in this folder to make `Stratum Piano.vst3` use real piano sounds.

Supported formats:

- `.wav`
- `.aif`
- `.aiff`
- `.mp3`
- `.flac`

Name each file by its root MIDI note or note name:

```text
C3.wav
D#3.wav
F3.wav
A3.wav
C4.wav
E4.wav
G4.wav
C5.wav
60.wav
```

The plugin picks the closest available sample and pitch-shifts it to the note being played. You do not need a sample for every key. A good starter set is:

```text
C2.wav
F2.wav
C3.wav
F3.wav
C4.wav
F4.wav
C5.wav
F5.wav
C6.wav
```

If this folder has no valid samples, the plugin still makes sound using its built-in synthesized piano fallback.
