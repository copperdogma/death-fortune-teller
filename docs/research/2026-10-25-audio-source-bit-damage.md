# Repeating residual clicks were present in the backed-up WAV data

After fresh allocation, Welcome, fortune preamble 2 and Goodbye streamed with exact hashes against the card backup and zero underruns. Cam initially heard clean Goodbye, then corrected this to about half a dozen minor clicks. Replays reproduced the clicks at the same positions: one around halfway and several in the last five seconds. Jaw held fixed throughout, excluding jaw movement as the source of those heard clicks in this comparison.

## Direct recording comparison

The repository and card-backup WAVs have matching recording lengths and PCM parameters. Six backup recordings differ from repository PCM in sparse bytes, predominantly single-bit changes. Three are byte-identical in PCM. This is data damage, not a different performance/edit or routine gain change. Earlier statements calling the card backup a qualified reference describe copy/read verification, not proof of pristine original recording data; this distinction matters.

| Clip | Differing PCM bytes | Isolated large impulses, backup / project |
|---|---:|---:|
| finger_prompt | 170 | 7 / 0 |
| finger_snap | 56 | 6 / 0 |
| fortune_2 | 0 | 0 / 0 |
| fortune_3 | 497 | 23 / 0 |
| fortune_4 | 513 | 17 / 0 |
| goodbye | 616 | 26 / 0 |
| initialized | 0 | 0 / 0 |
| no_finger | 0 | 0 / 0 |
| welcome | 721 | 24 / 0 |

In Goodbye, 616 differing bytes contain 635 differing bits; 96.9% of differing bytes have one bit changed. At 10.940476 seconds, consecutive left samples in backup are `-21, -32766, 23`; project samples are `-21, 2, 23`. Further large impulses occur around 15.25–15.96, 16.33, 17.07/17.66, 18.42 and 19.11 seconds, matching Cam's reported distribution. Counts above describe a diagnostic isolated-impulse detector, not a perceptual count of audible clicks.

Original project Goodbye: file SHA256 `cce078c0294bba663dbf4cdbeaec2ee2cd39bbfa12ef1f3befa9cbea67f0b34c`, PCM FNV1a `487245a5`, 887196 frames, peak19552. Backup FNV `6a740064`/peak32766 reflects damaged samples. Nonzero sample count1771650 is identical, so that statistic alone missed the bit damage. Exact device hash against backup proved faithful playback of damaged source bytes.

## Decision and verification

Restore original project WAVs at fresh card allocations, preserving `/audio-original-20261025` and archiving the backup-derived active folder as `/audio-backup-copy-20261025`. No runtime de-click filter or further buffer change is justified by this finding. All nine project files were read uncached twice, independently snapshotted, and verified; snapshot/manifest retained in ignored validation directory. Source-only local switch rehearsal verified both archives remain intact. On-card and streamed hashes must now be compared against the project originals, not the damaged backup.

No claim is made about when the original card bytes became damaged, nor whether every possible Bluetooth click is resolved. Repeated listening and full-flow tests remain necessary. Root cause of the card's unstable original-location reads is still separate from the demonstrated damage already captured in the backup.
