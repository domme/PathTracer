---
name: code-review
description: >
  Performs a code review of current changes (staged, unstaged, or a branch diff).
  Invoke this skill whenever the user asks for a code review, asks to "review my changes",
  or uses similar phrasing. The review is always delegated to a fresh sub-agent that has
  no knowledge of the current conversation — ensuring objective, unbiased feedback.
---

## What this skill does

When the user requests a code review, **do not perform the review inline**.
Instead, delegate to a `code-review` sub-agent using the `task` tool. This gives fresh eyes:
the sub-agent has no conversation history and no prior context, so it cannot be influenced
by earlier discussion or decisions in the current session.

## How to invoke the sub-agent

Use the `task` tool with these parameters:

| Parameter    | Value             |
|--------------|-------------------|
| `agent_type` | `"code-review"`   |
| `model`      | `"claude-sonnet-4"` |
| `mode`       | `"background"`    |

## What to include in the sub-agent prompt

Before invoking the sub-agent, read `.copilot/copilot-instructions.md` so you can
pass the project's full code style guide to the reviewer as context.
The sub-agent won't have it otherwise.

Construct the prompt as follows:

```
You are reviewing code changes in the PathTracer / FANCY engine repository.

## Project code style guide

<paste full contents of .copilot/copilot-instructions.md here>

## Review scope

<describe what is being reviewed: staged changes, unstaged changes, specific files, or diff against a branch>

## Instructions

Review the changes described above. Focus exclusively on issues that genuinely matter:

- Bugs or logic errors
- Security vulnerabilities
- Incorrect API usage (e.g. D3D12 barrier misuse, missing synchronization)
- Missing or incorrect error handling
- Correctness problems that will cause crashes, data corruption, or undefined behaviour

Do NOT comment on:
- Code style, formatting, or whitespace (clang-format handles this)
- Naming conventions (unless a name is actively misleading)
- Trivial observations with no functional impact

For each issue found, report:
- File and line number (where applicable)
- Severity: critical / major / minor
- A clear, concise explanation of the problem and why it matters
```

## Scope resolution

If the user doesn't specify what to review, default to **all staged and unstaged changes**
in both the PathTracer repo and the FANCY submodule (`git diff HEAD` in both).

If the user says "review the barrier changes" or similar, scope the review to the relevant
files from the current working-tree diff.
