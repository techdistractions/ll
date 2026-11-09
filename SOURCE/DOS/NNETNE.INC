; Native NETWORK NE2000 transport. One connection, 8086, polling, no imports.
; Shared HTTP/receive/event semantics are compiled from NNETRS.INC.
; Runtime-selected NIC and IPv4; no implicit physical LAN configuration.
include NNETDEF.INC
; No application-visible packet API, allocation, IRQ, TSR or external stack.
IP_SEQ EQU 3840
IP_ACK EQU 3844
IP_TXSEQ EQU 3848
IP_TXLEN EQU 3852
IP_FLAGS EQU 3854
IP_STATE EQU 3856
IP_RXLEN EQU 3858
IP_RXPOS EQU 3860
IP_PEER EQU 3862
IP_MAC EQU 3866
IP_ID EQU 3872
IP_PORT EQU 3874
IP_WINDOW EQU 3876
IP_RETRY EQU 3878
IP_TIME EQU 3880
IP_WAIT EQU 3882
IP_ERROR EQU 3884
IP_BUSY EQU 3886
IP_DNS EQU 3904
IP_END EQU 3890
IP_TCP EQU 3892
IP_NLEN EQU 3894
IP_SAVED EQU 3896
IP_RTO EQU 3898
IP_RX EQU 4096
IP_TX EQU 4608
; RX frame processing and TX serialization never overlap. Reuse the NIC's
; consumed frame buffer rather than crossing the compiler's global DATA area.
IP_FRAME EQU 2288
; DNS completes before TCP owns the transmit buffer.
IP_QUERY EQU 4608
net_ne_start:
    cmp di,37
    je ip_resolve_entry
    cmp di,36
    je ip_dial
    cmp di,40
    je ip_cleanup
    cmp di,46
    je ip_checksum_entry
    cmp di,47
    je ip_ticks_entry
    jmp net_rs_start
ip_cleanup:
    mov word ptr [bx+IP_STATE],0
    mov word ptr [bx+IP_WAIT],0
    mov word ptr [bx+IP_RXLEN],0
    push di
    mov di,40
    call ip_raw
    pop di
    ret
ip_ticks_entry:
    call ip_ticks
    xor dx,dx
    ret
ip_checksum_entry:
    call ip_sum
    xor dx,dx
    ret
; Relocated by NOWNNET against the common runtime's nr_error.
nr_error:
    mov [bx+IP_ERROR],ax
    ret
ip_error_jump dw 0
nr_ui_wait:
IFDEF LITELINK_UI
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push es
    call _ll_ui_idle
    pop es
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
ENDIF
    ret
ip_ui_jump dw 0
NETWORK_NE_TEMPLATE EQU 1
include NNETRS.INC

ip_raw:
    push bx
    add bx,1920
    call raw_nic_start
    pop bx
    ret
ip_open:
IFDEF LL_DIAGNOSTIC
    mov word ptr _diag_stage,1
ENDIF
    push ax
    push cx
    push si
    push di
    cmp word ptr [bx+1928],1
    je ip_open_ok
    mov di,32
    call ip_raw
    or ax,ax
    jnz ip_open_bad
ip_open_ok:
    pop di
    pop si
    pop cx
    pop ax
    clc
    ret
ip_open_bad:
    call nr_error
    pop di
    pop si
    pop cx
    pop ax
    stc
    ret
ip_ticks:
    push es
    mov ax,40h
    mov es,ax
    mov ax,es:[6Ch]
    pop es
    ret
; Checksum in host word order. BX and DX preserved; SI/CX consumed.
ip_sum:
    push dx
    push di
    xor dx,dx
    mov di,cx
    shr cx,1
    jcxz ip_sum_odd
ip_sum_next:
    lodsw
    xchg al,ah
    add dx,ax
    adc dx,0
    loop ip_sum_next
ip_sum_odd:
    test di,1
    jz ip_sum_done
    lodsb
    mov ah,al
    xor al,al
    add dx,ax
    adc dx,0
ip_sum_done:
    mov ax,dx
    not ax
    pop di
    pop dx
    ret
ip_copy:
    push es
    push ds
    pop es
    rep movsb
    pop es
    ret
ip_zero_frame:
    push es
    push ds
    pop es
    lea di,[bx+IP_FRAME]
    mov cx,1514
    xor ax,ax
    rep stosb
    pop es
    ret
ip_ether:
    lea si,[bx+IP_MAC]
    lea di,[bx+IP_FRAME]
    mov cx,6
    call ip_copy
    lea si,[bx+CFG_HWMAC]
    mov cx,6
ip_ether_mac:
    mov al,[si]
    mov [di],al
    inc si
    inc di
    loop ip_ether_mac
    mov word ptr [bx+IP_FRAME+12],0008h
    ret
ip_send_frame:
    lea si,[bx+IP_FRAME]
    mov di,34
    call ip_raw
    or ax,ax
    jz ip_send_ok
    cmp ax,428
    je ip_send_busy
    mov [bx+IP_ERROR],ax
ip_send_busy:
    stc
    ret
ip_send_ok:
    clc
    ret
; Gateway ARP request. Also services ARP requests for our local address.
ip_arp_send:
IFDEF LL_DIAGNOSTIC
    inc word ptr _diag_arp_tx
