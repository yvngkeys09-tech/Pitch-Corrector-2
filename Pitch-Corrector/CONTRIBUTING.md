# Contributing
- C++17, formatted with `clang-format` (see `.clang-format`).
- No allocation, locks, or I/O in the audio thread (`processBlock` and everything it calls).
- Add or update unit tests in `tests/` for any DSP change.
- Keep PRs focused; describe the audible effect of DSP changes and any latency change.
- By contributing you agree your code is licensed under GPLv3.
