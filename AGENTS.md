# AI Agent Guidelines

## This Project is AI-First
I may do some hand-coding now and then, but 99% of the code will be written by AI. Do not expect the user to do anything in the code themselves.

## Do Not Check In Without an Explicit Request To Do So

## Story-Driven Development
/docs/stories/stories.md is the master list of stories. When done a story and it's verified by a human, mark it as complete.

Each story has an explicit story document, e.g.: docs/stories/story-003b-built-in-sd-card-migration.md

The story document is to be used constantly as a work log for the story, including everything you try, the result, and the next steps. See the build-story.md command for more details.

## Debugging native firmware

Read the root [README debugging guide](README.md#debugging-the-native-firmware)
before investigating hardware/runtime failures. Capture `status`, `fault-log`
and available SD `diagnostics/fault-*.log` evidence before rebooting or changing
firmware/card contents. Use the active `firmware/` implementation and its native
README; historical Arduino instructions refer to a different runtime. Keep raw
captures private, preserve unrelated work, and record experiments in the relevant
story. Follow the guide’s evidence-first, one-change-at-a-time process.