ENDIF
    call ip_zero_frame
    call ip_ether
    mov word ptr [bx+IP_FRAME],-1
    mov word ptr [bx+IP_FRAME+2],-1
    mov word ptr [bx+IP_FRAME+4],-1
    mov word ptr [bx+IP_FRAME+12],0608h
    mov word ptr [bx+IP_FRAME+14],0100h
    mov word ptr [bx+IP_FRAME+16],0008h
    mov word ptr [bx+IP_FRAME+18],0406h
    mov word ptr [bx+IP_FRAME+20],0100h
    lea si,[bx+IP_FRAME+6]
    lea di,[bx+IP_FRAME+22]
    mov cx,6
    call ip_copy
    mov ax,[bx+CFG_BIN]
    mov [bx+IP_FRAME+28],ax
    mov ax,[bx+CFG_BIN+2]
    mov [bx+IP_FRAME+30],ax
    mov ax,[bx+CFG_ROUTE]
    mov [bx+IP_FRAME+38],ax
    mov ax,[bx+CFG_ROUTE+2]
    mov [bx+IP_FRAME+40],ax
    mov cx,42
    jmp ip_send_frame
; AL protocol, CX transport length; IP_PEER destination, frame transport at34.
ip_header:
    push ax
    push cx
    call ip_ether
    pop cx
    pop ax
    mov byte ptr [bx+IP_FRAME+14],45h
    mov byte ptr [bx+IP_FRAME+23],al
    mov byte ptr [bx+IP_FRAME+22],64
    add cx,20
    mov ax,cx
    xchg al,ah
    mov [bx+IP_FRAME+16],ax
    inc word ptr [bx+IP_ID]
    mov ax,[bx+IP_ID]
    xchg al,ah
    mov [bx+IP_FRAME+18],ax
    mov word ptr [bx+IP_FRAME+20],0040h
    mov word ptr [bx+IP_FRAME+24],0
    mov ax,[bx+CFG_BIN]
    mov [bx+IP_FRAME+26],ax
    mov ax,[bx+CFG_BIN+2]
    mov [bx+IP_FRAME+28],ax
    mov ax,[bx+IP_PEER]
    mov [bx+IP_FRAME+30],ax
    mov ax,[bx+IP_PEER+2]
    mov [bx+IP_FRAME+32],ax
    push cx
    lea si,[bx+IP_FRAME+14]
    mov cx,20
    call ip_sum
    xchg al,ah
    mov [bx+IP_FRAME+24],ax
    pop cx
    add cx,14
    ret
; TCP checksum includes pseudoheader; IP header must already be filled.
ip_tcp_sum:
    lea si,[bx+IP_FRAME+26]
    push cx
    mov cx,8
    call ip_sum
    not ax
    mov dx,ax
    pop cx
    add dx,cx
    adc dx,0
    add dx,6
    adc dx,0
    push dx
    lea si,[bx+IP_FRAME+34]
    call ip_sum
    not ax
    pop dx
    add ax,dx
    adc ax,0
    not ax
    ret
; Retransmit saved TCP segment. Flags and sequence remain stable until ACK.
ip_tcp_send:
IFDEF LL_DIAGNOSTIC
    inc word ptr _diag_tcp_tx
ENDIF
    call ip_zero_frame
    mov ax,[bx+IP_PORT]
    xchg al,ah
    mov [bx+IP_FRAME+34],ax
    mov ax,[bx+4]
    xchg al,ah
    mov [bx+IP_FRAME+36],ax
    mov ax,[bx+IP_TXSEQ+2]
    xchg al,ah
    mov [bx+IP_FRAME+38],ax
    mov ax,[bx+IP_TXSEQ]
    xchg al,ah
    mov [bx+IP_FRAME+40],ax
    mov ax,[bx+IP_ACK+2]
    xchg al,ah
    mov [bx+IP_FRAME+42],ax
    mov ax,[bx+IP_ACK]
    xchg al,ah
    mov [bx+IP_FRAME+44],ax
    mov byte ptr [bx+IP_FRAME+46],50h
    mov ax,[bx+IP_FLAGS]
    mov [bx+IP_FRAME+47],al
    ; One cached segment: partial free bytes are not appendable space.
    xor ax,ax
    cmp word ptr [bx+IP_RXLEN],0
    jne ip_receive_window_ready
    mov ax,512
ip_receive_window_ready:
    xchg al,ah
    mov [bx+IP_FRAME+48],ax
    mov cx,[bx+IP_TXLEN]
    lea si,[bx+IP_TX]
    lea di,[bx+IP_FRAME+54]
    call ip_copy
    mov cx,[bx+IP_TXLEN]
    add cx,20
    test word ptr [bx+IP_FLAGS],2
    jz ip_tcp_no_options
    mov byte ptr [bx+IP_FRAME+46],60h
    mov word ptr [bx+IP_FRAME+54],0402h
    mov word ptr [bx+IP_FRAME+56],0002h ; MSS 512
    add cx,4
ip_tcp_no_options:
    push cx
    mov al,6
    call ip_header
    pop cx
    push cx
    call ip_tcp_sum
    xchg al,ah
    mov [bx+IP_FRAME+50],ax
    pop cx
    add cx,34
    call ip_send_frame
    ret
