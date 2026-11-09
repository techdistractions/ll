.8086
.data
extrn _ll_ntp_request:byte,_ll_ntp_reply:byte,_ll_ntp_port:word,_ll_ntp_active:word,_ll_ntp_received:word
.code
DPROLOG macro
 push bp
 push bx
 push cx
 push dx
 push si
 push di
 push es
 mov bx,offset _netstate
endm
DEPILOG macro
 pop es
 pop di
 pop si
 pop dx
 pop cx
 pop bx
 pop bp
 ret
endm
public _ll_prepare
_ll_prepare proc near
 DPROLOG
 mov word ptr [bx+3884],0
 call ip_open
 jc dp_bad
 call rs_start_clock
 lea si,[bx+20]
 mov cx,[bx+18]
 call ip_resolve_host
 jc dp_bad
 call ip_route
 jc dp_bad
 xor ax,ax
 jmp dp_done
dp_bad:
 mov ax,-1
dp_done:
 DEPILOG
_ll_prepare endp

; Hardware-only probe. Ownership and cleanup are identical to the transport.
public _ll_nic_open,_ll_nic_close
_ll_nic_open proc near
 DPROLOG
 mov di,32
 call ip_raw
 mov [bx+3884],ax
 DEPILOG
_ll_nic_open endp
_ll_nic_close proc near
 DPROLOG
 mov di,40
 call ip_raw
 DEPILOG
_ll_nic_close endp
