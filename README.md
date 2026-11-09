# Lite Link (LL)

**Experimental micro-networking for DOS and XT-class computers.**

## What it is

Lite Link (LL) is an experimental micro-networking suite for DOS, designed for systems with 8088 CPUs and 256 KB RAM or more.

Rather than relying on a conventional resident network driver and full networking stack, LL uses register-level hardware access and a lightweight, minimal TCP/UDP implementation to communicate over a local network or the Internet.

Lite Link provides a common networking foundation for a growing suite of tools and utilities, designed to bring useful networking capabilities to early DOS computers without the overhead of traditional networking software.

## The Lite Link suite

### Applications

| Tool | Purpose |
| --- | --- |
| **LLShell (`LLSHELL.EXE`)** | DOS dual-pane file manager. Browse and transfer files between DOS and Windows, manage directories, copy folder trees and submit remote DOS commands. |
| **LLShell32 (`LLSH32.EXE`)** | Windows graphical file manager and network listener. Runs on Windows 3.1 with Win32s/Winsock and on modern Windows. |

### Network utilities

| Tool | Purpose |
| --- | --- |
| **LLGet (`LLGET.EXE`)** | Download files via HTTP with bounded redirects and optional expected CRC32 checking. HTTPS/TLS is not included. |
| **LLTime (`LLTIME.EXE`)** | Query an SNTP server, display a proposed date/time or set the DOS clock. |
| **LLNIC (`LLNIC.EXE`)** | Probe and initialize supported native network hardware and report MAC/configuration details. |
| **NETDIAG (`NETDIAG.EXE`)** | Inspect NIC hardware, run NE2000 loopback, check ARP, ICMP ping, TCP connections and basic HTTP. |

### Support tools

| Tool | Purpose |
| --- | --- |
| **AUMON (`AUMON.COM`)** | Transient DOS memory-snapshot helper for remote-command supervision and diagnostics. |
| **LLSTART (`LLSTART.BAT`)** | DOS supervisor for LLShell. Unloads the manager while remote commands execute, then restarts it. |
| **LLDEBUG (`LLDEBUG.BAT`)** | Collect network diagnostics and logs into a separate report directory. |

Lite Link is intended to grow beyond these tools. LLShell is **one application within the suite**, not the networking foundation itself.

## Supported systems

| DOS computer | Windows computer |
| --- | --- |
| DOS 3.3 or later | Windows 3.1 with Win32s 1.1 and a compatible Winsock provider, or modern Windows |
| 8086/8088-compatible processor (8088-class design target) | TCP/IP connection to the DOS computer |
| 256 KB RAM or more (project design target) | `LLSH32.EXE` Windows manager/listener |
| NE2000-compatible adapter, PicoMEM NE2000 emulation or 3Com 3C509B | Configured local IPv4 networking |

DOS tools use a direct polled NIC backend; they do **not** require a resident packet driver. Stop other software using the same NIC while running an LL tool. The DOS network configuration uses static IPv4 rather than DHCP.

## Downloads

| Package | Contents |
| --- | --- |
| `LITELINK-0.0.13-BINARY.ZIP` | Ready-to-run DOS and Windows programs, examples and user guides |
| `LITELINK-0.0.13-SOURCE.ZIP` | Source, examples, documentation and Open Watcom build files |

Release 0.0.13 includes LLShell32 **0.0.13**, DOS LLShell **0.0.12** and the networking utilities **0.0.11r1**. The component version banners are intentionally different.

## Getting started

