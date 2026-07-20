# Death the Fortune-Telling Skeleton — Project Context

This document is the persistent source of truth for the project's overall purpose, visitor experience, hardware architecture, physical build decisions, and current unresolved questions. Update it when major design decisions change.

## Core concept

Death is a Halloween/carnival animatronic skeleton that speaks to visitors, asks them to place a finger in his mouth, gives a spoken fortune, and prints a physical fortune slip.

The project began as an adaptation of the TwoSkulls codebase but is now a single-skull prop with its own controller architecture, content, printer, lighting, sensing, and Matter integration.

## Original visitor experience

The original flow used occupancy detection:

1. A visitor entered the detection zone.
2. Death began speaking automatically.
3. Death instructed the visitor to place a finger in his mouth.
4. A capacitive sensor detected the finger.
5. Death snapped his jaw after a short randomized delay.
6. Death spoke and printed the fortune.
7. The system entered cooldown.

Once a skit began, it generally ran to completion rather than reacting to new occupancy or mode signals mid-skit.

## Why the original trigger failed in practice

In a calm, focused environment, the motion-triggered sequence could work. During actual trick-or-treating, it did not.

At a casual glance, Death looked like a static talking prop. Kids were moving around, talking, and focused on candy. When Death began speaking merely because someone entered the area, they often did not realize he was responding to them, did not listen closely, and missed the instruction to place a finger in his mouth.

The fundamental interaction problem was therefore not the skit itself. It was the lack of an obvious, intentional starting action.

## 2026 interaction redesign: illuminated button box

The new box replaces motion detection as the primary trigger.

The intended experience is:

1. The box attracts attention with a large illuminated button and marquee lighting.
2. A child deliberately presses the button.
3. Death immediately springs to life in direct response to that action.
4. Because the child caused the response, they are now focused on Death and are much more likely to listen to the instructions.
5. The remainder of the existing finger-in-mouth and fortune-printing sequence proceeds normally.

This changes the opening interaction from passive detection to explicit cause and effect.

## Occupancy sensing going forward

Occupancy sensing is no longer planned as the normal skit trigger.

A possible secondary use remains open: if someone approaches but does not press the button, Death might occasionally invite them to press it. This would be an attract-mode feature only, not the main start mechanism.

It is also acceptable to remove occupancy sensing entirely if it adds complexity without improving the experience.

## Button box role

The box is intentionally a mostly self-contained trigger console rather than a second major prop controller.

Its likely responsibilities are:

- Read the illuminated pushbutton.
- Control the button lamp.
- Control the decorative marquee lights.
- Optionally play a short local light animation when pressed.
- Send a trigger signal to Death.

It does **not** generate fortunes, animate Death, control the jaw, play Death's audio, or print fortunes.

All of Death's main electronics remain inside the skull.

## Trigger communication options

The exact link between the box and Death is not yet finalized.

### Option A: direct wired trigger

Run a physical cable from the box to the main controller inside Death and treat the button as a direct input.

Advantages:

- Very simple.
- Deterministic.
- Minimal protocol or pairing concerns.
- Likely the most reliable possible arrangement.

Disadvantages:

- Requires a physical signal cable between the box and Death.
- The box may still need its own microcontroller for lighting effects.
- Less flexible for future placement or reuse.

### Option B: Matter-based trigger

Give the box its own ESP32 and make it a Matter controller/device that triggers Death through the existing Matter architecture.

Advantages:

- Mostly a software solution once the box already has an ESP32.
- Avoids adding a dedicated signal wire between the box and Death.
- Fits the existing Death Matter Controller direction.
- Allows future automation or reuse.

Disadvantages:

- Depends on wireless/network/Matter reliability.
- May introduce latency, reconnect behaviour, or pairing complexity.

### Current plan

Start by trying the Matter approach because it may require no additional hardware connection beyond the box's own controller. If it proves immediate and completely reliable in real operation, keep it. If not, use a direct wired trigger.

