; LiteLink SNTP over the existing validated IPv4/NE2000 pump.
public _ll_ntp
_ll_ntp proc near
 DPROLOG
 mov word ptr _ll_ntp_received,0
 mov word ptr _ll_ntp_active,1
 mov word ptr [bx+IP_ERROR],0
 call rs_start_clock
 call ip_zero_frame
 mov word ptr [bx+IP_FRAME+34],01C0h ; local UDP 49153
 mov ax,_ll_ntp_port
 xchg al,ah
 mov [bx+IP_FRAME+36],ax
 mov word ptr [bx+IP_FRAME+38],3800h ; 56 bytes, IPv4 UDP checksum optional
 lea si,_ll_ntp_request
 lea di,[bx+IP_FRAME+42]
 mov cx,48
 call ip_copy
 mov al,17
 mov cx,56
 call ip_header
 call ip_send_frame
 jc lt_bad
lt_wait:
 call ip_pump
 cmp word ptr _ll_ntp_received,1
 je lt_good
 cmp word ptr [bx+IP_ERROR],0
 jne lt_bad
 call rs_expired
 jnc lt_wait
 mov word ptr [bx+IP_ERROR],1460
lt_bad:
 mov ax,-1
 jmp lt_done
lt_good:
 xor ax,ax
lt_done:
 mov word ptr _ll_ntp_active,0
 DEPILOG
_ll_ntp endp
ll_udp_dispatch:
 cmp word ptr _ll_ntp_active,0
 je ip_dns_read
 ; DI points to UDP after variable IPv4 header. IP source/destination checked.
 mov ax,[bx+IP_END]
 add ax,bx
 add ax,IP_FRAME
 sub ax,di
 cmp ax,56
 jb ip_pump_timer
 mov cx,[di+4]
 xchg cl,ch
 cmp cx,56
 jb ip_pump_timer
 cmp cx,ax
 ja ip_pump_timer
 mov bp,cx
 mov ax,_ll_ntp_port
 xchg al,ah
 cmp ax,[di]
 jne ip_pump_timer
 cmp word ptr [di+2],01C0h
 jne ip_pump_timer
 cmp word ptr [di+6],0
 je lt_checksum_ok
 lea si,[bx+IP_FRAME+26]
 mov cx,8
 call ip_sum
 not ax
 add ax,bp
 adc ax,0
 add ax,17
 adc ax,0
 push ax
 mov si,di
 mov cx,bp
 call ip_sum
 not ax
 pop dx
 add ax,dx
 adc ax,0
 cmp ax,-1
 jne ip_pump_timer
lt_checksum_ok:
 lea si,[di+8]
 lea di,_ll_ntp_reply
 mov cx,48
 call ip_copy
 mov word ptr _ll_ntp_received,1
 jmp ip_pump_timer
