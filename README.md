# LiteLink

File transfer and remote DOS commands for DOS and Windows. LiteLink combines
an 80-column DOS file manager with a Windows two-pane manager, plus utilities
for time synchronization and HTTP downloads.

- Tag multiple files and folders; copy in either direction.
- Browse, create and rename directories; copy folder contents recursively.
- Preserve existing files and verify transferred data before committing it.
- Run DOS commands remotely: the DOS manager exits while the command runs,
  then restarts and reconnects automatically.
- Share one local `LL.INI` among the tools on each computer.

## Downloads

Use the files attached to a release:

| Package | Contents |
| --- | --- |
| `LITELINK-0.0.13-BINARY.ZIP` | Ready-to-run DOS/Windows programs, examples and user guides |
| `LITELINK-0.0.13-SOURCE.ZIP` | Source, configuration examples, guides and the Watcom build |

Release 0.0.13 contains LLShell32 0.0.13, DOS LLShell 0.0.12 and utilities
0.0.11r1. The Windows update adds an embedded icon and fixes path-label repainting.

## Requirements

**DOS:** DOS 3.3+, an 8086-compatible processor, and a supported NE2000-compatible
adapter, PicoMEM NE2000 emulation or 3C509B. The apps use native polling without
a resident packet driver. Stop other software using the same card.

**Windows:** the same `LLSH32.EXE` runs on Windows 11 x64 and Windows 3.1 with
Win32s 1.1 and an existing Winsock provider such as Trumpet. Start the provider
before LiteLink on Windows 3.1. Operating systems and network providers are not
included. The native Windows host does not require an emulator.

## Quick start

1. Copy `WINDOWS/` from the binary package into `C:\LITELINK` on Windows and
   create `C:\LITELINK\FILES`. Edit `[HOST]` in `LL.INI` beside the EXE, then
   start `LLSH32.EXE`. The sample listens on TCP port 21210.
2. Copy `DOS/` into `C:\LITELINK` on DOS and create its `FILES` folder. Copy a
   matching `CONFIG/` example as `LL.INI`. Set the actual NIC base, DOS network
   addresses and the Windows listener's IP under `[SHELL] HOST`.
3. From the DOS installation directory, run `LLSTART.BAT`.

The sample addresses are placeholders for a private LAN: Windows
`192.168.0.10`, DOS `192.168.0.20`, gateway/DNS `192.168.0.1`. Replace them with
settings appropriate to your network. Each endpoint keeps its own `LL.INI`.

Tab changes pane on DOS; Space/Insert tags entries; F5 copies to the opposite
pane. On Windows use Ctrl-click/Ctrl+A and F5. Enter/double-click opens folders;
Backspace goes up. Existing files are preserved; folder contents merge.

Remote commands require `[EXECUTION] ALLOWRUN=ON` and the DOS batch supervisor.
For example, run `DIR > LISTING.TXT`, wait for reconnection, then copy the result
back. Windows applications launched through DOS must exit Windows normally
before DOS supervision can resume.

## Documentation

- [Plain-text quick start](README.TXT)
- [Installation and upgrades](DOCS/INSTALL.TXT)
- [Configuration and NICs](DOCS/CONFIG.TXT)
- [File manager, utilities and commands](DOCS/USAGE.TXT)
- [Troubleshooting](DOCS/TROUBLE.TXT)
- [Limits and recovery behavior](DOCS/LIMITS.TXT)

The source release also includes `DOCS/BUILD.TXT`, `DOCS/DEVELOP.md` and
`DOCS/PROTOCOL.md` for compilation and maintenance.

## Building

Use Open Watcom with both DOS and Win32 targets. From the source root, set
`WATCOM=C:\WATCOM` and run `BUILD nt64` in Windows x64 Command Prompt. For a
DOS-hosted build, add `%WATCOM%\BINW` to PATH and run `BUILD` from DOS on a
suitable 386+ build machine. Programs appear in `OUT/`.

The build uses WMAKE, WCC/WCC386, WASM, WRC and WLINK. No Python, PowerShell,
source generator or image converter is required. The checked-in icon is
embedded in the Windows EXE at build time.

Use a trusted LAN: LiteLink does not provide authentication or encryption.
LLGet is HTTP-only. See the guides for DOS filename/path limits and queue recovery.
