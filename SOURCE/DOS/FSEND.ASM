.8086
_BSS segment word public 'BSS'
public _fs_bssend
_fs_bssend db ?
_BSS ends
; Real retained boundary byte, ahead of installer-only literal strings.
FSLIMIT segment word public 'DATA'
public _fs_resend
_fs_resend db 0
FSLIMIT ends
DGROUP group _BSS,FSLIMIT
end
