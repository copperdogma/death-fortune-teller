# Story 022 — Continuous Bluetooth Speaker Reconnect

Status: Done — Cam verified delayed speaker power-on and audible startup on 2026-10-27.

## Requirement

Death must keep checking for its configured Bluetooth speaker indefinitely.
Turning the speaker on after Death boots, or after its initial attempts finish,
must recover without USB, console commands, new pairing or reboot.

## Acceptance

- Repeated connection/discovery episodes continue while the speaker is absent.
- Speaker turned on later connects automatically; saved selection stays intact.
- Established-link loss recovers automatically.
- No concurrent open attempts or discovery during an active speaker connection.
- Media readiness/start retries also continue until a usable stream exists.
- Human confirms actual audio and delayed-speaker behavior.

## Work log — 2026-10-27

- Cam booted Death with the speaker off, requested Home Start, then powered the
  speaker and tried its Bluetooth pairing button. No automatic connection.
- Attached a single USB serial relay with DTR/RTS both asserted, without a reset
  request. Existing uptime ~339 seconds; Waiting, SD mounted/assets9,
  speaker0/stream0, callbacks0, radio fault0/drop0, no retained interruption or
  pending report. Earlier boot logs unavailable; exact initial failure sequence
  cannot be reconstructed from this capture.
- At uptime346866ms sent existing `retry`. Saved-address attempt1 connected at
  348516ms; stream ready348626ms; startup announcement delivered353076ms.
  Subsequent status remained speaker1/stream1. No reboot/re-pair/flash required
  for this recovery. User has not yet confirmed audible output in this chat.
- Owning native source deliberately ended recovery after three cached-address
  attempts and one finite inquiry. Separate75second connection/60second ACL
  deadlines permanently latched recovery off; media start also stopped after3.
- User explicitly requires indefinite checking. Implement recurring5second
  backoff between failed attempts/episodes, retaining event-confirmed gates for
  pending A2DP opens and ACL teardown. Remove permanent timeout latches; slow
  completion resumes recovery. Keep three remembered attempts per episode plus
  exact-name discovery. Media retries continue with2seconds initially,5seconds
  thereafter. No shared-host restart or Matter/fabric reset.
- Research: see ../research/2026-10-27-continuous-speaker-reconnect.md. Production
  radio host harness and independent review requested from a project-authorized
  subagent. Native build and existing host/Death suites running.

Private live capture: firmware/build/validation/death-first-flash-20261025/bluetooth-late-speaker-20261027.log.

- Production-source radio harness now exercises20 complete absent-speaker
  episodes (61 cached connection requests/20 inquiries), uncached repeated
  discovery and failed discovered opens,31 synchronous API rejections with
  pacing,450second pending-open completion,300second ACL teardown and both
  callback orderings, and >20 media retries followed by success. Independent
  review found no production issue; ASan/UBSan radio harness PASS. Existing
  host suites and Death show/assets/fortune suites PASS before radio addition;
  rerunning host suite including new radio harness. Cam has turned speaker off
  for delayed-power-on hardware verification.

- Actual old-firmware failure reproduced before upload: speaker power-off caused
  A2DP disconnect541676ms, remembered attempts546776/556926/567076ms, inquiry
 572216..582466ms with no match. No further automatic attempts through621936ms.
  This directly confirms the old give-up behavior rather than merely inferring
  it from source. All updated host suites including radio and Death suites PASS;
  native build PASS. Application-only upload started; NVS/table retained.

- Uploaded application at0x30000, esptool hash verified. Binary SHA256
 85533f14632de9d409c1d092592e4ae3fe3e502eb5b28830e009481b12b8006d.
  Native boot running with speaker deliberately off, SD1/assets9, Waiting, no
  radio fault/drop. Capture story022-delayed-speaker-boot.log in private validation
  folder. Speaker cache is being used; Home provisioning partition preserved.

- Hardware absent-speaker check: updated boot completed third saved attempt
  31086ms, inquiry36116..46366ms, then automatically started a fresh cached
  episode51386ms. Second episode continued through61536/71686ms and another
  inquiry81836ms without console `retry`. This demonstrates actual continued
  checking beyond the original lifetime budget. Asked Cam to power speaker on
  normally without pairing mode; awaiting connection/audio observation.

- Hardware recovery: continued through a third episode automatically; cached
  attempt117406ms connected120646ms, stream started120716ms, startup delivered
  125166ms. Status126926ms speaker1/stream1, callbacks2114, radio fault0/drop0.
  Cam: “It's back on. And I heard the announcement.” No console retry, pairing
  reset or reboot after delayed speaker power-on. Human delayed-speaker/audio
  verification satisfied; story complete. Established-session-loss shares the
  same continuous recovery path; no additional physical endurance run claimed.
- No commit/push. Unrelated plan edit and schematics preserved.
