# GEMINI.md

This repository uses the shared Tacitsoft agent bootstrap for operating context.
Read `./tsctl-agents-bootstrap.md` for dispatch rules, validation requirements, and shared
control-plane conventions before beginning any implementation work.

**Read [`AGENTS.md`](AGENTS.md) first** — it is the authoritative repo-local instruction
file (build system, code style, commit format, architecture, security checklist) and applies
to every agent (Claude, Codex, Gemini, Copilot). File all non-trivial work as an SDD v1.1
spec issue (`.github/ISSUE_TEMPLATE/*-spec.yml`) before implementing.



<!-- tsctl-agent-bootstrap-reference:start -->

## Shared tsctl Agent Bootstrap

Also read `./tsctl-agents-bootstrap.md` for Tacitsoft agent dispatch rules,
`tsctl` interaction patterns, `repos.yaml` contract expectations, issue
taxonomy, review/merge workflow, runtime selection behavior, validation flow,
post-agent PR/branch hygiene, and shared control-plane operating rules.

Treat it as additive control-plane context. Keep this repo's local product,
domain, architecture, and coding instructions authoritative for repo-specific
behavior.

After agent runs complete, inventory PR and branch sprawl before creating any
new branch or PR:

```bash
gh pr list && git fetch && git branch && git branch -r
```

If a PR already exists for the issue work, review, resolve, and merge that
existing PR. Create a new branch or PR only when no PR exists for completed
work, or work never reached a PR and needs branch recovery.

<!-- tsctl-agent-bootstrap-reference:end -->