; ACK cannot replace an outstanding reliable segment.
ip_ack_send:
    push word ptr [bx+IP_TXSEQ]
    push word ptr [bx+IP_TXSEQ+2]
    push word ptr [bx+IP_TXLEN]
    push word ptr [bx+IP_FLAGS]
    mov ax,[bx+IP_SEQ]
    mov [bx+IP_TXSEQ],ax
    mov ax,[bx+IP_SEQ+2]
    mov [bx+IP_TXSEQ+2],ax
    mov word ptr [bx+IP_TXLEN],0
    mov word ptr [bx+IP_FLAGS],10h
    call ip_tcp_send
    pop word ptr [bx+IP_FLAGS]
    pop word ptr [bx+IP_TXLEN]
    pop word ptr [bx+IP_TXSEQ+2]
    pop word ptr [bx+IP_TXSEQ]
    ret
; Copy one received frame to shared scratch. All packet lengths are checked.
ip_pump:
    push ax
    push cx
    push dx
    push si
    push di
    push bp
    call nr_ui_wait
    cmp word ptr [bx+NR_CANCEL],0
    je ip_pump_not_cancelled
    mov word ptr [bx+IP_ERROR],995
    jmp ip_pump_out
ip_pump_not_cancelled:
    cmp word ptr [bx+IP_BUSY],0
    jne ip_pump_out
    mov word ptr [bx+IP_BUSY],1
    mov word ptr [bx+IP_NLEN],1514
    mov word ptr [bx+IP_NLEN+2],0
    lea ax,[bx+IP_NLEN]
    mov di,35
    call ip_raw
    or ax,ax
    jz ip_pump_received
    mov [bx+IP_ERROR],ax
    jmp ip_pump_timer
ip_pump_received:
IFDEF LL_DIAGNOSTIC
    jcxz diag_empty_rx
    inc word ptr _diag_rx
diag_empty_rx:
ENDIF
    cmp cx,42
    jb ip_pump_timer
    mov [bx+IP_NLEN],cx
    lea di,[bx+IP_FRAME]
    call ip_copy
    cmp word ptr [bx+IP_FRAME+12],0608h
    je ip_arp_read
    cmp word ptr [bx+IP_FRAME+12],0008h
    jne ip_pump_timer
    mov al,[bx+IP_FRAME+14]
    cmp al,45h
    jb ip_pump_timer
    cmp al,4Fh
    ja ip_pump_timer
    and ax,15
    shl ax,1
    shl ax,1
    mov bp,ax
    add ax,14
    cmp ax,[bx+IP_NLEN]
    ja ip_pump_timer
    push ax
    mov cx,bp
    lea si,[bx+IP_FRAME+14]
    call ip_sum
    pop di
    or ax,ax
    jnz ip_pump_timer
    mov ax,[bx+IP_FRAME+16]
    xchg al,ah
    cmp ax,bp
    jb ip_pump_timer
    add ax,14
    cmp ax,[bx+IP_NLEN]
    ja ip_pump_timer
    mov [bx+IP_END],ax
    ; MF and fragment offset are unsupported; DF is permitted.
    test word ptr [bx+IP_FRAME+20],0FF3Fh
    jnz ip_pump_timer
    mov ax,[bx+CFG_BIN]
    cmp [bx+IP_FRAME+30],ax
    jne ip_pump_timer
    mov ax,[bx+CFG_BIN+2]
    cmp [bx+IP_FRAME+32],ax
    jne ip_pump_timer
    mov ax,[bx+IP_PEER]
    cmp ax,[bx+IP_FRAME+26]
    jne ip_pump_timer
    mov ax,[bx+IP_PEER+2]
    cmp ax,[bx+IP_FRAME+28]
    jne ip_pump_timer
    add di,bx
    add di,IP_FRAME
    mov [bx+IP_TCP],di
IFDEF LL_DIAGNOSTIC
    cmp byte ptr [bx+IP_FRAME+23],1
    je diag_icmp_read
ENDIF
    cmp byte ptr [bx+IP_FRAME+23],17
IFDEF LL_UTILITY
    je ll_udp_dispatch
ELSE
    je ip_dns_read
ENDIF
    cmp byte ptr [bx+IP_FRAME+23],6
    jne ip_pump_timer
    jmp ip_tcp_read
ip_arp_read:
IFDEF LL_DIAGNOSTIC
    inc word ptr _diag_arp_rx
    call diag_save_arp
ENDIF
    cmp word ptr [bx+IP_FRAME+14],0100h
    jne ip_pump_timer
    cmp word ptr [bx+IP_FRAME+16],0008h
    jne ip_pump_timer
    cmp word ptr [bx+IP_FRAME+18],0406h
    jne ip_pump_timer
    mov ax,[bx+CFG_ROUTE]
    cmp [bx+IP_FRAME+28],ax
    jne ip_arp_request
    mov ax,[bx+CFG_ROUTE+2]
    cmp [bx+IP_FRAME+30],ax
    jne ip_arp_request
    lea si,[bx+IP_FRAME+22]
    lea di,[bx+IP_MAC]
    mov cx,6
    call ip_copy
ip_arp_request:
    cmp word ptr [bx+IP_FRAME+20],0100h
    jne ip_pump_timer
    mov ax,[bx+CFG_BIN]
    cmp [bx+IP_FRAME+38],ax
    jne ip_pump_timer
    mov ax,[bx+CFG_BIN+2]
    cmp [bx+IP_FRAME+40],ax
    jne ip_pump_timer
    lea si,[bx+IP_FRAME+22]
    lea di,[bx+IP_FRAME]
    mov cx,6
    call ip_copy
    lea si,[bx+IP_FRAME+22]
    lea di,[bx+IP_FRAME+32]
    mov cx,10
    call ip_copy
    lea si,[bx+CFG_HWMAC]
    lea di,[bx+IP_FRAME+6]
    mov cx,6
