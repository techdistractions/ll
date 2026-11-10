LITELINK 0.0.14
==============
Lite Link (LL) is an experimental micro-networking SUITE for DOS, designed for
8088-class computers with 256 KB RAM or more. Instead of a conventional
resident network driver and full stack, it uses a native register-level NIC
backend and a compact TCP/UDP implementation for LAN/Internet networking.

LLShell is ONE application in the suite, not Lite Link itself. The bundled
utilities share the lightweight networking foundation; more may be added.

CHOOSE A PACKAGE
  LITELINK-0.0.14-BINARY.ZIP  Programs, configuration examples and user guides.
  LITELINK-0.0.14-SOURCE.ZIP  Source, examples, documentation and Watcom build.

DOS: DOS 3.3+, 8086-compatible processor and supported Ethernet hardware.
Supported native backends: NE2000, PicoMEM NE2000 emulation and 3C509B.
The DOS apps poll the card directly; do not load a packet driver for them.

Windows target: Windows 11 x64 or Windows 3.1 with Win32s/Winsock. The new
0.0.14 host is verified on Windows 11; its Win32s runtime is UNVERIFIED.
The 0.0.13 host was tested on both. Start the Winsock provider first on 3.1.
Windows, Win32s, Winsock and compilers are not bundled.

The package contains LLShell32 0.0.14, DOS LLShell 0.0.12 and utilities
0.0.11r1. Different component version banners are expected.

QUICK START
1. On Windows, copy the WINDOWS folder contents into C:\LITELINK.
   Create C:\LITELINK\FILES. Edit LL.INI beside LLSH32.EXE:
     [HOST]
     BIND=0.0.0.0
     PORT=21210
     ROOT=C:\LITELINK\FILES
     LOG=C:\LITELINK\LLHOST.LOG
   Run LLSH32.EXE. It should display its listening address.

2. On DOS, copy the DOS folder contents into C:\LITELINK.
   Create C:\LITELINK\FILES. Copy a matching CONFIG example as LL.INI:
     NE2000.INI   Standard NE2000-compatible card.
     PICOMEM.INI  PicoMEM emulated NE2000, packed PROM format.
     3C509.INI    Native 3C509B backend.
   Edit the DOS IP, mask, gateway, DNS and actual hardware I/O base.
   Set [SHELL] HOST to the Windows PC's current LAN IPv4 address and set
   PORT to match Windows. Use a free DOS IP address on the correct subnet.

3. From C:\LITELINK on DOS, run LLSTART.BAT. The managers should connect.
   Run the batch from the installation folder; it supervises remote jobs.
   File transfers do not require remote command execution to be enabled.

All sample addresses are examples. Replace them before connecting. The samples
use Windows 192.168.0.10, DOS 192.168.0.20 and gateway/DNS 192.168.0.1.
These must not be used if they conflict with your network. See DOCS\CONFIG.TXT.
Keep LL.INI beside the programs. Each endpoint has its own local master file.

EVERYDAY USE
Local is this computer; Peer is the connected computer. Select the source pane
and copy to the folder shown in the opposite pane. Enter/double-click opens
folders; Backspace moves up. The . and .. entries stay within the export root.

DOS keys:
  Tab                 Switch pane.
  Arrows / PgUp/PgDn   Move selection.
  Space / Insert      Toggle a tag.
  +                   Select a wildcard pattern.
  * / -               Select displayed entries / clear tags.
  F2 / F5             Refresh / copy selected files or folders.
  F6 / F7             Rename / create a folder.
  F8                  Enter a remote DOS command.
  Delete              Confirm file deletion; folder deletion is not recursive.
  Esc / F10           Cancel at a file boundary / quit.

Windows:
  Ctrl-click, Ctrl+A, Space or Insert select entries. F5 copies; F4 retries a
  saved queue. Use the Name field for Rename/Folder and the Command field for
  Run command. Closing normally tells the DOS peer to end its connection.

Folders copy recursively, including empty folders. Existing directories merge;
existing destination files are preserved and reported as conflicts. Copying
uses length/sequence/CRC checks, temporary files and verified commit. Recovery
resumes at file boundaries, not midway through a partial file. See DOCS\USAGE.TXT.

REMOTE COMMANDS
Set [EXECUTION] ALLOWRUN=ON on DOS and start through LLSTART.BAT. Enter a DOS
command in the Windows Command field, for example:
  DIR > LISTING.TXT
LiteLink saves the job and exits; COMMAND.COM runs it; the supervisor restarts
LiteLink afterward. Refresh and copy LISTING.TXT back through the file manager.
Disconnection while the command runs is expected. Acceptance means the job was
saved, not that it has completed. Wait for completion and reconnection.

A Windows application may be started through WIN.COM using its installed path.
Windows must exit normally and return to DOS before LiteLink can resume. For
unattended jobs, the deployed application or command sequence must arrange that
exit and save its own results. LiteLink does not remain running to close Windows.