1. **Windows:** Extract the `WINDOWS` contents from the binary release into a directory such as `C:\LITELINK`. Create `C:\LITELINK\FILES`. Edit `LL.INI` beside `LLSH32.EXE`: set `[HOST] ROOT`, `LOG`, `BIND` and `PORT`. Start the Winsock provider first if using Windows 3.1, then run `LLSH32.EXE`.
2. **DOS:** Extract the `DOS` contents to `C:\LITELINK` and create a `FILES` folder. Choose the matching sample from `CONFIG` (`NE2000.INI`, `PICOMEM.INI` or `3C509.INI`) and save it as `LL.INI` beside the executables. Set `[NETWORK]` for your actual adapter and LAN, and `[SHELL] HOST` to the Windows computer's **real** LAN IPv4 address.
3. **Test and connect:** With LLShell not running, use `LLNIC`, `NETDIAG ARP`, `NETDIAG PING` and `NETDIAG TCP` as needed. Then start the DOS manager with `LLSTART.BAT` from its installation directory.

```dos
C:
CD \LITELINK
LLNIC
NETDIAG ARP 192.168.0.10
NETDIAG TCP 192.168.0.10 21210
LLSTART
```

The addresses above are examples only. Choose an unused static DOS IP and use the Windows listener's actual IP and port. **Do not copy the same LL.INI to both computers**: DOS and Windows use different settings.

For an explanation of **every shipped and optional `LL.INI` key**, see [Configuration Reference](DOCS/CONFIG.TXT). For **every executable and command-line operation**, see [Command-Line Reference](DOCS/COMMANDS.TXT).

## Using LLShell

The DOS and Windows file managers show local and remote files. Select a source pane and copy to the folder in the opposite pane. Directory copies include subdirectories and empty folders. Existing destination files are preserved rather than silently overwritten.

On DOS, **Tab** switches panes, **Space/Insert** tags files, **F5** copies, **F2** refreshes, **F8** enters a command and **F10** exits. On Windows, select multiple items with Ctrl-click/Ctrl+A; **F5** copies and **F4** retries a paused queue.

Remote DOS commands require `[EXECUTION] ALLOWRUN=ON` and DOS startup through `LLSTART.BAT`. The DOS manager exits while the command executes and reconnects afterward. Remote commands are not restricted to the shared-file directory, so leave this feature disabled unless needed on a trusted network.

## Documentation

- [Configuration reference: every LL.INI line, Windows and DOS setup](DOCS/CONFIG.TXT)
- [Command-line reference: every bundled executable/batch utility](DOCS/COMMANDS.TXT)
- [Installation and upgrades](DOCS/INSTALL.TXT)
- [File manager keys, file transfers and remote commands](DOCS/USAGE.TXT)
- [Troubleshooting and collecting diagnostic reports](DOCS/TROUBLE.TXT)
- [Filename, transfer and recovery limits](DOCS/LIMITS.TXT)
- [Build guide](DOCS/BUILD.TXT)
- [Developer notes](DOCS/DEVELOP.md)
- [Protocol details](DOCS/PROTOCOL.md)

## AI-Assisted Development

Lite Link is an experimental, predominantly AI-coded software project developed with OpenAI Codex.

The project's architecture, design goals, feature requirements and development direction are human-led, while much of the implementation, debugging and code generation is performed using AI.

Development includes testing on real vintage hardware, including original IBM XT-class 8088 systems, alongside emulated DOS and Windows environments.

Lite Link is also an experiment in how effectively modern AI-assisted development techniques can produce useful, efficient software for hardware dating back to the early 1980s.

As an experimental project, AI-generated code should not be assumed to be free of defects. Community testing, feedback and contributions are welcome.

**AI development assistance:** OpenAI Codex.

## Build from source

The source archive builds with Open Watcom (DOS and Win32 targets). See [the build guide](DOCS/BUILD.TXT) for host requirements and exact build commands. Generated programs are written to `OUT/`.

## Security and limitations

Lite Link 0.0.13 is experimental and intended for a **trusted LAN**. It does not authenticate peers or encrypt transfers, provides no native HTTPS/TLS support and does not offer a DOS DHCP client. CRC checks detect accidental corruption; they are not cryptographic authentication. Do not expose command execution or shared folders to an untrusted network.