ip_arp_mac_reply:
    mov al,[si]
    mov [di],al
    mov [di+16],al
    inc di
    inc si
    loop ip_arp_mac_reply
    mov word ptr [bx+IP_FRAME+20],0200h
    mov ax,[bx+CFG_BIN]
    mov [bx+IP_FRAME+28],ax
    mov ax,[bx+CFG_BIN+2]
    mov [bx+IP_FRAME+30],ax
    mov cx,42
    call ip_send_frame
    jmp ip_pump_timer
ip_tcp_read:
    mov ax,[bx+IP_END]
    sub ax,14
    sub ax,bp
    cmp ax,20
    jb ip_pump_timer
    mov bp,ax
    mov ax,[bx+IP_PORT]
    xchg al,ah
    cmp ax,[di+2]
    jne ip_pump_timer
    mov ax,[bx+4]
    xchg al,ah
    cmp ax,[di]
    jne ip_pump_timer
    ; Verify pseudoheader and segment checksum with variable IP options.
    lea si,[bx+IP_FRAME+26]
    mov cx,8
    call ip_sum
    not ax
    add ax,bp
    adc ax,0
    add ax,6
    adc ax,0
    push ax
    mov si,[bx+IP_TCP]
    mov cx,bp
    call ip_sum
    not ax
    pop dx
    add ax,dx
    adc ax,0
    cmp ax,-1
    jne ip_pump_timer
    mov di,[bx+IP_TCP]
    xor ax,ax
    mov al,[di+12]
    and al,0F0h
    shr ax,1
    shr ax,1
    cmp ax,20
    jb ip_pump_timer
    cmp ax,bp
    ja ip_pump_timer
    sub bp,ax
    add ax,di
    mov [bx+IP_SAVED],ax ; payload address
    mov ax,[di+14]
    xchg al,ah
    mov [bx+IP_WINDOW],ax
    cmp word ptr [bx+IP_STATE],1
    je ip_tcp_synack
    mov ax,[di+6]
    xchg al,ah
    cmp ax,[bx+IP_ACK]
    jne ip_tcp_repeat
    mov ax,[di+4]
    xchg al,ah
    cmp ax,[bx+IP_ACK+2]
    jne ip_tcp_repeat
    test byte ptr [di+13],4
    jz ip_tcp_ack
    mov word ptr [bx+IP_ERROR],446
    mov word ptr [bx+IP_STATE],0
    jmp ip_pump_timer
ip_tcp_synack:
    test byte ptr [di+13],10h
    jz ip_pump_timer
    mov ax,[di+10]
    xchg al,ah
    cmp ax,[bx+IP_SEQ]
    jne ip_pump_timer
    mov ax,[di+8]
    xchg al,ah
    cmp ax,[bx+IP_SEQ+2]
    jne ip_pump_timer
    test byte ptr [di+13],4
    jnz ip_tcp_refused
    test byte ptr [di+13],2
    jz ip_pump_timer
    mov ax,[di+6]
    xchg al,ah
    mov [bx+IP_ACK],ax
    mov ax,[di+4]
    xchg al,ah
    mov [bx+IP_ACK+2],ax
    add word ptr [bx+IP_ACK],1
    adc word ptr [bx+IP_ACK+2],0
    mov word ptr [bx+IP_WAIT],0
    mov word ptr [bx+IP_STATE],2
    mov word ptr [bx+14],0
    call ip_ack_send
    jmp ip_pump_timer
ip_tcp_refused:
    mov word ptr [bx+IP_ERROR],446
    jmp ip_pump_timer
ip_tcp_repeat:
    ; Duplicate/out-of-order data is not exposed twice. Repeat current ACK.
    test byte ptr [di+13],7
    jnz ip_tcp_repeat_ack
    or bp,bp
    jz ip_pump_timer
ip_tcp_repeat_ack:
    call ip_ack_send
    jmp ip_pump_timer
ip_tcp_ack:
    test byte ptr [di+13],10h
    jz ip_pump_timer
    mov ax,[di+10]
    xchg al,ah
    cmp ax,[bx+IP_SEQ]
    jne ip_tcp_data
    mov ax,[di+8]
    xchg al,ah
    cmp ax,[bx+IP_SEQ+2]
    jne ip_tcp_data
    mov word ptr [bx+IP_WAIT],0
    mov word ptr [bx+14],0
ip_tcp_data:
    cmp bp,512
    ja ip_tcp_repeat_ack
    or bp,bp
    jz ip_tcp_fin
    cmp word ptr [bx+IP_RXLEN],0
    jne ip_tcp_repeat_ack
    mov si,[bx+IP_SAVED]
    lea di,[bx+IP_RX]
    mov cx,bp
    call ip_copy
    mov [bx+IP_RXLEN],bp
    mov word ptr [bx+IP_RXPOS],0
    add [bx+IP_ACK],bp
    adc word ptr [bx+IP_ACK+2],0
ip_tcp_fin:
    mov di,[bx+IP_TCP]
    test byte ptr [di+13],1
    jz ip_tcp_ack_data
    add word ptr [bx+IP_ACK],1
    adc word ptr [bx+IP_ACK+2],0
    mov word ptr [bx+IP_STATE],4
ip_tcp_ack_data:
    or bp,bp
    jnz ip_tcp_repeat_ack
    cmp word ptr [bx+IP_STATE],4
    je ip_tcp_repeat_ack