COMPLETE SUITE (0.0.14 PACKAGE)
  LLFS.EXE       Optional resident mapped drive; actual DOS 3.30 only.
  LLRWTEST.EXE   Optional LLFS verifier; see DOCS\TESTING.TXT.
  LLSHELL.EXE    DOS dual-pane file manager and remote command endpoint.
  LLSH32.EXE     Windows file manager and TCP listener.
  LLGET.EXE      HTTP downloader; optional expected CRC32, no native HTTPS.
  LLTIME.EXE     SNTP clock check/set utility.
  LLNIC.EXE      Native NIC/MAC hardware initialization probe.
  NETDIAG.EXE    MAC, loopback, ARP, ping, TCP and basic HTTP diagnostics.
  AUMON.COM      Transient binary memory monitor for command supervision.
  LLSTART.BAT    DOS manager supervisor, needed for remote commands.
  LLDEBUG.BAT    Diagnostic log collector.

Stop the DOS manager before manually running another tool using the NIC.
  LLNIC
  NETDIAG ARP 192.168.0.10
  NETDIAG PING 192.168.0.10
  NETDIAG TCP 192.168.0.10 21210
  LLTIME /CHECK
  LLTIME /SET
  LLGET http://example.com/file.zip NEWFILE.ZIP
  LLDEBUG 192.168.0.10 21210

See DOCS\COMMANDS.TXT for EVERY utility's exact command-line syntax and
DOCS\CONFIG.TXT for EVERY LL.INI setting (Windows and DOS). Addresses above
are examples only. LLTIME is check-only by default; /SET changes DOS time.

UPGRADING
Close the managers before copying new EXEs. Keep your working LL.INI and shared
files. This version adds LLFS 0.0.2 and its Windows host service. Install the matching
0.0.14 Windows host to use LLFS. Existing managers/utilities remain included.
The new optional FILESYSTEM/ISO configuration sections are accepted by the suite.

BUILD FROM SOURCE
Use a complete Open Watcom installation with DOS and Win32 tools/libraries.
No Python, PowerShell, source generator or image converter is needed.
Extract the SOURCE ZIP preserving folders into a short path such as C:\LLSRC.
Set WATCOM to the compiler installation, then choose the appropriate command:

  SET WATCOM=C:\WATCOM
  C:
  CD \LLSRC

  BUILD nt64            Windows x64 Command Prompt, using BINNT64 tools.
  BUILD nt              Native 32-bit Windows tools from BINNT.

For a DOS-hosted build on a suitable 386+ machine with extended memory:
  SET PATH=C:\WATCOM\BINW;%PATH%
  BUILD

Use only the command for your build host. The DOS tools use DOS4GW; keep it on
PATH. Building needs more hardware than running the DOS apps. On a Windows 3.1
machine, use the DOS-hosted tools from DOS. Windows 11 x64 cannot run DOS EXEs
directly. Windows 3.1 runtime support does not imply the compiler runs in Win32s.

Successful output is in OUT: LLSHELL.EXE, LLGET.EXE, LLTIME.EXE, LLNIC.EXE,
NETDIAG.EXE, AUMON.COM and LLSH32.EXE. The DOS target also copies LLSTART.BAT.
OBJ/MAP/RES files are build products. BUILD.LOG records make diagnostics.

For target-only builds use BUILD dos / BUILD windows under DOS, or
BUILD nt64 dos / BUILD nt64 windows on Windows x64. Full requirements, compiler
flags, resource handling and troubleshooting are in DOCS\BUILD.TXT.

DOCUMENTATION
  DOCS\INSTALL.TXT  Installation, upgrades and directory layout.
  DOCS\CONFIG.TXT   All LL.INI settings and step-by-step setup.
  DOCS\COMMANDS.TXT Command-line reference for all bundled tools.
  DOCS\USAGE.TXT    File management, utilities and remote commands.
  DOCS\TROUBLE.TXT  Connection errors, logs and interrupted jobs.
  DOCS\LIMITS.TXT   Filename, path and recovery limits.
  DOCS\BUILD.TXT    Complete build and deployment guide (source release).

Use a trusted LAN. Transfers and commands are not authenticated or encrypted.
ALLOWRUN permits arbitrary DOS commands; the shared-file root is not a command
sandbox. Keep ALLOWRUN=OFF unless command execution is needed.

LLFS MAPPED DRIVE - NEW IN 0.0.14
Run LLFS separately from LLSTART. The mapped drive keeps a resident network
stack while mounted (13,184 bytes plus a 16-byte DOS header). This optional
mode is different from unloaded LLShell remote commands. Choose C through Z
in [FILESYSTEM] DRIVE, with LASTDRIVE covering it. C is suitable for a PC
booted from floppy with no local hard disk. Actual DOS 3.30 is required.
For writes, set both DOS [FILESYSTEM] READONLY=OFF and Windows [HOST]
READONLY=OFF. Both default to ON. Host ROOT chooses the exported folder.
LLFS /U releases the NIC and memory after files close and you select a local
drive. See DOCS\LLFS.TXT for recovery/limits and DOCS\TESTING.TXT to test.
Physical-NIC LLFS and this version's Win32s host runtime are UNVERIFIED.
The latest implemented version is 0.0.14; individual component banners differ.
LICENSE
Original Lite Link code: The Unlicense (LICENSE and UNLICENSE). Third-party
MIT structures and Open Watcom runtime keep their separate terms/notices.
See THIRDPARTY.TXT, LICENSES and DOCS\LICENSING.TXT. The published v0.0.13
release is unchanged. OpenAI Codex attribution is preserved.
