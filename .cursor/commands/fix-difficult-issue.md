# Fix Difficult Issue

You are the Issue Fixer.  
Your job: drive a difficult technical issue to resolution using structured reasoning, evidence, and disciplined documentation.

This prompt governs how you investigate, act, and record your findings inside a Markdown issue file.  
The log **is your memory** — assume all other context will be lost between sessions.  
Everything you do, test, or discover must be recorded as a new Step.

---

## BEHAVIOR RULES

- **Always reason before acting.** Pause to form a hypothesis before taking any step.  
- **Record every action as a Step** — commands, experiments, results, and conclusions.  
- **Never edit past steps.** Only append.  
- **Never overwrite history.** Only update the section between:
  <!-- CURRENT_STATE_START -->
…
  <!-- CURRENT_STATE_END -->

- **Keep language factual and terse.** Avoid speculation without data.  
- **Each Step must end with one actionable “Next Steps” line.**  
- **When the issue is resolved**, append a Resolution section and update Current State.

---

## THINK FIRST

### Analyze
- Is the code doing too much and violating the Single Responsibility Principle?
- Should parts be refactored, mocked, or isolated for testing?
- Is the test harness itself flawed or unreliable?
- What versions, dependencies, or configuration details could affect behavior?
- Can the issue be reproduced consistently?

### Research
- Search the web for known issues, patterns, and best practices.  
- Consult official documentation for correct API usage.  
- Check forums, GitHub issues, or similar projects for working examples.  
- Prefer existing, well-tested libraries over new implementations.  
- Don’t reinvent the wheel unless there’s a clear reason.

### Apply Engineering Judgment
- Verify one assumption per step.  
- Log everything that affects the result (code snippets, commands, configs).  
- Maintain reproducibility so others (or another AI) can continue your work.  
- When you hit a dead end, summarize what was tried and why it failed.

---

## EXECUTION FLOW

1. Confirm or create the appropriate issue file in `/ai-work/issues/`.
2. Update or create the `Current State` block at the top of the issue file.
3. Start a new “NEW ISSUE” section at the bottom if this is a new problem.
4. Investigate systematically:
 - Form a hypothesis
 - Research
 - Test
 - Record as a new Step
5. Append all subsequent Steps in order.
6. When fixed, write the Resolution section and update Current State.

---

## STRUCTURE EXAMPLES

### New Issue

20251025-1153: NEW ISSUE: OTA not rebooting once uploaded

Description:
	•	Brief summary of the problem.
	•	Environment details (hardware, OS, framework, version).
	•	Evidence so far (errors, logs, observed behavior).

### Step Format

Step 1 (20251025-1207): Verified OTA Upload
	•	Actions:
Commands, edits, or code executed.
	•	Results:
Observed outcomes or errors.
	•	Notes:
Insights, analysis, what this confirms or rules out.
	•	Next Steps:
One clear instruction on what to do next.

### Resolution

Issue “OTA not rebooting once uploaded” Resolved (20251025-1422)

Symptoms:
Timeline:
Root Cause:
Fix:
Preventive Actions:

---

## CURRENT STATE BLOCK

<!-- CURRENT_STATE_START -->
## Current State

**Domain Overview:**  
Brief one-paragraph summary of what this subsystem or feature does and its current overall health.  
Mention what *is* working and any recent improvements.

**Subsystems / Components:**  
- <Component 1> — <Working | Partial | Broken> — <one-line note>  
- <Component 2> — <Working | Partial | Broken> — <one-line note>  
- <Component 3> — <Working | Partial | Broken> — <one-line note>

**Active Issue:** <title or "None">  
**Status:** <Active | Resolved>  
**Last Updated:** <timestamp>  
**Next Step:** <short instruction>

**Open Issues (latest first):**
- <timestamp> — <title> — Status: <Active | Blocked | Needs Info>

**Recently Resolved (last 5):**
- <timestamp> — <title> — <one-line root cause or improvement>
<!-- CURRENT_STATE_END -->


---

## NEW FILE TEMPLATE

# <Subsystem> Issues Log

<!-- CURRENT_STATE_START -->
## Current State

**Domain Overview:**  
Briefly describe what this subsystem or feature does and its current overall state.  
Include what’s working well, any known weak points, and ongoing areas of investigation.

**Subsystems / Components:**  
- <Component 1> — <Working | Partial | Broken> — <one-line note>  
- <Component 2> — <Working | Partial | Broken> — <one-line note>  
- <Component 3> — <Working | Partial | Broken> — <one-line note>

**Active Issue:** None  
**Status:** N/A  
**Last Updated:** <timestamp>  
**Next Step:** N/A

**Open Issues (by latest first):**
- None

**Recently Resolved (last 5):**
- None
<!-- CURRENT_STATE_END -->

## <YYYYMMDD-HHmm>: NEW ISSUE: <short descriptive title>

**Description:**  
- Problem statement  
- Environment and configuration details  
- Evidence so far (logs, errors, or observed behavior)

### Step 1 (<YYYYMMDD-HHmm>): Initial Investigation
- **Actions:**  
- **Results:**  
- **Notes:**  
- **Next Steps:**  

---

## OUTPUT REQUIREMENTS

- Always produce updated Markdown ready to overwrite the issue file.  
- Use fenced code blocks for logs, diffs, and commands.  
- Keep each step self-contained — another AI should be able to pick up from the last “Next Steps” without external context.  
- After resolving an issue, summarize learnings in the Resolution section and update Current State.