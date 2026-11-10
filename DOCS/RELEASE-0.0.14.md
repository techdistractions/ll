# Lite Link 0.0.14

Lite Link adds **LLFS 0.0.2**, a resident mapped-drive client for actual MS-DOS 3.30, and its matching **LLShell32 0.0.14** Windows filesystem service. Existing DOS LLShell 0.0.12 and utilities 0.0.11r1 remain included.

## License update

The standard Unlicense replaces the first-party GPLv3 notice for this version. This does not relicense third-party code or alter previously published v0.0.13 packages. Executables that link Open Watcom runtime routines include a source/license availability notice, with full terms in the archives. OpenAI Codex attribution remains.

## Added

- Configurable DOS drive C through Z backed by the Windows host's configured ROOT folder, including floppy-boot systems without a local hard disk.
- Read/write file operations: create, read/write/append, seek, truncate, commit/close, duplicate handles, folders, exact rename, deletion, attributes, DOS timestamps, sharing modes and byte-range locks.
- Independent read-only gates in DOS `[FILESYSTEM]` and Windows `[HOST]`. Both default to ON; writable access requires both OFF.
- Safe unmount with memory/NIC release and refusal while files are open or a later interrupt hook owns the chain.
- Public `LLRWTEST.EXE` verifier with configurable target drive/loader path and documented physical-hardware test steps.

## Fixes and implementation

- Compact network-state offsets use the configured layout when selecting a NIC.
- The LLFS build explicitly selects its own shared header instead of accidentally finding a parent checkout.
- Startup-only code/data and the environment are freed after mounting. Resident allocation is **13,184 bytes**, plus a **16-byte DOS memory header**: **13,200 bytes total**, 5,040 resident bytes less than the first writable implementation.
- Both NIC backends, 512-byte TCP queues and CRC32 checks remain. Mutations are not automatically replayed. A write reply is sent after a successful host flush; a lost acknowledgment causes an error/uncertain outcome rather than a blind retry.
- The GUI separates mapped-drive service from manager/RPC traffic, and supports older read-only LLFS clients.

## Validation and unverified combinations

See [the validation record](VALIDATION-0.0.14.md). Emulator tests exercised actual DOS 3.30 file APIs through native Windows TCP, including complete memory recovery and a lost successful write acknowledgment. A diskless emulated 8088/640 KiB/VGA system launched Windows 2.03 from the writable drive; a Win16 program saved/reopened a file and updated WIN.INI. These are emulator results, not physical-LAN verification.

**Unverified:** physical-NIC LLFS TSR operation and this version's changed Windows 3.1/Win32s host runtime. The prior host's compatibility results do not qualify these additions. LLFS rejects DOS versions other than 3.30. ISO/MSCDEX, wildcard rename, automatic reconnect and simultaneous NIC ownership are not implemented. Normal Windows-to-DOS shutdown in the diskless fixture is unverified.

## Upgrade and packages

Use the matching Windows host, preserve your working LL.INI and add the optional FILESYSTEM/ISO sections. LLFS is an alternative resident mode; unmount it before the other DOS network tools use the same card. Read `DOCS/LLFS.TXT`, `DOCS/CONFIG.TXT` and `DOCS/TESTING.TXT` before enabling writes. No emulator, Python or PowerShell is needed to use or compile the public package. No OS disk images, ROMs, compiler or private test network setup is bundled.

The package layout remains DOS, WINDOWS, CONFIG and DOCS. Source and binary archives have SHA256 checksums. Original Lite Link code now uses **The Unlicense**, as requested by the maintainer. MIT notices for the adapted DOS structures and separate Open Watcom runtime terms are retained. See `DOCS/LICENSING.TXT` and `THIRDPARTY.TXT`. The existing v0.0.13 release, tag and assets are unchanged.

## AI-assisted development

Lite Link is an experimental, predominantly AI-coded project developed with **OpenAI Codex**. Architecture, design goals, requirements and development direction are human-led; much of the implementation, debugging and code generation is AI-assisted. Earlier project testing includes real vintage hardware, while the new LLFS evidence above is specifically emulated. AI-generated code may contain defects; community tests and reports are welcome.

**AI development assistance: OpenAI Codex.**
