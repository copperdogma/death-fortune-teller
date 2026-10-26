# TwoSkulls final working audio baseline audit — 2026-10-25

Scope: read-only inspection of TwoSkulls Git history/worktrees, final source,
retained artifacts, and qualification evidence. No hardware actions or code edits.
Generated configuration and compiler equivalence are a separate concurrent audit.

## Canonical source and artifact

The original `/Users/cam/Documents/Projects/TwoSkulls` checkout is legacy `main`
`e4b48ff`. Native shared-WROVER firmware is in
`/Users/cam/.codex/worktrees/onboard-sd-migration/TwoSkulls`, branch
`codex/onboard-sd-migration`, HEAD `57ff5cc`.

The final audio/recovery stabilization commit is
`f172a86843644a6e5164078978975d4c9b4eb594` (2026-10-15).
`git diff f172a86..57ff5cc -- firmware` is empty: the later commit adds hardware
and assembly material, not a different firmware baseline. Current worktree
modifications are a hardware README and three untracked battery/perfboard research
notes; no firmware changes. Git branches/worktrees/recent reflog show no later
native firmware version on this host. This does not establish absence of copies
outside this repository or another machine.

Actual retained `firmware/build/anthony_wrover.bin` SHA256:
`0cf0961ed9fdaa414b867ca3258a358eff1464f29c5ef5f0a2a5f4a1879644d9`.
Both `firmware/build/anthony_wrover.elf` and
`build/validation/2026-10-15-catalogue-release/flashed-image.elf` SHA256:
`003f1adabab1a027002db841756f4685d13b09c8dbb8d0253d8f71f4551d005a`.
These match `docs/validation/2026-10-15-code-qualification.md:13-16`.
The `cf4692a7...` image in the earlier implementation log/check-in note is an
intermediate acceptance image, not this final catalogue release.

## What actually solved the earlier problem

Primary local evidence is the final native code and these native-worktree notes:

- `docs/research/2026-10-15-radio-memory-debugging.md:243-274`: measured sustained
  production below consumption; a 20 ms block followed by an unconditional 10 ms
  sleep left too little budget for I/O, DSP and scheduling. An 8192-byte PSRAM
  stdio buffer reduced read cost but still left 39 underruns in the intermediate
  run. The next fix permits two successful publications per burst, then blocks
  for at least one RTOS tick. Every non-producing iteration also blocks.
- `firmware/main/skull_audio.cpp:26-29,105-109,190-214`: final FIFO remains
  **8 x 882 stereo frames (160 ms)**, with one **8192-byte PSRAM stdio buffer**
  passed to `setvbuf(...,_IOFBF,8192)`. PCM payloads are PSRAM; queue controls
  and task stacks are internal. The worker remains priority 5, 4096-byte stack.
- `docs/research/2026-10-15-a2dp-throughput.md`: 240 MHz CPU improved but did not
  eliminate sampled BTC/SBC encoder starvation. The final CMake include applies
  `-O2` after `-Os` to exactly eight SDK SBC encoder translation units, while
  all 308 other Bluetooth files stay size-optimized. No SDK source edit,
  producer affinity change, larger FIFO, removed yield or watchdog suppression.
- `docs/validation/2026-10-15-code-qualification.md:64-84`: the 240 MHz image
  still had 488 secondary pumpkin underruns and 11 watchdogs. Targeted SBC
  optimization cleared repeated troublesome clips, followed by the full release.
- `docs/research/2026-10-15-ledc-duty-start.md` and final qualification: once-only
  LEDC peripheral reset, applied-duty caching and nonblocking pending-duty guards
  remove a demonstrated unbounded critical-section spin/reset loop. This is
  relevant to shared realtime audio even though it is not a FIFO change.
- Native `firmware/main/skull_storage.cpp:35-46` uses 1-bit SDMMC, explicit
  20000 kHz, internal pullups and GPIO15/GPIO2 pullups. It has no later 40 MHz
  or aligned-reader fix hidden in the final commit.

The general problem class was sustained realtime producer/encoder throughput
with scheduler starvation, not insufficient queue depth alone. The project notes
cite Espressif's pinned v5.5.4 performance guide:
https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-guides/performance/speed.html
and distinguish local measurements from upstream advice. This audit needed no
new internet research because the exact pinned source, experiment decisions and
raw final evidence remain available locally.

## Verified success evidence

Read `build/validation/2026-10-15-catalogue-release/summary.json` directly:
`complete=true`, `passed=true`, `error=null`, `busy_reboot_refused=true`.
There are 11 skit results, each with exact source-frame completion, zero underruns
and zero watchdogs on both boards. Diagnostics report one boot and no panic or
watchdog on either board. This validates the summary against retained data,
not just a narrative. Raw serial logs contain private Matter traffic and were
not copied into this report.

`docs/validation/2026-10-15-code-qualification.md:3-7,20-28` records Cam's clean
dialogue/normal timing confirmation and Home interaction during Names. Acceptance
was bounded: outdoor soak, acoustic alignment, Jeff's disconnected actuators and
primary memory margin remained open. Zero PCM underruns alone was explicitly
not treated as proof of audible quality or hard realtime behavior.

## Implication for Death

Choosing native HEAD `57ff5cc` did include the final working source fixes. There
is no evidence here that Death was seeded from the earlier buffer-only source.
However, copying source and defaults does not prove effective generated build
configuration equivalence; verify the retained successful settings/commands.

Current Death intentionally diverges from the qualified audio data path with
32 FIFO blocks and a worker-owned aligned 4096-byte internal DMA POSIX reader
instead of 8 blocks and 8192-byte PSRAM buffered stdio. Its two-block yield and
LEDC guard are present. These differences must be described as new Death
experiments, not exact reuse of the proven path. Do not assume larger FIFO
replicates the original fix.

Decision: preserve this final source/artifact identity as the comparison point;
finish effective-build comparison before further speculative tuning. Remaining
uncertainty: which effective SDK differences or Death-specific SD/read behavior
explain current varying native PCM hashes. The final TwoSkulls qualification
does not independently settle Death's current data-integrity failure.
