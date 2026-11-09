# LiteLink wire protocol

One DOS agent initiates one TCP connection to the configured Windows host. Transfer framing and payloads retain the version-1 AUAT wire signature. HELLO capabilities and the current tree extension are specified below.

## Frame and transfer messages

The 16-byte header is ASCII AUAT (offset 0, four bytes), version 1 (offset 4), type (offset 5), little-endian payload length <=512 (offset 6, u16), per-direction sequential counter starting at zero (offset 8, u32), and payload CRC-32 (offset 12, u32). CRC uses reflected EDB88320h, initial/final FFFFFFFFh. Empty payload CRC is zero. Malformed frames/counters/CRC, EOF and network deadlines fail the session.

Existing message numbers: HELLO=1, WELCOME=2, PUT=3, READY=4, DATA=5, END=6, RESULT=7, GET=8, QUIT=9, REQUEST=10, LIST=11, ENTRY=12, CHDIR=13, MKDIR=14, RENAME=15, DELETE=16. PUT metadata remains 13-byte padded leaf name + u32 length + u32 file CRC. File DATA blocks are 1..512 bytes. READY/RESULT contain u16 status. ENTRY is the original 22-byte name/size/attributes/DOS-date/time record.

HELLO is 13 ASCII application-version bytes followed by a flags byte: bit 0 initial file request, bit 1 completed job awaiting resume association. Legacy WELCOME is empty; 0.0.12 negotiates tree support as described below. A completed job suppresses the initial REQUEST. GET accepts a NUL-terminated leaf or validated absolute DOS path below ROOT, up to 64 bytes including NUL; returned PUT metadata still contains the leaf. File status values remain 0 OK, 1 invalid, 2 missing, 3 exists, 4 I/O, 5 integrity, 6 protocol.

## Unloaded job extension

| Type | Name | Payload |
| --- | --- | --- |
| 20 | JOB_SUBMIT | u32 host job ID followed by 1..120 exact printable ASCII command bytes, no NUL |
| 21 | JOB_ACCEPT | u32 ID, u16 acceptance status, u16 flags, u32 command CRC-32 |
| 22 | JOB_RESUME | u32 ID, u16 COMMAND errorlevel, u16 flags, u32 command CRC-32 |
| 23 | JOB_RESUME_ACK | u32 matching ID |
| 24 | JOB_READY | u32 matching ID |

Acceptance status is 0 committed, 1 disabled, 2 invalid command/path/limits, 3 disk state/write failure, 4 existing job busy, 5 supervisor unavailable. Accepted flags=1 mean the disk batch and job record are committed; they do not mean the command ran. A rejected request does not trigger exit or restart.

After JOB_ACCEPT(success), the agent stops the NIC, closes files and exits through DOS AH=4Ch with errorlevel zero. The host closes the old socket and waits. The batch supervisor CALLs the saved job only after LLShell has returned. It records the job's COMMAND errorlevel, restores the control directory and starts LLShell again.

The new connection resets frame counters, sends HELLO with resume bit and obtains WELCOME. The agent sends JOB_RESUME with the original ID/CRC and flags=3 (matching supervisor completion record; unloaded job mode). The host verifies both, sends JOB_RESUME_ACK, and waits for JOB_READY. Execution succeeds only after this complete association and an acceptable errorlevel. A nonzero COMMAND errorlevel is not normal success.

The next host command confirms that association progressed; the agent then clears the completed job record. A dropped connection before that leaves the record available for another resume attempt. Exactly-once execution across arbitrary power loss is not guaranteed. A record without a matching supervisor completion file cannot be reported as completed.

The disk AUJOB.DAT is 200 bytes: AUJ3 magic at 0, ID at 4, command CRC at 8, NUL-terminated launch directory in bytes 12..75, command length at 76, command bytes at 77..196, remaining padding zero. AUDONE.TXT is the eight uppercase hex ID digits, a space and decimal errorlevel with CRLF.

Wire types 17..19 are reserved and rejected. The Run action submits JOB_SUBMIT. There is no DOS EXEC result/termination type for this mode: completion is the shell's ERRORLEVEL. Network absence during a job is expected, and JOBWAIT limits the host's reconnect wait rather than timing or aborting the DOS program.

No authentication or encryption is provided. Command execution is explicitly gated by ALLOWRUN and intended for a controlled trusted connection. Transfer-root validation does not sandbox an arbitrary DOS command.


## Interactive manager turns

