# Lite Link 0.0.14 validation

Checked on 10 November 2026. The accompanying JSON records exact supplied executable hashes and result summaries.

| Check | Result and scope |
| --- | --- |
| Native x64 Watcom build | All eight application targets and the optional public DOS verifier built. Source-ZIP clean rebuild is checked before packaging is finalized. |
| Actual DOS 3.30, emulated 8086 at 750 cycles | Public LLRWTEST: PASS, zero errors. Create/read/write, append, seek above 64 KiB, truncate, commit, duplicate handles, locks/sharing, folders, exact rename, attributes and timestamps exercised. |
| Independent binary comparison | 4,099 bytes returned over the network, SHA256 `5f080242526981d5b61117c08f2fc5783ac1d7d6fc30ab7bde2f0e0016767d2a`. |
| Memory and unmount | 13,184 resident bytes plus 16-byte DOS MCB; largest free block restored exactly. Open-file unload refused. Private stack high-water 140 bytes in this workload, with 512 reserved. |
| Lost successful write reply | Host committed one 19-byte write; the proxy discarded its reply. DOS reported failure; a second call did not reconnect/replay. Unmount warned of uncertainty and restored memory. |
| Protocol guards | Bad CRC and duplicate sequence rejected; traversal/devices rejected; host read-only and requested-mount permissions enforced; older LLFS client stays read-only. |
| Existing tools | Bidirectional CRC transfer, actual HTTP download, SNTP check-only UDP, three unloaded RPC jobs and four connections passed. |
| Diskless 8088 | 4.77 MHz, 640 KiB, VGA, actual DOS 3.30 floppy, no HDD. Configured C: mounted read/write. Windows 2.03 started over virtual Ethernet; a Win16 application saved/reopened a file and changed WIN.INI on the private host copy. |
| Native Windows 11, no emulator | GUI/Winsock/bind/listen/SO_ACCEPTCONN initialization logged success for 127.0.0.1 and 0.0.0.0. A separate native TCP client connected, and the GUI remained alive after it disconnected. Windows IP Helper independently returned the owning PID/local port. |

The separate netstat listener-enumeration check was **inconclusive**: it did not display the listener, and the IP Helper row had raw state 0. That state is not reported here as a LISTENING-state PASS. Actual native client connections and filesystem protocol transactions succeeded. No firewall settings were changed.

The emulators used private loopback/virtual Ethernet fixtures. These are **not physical LAN tests**. Physical-card LLFS TSR, the changed Windows 3.1/Win32s host and normal diskless Windows-to-DOS shutdown are **unverified**. Earlier suite tests do not qualify those changed combinations. Other DOS redirector layouts, ISO/MSCDEX and automatic reconnect are not implemented.

During integration, the optional verifier initially failed to link because a toolchain SYSTEM DOS preset was not found; its explicit FORMAT DOS response file fixed this. The first old-tool regression fixture used an incorrectly substituted target address; the corrected fixture passed. Initial netstat assertions failed as described above and are not counted as passed. The final production build and listed workload checks completed.

A prior hostname-resolution test used the exact same LLFS executable hash; DNS completed before resident setup code was discarded. The public release does not include that private responder, emulator configuration, licensed OS files or developer script dependencies. Public hardware test instructions are in TESTING.TXT.

## Final licensing rebuild

The Unlicense revision adds documentation and source/license notices to the DOS executables containing Open Watcom runtime code. All application targets and the public verifier rebuilt. The public DOS API test again passed with zero errors and full memory recovery; transfer/HTTP/SNTP and the three-job unloaded RPC regression passed again using the rebuilt files. LLFS remains byte-identical to the diskless Windows 2-tested executable, and Windows host code/data match that tested build apart from defined PE/resource link timestamps. Notice strings were checked in all five runtime-using executables. The JSON lists the final supplied hashes.
