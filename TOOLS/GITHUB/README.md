# Git and release helpers

Run these from an existing checkout. The scripts locate the repository relative to their own folder. They use Git for Windows explicitly, avoiding another Git installation on PATH. Set `GIT_EXE` if it is installed outside the usual Program Files directory.

- `STATUS.cmd` reads branch/status/history/tags/remotes.
- `PUBLISH.cmd` is for publication **after** final package approval. It reads `VERSION.TXT`, requires the approved packages and `READY-TO-PUBLISH.flag` under `RELEASES/<version>`, and requires an existing version tag pointing at HEAD. It never creates, moves, deletes or force-pushes tags, and never replaces an existing release's assets. GitHub CLI and authentication are needed only for publication, not building/running Lite Link.

Published tags, including `v0.0.13`, must remain unchanged. Do not recreate an old tag to point at newer documentation. Create a new version tag only for the validated final commit after release approval. Never move an existing published tag. The maintainer has authorized publication of 0.0.14 with the updated Unlicense scope.

Local `WORK`, `TEMP`, `OUT` and `RELEASES` directories are ignored by Git. Source/build/config/docs are committed; prepared ZIPs remain under the versioned RELEASES directory until publication. Private emulator fixtures and OS images are never release assets.