ip_pump_timer:
    cmp word ptr [bx+IP_WAIT],0
    je ip_pump_unlock
    call ip_ticks
    sub ax,[bx+IP_TIME]
    cmp ax,[bx+IP_RTO]
    jb ip_pump_unlock
    cmp word ptr [bx+IP_RETRY],4
    jae ip_pump_timeout
    inc word ptr [bx+IP_RETRY]
    shl word ptr [bx+IP_RTO],1
    call ip_ticks
    mov [bx+IP_TIME],ax
    call ip_tcp_send
    jmp ip_pump_unlock
ip_pump_timeout:
    mov word ptr [bx+IP_ERROR],1460
    mov word ptr [bx+IP_WAIT],0
ip_pump_unlock:
    mov word ptr [bx+IP_BUSY],0
ip_pump_out:
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop ax
    ret
; Literal parser, or bounded DNS A query to the configured native DNS server.
ip_resolve_host:
    mov word ptr [bx+IP_PEER],0
    mov word ptr [bx+IP_PEER+2],0
    lea di,[bx+IP_PEER]
    xor bp,bp
    xor dx,dx
    push si
    push cx
ip_literal_next:
    lodsb
    cmp al,'.'
    je ip_literal_dot
    sub al,'0'
    cmp al,9
    ja ip_literal_dns
    xor ah,ah
    push ax
    mov ax,bp
    push dx
    mov dx,10
    mul dx
    pop dx
    mov bp,ax
    pop ax
    add bp,ax
    cmp bp,255
    ja ip_literal_dns
    inc dh
    cmp dh,3
    ja ip_literal_dns
    loop ip_literal_next
    cmp dl,3
    jne ip_literal_dns
    or dh,dh
    jz ip_literal_dns
    mov ax,bp
    mov [di],al
    pop cx
    pop si
    clc
    ret
ip_literal_dot:
    or dh,dh
    jz ip_literal_dns
    inc dl
    cmp dl,3
    ja ip_literal_dns
    mov ax,bp
    mov [di],al
    inc di
    xor bp,bp
    xor dh,dh
    loop ip_literal_next
ip_literal_dns:
    pop cx
    pop si
    cmp cx,253
    ja ip_connect_invalid
    or cx,cx
    jz ip_connect_invalid
    mov ax,[bx+CFG_BIN+12]
    mov dx,[bx+CFG_BIN+14]
    mov [bx+IP_PEER],ax
    mov [bx+IP_PEER+2],dx
    or ax,dx
    jz ip_connect_invalid
    push si
    push cx
    call ip_route
    pop cx
    pop si
    jc ip_connect_error
    mov word ptr [bx+IP_DNS],0
    ; Query is retained separately for retransmission.
    push cx
    push si
    lea di,[bx+IP_QUERY]
    xor ax,ax
    mov cx,512
    push es
    push ds
    pop es
    rep stosb
    pop es
    pop si
    pop cx
    call ip_ticks
    inc ax
    mov [bx+IP_QUERY],ax
    mov word ptr [bx+IP_QUERY+2],0001h
    mov word ptr [bx+IP_QUERY+4],0100h
    lea di,[bx+IP_QUERY+12]
    mov bp,di
    inc di
    xor dx,dx
ip_dns_label:
    lodsb
    cmp al,'.'
    je ip_dns_dot
    cmp al,33
    jb ip_connect_invalid
    cmp al,127
    jae ip_connect_invalid
    mov [di],al
    inc di
    inc dl
    cmp dl,63
    ja ip_connect_invalid
    loop ip_dns_label
    or dl,dl
    jz ip_connect_invalid
    mov ds:[bp],dl
    mov byte ptr [di],0
    inc di
    mov word ptr [di],0100h
    mov word ptr [di+2],0100h
    add di,4
    sub di,bx
    sub di,IP_QUERY
    mov [bx+IP_DNS+4],di
    mov word ptr [bx+IP_DNS+6],0
ip_dns_retry:
IFDEF LL_DIAGNOSTIC
    mov word ptr _diag_stage,3
    inc word ptr _diag_dns_tx
ENDIF
    call ip_zero_frame
    mov word ptr [bx+IP_FRAME+34],00C0h ; local UDP 49152
    mov word ptr [bx+IP_FRAME+36],3500h
    mov ax,[bx+IP_DNS+4]
    add ax,8
    mov cx,ax
    xchg al,ah
    mov [bx+IP_FRAME+38],ax
    push cx
    lea si,[bx+IP_QUERY]
    lea di,[bx+IP_FRAME+42]
    mov cx,[bx+IP_DNS+4]
    call ip_copy
    pop cx
    mov al,17
    call ip_header
    call ip_send_frame
    call ip_ticks
    mov [bx+IP_DNS+2],ax
ip_dns_wait:
    call ip_pump
    cmp word ptr [bx+IP_ERROR],0
    jne ip_connect_error
    cmp word ptr [bx+IP_DNS],1
    je ip_resolved
    call rs_expired
    jc ip_connect_timeout
    call ip_ticks
    sub ax,[bx+IP_DNS+2]
    cmp ax,36
    jb ip_dns_wait
    inc word ptr [bx+IP_DNS+6]
    cmp word ptr [bx+IP_DNS+6],4
    jb ip_dns_retry
    jmp ip_connect_timeout
ip_dns_dot:
    or dl,dl
    jz ip_connect_invalid
    mov ds:[bp],dl
    mov bp,di
    inc di
    xor dl,dl
    dec cx
    jnz ip_dns_label
    jmp ip_connect_invalid