HELLO bit 2 (value 4) advertises the DOS interactive manager. Existing request/resume bits retain their meanings. The host sends LL_POLL=25 with its current DOS-selected Windows export-relative folder and terminating NUL (1..64 bytes). DOS replies LL_IDLE=27 with no payload or LL_CLIENT=26 with operation byte followed by NUL-terminated 8.3 names, rename pairs or printable job command. Windows services that transaction synchronously using existing list/file/job messages before granting another turn. DOS sends no unsolicited operation. LL_CANCEL=28 confirms cancellation after the current file. Windows GUI and DOS local folders remain distinct from the folder each peer exports. Legacy agents are never polled with this extension. Type 30 is reserved for non-release testing and is not a supported public operation.

Both copies preserve destinations; queue retry is whole-file and never byte-offset resume. Host job recovery journals ID/CRC/peer durably (LLJOB.DAT/NEW/BAK) and validates resumed completion. Exactly-once behavior across arbitrary power loss is not guaranteed.

## Tree operations and compatibility

The fixed 13-byte HELLO identity is `LLSHELL 0.012`; the existing capability byte
at 13 is unchanged. A tree-capable host recognizes this identity and replies with WELCOME
containing a little-endian 16-bit capability word (bit 0 tree support). It sends
empty WELCOME to older identities. The DOS manager accepts empty or 2-byte
WELCOME. Old peers can still browse/copy files and run jobs; recursive copying
requires the negotiated tree capability and compatible managers at both ends.

LL_TREE=31 carries `[operation:u8, ordinal:u16le, path:NUL-terminated string]`,
4..67 bytes. Paths are export-root-relative, at most 63 bytes, with validated 8.3
components. Empty path is allowed for root SCAN. Absolute paths, drive prefixes,
empty components, trailing separators and dot components are rejected. The UI's
synthetic . and .. entries never become tree-copy paths. Windows checks every
path component for reparse points; no symlink/junction traversal is performed.

Operations:
- 1 SCAN: return one 22-byte ENTRY-byte record at the requested zero-based ordinal, or
 RESULT/OK at end. Ordinals count real children, excluding . and ... Unsupported
 names/reparse points produce RESULT/7 (TREE_SKIP); the client records a skipped
 item, advances one ordinal and continues. Other errors end that subtree with a
 failure. At most 4096 real children per directory are processed; excess is an
 explicit error, not a successful truncated copy.
- 2 MKDIR: create a directory or succeed if the destination already is a real
 directory. Existing files with that name are preserved and reported as conflicts.
- 3 GET: return the existing PUT/DATA/END file transaction, or RESULT error. The
 PUT metadata still contains only the leaf filename, size and CRC32.
- 4 PUT: server validates the target parent and returns RESULT/OK. The requester
 then starts the existing PUT/DATA/END transaction. The metadata leaf must match
 the requested path. A rejected tree request has no following PUT transaction.

For DOS-originated work, LL_CLIENT=26 contains operation31 followed by the same
body. The host handles it entirely during that granted turn. Folder operations
restore the prior exported filesystem directory after each transaction. Tree
requests do not change GUI folder selections or cause unsolicited wire traffic.

Traversal is bounded iterative DFS, 16 levels, reusing the existing file integrity
checks and staging files. Sequential directory cursors are cached to avoid a new
full directory enumeration for every file. Empty folders are included. Existing
directories merge; existing files are never overwritten. Cancellation happens at
completed operations/file boundaries. It is not a transactional snapshot of a
concurrently changing source tree; keep source folders stable while copying.

DOS view journal LLV2 remains 2048 bytes: original headers/names through 1807,
128 directory flags at 1808,100-byte traversal state at 1936 (path 64,16 u16 ordinals,
depth,phase), copied/failed/root-failure-baseline/folder counters at 2036..2043,
CRC32 at 2044. LLV1 file-only records are accepted. Windows LLQ2 is 4096 bytes:
original queue header/names through 3475,256 directory flags at 3476, tree state at
3732, root-failure/folder counters at3832/3834, CRC32 at 4092. LLQ1 file-only records
are accepted. Windows writes/flushes LLQUEUE.NEW before replacing DAT and checks
NEW then DAT on recovery. Both queues retain the current tree position and resume
without restarting already-acknowledged files. A lost final acknowledgement may
leave an already-created file, which is safely preserved/reported as existing.

No metadata preservation, byte-offset resume, recursive deletion or automatic
name mangling is introduced. Recursive copy covers representable DOS 8.3 names
within the documented path/depth bounds. Display limits (128 DOS/256 Windows,
including navigation entries) do not limit traversal inside a selected folder.

## Windows applications through DOS commands

Starting WIN.COM uses the same job handoff and resume semantics. Windows must
exit normally and WIN.COM must return to COMMAND.COM before the supervisor can
record completion and restart LLShell. A deployed application/command sequence
must arrange normal Windows exit for unattended operation. Launch acceptance
alone is not command completion.
