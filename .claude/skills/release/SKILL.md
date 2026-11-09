---
name: release
description: Publish a TaskManager version as a GitHub release (kiadás). Use only when the user asks to release or publish a version that is already committed and pushed.
---

# GitHub release

`gh` CLI is installed and logged in as the repo owner. The in-app update check reads `releases/latest`, so an unreleased version is invisible to users. Publish only when the user asks, after the version commit is pushed.

1. Get the full commit SHA: `git rev-parse <commit>`. `--target` needs all 40 characters; a short hash fails with `HTTP 422 ... target_commitish is invalid`.
2. Check that `TaskManager.exe` was built from that commit: `git diff --quiet HEAD -- TaskManager.exe`. Always attach that build. Users download it via `https://github.com/cncDAni2/TaskManager/releases/latest/download/TaskManager.exe`.
3. Keep the asset name `TaskManager.exe` and a correct version resource, or old clients fall back to the browser (see `src/SelfUpdate.h`).
4. Write the release notes in Hungarian: a few one-line bullets in plain words, no explanations (for example "- Fix: A frissítés jobban látható és újrapróbálható"). Keep the notes file in the scratchpad, not in the repo.
5. Show the draft to the user and wait for approval before publishing.
6. Publish:

```
gh release create v<X.Y.Z> TaskManager.exe --repo cncDAni2/TaskManager --target <FULL commit SHA> --title "TaskManager v<X.Y.Z>" --notes-file <notes.md>
```