ip_dns_read:
    mov ax,[bx+IP_END]
    add ax,bx
    add ax,IP_FRAME
    mov bp,ax
    mov ax,di
    add ax,20
    cmp ax,bp
    ja ip_pump_timer
    cmp word ptr [di],3500h
    jne ip_pump_timer
    cmp word ptr [di+2],00C0h
    jne ip_pump_timer
    mov ax,[di+4]
    xchg al,ah
    cmp ax,20
    jb ip_pump_timer
    add ax,di
    cmp ax,bp
    ja ip_pump_timer
    mov bp,ax
    add di,8
    mov ax,[bx+IP_QUERY]
    cmp ax,[di]
    jne ip_pump_timer
    test byte ptr [di+2],80h
    jz ip_pump_timer
    test byte ptr [di+2],2
    jnz ip_pump_timer ; truncated response, no out-of-bounds fallback
    test byte ptr [di+3],0Fh
    jnz ip_dns_failure
    cmp word ptr [di+4],0100h
    jne ip_pump_timer
    mov dx,[di+6]
    xchg dl,dh
    cmp dx,32
    ja ip_pump_timer
    add di,12
    call ip_dns_skip
    jc ip_pump_timer
    add di,4
ip_dns_answer:
    or dx,dx
    jz ip_dns_failure
    dec dx
    call ip_dns_skip
    jc ip_pump_timer
    mov ax,di
    add ax,10
    cmp ax,bp
    ja ip_pump_timer
    mov cx,[di+8]
    xchg cl,ch
    add ax,cx
    jc ip_pump_timer
    cmp ax,bp
    ja ip_pump_timer
    cmp word ptr [di],0100h
    jne ip_dns_next
    cmp word ptr [di+2],0100h
    jne ip_dns_next
    cmp cx,4
    jne ip_pump_timer
    mov ax,[di+10]
    mov [bx+IP_PEER],ax
    mov ax,[di+12]
    mov [bx+IP_PEER+2],ax
    mov word ptr [bx+IP_DNS],1
    jmp ip_pump_timer
ip_dns_next:
    mov di,ax
    jmp ip_dns_answer
ip_dns_failure:
    mov word ptr [bx+IP_ERROR],436
    jmp ip_pump_timer
; Skip encoded DNS name without following pointers. Bounds on every byte.
ip_dns_skip:
    cmp di,bp
    jae ip_dns_skip_bad
    mov al,[di]
    inc di
    or al,al
    jz ip_dns_skip_ok
    cmp al,0C0h
    jae ip_dns_pointer
    cmp al,63
    ja ip_dns_skip_bad
    xor ah,ah
    add di,ax
    jmp ip_dns_skip
ip_dns_pointer:
    cmp di,bp
    jae ip_dns_skip_bad
    inc di
ip_dns_skip_ok:
    clc
    ret
ip_dns_skip_bad:
    stc
    ret
; Pick a directly connected peer, otherwise the configured on-LAN gateway.
; Re-resolve ARP when DNS and TCP have different next hops.
ip_route:
IFDEF LL_DIAGNOSTIC
    mov word ptr _diag_stage,2
ENDIF
    mov ax,[bx+IP_PEER]
    xor ax,[bx+CFG_BIN]
    and ax,[bx+CFG_BIN+4]
    mov dx,ax
    mov ax,[bx+IP_PEER+2]
    xor ax,[bx+CFG_BIN+2]
    and ax,[bx+CFG_BIN+6]
    or dx,ax
    jnz ip_route_gateway
    mov ax,[bx+IP_PEER]
    mov dx,[bx+IP_PEER+2]
    jmp ip_route_selected
ip_route_gateway:
    mov ax,[bx+CFG_BIN+8]
    mov dx,[bx+CFG_BIN+10]
    push ax
    or ax,dx
    pop ax
    jz ip_route_invalid
    push ax
    push dx
    xor ax,[bx+CFG_BIN]
    and ax,[bx+CFG_BIN+4]
    xor dx,[bx+CFG_BIN+2]
    and dx,[bx+CFG_BIN+6]
    or ax,dx
    pop dx
    pop ax
    jnz ip_route_invalid
ip_route_selected:
    mov [bx+CFG_ROUTE],ax
    mov [bx+CFG_ROUTE+2],dx
    mov word ptr [bx+IP_MAC],0
    mov bp,4
ip_connect_arp:
    push bp
    call ip_arp_send
    call ip_ticks
    mov [bx+IP_TIME],ax
ip_connect_arp_wait:
    call ip_pump
    cmp word ptr [bx+IP_ERROR],0
    jne ip_connect_arp_error
    cmp word ptr [bx+IP_MAC],0
    jne ip_connect_arp_ready
    call rs_expired
    jc ip_connect_arp_timeout
    call ip_ticks
    sub ax,[bx+IP_TIME]
    cmp ax,36
    jb ip_connect_arp_wait
    pop bp
    dec bp
    jnz ip_connect_arp
    jmp ip_route_timeout
ip_connect_arp_error:
    pop bp
    jmp ip_route_error
ip_connect_arp_timeout:
    pop bp
    jmp ip_route_timeout
ip_connect_arp_ready:
    pop bp
    clc
    ret
ip_route_timeout:
    mov word ptr [bx+IP_ERROR],1460
ip_route_error:
    stc
    ret
ip_route_invalid:
    mov word ptr [bx+IP_ERROR],5
    stc
    ret