## Physical box design

The chosen visual direction is an antique carnival control box with:

- Wooden construction.
- A sloped front control deck.
- A large glowing red button mounted low on the deck.
- Large `PRESS` lettering above the button.
- Smaller `to hear your fortune` lettering below.
- Decorative marquee lights around the structure.
- A raised backboard.
- Death's skull logo integrated into the top as a dimensional cast-copper-looking crest or boss.
- Aged paint, rubbed finishes, copper details, and an old carnival-machine appearance.

Approximate concept dimensions from the visual design were:

- Width: about 370 mm / 14.6 in.
- Overall height: about 280 mm / 11 in.
- Base depth: about 200 mm / 7.9 in.
- Upper sign area: about 260 × 125 mm.
- Sloped control panel height: about 135 mm.

These were visual estimates, not final shop drawings.

A full-size mockup has now been built from the concept images and approximate dimensions. It will be adjusted as needed and then used as the pattern for the final wooden build.

## Likely box materials

Available or discussed materials include:

- Plywood or other sheet wood.
- Layered or laminated wood for shaped profiles.
- Thin copper sheet.
- Copper, gold, and silver leaf.
- Copper-wire fairy lights.
- Glass marbles used as vintage bulb lenses.
- Wooden, tubing, or similar collars as light standoffs.
- Decorative copper-looking edge rivets.
- Distressed paint and artificial aging techniques.

The box should be buildable with a jigsaw, ordinary hand tools, and basic power tools rather than requiring a full wood shop.

## Existing printer arrangement

The printer arrangement from the previous season worked well and will remain unchanged.

The thermal printer is embedded inside a manual typewriter placed in front of Death. It has a direct connection to the ESP32 inside Death. Death's controller generates the fortune and sends it to the printer.

The new illuminated-button box does not contain or control the printer.

## Main electronics architecture

The main controller remains inside Death's skull and handles:

- Audio playback from SD card.
- Bluetooth A2DP output.
- Jaw servo animation.
- Eye and mouth lighting.
- Capacitive finger sensing.
- Skit state machine.
- Fortune generation.
- Thermal-printer output.
- Configuration and logging.
- Communication with the Matter controller.

The jaw is designed to fail safely:

- The servo actively opens the jaw.
- Elastic tension closes it.
- The servo is not rigidly linked in a way that could clamp strongly.
- Closing force should remain very light around a child's finger.

## Operating modes

The planned operating modes are:

- Little Kid.
- Big Kid.
- We're Out — Take One.
- Closed for the Night.

Little Kid should remain the safe fallback if mode state is uncertain.

## Fortune and audio system

Fortunes are generated from templates and printed as physical slips. The project uses SD-card audio and content, Bluetooth audio output, synchronized jaw animation, and JSON-based fortune material.

The exact script and intensity can vary by operating mode.

## Current decisions

The following should be treated as settled unless deliberately revisited:

- The illuminated button becomes the primary skit trigger.
- Motion sensing is not the normal start trigger.
- The box is a mostly dumb, self-contained trigger and lighting device.
- Death's main electronics remain in the skull.
- The working typewriter/printer arrangement remains unchanged.
- The full-size mockup will guide the final enclosure build.
- Matter will be tested first as the trigger link.
- A direct wired trigger remains the reliability fallback.

## Remaining open questions

1. Whether the final trigger link is Matter or a direct wire.
2. What ESP32 or other controller is used inside the box.
3. Exact button voltage and lamp-driving circuit.
4. Exact marquee-light hardware and animation pattern.
5. Whether the box needs local configuration or only fixed firmware.
6. Whether an occupancy sensor is retained solely for an occasional invitation to press the button.
7. Final physical dimensions after mockup refinement.
8. Final internal mounting, power supply, service access, and cable routing.
9. The precise command and acknowledgement behaviour between the box and Death, including debouncing, duplicate suppression, busy state, and cooldown handling.
