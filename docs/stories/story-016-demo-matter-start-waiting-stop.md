# Story 016 — Demo Matter Start, Waiting and Dead Stop

**Status:** Done

## Scope

For the demo, Death exposes three Home-compatible Matter controls. Start runs the existing welcome followed automatically by fortune generation, fortune speech/printing, goodbye, and configured cooldown. Waiting is an idle indicator with no action. Dead Stop cancels current activity and reports Waiting immediately. The Box of Death is a separate Matter controller; its Home automation will turn on Start. No Box of Death project changes are included.

## Acceptance criteria

- Start is accepted only when ready and remains active throughout the complete flow and cooldown.
- Repeated Start commands during activity/cooldown are dropped, never queued or replayed.
- No Near, finger, sensor, or timeout trigger is required to complete the sequence.
- Fortune prints once per accepted Start with the existing pirate-skull header; goodbye and cooldown finish before another Start is accepted.
- Dead Stop cancels startup audio, diagnostic tone, show audio and printer work; Waiting appears immediately, without a cooldown. New Start remains gated until resource ownership is safely released.
- Waiting writes do nothing; turning Start Off is not an implicit Stop.
- Three endpoints work with the existing Matter fabric. Hydrated On values never start a show.
- Human confirms the Home controls, physical print, complete sequence and Stop behavior.

## Tasks

- [x] Verify consecutive clean logo/text jobs with the printer kept powered after the corruption report.

- [x] Implement automatic show and cancellation gates.
- [x] Add production-source sanitizer tests for full flow, repeats and cancellation races.
- [x] Add bounded startup cancellation and tests.
- [x] Implement coalesced Matter-thread synchronization and semantic labels.
- [x] Complete native build and application-only flash.
- [x] Confirm three Home controls and Start command on hardware.
- [x] Identify Waiting/Dead Stop controls and confirm Home Stop command; Start renamed in Home.
- [x] Confirm readable physical fortune from Home Start.
- [x] Confirm complete final flow and12-secondcooldown on hardware.
- [x] Verify SD-error cooldown behavior in production-source tests.
- [x] Confirm Dead Stop returns to Waiting from Home; readiness gating covered by host and local bench tests.
- [x] Restore pirate-skull header and confirm its physical print.
- [x] Reconfirm human acceptance after the reported printer regression.

## Work log

### 20261025-1707 — Implemented the simplified demo flow
- **Result:** Host flow/asset/fortune tests and startup cancellation sanitizer tests pass. Native build underway after increasing dynamic endpoint capacity to four (root plus three controls).
- **Design:** Keep endpoint1 as Start, repurpose endpoint2 as Dead Stop, append endpoint3 as Waiting. Publish Start through cooldown, Waiting otherwise; Dead Stop is a momentary action. State writes occur only on Matter's task with an internal-update guard and boot hydration gate. Manufacturer semantic labels added; Apple Home may retain existing tile names.
- **Flow:** Welcome → fortune preamble and one print → print completion → goodbye → cooldown → Waiting. Stop closes the gate immediately and returns the published mode to Waiting while existing audio/printer ownership safely drains.
- **Scope decision:** Cam explicitly paused further audio-card remediation for the demo. The one approved test clip remains; no folder rebuild, additional card writes, commits or pushes. Original audio read-integrity limitation remains documented in Story015.
- **Next:** Review synchronization, finish build, flash preserving NVS, then Home/print/Stop bench checks. Printer readiness requested from Cam.

### 20261025-1711 — Built, flashed and verified native boot
- **Result:** Native build, all host sanitizer suites and Death flow/asset/fortune suites pass. Read-only integration review found no blocking issue. Application-only flash at0x30000 verified SHA25606a810e50d4e2d2f2ae3e01ef93ca8144fcd0a4186939c21386b711e99ede8ae; native NVS preserved.
- **Boot:** Three endpoints created(Start1,DeadStop2,Waiting3); published Waiting(StartOff/WaitingOn/StopOff). JBL first-attempt reconnect, startup speech completed, no panic. Internalfree26.4KiB/min16.1KiB in boundedcapture.
- **Safety:** Dead Stop clears tone with saturating callback decrement, cancels immutable startup without replay, closes jaw, and signals existing worker/printer cancellation. Home updates are coalesced on the Matter task, not called inline during incoming attribute writes.
- **Next:** Requested Home endpoint/name visibility and a Start tap; printer power/paper readiness question pending. Full physical sequence and Stop acceptance still open. Updated active READMEs; no card writes or commits.