; Public Resolve uses the same UDP/DNS path as Connect, then frees the NIC.
ip_resolve_entry:
    push bp
    push es
    call ip_open
    jc ip_resolve_close
    call rs_start_clock
    jc ip_resolve_close
    mov word ptr [bx+IP_ERROR],0
    mov word ptr [bx+IP_STATE],0
    mov word ptr [bx+IP_WAIT],0
    call ip_resolve_host
    jc ip_resolve_close
    mov di,33
    call ip_raw
    lea si,[bx+IP_PEER]
    lea di,[bx+1664]
    push ds
    pop es
    mov cx,4
ip_resolve_octet:
    lodsb
    xor ah,ah
    push cx
    push si
    call ip_decimal
    pop si
    pop cx
    dec cx
    jz ip_resolve_text_ready
    mov byte ptr [di],'.'
    inc di
    jmp ip_resolve_octet
ip_resolve_text_ready:
    lea si,[bx+1664]
    mov cx,di
    sub cx,si
    mov byte ptr [di],0
    xor dx,dx
    pop es
    pop bp
    ret
ip_resolve_close:
    mov di,33
    call ip_raw
    xor cx,cx
    lea si,[bx+1664]
    pop es
    pop bp
    ret
ip_decimal:
    xor cx,cx
    mov bp,10
ip_decimal_div:
    xor dx,dx
    div bp
    push dx
    inc cx
    or ax,ax
    jnz ip_decimal_div
ip_decimal_digit:
    pop ax
    add al,'0'
    stosb
    loop ip_decimal_digit
    ret
ip_connect_entry:
    call ip_connect
    jc rs_empty
    jmp rs_done
ip_http_connect:
    cmp word ptr [bx+4],0
    jne ip_connect
    mov word ptr [bx+4],80
    jmp ip_connect
ip_dial:
    mov ax,445
    call nr_error
    xor cx,cx
    ret
ip_connect:
    cmp word ptr [bx+8],1
    je ip_resolved
    call ip_open
    jc ip_connect_return
    cmp word ptr [bx+4],0
    je ip_connect_invalid
    mov word ptr [bx+IP_ERROR],0
    mov word ptr [bx+IP_STATE],0
    mov word ptr [bx+IP_WAIT],0
    mov word ptr [bx+IP_RXLEN],0
    mov word ptr [bx+IP_MAC],0
    mov word ptr [bx+10],0
    call rs_start_clock
    jc ip_connect_return
    lea si,[bx+20]
    mov cx,[bx+18]
    call ip_resolve_host
    jc ip_connect_fail_close
    call ip_route
    jc ip_connect_error
    call ip_ticks
    add ax,49152
    mov [bx+IP_PORT],ax
    mov [bx+IP_SEQ],ax
    mov [bx+IP_TXSEQ],ax
    mov word ptr [bx+IP_SEQ+2],1
    mov word ptr [bx+IP_TXSEQ+2],1
    add word ptr [bx+IP_SEQ],1
    adc word ptr [bx+IP_SEQ+2],0
    mov word ptr [bx+IP_ACK],0
    mov word ptr [bx+IP_ACK+2],0
    mov word ptr [bx+IP_TXLEN],0
IFDEF LL_DIAGNOSTIC
    mov word ptr _diag_stage,4
ENDIF
    mov word ptr [bx+IP_FLAGS],2
    mov word ptr [bx+IP_STATE],1
    call ip_reliable
ip_connect_wait:
    call ip_pump
    cmp word ptr [bx+IP_ERROR],0
    jne ip_connect_error
    cmp word ptr [bx+IP_STATE],2
    je ip_connect_ready
    call rs_expired
    jnc ip_connect_wait
ip_connect_timeout:
    mov word ptr [bx+IP_ERROR],1460
ip_connect_error:
    mov di,33
    call ip_raw
    mov ax,[bx+IP_ERROR]
    call nr_error
    mov word ptr [bx+IP_STATE],0
    mov word ptr [bx+IP_WAIT],0
    stc
    ret
ip_connect_fail_close:
    mov di,33
    call ip_raw
    stc
    ret
ip_connect_invalid:
    mov di,33
    call ip_raw
    mov ax,5
    call nr_error
    stc
    ret
ip_connect_ready:
    mov word ptr [bx+352],1
    call rs_set_connected
ip_resolved:
    clc
ip_connect_return:
    ret
ip_reliable:
    mov word ptr [bx+IP_WAIT],1
    mov word ptr [bx+IP_RETRY],0
    mov word ptr [bx+IP_RTO],18
    call ip_ticks
    mov [bx+IP_TIME],ax
    call ip_tcp_send
    ret
ip_write:
    cmp word ptr [bx+IP_STATE],2
    jne ip_write_closed
    push si
    push cx
    call rs_start_clock
    jc ip_write_fail
ip_write_loop:
    or cx,cx
    jz ip_write_ok
    push si
    push cx
ip_write_wait:
    call ip_pump
    cmp word ptr [bx+IP_ERROR],0
    jne ip_write_error_pop
    cmp word ptr [bx+IP_WAIT],0
    jne ip_write_expired
    cmp word ptr [bx+IP_WINDOW],0
    jne ip_write_space
ip_write_expired:
    call rs_expired
    jnc ip_write_wait
    mov word ptr [bx+IP_ERROR],1460
ip_write_error_pop:
    pop cx
    pop si
    jmp ip_write_error
ip_write_space:
    pop cx
    pop si
    mov ax,cx
    cmp ax,512
    jbe ip_write_window
    mov ax,512
