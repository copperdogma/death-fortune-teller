# Story 021 — Fortune Sentence Spacing

**Status:** Done — human verified sentence spacing

## Scope

Cam reports missing spaces in "trousers.Seek" and "clown.On". Printing and timing from Story020 are human accepted. Correct the pre-existing prose issue without changing print layout, timing, word selection or SD contents.

## Work log

### 2026-10-26 — Source and regression fix

- Current boot explicitly loaded `/sdcard/fortunes/little_kid_fortunes.json`; repository copy already has spaces, but verified embedded fallback preserves the old card template containing `.Seek ye` and `.On the dawn`. Thus a firmware fix needs to cover both loaded-card and embedded prose.
- After token substitution, insert one space at a lowercase-letter/period/uppercase-letter boundary. This narrowly handles the observed capitalized sentences; existing whitespace, decimals, initials and ellipses are preserved. It is not a general natural-language sentence parser and does not claim to handle every punctuation/language convention.
- Keep the verified embedded JSON bytes and source assets intact. Maintain the1024-byte generated-fortune limit including inserted spaces.
- Regression tests cover the reported token-adjacent boundaries, already spaced/newline/tab prose, decimals, initials, ellipses, overflow after inserted spaces, and100 built-in fortunes with both repaired sentence breaks. Death show/asset/fortune suites pass under ASan/UBSan; build/install underway.
- Story020 marked Done following Cam's explicit centered-logo/signature/timing acceptance. No commit/push requested or made.

## Remaining

- [x] Build/install and confirm clean boot.
- [x] Human confirms sentence spaces on the next normal print.

### 2026-10-26 — Installed

- Native build and whitespace checks PASS. Application-only flash at0x30000 hash verified; binary SHA256 `40ce2255630c7fa917361cb85fe415deda930275a9201414669b324e25952c1d`. NVS, bootloader, partition table and SD contents untouched.
- Boot reports9assets, show Waiting, printer ready1/busy0 and speaker1/stream1. No extra print triggered. Private evidence: ignored `firmware/build/story021-{tests,build,flash,boot}.log`.
- Next normal print provides physical confirmation of the repaired sentence spacing. No commit/push.

### 2026-10-26 — Human acceptance and closeout

- Cam: "Works perfectly. Finish and push". Sentence spacing accepted on hardware; Story021 complete. Story020 layout, signature and timing already accepted.
- Authorized closeout includes Stories020/021 firmware, generator, tests and documentation only. Preserve unrelated OTA-plan edits and untracked schematics. Existing native build, targeted sanitizer tests, flash-hash verification and human physical acceptance cover the installed final code.