### 20261025-1723 — Home Start and readable text proven; restored pirate header
- **Hardware result:** Cam saw three generic outlets and tapped the first Off one. Board logged Matter Start accepted, automatic Welcome→fortune-preamble→one queued print→goodbye. Cam confirmed readable fortune and renamed that control Start. Waiting was the initially-On control; the remaining one is Dead Stop. Requested rename/Stop test; still pending.
- **Failure found:** Goodbye hit a knownSD CRC0x109 and cancelled before normalcooldown. Updated failurecleanup to retainStart and enterconfiguredcooldown; onlyexplicitDeadStop bypassescooldown/returnsWaiting. Stop/error races covered by production-source tests. This change doesnotclaimtheSDissuefixed.
- **Logo:** Cam requested the missing pirate-skull header. Verified original card/repo BMP384x353,1-bit,SHA69eb0f3c6f47d07288b116f5454d569240925d2503628c212390b015ccc51710. Embedded16944 packed top-downbytes in flash; no SDread/extraheap. Emits353 complete56-byte GSv0 rowpackets before text, each drained before reuse/cancellation. Adds about21s serial time at9600baud; flowwaitsforprintercompletion.
- **Validation:** Allhost sanitizer suites includingfullbinarysequence/mid-rowcancel/nextjobframingpass. Independentreview comparedembeddedpixelsbyte-for-byte toBMP and foundnoblockingdefect. Nativebuild andflow/asset/fortune suitespass. NewimageSHA7fb9df6a86e649b9c29804921432fc082e349c795398f69e4b4be73cd0a6dc28; application-onlyflashunderway.
- **Next:** Bootnewimage, physicalpiratelogo+textcheck, fullflow/cooldown andHomeDeadStop acceptance. NoadditionalSDwrites, commitsorpushes.

### 20261025-1735 — Corrected printer transport; pirate header physically confirmed
- **Failure:** Firstlogoattempt with256-byteUARTsoftwareTXring failed about800ms afterqueueing andlatchedtransportnotready; Cam sawnooutput. Exactfailurecodewasnotcapturedinthatimage, so specificrootcauseremainsunproven.
- **Decision:** InstalledSDK documents TXring0 as synchronousFIFO submission, while TXring>0queuesforlaterISRtransfer. The caller alreadywaitsforwirecompletionaftereach<=64-bytepacket, soremoved redundantsoftwareTXring whilekeepingbaud9600andoriginalboundeddrainmargin. Addedfailurelogs(bytesaccepted/drainerror/elapsed/row), andanidle-only `print-test` consolecommand toseparateprinteroutputfromSDaudio. Allhostsanitizerspass; source/decision inresearch/2026-10-25-printer-logo.md.
- **Hardware:** Cam powercycledprinter. App-onlyflash SHA256dcd95e25495b048f6912969224b3e6c511eab2a119d850596c8f8eb422126beb verified. `print-test` completedonejob,ready1/sent1/failed0; Cam confirmed “Printed perfectly!” forlogoandfortune. NoSDI/Oforlogo.
- **Historicalcorrection:** Cam clarifiedoldcodeprintedlogo/textbutoccasionallyglitchedorproducedsixfeetofgibberish; donotcalltheoldprinterpathflawless. Currentembeddedbitmaphasfixedverifieddimensions andcompleteboundedrowpackets.
- **Next:** Finalfullflowwithconcurrentaudio+printing, cooldown, HomeDeadStop andfinalWaiting. NoSDwritesorcommits.

### 20261025-1737 — Complete audio/print/cooldown cycle passed
- **Result:** Finalimage ran Welcome(123632ms)→preamble(136292ms)→onequeuedfortune(136692ms)→printwait(153382ms)→goodbye(158092ms)→cooldown(179732ms)→Waiting(191742ms). Cooldown12.010s. MatterreportedWaiting/StartOff/StopOff aftercompletion. NoSDCRC orprintertransporterror duringthisrun.
- **Printer:** Directtest plusfullflow completed2jobs,0failed,0cancelled; readyremainedtrue. Fulljobfinishedbeforegoodbye, provingtheflowwaitedforimage/textwork. Humanpiratelogo/fortunereadabilityalreadyconfirmed.
- **State:** Waitingwithspeakerconnected. RequestedfinalHomeStart→DeadStop check; nofurtherfirmwarechangesplannedunlessitfails. StoryInReviewpendingthatcheck/humanacceptance; noadditionalcardwritesorcommits.

