# Lite Link 0.0.13 — Release Notes

Lite Link (LL) is an experimental micro-networking suite for DOS, targeted at 8088-class computers with 256 KB RAM or more. It uses direct register-level network hardware access and a lightweight TCP/UDP implementation, without requiring a resident packet driver.

## Included tools

- **LLShell** — DOS dual-pane file manager and file transfer application.
- **LLShell32** — Windows graphical companion and network listener.
- **LLGET** — HTTP file downloader.
- **LLTIME** — SNTP time utility.
- **LLNIC** — Network adapter detection and initialization.
- **NETDIAG** — Network diagnostic utility.
- **AUMON** — DOS memory-snapshot helper.
- **LLSTART** — LLShell command supervisor.
- **LLDEBUG** — Diagnostic report collection.

## Documentation

The source repository includes detailed configuration, installation, command-line, troubleshooting, build and protocol documentation. Read `DOCS/CONFIG.TXT` for the `LL.INI` reference and `DOCS/COMMANDS.TXT` for command syntax.

## AI-assisted development

Lite Link is an experimental, predominantly AI-coded project developed with **OpenAI Codex**. Architecture, design goals, requirements, and development direction are human-led; much of the implementation, debugging, and code generation is AI-assisted. Testing includes original IBM XT-class 8088 hardware and emulated DOS/Windows environments.

AI-generated code may contain defects. Testing, reports and community contributions are welcome.

## Notes and limitations

Lite Link 0.0.13 is intended for trusted local networks. It does not authenticate peers or encrypt traffic, and it has no native HTTPS/TLS or DOS DHCP client. Do not expose remote command execution or file shares to untrusted networks.

Component version banners differ: LLShell32 is 0.0.13, DOS LLShell is 0.0.12, and the networking utilities are 0.0.11r1.

**Release status:** These notes are prepared for the upcoming GitHub Release. Release ZIPs and checksums must be regenerated and verified against the final source before publication.
