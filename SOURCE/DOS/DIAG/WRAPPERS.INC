.8086
.data
extrn _diag_stage:word,_diag_arp_tx:word,_diag_arp_rx:word,_diag_rx:word,_diag_dns_tx:word,_diag_tcp_tx:word
extrn _diag_ping_seq:word,_diag_ping_reply:word,_diag_ping_ttl:word,_diag_ping_bytes:word
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
public _diag_mac,_diag_prepare,_diag_ping
_diag_mac proc near
 DPROLOG
 call ip_open
 mov ax,0
 jnc dm_done
 mov ax,[bx+3884]
dm_done:
 DEPILOG
_diag_mac endp
_diag_prepare proc near
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
_diag_prepare endp
_diag_ping proc near
 DPROLOG
 mov word ptr [bx+3884],0
 mov word ptr _diag_ping_reply,0
 mov word ptr _diag_stage,5
 call rs_start_clock
 call ip_zero_frame
 mov word ptr [bx+IP_FRAME+34],0008h
 mov word ptr [bx+IP_FRAME+38],5541h
 mov ax,_diag_ping_seq
 xchg al,ah
 mov [bx+IP_FRAME+40],ax
 lea di,[bx+IP_FRAME+42]
 mov cx,32
 xor al,al
dg_payload:
 mov [di],al
 inc di
 inc al
 loop dg_payload
 lea si,[bx+IP_FRAME+34]
 mov cx,40
 call ip_sum
 xchg al,ah
 mov [bx+IP_FRAME+36],ax
 mov al,1
 mov cx,40
 call ip_header
 call ip_send_frame
 jc dg_bad
dg_wait:
 call ip_pump
 cmp word ptr _diag_ping_reply,1
 je dg_good
 cmp word ptr [bx+3884],0
 jne dg_bad
 call rs_expired
 jnc dg_wait
 mov word ptr [bx+3884],1460
dg_bad:
 mov ax,-1
 jmp dg_done
dg_good:
 xor ax,ax
dg_done:
 DEPILOG
_diag_ping endp

public _diag_decode_test
_diag_decode_test proc near
 DPROLOG
 add bx,1920
 call ne_select_mac
 DEPILOG
_diag_decode_test endp