### 20261025-2100 — Human confirmed Home Dead Stop; demo story accepted
- **Result:** Cam confirmed the final Home Start→Dead Stop check: “Yes, stopped and Waiting is On.” Together with the verified complete flow/cooldown and “Printed perfectly!” logo/text acceptance, this completes Story016.
- **Evidence boundary:** Final Home Stop confirmation is human-observed; the earlier bounded serial capture had already ended, so no new callback timestamp is claimed. Local captured Stop returned the published mode to Waiting in20ms and relinquished audio ownership in80ms.
- **Handoff:** Final image remainsdcd95e25495b048f6912969224b3e6c511eab2a119d850596c8f8eb422126beb. Death is left in Waiting per human confirmation. Home automation for the separate Box of Death should turn Start On. Start stays locked throughout flow/cooldown; Waiting has no trigger; Dead Stop cancels back to Waiting.
- **Scope:** Audio/SD integrity investigation remains deferred under Story015, which is not marked complete. No further firmware/card changes, commits or pushes.

### 20261025-2115 — Added three extra tear-off clearance lines; flash pending USB
- **Request:** Cam reported the Death signature against the tear line and requested three extra lines after the fortune.
- **Change:** Formatter now emits five blank lines after the signature instead of two, exactly three additional feeds. Updated existing formatter/binary-output expectations; all host sanitizer suites and native build pass. Built image SHA256d1e351d9b162d8f333a720fbbfdc01c098de01987bb375b4bf1a66eeb588ce78.
- **Hardware:** Upload failed before opening the serial port: /dev/cu.usbserial-10 no longer exists. No flash bytes were written; verified demo image dcd95e25… remains installed. Requested USB reconnection, with option to defer flashing until after demo.
- **Next:** Flash only the application once connected, then confirm normal Waiting boot. Physical tear clearance will be visible on the next print.

### 20261025-2119 — Flashed the extra tear-clearance lines
- **Result:** Cam reconnected USB. Application-onlyflash verified SHA256d1e351d9b162d8f333a720fbbfdc01c098de01987bb375b4bf1a66eeb588ce78, preservingnativeNVS/Homecontrolidentities.
- **Boot:** Waitingpublished, savedJBLreconnectedonfirstattempt, startupannouncementcompleted. ThreeextraLFareverifiedbyexistingformatter/binarytransporttests; noadditionalprintwastriggered. Physicalclearancewillbecheckedonthenextnormalfortune.
- **Handoff:** LeftWaitingwithspeakerconnected. Nootherbehavior/cardchanges, commitsorpushes.

### 20261025-2135 — Reopened printer acceptance after consecutive corruption
- **Report:** After spacing update, Cam reported half a logo followed by garbage, then an entirely garbled next print. SuppliedIMG_1345.HEIC shows densehorizontalrasterstreaks withgarbledcharacters. Preservedconvertedphoto inignoredvalidationfolder. Firmwarestatusreportedready1/sent2/failed0: hostUARTdrain didnotdetectreceiverfailure. Earlieronegoodprintwasnotproof ofrepeatedreliability; printeracceptanceisreopened.
- **Power/wiring:** Camconfirmed5V/5Asharedsupplyandthreelargecapacitors. Reviewedhardware/perfboard/constructiondocs: separatebranches,commonground,printer1000–2200µF/servo470–1000µFdecoupling; DTRexplicitlyunconnected. Do notassumeundersizedadapter; actualtransient/heatfaultnotestablished.
- **Controlledexperiment:** Added50msprocessinggapaftereachfullydrained56-byterow, preservingalllogobytes,baud9600,TX0anddrainmargin. Hosttestsverifyall353gapsandcancellationduringgapbeforefurtherbytes. Extendedprintwaitwatchdog30→60s forslowerheader/shortaudio; addedidle-onlyprinter-health(DLEEOT3)debugquery. Nativebuild/allhost/flowtestsPASS. App-onlyimage8cd68513b0009f41c4ba19a799382600521921080e3a9cff6493b00b2efdf5d2 flashedandverified.
- **Baseline:** Campowercycledprinterafterflash; statusqueryreturnedonebyte0x12, noindicatedvoltage/heaterrorattheprobeinstant. Beganfirstpacedprinter-onlyfortune. Needtwohuman-cleanconsecutivejobswithoutreset, thenconcurrentaudio/printing. Pacingcouldhelpreceiverprocessingorload; neitherrootcauseisproven. Sources/decisioninresearch/2026-10-25-printer-receiver-pacing.md.

