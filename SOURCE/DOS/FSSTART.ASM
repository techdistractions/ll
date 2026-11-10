LL_UTILITY EQU 1
LLFS_COMPACT EQU 1

.8086
.model small
.stack 1024
.data?
bss_begin label byte
.data
extrn _netstate:byte
.code
IFDEF LL_UTILITY
extrn _ll_main:near
ELSE
IFDEF LL_DIAGNOSTIC
extrn _diag_main:near
ELSE
extrn _agent_main:near
ENDIF
ENDIF
extrn _fs_keep:word,_fs_paras:word,_fs_resend:byte,_fs_bssend:byte
public start
start:
    mov dx,es
    mov ax,@data
    mov ds,ax
    cld
    ; Small-model C requires SS=DS. Rebase the DOS loader's stack.
    mov bx,ss
    sub bx,ax
    mov cl,4
    shl bx,cl
    add bx,sp
    cli
    mov ss,ax
    mov sp,bx
    sti
    ; Clear only BSS: initialized data and CRC table follow BSS in this layout.
    mov es,ax
    mov di,offset bss_begin
    mov cx,offset _fs_bssend
    sub cx,di
    xor ax,ax
    rep stosb
    mov ax,ds
    mov bx,offset _fs_resend
    ; Retain CODE/DATA/BSS only. Setup code and DOS startup stack are discarded.
    add bx,15
    mov cl,4
    shr bx,cl
    add bx,ax
    sub bx,dx
    mov _fs_paras,bx
    ; Setup code is beyond the resident stack. DOS AH=31h discards it later.
    mov ax,ds
    mov es,ax
IFDEF LL_UTILITY
    call _ll_main
ELSE
IFDEF LL_DIAGNOSTIC
    call _diag_main
ELSE
    call _agent_main
ENDIF
ENDIF
    cmp _fs_keep,1
    jne fs_not_resident
    mov dx,_fs_paras
    mov ax,3100h
    int 21h
fs_not_resident:
    mov ah,4ch
    int 21h
public _net_open,_net_send,_net_recv,_net_close,_net_ticks
_net_ticks proc near
    push es
    mov ax,40h
    mov es,ax
    mov ax,es:[6ch]
    pop es
    ret
_net_ticks endp
_TEXT ends
FSINIT segment word public 'INITCODE'
_net_open proc near
    push bp
    mov bp,sp
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov bx,offset _netstate
    call ip_connect
    mov ax,0
    jnc open_done
    dec ax
open_done:
    mov byte ptr cs:fs_net_setup,0
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop bp
    ret
_net_open endp
FSINIT ends
_TEXT segment word public 'CODE'
_net_send proc near
    push bp
    mov bp,sp
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov bx,offset _netstate
    mov si,[bp+4]
    mov cx,[bp+6]
    call ip_write
    mov ax,0
    jnc send_done
    dec ax
send_done:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop bp
    ret
_net_send endp
_net_recv proc near
    push bp
    mov bp,sp
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov bx,offset _netstate
    call ip_pump
    mov ax,-1
    cmp word ptr [bx+IP_ERROR],0
    jne recv_done
    mov cx,[bx+IP_RXLEN]
    or cx,cx
    jnz recv_data
    mov ax,0
    cmp word ptr [bx+IP_STATE],4
    jne recv_done
    mov ax,-1
    jmp recv_done
recv_data:
    cmp cx,[bp+6]
    jbe recv_count
    mov cx,[bp+6]
recv_count:
    mov dx,cx
    mov si,[bx+IP_RXPOS]
    add si,bx
    add si,IP_RX
    mov di,[bp+4]
    call ip_copy
    add [bx+IP_RXPOS],dx
    sub [bx+IP_RXLEN],dx
    push dx
    call ip_ack_send
    pop ax
recv_done:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop bp
    ret
_net_recv endp
_net_close proc near
    push bp
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov bx,offset _netstate
    ; Bounded graceful close uses rs_done stack convention.
    call close_adapter
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop bp
    ret
_net_close endp
close_adapter:
    push bp
    push es
    jmp ip_disconnect_entry

IFDEF LL_DIAGNOSTIC
include WRAPPERS.INC
ENDIF
include NNETNE.INC
fs_net_setup db 1
ll_udp_dispatch:
    ; DNS code is discarded after connect: never dispatch UDP into freed memory.
    cmp byte ptr cs:fs_net_setup,0
    je ip_pump_timer
    jmp ip_dns_read

IFDEF LL_DIAGNOSTIC
include ICMP.INC
include DIAGTX.INC
ENDIF
end start
