LITELINK 0.0.13
==============
File transfer and remote DOS commands for DOS and Windows.

LiteLink provides two-pane file managers, tagged file selection, recursive
folder copying, a DOS clock utility and an HTTP downloader. The DOS manager
exits completely while a remote command runs, then reconnects when it ends.

CHOOSE A PACKAGE
  LITELINK-0.0.13-BINARY.ZIP  Programs, configuration examples and user guides.
  LITELINK-0.0.13-SOURCE.ZIP  Source, examples, documentation and Watcom build.

DOS: DOS 3.3+, 8086-compatible processor and supported Ethernet hardware.
Supported native backends: NE2000, PicoMEM NE2000 emulation and 3C509B.
The DOS apps poll the card directly; do not load a packet driver for them.

Windows: the same LLSH32.EXE runs on Windows 11 x64 and on Windows 3.1 with
Win32s 1.1 and a Winsock provider such as Trumpet. On Windows 3.1 start the
provider first. Windows, Win32s, Winsock and compilers are not bundled.

The package contains LLShell32 0.0.13, DOS LLShell 0.0.12 and utilities
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

UTILITIES
Stop the DOS manager before manually running another program using that NIC.
  LLNIC
  NETDIAG ARP 192.168.0.10
  NETDIAG PING 192.168.0.10
  NETDIAG TCP 192.168.0.10 21210
  LLTIME /CHECK
  LLTIME /SET
  LLGET http://example.com/file.zip NEWFILE.ZIP
LLTIME defaults to check-only. Configure its server and local UTC offset before
using /SET. LLGET supports HTTP, not native HTTPS; existing files are preserved.

UPGRADING
Close the managers before copying new EXEs. Keep your working LL.INI and shared
files. For this release, the Windows icon and path-label redraw are updated;
no DOS binary update is required if those components are already current.

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
  DOCS\CONFIG.TXT   LL.INI settings and NIC selection.
  DOCS\USAGE.TXT    File management, utilities and remote commands.
  DOCS\TROUBLE.TXT  Connection errors, logs and interrupted jobs.
  DOCS\LIMITS.TXT   Filename, path and recovery limits.
  DOCS\BUILD.TXT    Complete build and deployment guide (source release).

Use a trusted LAN. Transfers and commands are not authenticated or encrypted.
ALLOWRUN permits arbitrary DOS commands; the shared-file root is not a command
sandbox. Keep ALLOWRUN=OFF unless command execution is needed.