### 20261025-2148 — Paced image clean; corrected explicit font and physical feeds
- **Observation:** Firstpacedjob completedhostsent1/failed0; printererrorstatus0x12before/after. Camreportedcleanimage/textbutnousefulextralinesandsmall/compressedsignatureagainstbody. IMG_1347.HEIC confirmscleanpirateimage/readablebodyandpoorfooterlayout; savedconvertedphotoignored. No secondprintsentintothatlayoutstate.
- **Decision:** VendorcommandssupportexplicitfontA12x24,normalstyle/size,30dotspacing,leftalignment. ReassertedtheseafterrasterwithoutanotherESC@. Formatterblankcountsremainunchanged; emptylinesnowemitphysicalESCJ30with100msprocessingwaitsplitintwo50mscancelchecks. Thuspresignatureblank30dots andfivefooterblanks150dots(18.75mm)afteritsnormalLF. Thisdoesnotprovewhatcausedtheobservedfontdifference; itremovesimplicitlayoutstatefromthetest.
- **Validation:** Allhostsanitizersandnativebuildpass, includingexactmode/feedbytesandcancellationduringphysicalfeedwait. Logo50msrowgapunchanged. CandidateimageSHAbbd62490f07e3a5652e3460713332ed47e7e469496bdbf7b6e48a7eeb1841f27; app-onlyflashunderway.
- **Next:** Twohuman-cleanconsecutiveprintswithoutprinterreset, checkingfontsizeandsignaturevisibilitybeforepullingpaper. Printer/flowacceptanceremainsInReview.

### 20261025-2154 — Layout accepted; uncovered missing-template fallback
- **Human result:** Cam confirmed cleanimage/textand“perfectamountofspacebeneathit” ontheexplicitfont/feedcandidate. Signaturevisibility/clearanceisaccepted. Thisprintcontained“The spirits are silent...” ratherthanaregularfortune; donotcallthatintentionaltesttext.
- **Diagnosis:** BootlogexplicitlyreportsFortunetemplatesunavailable. VerifiedcardbackupJSONisvalid727bytes/version1,one templateandten3-choicewordlists,SHA50d46c691e8dbdf6808a2321467f4e62861167a5f1748fec11c73c5cc2d24661. BothSDloadattemptsfailedthisboot; thisdoesnotestablishtheexactread/parsefailure.
- **Next:** PreserveSDfirstloadingandaddthisexactverifiedcardtemplateasthefirmwarefallback, ratherthanthestaticSilentmessage. Generator/testsbeingupdatedindependentlyofprintertransport. Requirearealfortuneandrepeatcleanprintwithoutprinterpowercyclebeforefinalacceptance. NoSDwrites.

### 20261025-2158 — Built verified embedded fortune fallback
- **Change:** ConfiguredSDpath anddefaultSDpath remainpreferred. Ifbothfail, sharedboundedstrictJSONparserloads theexact727-byte verifiedcardtemplateembeddedinflash, withsourceexplicitlylogged. No64KiBSDreadbufferneededforbuiltinparse. Generator/flow/printertimingunchanged.
- **Tests:** Deathflow/asset/fortune suitesandallgeneralhostsanitizersPASS. Testscoverfallbackorder,100nonemptygeneratedbuiltinfortuneswithoutSilent/unresolvedtokens,andfailedloadclearingbehavior. NativebuildPASS; candidatec9f3c3861fa65c841d50e94f1e250aa3573bf12fcf21e85b570ca925ea14d24c application-onlyflashunderway.
- **Next:** Bootsourcecheck, reprintfullrealfortuneagainstapprovedlayoutwhileprinterstayspowered, thenconfirmrepeat/concurrentoperation. NoSDwrites.

### 20261025-2203 — Repeat real-fortune print accepted; left Waiting
- **Result:** Finalc9f3c386…image boot explicitlyloaded `embedded verified card template`, exercisingthefallbackonhardware. Repeatprinter-onlyjobcompletedready1/sent1/failed0. Camconfirmedaregularfortune,cleanlogo,normalsignatureandgoodtearspace:“Yes,everythinglooksright.” Theprecedinglayoutprintwasalsovisuallyaccepted;printerwaskeptpoweredacrossthefortune-onlyESP32update,withnoprinterresetrequestedbetweenacceptedjobs.
- **Health:** FinalidleDLEEOT3response0x12; noindicatedvoltage/heatflagsatthesampledinstant. TheseidleprobesdonotruleoutunobservedtransientsorproveanRX-overflowrootcause. Slowerpacing/explicitmode/feedarethetestedmitigation; no claimofunlimited-soakreliability.
- **Handoff:** Story016acceptedagainwithfinalimageSHA256c9f3c3861fa65c841d50e94f1e250aa3573bf12fcf21e85b570ca925ea14d24c; nativebuild/generalhost/Deathsuitespass. LeftWaiting,speakerconnected,printeridle. Runtimeinternalfreeabout25.9KiB; transientbootminimum3.7KiBloggedforfuture resourcequalification, noresetobserved. SD/audiointegrityinvestigationremainsdeferredinStory015. NoSDwrites,commitsorpushes.
