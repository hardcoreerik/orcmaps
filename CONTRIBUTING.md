# Contributing to OrcMaps

Thanks for considering a contribution. This document covers two things:
practical mechanics, and — because they matter more here than in most
projects — how contributions interact with OrcMaps' licensing model and
provenance rules.

## Practical mechanics

- Open an issue or discussion before a large change; small fixes can go
  straight to a PR.
- Run the relevant test suites before submitting (see `STATUS.md`'s "Test
  status" for exact commands): host C++ tests, the consumer smoke test,
  `tools/check_documentation_truth.py`, and `tools/check_data_provenance.py`
  if you touched anything under `data/sources/`.
- Keep PRs scoped. A bug fix doesn't need an accompanying refactor.
- If your change affects a durable architectural decision, update
  `PROJECT_TRUTH.md` as part of the same PR — see its own "Document
  authority" section.

## Licensing and copyright — read this before submitting code

OrcMaps currently intends to preserve a dual-licensing model:
**AGPL-3.0-only**, with commercial licensing available separately by
agreement (see `LICENSING.md`). This is not a decision to monetize the
project today — it's a decision to keep that door open rather than close
it accidentally through how contributions are accepted. This document
exists so that stays true.

**By submitting a contribution (a pull request, patch, or similar) to
this repository, you certify:**

1. You wrote the contribution yourself, or you have the right to submit it
   under the project's license (this is the standard
   [Developer Certificate of Origin](https://developercertificate.org/)
   model — the same one the Linux kernel, Docker, and many other projects
   use; we're not inventing new terms here).
2. You understand the contribution will be distributed under
   AGPL-3.0-only, **and** you grant the OrcMaps maintainers permission to
   also distribute it under the project's separate commercial license
   terms, consistent with the dual-license model described in
   `LICENSING.md`. You retain your own copyright — this is a license
   grant for distribution, not a transfer of ownership.
3. If you want your contribution to be AGPL-only and **not** available for
   commercial relicensing, say so explicitly in the PR before it's
   merged. We will respect that, but we need to know before merging, not
   discover it later — silence is treated as agreement to item 2, not as
   an opt-out.

We are not adding a signed CLA tool or bot for this today — that's more
process than the current contribution volume justifies, and we'd rather
not introduce friction prematurely. **If OrcMaps ever accepts contributions
at a scale where this lightweight, in-PR approach becomes unclear or
disputed, a formal CLA (and a review by an actual attorney, not an AI
agent) is the right next step — this paragraph is a flag for that future
decision, not a substitute for it.**

## Provenance — the same rule applies to contributions as to our own work

Every external dependency, dataset, asset, font, icon, schema, or other
third-party material needs known provenance and known usage rights before
it's incorporated — see `PROJECT_TRUTH.md`'s IP/provenance-safety
principle and `docs/DATA_AND_LICENSING.md` in full. This applies to
contributions exactly as it applies to our own commits:

- If your PR adds a code dependency, update `DEPENDENCY_LEDGER.md` in the
  same PR.
- If your PR adds or changes a map data source, add/update a record under
  `data/sources/` and make sure `tools/check_data_provenance.py` passes.
- If your PR includes a font, icon, sample fixture, or other non-map
  asset you didn't create yourself, say where it came from and under what
  license, in the PR description at minimum.
- "I found this on GitHub and it solved the problem" is not sufficient
  provenance for inclusion, regardless of whether a human or an AI agent
  found it — see `docs/DATA_AND_LICENSING.md` "Contribution / AI-assisted
  development safeguard".
- If you're not sure about a piece of source material's license, say so.
  `REVIEW_REQUIRED`/unknown is a normal, acceptable state to submit a PR
  in — it just means that piece can't be merged as an *official*, approved
  part of the project until someone resolves it. Guessing and writing down
  the guess as if it were confirmed is the one thing we don't want.

## Code style

Follow the conventions already in the codebase (see `PROJECT_TRUTH.md`
"Naming Conventions") rather than introducing a new style in your own
files. Minimal comments — only where the *why* isn't obvious from the
code itself.