ip_write_window:
    cmp ax,[bx+IP_WINDOW]
    jbe ip_write_copy
    mov ax,[bx+IP_WINDOW]
ip_write_copy:
    push cx
    mov cx,ax
    mov [bx+IP_TXLEN],ax
    lea di,[bx+IP_TX]
    call ip_copy
    pop cx
    sub cx,ax
    mov ax,[bx+IP_SEQ]
    mov [bx+IP_TXSEQ],ax
    mov ax,[bx+IP_SEQ+2]
    mov [bx+IP_TXSEQ+2],ax
    mov ax,[bx+IP_TXLEN]
    mov [bx+14],ax
    add [bx+IP_SEQ],ax
    adc word ptr [bx+IP_SEQ+2],0
    mov word ptr [bx+IP_FLAGS],18h
    push si
    push cx
    call ip_reliable
    pop cx
    pop si
    jmp ip_write_loop
ip_write_error:
    mov ax,[bx+IP_ERROR]
    call nr_error
ip_write_fail:
    pop cx
    pop si
    stc
    ret
ip_write_ok:
    pop cx
    pop si
    mov ax,cx
    clc
    ret
ip_write_closed:
    mov ax,57
    call nr_error
    stc
    ret
ip_read:
    call ip_pump
    cmp word ptr [bx+IP_ERROR],0
    jne ip_read_error
    cmp word ptr [bx+IP_RXLEN],0
    je ip_read_empty
    mov si,[bx+IP_RXPOS]
    add si,bx
    add si,IP_RX
    inc word ptr [bx+IP_RXPOS]
    dec word ptr [bx+IP_RXLEN]
    jnz ip_read_value
    push si
    call ip_ack_send ; reopen advertised receive window
    pop si
ip_read_value:
    mov cx,1
    clc
    ret
ip_read_empty:
    cmp word ptr [bx+IP_STATE],4
    jne ip_read_none
    cmp word ptr [bx+350],1
    je ip_read_short_http
    cmp word ptr [bx+350],2
    jne ip_read_eof
    mov ax,[bx+370]
    or ax,[bx+372]
    jz ip_read_eof
ip_read_short_http:
    mov word ptr [bx+IP_ERROR],5
    jmp ip_read_error
ip_read_eof:
    call rs_set_disconnected
ip_read_none:
    xor cx,cx
    clc
    ret
ip_read_error:
    mov ax,[bx+IP_ERROR]
    call nr_error
    call rs_set_disconnected
    xor cx,cx
    stc
    ret
ip_connected:
    call ip_pump
    cmp word ptr [bx+IP_STATE],4
    jne ip_connected_value
    cmp word ptr [bx+IP_RXLEN],0
    jne ip_connected_value
    cmp word ptr [bx+10],0
    jne ip_connected_value
    call rs_set_disconnected
ip_connected_value:
    mov ax,[bx+8]
    xor dx,dx
    jmp rs_done
ip_status:
    call ip_pump
    ret
ip_disconnect_entry:
    cmp word ptr [bx+IP_STATE],2
    je ip_disconnect_active
    cmp word ptr [bx+IP_STATE],4
    jne ip_disconnect_close
ip_disconnect_active:
    call rs_start_clock
    jc ip_disconnect_close
ip_disconnect_wait:
    call ip_pump
    cmp word ptr [bx+IP_WAIT],0
    je ip_disconnect_fin
    cmp word ptr [bx+IP_ERROR],0
    jne ip_disconnect_close
    call rs_expired
    jnc ip_disconnect_wait
    jmp ip_disconnect_close
ip_disconnect_fin:
    mov ax,[bx+IP_SEQ]
    mov [bx+IP_TXSEQ],ax
    mov ax,[bx+IP_SEQ+2]
    mov [bx+IP_TXSEQ+2],ax
    mov word ptr [bx+IP_TXLEN],0
    mov word ptr [bx+IP_FLAGS],11h
    add word ptr [bx+IP_SEQ],1
    adc word ptr [bx+IP_SEQ+2],0
    call ip_reliable
ip_disconnect_fin_wait:
    call ip_pump
    cmp word ptr [bx+IP_ERROR],0
    jne ip_disconnect_close
    cmp word ptr [bx+IP_WAIT],0
    je ip_disconnect_close
    call rs_expired
    jnc ip_disconnect_fin_wait
ip_disconnect_close:
    mov di,33
    call ip_raw
    mov word ptr [bx+IP_STATE],0
    mov word ptr [bx+IP_WAIT],0
    mov word ptr [bx+IP_RXLEN],0
    call rs_set_disconnected
    mov word ptr [bx+10],0
    mov word ptr [bx+14],0
    jmp rs_done
; Raw NIC state is private at NETWORK+1920.
include NNETNIC.INC
include NNET3C.INC
; Never probe another backend when the explicitly selected hardware fails.
raw_nic_start:
    cmp word ptr [bx+CFG_NICTYPE-1920],2
    je raw_3c_start
    cmp word ptr [bx+CFG_NICTYPE-1920],1
    jbe raw_ne_start
    mov ax,445
    ret
ne_description:
    mov word ptr [di],454Eh
    mov word ptr [di+2],3032h
    mov word ptr [di+4],3030h
    mov word ptr [di+6],6520h
    mov word ptr [di+8],7272h
    mov word ptr [di+10],726Fh
    mov byte ptr [di+12],0
    ret
net_ne_end:
