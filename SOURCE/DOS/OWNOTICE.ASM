; SPDX-License-Identifier: Unlicense
; Notice text only, linked into programs that use Open Watcom runtime code.
; It does not execute or replace the third-party runtime's own license.
.8086
.model small
.data
public _ow_notice
_ow_notice db 'Open Watcom runtime source is available under the ',13,10
 db 'Sybase Open Watcom Public License 1.0. See LICENSES/WATCOM.TXT and',13,10
 db 'https://github.com/open-watcom/open-watcom-v2/tree/master/bld/clib',13,10
 db 'Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.',13,10,0
.code
end
