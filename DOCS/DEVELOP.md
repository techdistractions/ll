# Source reference

Build instructions are in `BUILD.TXT`. This guide covers implementation
boundaries needed when changing the source. Runtime setup is in `INSTALL.TXT`.

## Modules

| Area | Files under SOURCE | Role |
| --- | --- | --- |
| Shared | `SHARED/au.h`, `SHARED/LLTREE.H` | Framing, CRC, names, paths and traversal helpers |
| DOS manager | `DOS/AGENT.C`, `DOS/SHELLUI.C`, `DOS/TREE.C` | File operations, text UI and folder copies |
| DOS jobs | `DOS/JOBS.C`, `DOS/LLSTART.BAT`, `DOS/SYSTEM.ASM` | Persisted command handoff and restart |
| DOS transport | `DOS/NNETNE.INC`, `DOS/NNETDEF.INC` | ARP/IPv4/DNS/TCP and network-state layout |
| Hardware | `DOS/NNETNIC.INC`, `DOS/NNET3C.INC` | Direct NE2000 and 3C509B polling |
| DOS utilities | `DOS/LL/` | HTTP, SNTP and NIC probe |
| Diagnostics | `DOS/DIAG/` | Instrumented transport and NETDIAG |
| Windows transport | `WIN32/HOST.C`, `WIN32/STARTUP.INC` | Winsock, file operations, config and errors |
| Windows manager | `WIN32/GUI.INC`, `STYLE.INC`, `PEER.C`, `QUEUE.C`, `TREE.C` | UI, peer turns, queues and folders |
| Resources | `WIN32/LLSH32.RC`, `LLICON.ICO`, `LLICON.SVG` | Embedded icon and editable artwork |
| Build | `BUILD/*.LNK`, `BUILD/HOST.CCF` | Static linker/compiler response files |

Several C files are intentionally included into their parent translation unit.
Do not compile JOBS.C, SHELLUI.C, COMMON.C, PEER.C or QUEUE.C independently.
The MAKEFILE lists the actual compilation units.

## DOS lifetime and hardware ownership

SHELLNET.ASM provides manager startup; START.ASM is shared by utility and
diagnostic adapters. They initialize data/BSS/stack and release excess DOS
allocation before entering C. The manager's UI callback yields from network
waits; utility builds do not install that callback.

The NIC backend uses direct polling, not a packet-driver API. Do not add an
IRQ handler or resident allocation to the unloaded-command path. NE2000 and
3C509B share the bounded raw-frame interface and network state. Use the named
state offsets in NNETDEF.INC rather than inventing offsets in C.

The TCP receiver caches one segment. It advertises a zero receive window while
cached data remains and reopens the window after draining it. Advertising
partly freed space without implementing append would lose incoming data.

LL_UTILITY selects wrappers/UDP dispatch. LL_DIAGNOSTIC adds NETDIAG hooks.
These are assembly conditionals over the maintained transport; no generated
assembly copy or scripting runtime is needed.

## Protocol and UI ownership

A frame is accepted only with the expected sequence, bounded length and payload
CRC. A malformed frame ends the session; there is no byte-scanning recovery.
Receivers stage output in AUTOUAT.$$$ and commit only verified complete files.
That filename and the AUAT wire signature are compatibility identifiers.

Windows serializes transactions and grants DOS interactive turns. Keyboard
callbacks queue intent rather than recursively starting a transaction. Windows
wait functions pump GUI messages, so `busy` must cover the whole operation AND
its final directory refresh. Otherwise a timer poll could interleave replies.

Each pane's selected folder is independent of the folder exported to its peer.
Save and restore server directory state around local browsing and tree work.
A queued operation captures its source side and folders before execution.

Tree copying is bounded iterative traversal, not C recursion or an unbounded
in-memory file list. Windows validates components against reparse points. Keep
path/depth limits, source stability rules and destination preservation intact.

## Journals and commands

DOS LLVIEW and Windows LLQUEUE journals save queue progress with CRC. Save a
checkpoint before acknowledging progress; restore pending queues paused until
an explicit retry. Resume operates at file boundaries.

LLSTART.BAT sets the supervisor marker AUSUP=1. JOBS.C commits AUJOB.DAT and the
job batch before acceptance. The DOS agent closes the NIC/files and exits via
DOS AH=4Ch. COMMAND.COM CALLs the job, records completion in AUDONE.TXT and
restarts the agent. Do not replace this with EXEC from a resident parent.

Resume matches job ID, command CRC and peer identity before success is reported.
Missing completion state is incomplete work, not a reason to replay an arbitrary
command. Preserve the manual reset and power-loss limits described to users.

## Windows compatibility and drawing

Keep GUI, LITELINK and LEGACY_WIN32S for the universal Windows target. It uses
classic USER/GDI and Winsock 1.1 imports, not a modern CRT. Check new imports
against Win32s and test both runtimes when changing platform calls.

LoadIconA uses embedded group 101. The ICO contains classic 16-colour 32x32 and
16x16 DIB images. WRC and WLINK embed it; runtime does not need an external ICO.

Path labels use SS_LEFTNOWORDWRAP and SS_NOPREFIX. Their text/background is
opaque, with full erase invalidation before repaint. Preserve this behavior
when changing labels: a transparent SS_SIMPLE update can leave old glyphs when
a long path is replaced by a shorter one.
