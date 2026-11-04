# Pitch-Corrector
Free, open-source (GPLv3) auto-pitch correction VST3 for monophonic vocals.

## Build
Requires CMake >= 3.21 and a C++17 compiler (MSVC, Xcode/Clang, GCC/Clang).
Linux also needs the JUCE dev packages listed in `.github/workflows/ci.yml`.

    git clone <repo> && cd Pitch-Corrector
    cmake --preset release
    cmake --build --preset release
    ctest --preset release

The `.vst3` is copied to your system VST3 folder after the build.

## Usage
The editor is a dark, hardware-style panel: knob columns left and right of a pitch display (target note
large, dial lit by how many cents the input is off), a piano strip showing the scale, target and detected notes
(click a key to set the root), the vibrato panel, and a scale/root row along the bottom.

- **Key / Scale:** notes the voice is snapped to.
- **Correction:** 0 % leaves the pitch alone, 100 % moves it fully onto the target note.
- **Retune Speed:** 0 gives slow glides to the note (about 400 ms), 100 gives near-instant snapping (about 5 ms).
- **Humanize:** the longer a note is held, the slower the correction becomes, so sustained notes keep their natural
  movement while short notes are still snapped at the Retune Speed. 0 = off.
- **Flex-Tune:** a tolerance window around each target note. Pitch inside the window is mostly left alone; pitch
  well outside it is fully corrected, with a smooth transition. 0 = off, 100 = widest window (about +/-30 cents).
- **Natural Vibrato:** the singer's vibrato is separated from the note being tuned. 100 % keeps it, 0 % flattens it,
  200 % doubles its depth. Tuning acts on the vibrato's centre, so vibrato no longer fights the correction.
- **Vibrato (artificial):** adds an LFO vibrato to held notes. *Shape* (sine, triangle, square, saw), *Rate* (Hz),
  *Depth* (cents, 0 = off), *Variation* (slow random-like drift of rate and depth), *Onset Delay* (ms after a note
  starts before vibrato begins) and *Onset Rise* (ms to reach full depth).
- **Bypass:** crossfades to the latency-matched dry signal.

## Algorithm
- **Detection:** YIN (2048-sample window, 512 hop, 0.15 threshold, parabolic interpolation). Unvoiced or quiet frames fade out the effect.
- **Correction:** (`src/dsp/CorrectionEngine.h`, no JUCE dependency, unit-tested) nearest in-scale note with 12-cent
  hysteresis, flex-tune soft knee, one-pole glide whose time constant grows with note age (Humanize).
- **Vibrato handling:** a 0.21 s moving average gives the vibrato centre; vibrato is detected from the window's
  spread and zero crossings, latched with hysteresis, and cross-faded in over about 150 ms. Jumps larger than the
  vibrato extent are treated as new notes. Output pitch = target + naturalVibrato x (input - centre) + LFO vibrato.
- **Shifting:** streaming TD-PSOLA with peak-snapped analysis marks. Grain shape is preserved, so formants stay put.
- **Latency:** about 2.75 periods of an 80 Hz voice (about 34 ms at 44.1 kHz), reported to the host and matched on the dry path.

## Known limitations
Monophonic only. No Studio/Rubber Band mode, formant toggle, or meters yet.
Vibrato detection needs about 0.2 s of steady vibrato before Natural Vibrato takes effect; Humanize/Flex-Tune/vibrato
behaviour is verified on synthetic pitch tracks, not yet by ear.
Voices below 80 Hz are not tracked. Not yet compiled or host-tested.

## License
GPLv3. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.
