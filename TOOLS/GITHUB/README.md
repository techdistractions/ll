# GitHub publishing helpers for LiteLink

These scripts live **inside** the existing Git repository at `C:\Retro\LITELINK\TOOLS\GITHUB`. No new setup files need to remain loose in `C:\Retro`.

1. Run `CLEAN-ROOT.cmd` once to move the previous root-level setup scripts into `TOOLS\GITHUB\LEGACY` and move `C:\Retro\LITELINK-RELEASES` to `C:\Retro\LITELINK\RELEASES`. Nothing is deleted or overwritten. It updates `.gitignore` to exclude both locations.
2. Run `STATUS.cmd` to inspect your local Git changes.
3. Edit/review the project documentation before publishing. The current `v0.0.13` tag is local and may need to be recreated on the final docs commit. Do not publish it in its current state.
4. Repack **both** official source and binary ZIPs with the approved documentation and update SHA256SUMS.TXT. The source ZIP should match the tagged tree. After a check, create the `RELEASES\0.0.13\READY-TO-PUBLISH.flag` gate file and run `PUBLISH.cmd`.

The publish script refuses to proceed until the release gate file exists; this is intentional to prevent an accidental public release while editing docs.

GitHub CLI: https://cli.github.com/ — install, reopen Command Prompt, run `gh --version` then `gh auth login` before publishing.

Do not re-run `SETUP-LITELINK-GIT.cmd` in the legacy folder: the Git repo is already initialized. Only the old files are archived there as a fallback.
