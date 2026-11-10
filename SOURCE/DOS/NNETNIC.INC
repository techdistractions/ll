; NETLAB native polling NE2000. 8086 instructions, runtime-selected ISA port.
; No IRQ ownership, Windows/DOS calls, heap allocation, TSR or external stack.
; BX=NETWORK state. Returns AX=0 success or explicit error; SI/CX=STRING.
NE_OWNER EQU 320
NE_PROM EQU 326
NE_NEXT EQU 358
NE_LEN EQU 360
NE_GUARD EQU 362
NE_HDR EQU 364
NE_BUF EQU 368
raw_ne_start:
    push bp
    push di
    push dx
    cmp word ptr [bx+NE_GUARD],0
    jne ne_reentrant
    mov word ptr [bx+NE_GUARD],1
    mov word ptr [bx+1890],0
    mov word ptr [bx+1892],0
    mov word ptr [bx+1900],0
    cmp di,32
IFDEF LLFS_COMPACT
    jne fs_skip_ne_open
    jmp ne_open
fs_skip_ne_open:
ELSE
    je ne_open
ENDIF
    cmp di,33
    je ne_close
    cmp di,40
    je ne_close_quiet
    cmp di,34
    je ne_send
    cmp di,35
    je ne_receive
    cmp di,39
    je ne_poll
    mov ax,445
    jmp ne_return
ne_reentrant:
    mov ax,424
    jmp ne_return_noguard
IFDEF LLFS_COMPACT
_TEXT ends
FSINIT segment word public 'INITCODE'
ENDIF
ne_open:
    cmp word ptr [bx+2],0
IFDEF LLFS_COMPACT
    je fs_long_17
    jmp ne_unsupported
fs_long_17:
ELSE
    jne ne_unsupported
ENDIF
    cmp word ptr [bx+8],0
IFDEF LLFS_COMPACT
    je fs_long_18
    jmp ne_success
fs_long_18:
ELSE
    jne ne_success
ENDIF
    mov dx,[bx+CFG_NIC-NNET_BASE]
    in al,dx
    cmp al,0FFh
IFDEF LLFS_COMPACT
    jne fs_long_19
    jmp ne_missing
fs_long_19:
ELSE
    je ne_missing
ENDIF
    ; STOP takes precedence: RTL8019AS CR=23h is stopped, despite STA=1.
    ; Refuse a running adapter (STA=1, STP=0), not the harmless STA latch.
    test al,1
    jnz ne_acquire
    test al,2
IFDEF LLFS_COMPACT
    jz fs_long_20
    jmp ne_owned
fs_long_20:
ELSE
    jnz ne_owned
ENDIF
ne_acquire:
    mov word ptr [bx+NE_OWNER],1
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,01Fh
    in al,dx
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    mov al,21h
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,007h
    in al,dx
    test al,80h
IFDEF LLFS_COMPACT
    jnz fs_long_21
    jmp ne_missing
fs_long_21:
ELSE
    jz ne_missing
ENDIF
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,00Fh
    xor al,al
    out dx,al                    ; IMR=0; never request IRQ5
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,00Eh
    mov al,48h                  ; byte DMA, FIFO threshold 8
    out dx,al
    xor ax,ax
    mov cx,32
    lea di,[bx+NE_PROM]
    call ne_read_dma
IFDEF LLFS_COMPACT
    jnc fs_long_22
    jmp ne_dma_error
fs_long_22:
ELSE
    jc ne_dma_error
ENDIF
    cmp byte ptr [bx+NE_PROM+28],57h
IFDEF LLFS_COMPACT
    je fs_long_23
    jmp ne_missing
fs_long_23:
ELSE
    jne ne_missing
ENDIF
    cmp byte ptr [bx+NE_PROM+30],57h
IFDEF LLFS_COMPACT
    je fs_long_24
    jmp ne_missing
fs_long_24:
ELSE
    jne ne_missing
ENDIF
    call ne_select_mac
IFDEF LLFS_COMPACT
    jnc fs_long_25
    jmp ne_bad_frame
fs_long_25:
ELSE
    jc ne_bad_frame
ENDIF
    mov dx,[bx+CFG_NIC-NNET_BASE]
    mov al,21h
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,001h
    mov al,46h
    out dx,al
    inc dx
    mov al,80h
    out dx,al
    inc dx
    mov al,46h
    out dx,al
    inc dx
    mov al,40h
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,00Ah
    xor al,al
    out dx,al
    inc dx
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,00Dh
    mov al,2                    ; loopback during ring initialization
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,00Ch
    mov al,4                    ; accept broadcasts, no promiscuous receive
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    mov al,61h
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,001h
    lea si,[bx+CFG_HWMAC-NNET_BASE]
    mov cx,6
ne_mac:
    mov al,[si]
    out dx,al
    inc si
    inc dx
    loop ne_mac
    mov al,47h
    out dx,al                    ; CURR
    inc dx
    xor al,al
    mov cx,8
ne_multicast:
    out dx,al
    inc dx
    loop ne_multicast
    mov word ptr [bx+NE_NEXT],47h
    mov word ptr [bx+NE_LEN],0
    mov word ptr [bx+322],0
    mov word ptr [bx+10],0
    mov word ptr [bx+14],0
    mov dx,[bx+CFG_NIC-NNET_BASE]
    mov al,22h
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,007h
    mov al,0FFh
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,00Dh
    xor al,al
    out dx,al                    ; normal TX
    mov word ptr [bx+8],1
    mov ax,[bx+1884]
    mov [bx+1890],ax
    jmp ne_success
IFDEF LLFS_COMPACT
FSINIT ends
_TEXT segment word public 'CODE'
ENDIF
ne_close:
    cmp word ptr [bx+8],0
    je ne_close_state
    mov ax,[bx+1886]
    mov [bx+1890],ax
ne_close_quiet:
    cmp word ptr [bx+8],0
    je ne_close_state
    mov dx,[bx+CFG_NIC-NNET_BASE]
    mov al,21h
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,00Fh
    xor al,al
    out dx,al
ne_close_state:
    mov word ptr [bx+NE_OWNER],0
    mov word ptr [bx+8],0
    mov word ptr [bx+NE_LEN],0
    mov word ptr [bx+322],0
    mov word ptr [bx+10],0
    mov word ptr [bx+14],0
    jmp ne_success
ne_send:
    cmp word ptr [bx+8],1
    jne ne_not_open
    cmp cx,14
    jb ne_bad_frame
    cmp cx,1514
    ja ne_bad_frame
    mov dx,[bx+CFG_NIC-NNET_BASE]
    in al,dx
    test al,4
    jnz ne_tx_busy
    mov bp,cx
    cmp bp,60
    jae ne_tx_length
    mov bp,60
ne_tx_length:
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,007h
    mov al,42h
    out dx,al                    ; clear DMA-complete and TX-complete
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,008h
    xor al,al
    out dx,al
    inc dx
    mov al,40h
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,00Ah
    mov ax,bp
    out dx,al
    inc dx
    mov al,ah
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    mov al,12h
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,010h
    mov di,cx
ne_tx_copy:
    lodsb
    out dx,al
    loop ne_tx_copy
    mov cx,bp
    sub cx,di
    jz ne_tx_copied
    xor al,al
ne_tx_pad:
    out dx,al
    loop ne_tx_pad
ne_tx_copied:
    call ne_dma_done
    jc ne_dma_error
IFDEF LL_DIAGNOSTIC
    call diag_verify_tx
    jc ne_dma_error
ENDIF
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,004h
    mov al,40h
    out dx,al
    inc dx
    mov ax,bp
    out dx,al
    inc dx
    mov al,ah
    out dx,al
    mov [bx+14],bp
    mov dx,[bx+CFG_NIC-NNET_BASE]
    mov al,26h
    out dx,al
IFDEF LL_DIAGNOSTIC
    call diag_wait_tx
ENDIF
    jmp ne_success
ne_receive:
    mov bp,ax                    ; ByRef LONG requested/actual byte count
    mov ax,ds:[bp]
    mov dx,ds:[bp+2]
    mov word ptr ds:[bp],0
    mov word ptr ds:[bp+2],0
    test dx,8000h
    jnz ne_bad_frame
    or dx,dx
    jnz ne_receive_max
    cmp ax,1514
    jbe ne_receive_limit
ne_receive_max:
    mov ax,1514
ne_receive_limit:
    mov [bx+324],ax
    call ne_rx_one
    jc ne_rx_error
    mov cx,[bx+324]
    cmp cx,[bx+NE_LEN]
    jbe ne_receive_copy
    mov cx,[bx+NE_LEN]
ne_receive_copy:
    mov ds:[bp],cx
    lea si,[bx+NE_BUF]
    add si,[bx+322]
    sub [bx+NE_LEN],cx
    add [bx+322],cx
    mov ax,[bx+NE_LEN]
    mov [bx+10],ax
    jmp ne_success
ne_poll:
    call ne_rx_one
    jc ne_rx_error
    cmp word ptr [bx+1900],0
    je ne_success
    mov ax,[bx+1888]
    mov [bx+1890],ax
    mov word ptr [bx+1892],1
    mov ax,[bx+NE_LEN]
    mov [bx+1896],ax
    mov word ptr [bx+1898],0
    jmp ne_success
ne_rx_error:
    ; Fatal ring/DMA faults discard cached data and require a fresh Connect.
    mov word ptr [bx+8],0
    mov word ptr [bx+10],0
    mov word ptr [bx+14],0
    mov word ptr [bx+NE_LEN],0
    mov word ptr [bx+322],0
    xor cx,cx
    jmp ne_return
ne_rx_one:
    cmp word ptr [bx+8],1
    jne ne_rx_empty
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,007h
    in al,dx
    test al,10h                  ; overrun: fail closed, caller reopens
    jnz ne_rx_overrun
    test al,0Ch                  ; TXE=08h, RXE=04h; PTX=02h is success
    jnz ne_rx_fault
    test al,2
    jz ne_rx_cached
    mov word ptr [bx+14],0
    mov al,2
    out dx,al
ne_rx_cached:
    cmp word ptr [bx+NE_LEN],0
    jne ne_rx_empty
    mov dx,[bx+CFG_NIC-NNET_BASE]
    mov al,62h
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,007h
    in al,dx
    mov ah,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    mov al,22h
    out dx,al
    cmp ah,byte ptr [bx+NE_NEXT]
    je ne_rx_empty
    mov ah,byte ptr [bx+NE_NEXT]
    xor al,al
    mov cx,4
    lea di,[bx+NE_HDR]
    call ne_read_dma
    jc ne_rx_dma
    mov al,[bx+NE_HDR+1]
    cmp al,46h
    jb ne_rx_invalid
    cmp al,80h
    jae ne_rx_invalid
    test byte ptr [bx+NE_HDR],1
    jz ne_rx_invalid
    mov cx,[bx+NE_HDR+2]
    cmp cx,64                    ; minimum 60-byte frame plus CRC
    jb ne_rx_invalid
    cmp cx,1518
    ja ne_rx_invalid
    sub cx,4
    mov [bx+NE_LEN],cx
    mov [bx+10],cx
    mov word ptr [bx+322],0
    mov ah,byte ptr [bx+NE_NEXT]
    mov al,4
    lea di,[bx+NE_BUF]
    call ne_read_dma             ; NIC DMA wraps at PSTOP to PSTART
    jc ne_rx_dma
    mov al,[bx+NE_HDR+1]
    mov byte ptr [bx+NE_NEXT],al
    dec al
    cmp al,46h
    jae ne_rx_boundary
    mov al,7Fh
ne_rx_boundary:
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,003h
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,007h
    mov al,1
    out dx,al
    mov word ptr [bx+1900],1
ne_rx_empty:
    xor ax,ax
    clc
    ret
ne_rx_overrun:
    mov ax,425
    stc
    ret
ne_rx_fault:
    mov ax,426
    stc
    ret
ne_rx_invalid:
    mov ax,427
    stc
    ret
ne_rx_dma:
    mov ax,423
    stc
    ret
ne_read_dma:
    push ax
    push cx
    push di
    push dx
    push bp
    mov bp,ax
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,007h
    mov al,40h
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,008h
    mov ax,bp
    out dx,al
    inc dx
    mov al,ah
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,00Ah
    mov ax,cx
    out dx,al
    inc dx
    mov al,ah
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    mov al,0Ah
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,010h
ne_rx_copy:
    in al,dx
    mov [di],al
    inc di
    loop ne_rx_copy
    call ne_dma_done
    pop bp
    pop dx
    pop di
    pop cx
    pop ax
    ret
ne_dma_done:
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,007h
    in al,dx
    test al,40h
    jz ne_dma_incomplete
    mov al,40h
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    mov al,22h
    out dx,al
    clc
    ret
ne_dma_incomplete:
    stc
    ret
ne_missing:
    mov ax,420
    jmp ne_return
ne_owned:
    mov ax,421
    jmp ne_return
ne_not_open:
    mov ax,422
    jmp ne_return
ne_dma_error:
    mov ax,423
    jmp ne_return
ne_bad_frame:
    mov ax,5
    jmp ne_return
ne_tx_busy:
    mov ax,428
    jmp ne_return
ne_unsupported:
    mov ax,445
    jmp ne_return
ne_success:
    xor ax,ax
ne_return:
    ; Failed initialization must leave our card stopped and interrupts masked.
    or ax,ax
    jz ne_return_ready
    cmp word ptr [bx+8],0
    jne ne_return_ready
    cmp word ptr [bx+NE_OWNER],0
    je ne_return_ready
    push ax
    mov dx,[bx+CFG_NIC-NNET_BASE]
    mov al,21h
    out dx,al
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,00Fh
    xor al,al
    out dx,al
    mov word ptr [bx+NE_OWNER],0
    pop ax
ne_return_ready:
    mov word ptr [bx+NE_GUARD],0
    or ax,ax
    jnz ne_return_noguard
    cmp word ptr [bx+1894],0
    jne ne_return_noguard
    cmp word ptr [bx+1890],0
    je ne_return_noguard
    push ax
    push cx
    push si
    push di
    push bp
    push bx
    mov word ptr [bx+1894],1
    mov dx,[bx+1890]
    cmp word ptr [bx+1892],0
    je ne_callback
    lea ax,[bx+1896]
    push ax
ne_callback:
    call dx
    pop bx
    mov word ptr [bx+1894],0
    pop bp
    pop di
    pop si
    pop cx
    pop ax
ne_return_noguard:
    pop dx
    pop di
    pop bp
    ret

; Raw 32-byte PROM remains untouched. Canonical six-byte MAC is shared by
; PAR programming, Ethernet source addresses and ARP replies.
; AUTO accepts standard duplicated-byte PROMs and PicoMEM packed PROMs.
IFDEF LLFS_COMPACT
_TEXT ends
FSINIT segment word public 'INITCODE'
ENDIF
ne_select_mac:
    mov word ptr [bx+CFG_MACSRC-NNET_BASE],0
    mov ax,[bx+CFG_MACMODE-NNET_BASE]
    cmp ax,2
    ja ne_invalid_mac
    or ax,ax
    jnz ne_mac_format
    mov ax,1
    lea si,[bx+NE_PROM]
    mov cx,6
ne_mac_pairs:
    mov dl,[si]
    cmp dl,[si+1]
    jne ne_mac_packed
    add si,2
    loop ne_mac_pairs
    jmp ne_mac_format
ne_mac_packed:
    mov ax,2
ne_mac_format:
    mov [bx+CFG_PROMFMT-NNET_BASE],ax
    lea si,[bx+NE_PROM]
    lea di,[bx+CFG_HWMAC-NNET_BASE]
    mov cx,6
ne_mac_decode:
    mov dl,[si]
    mov [di],dl
    inc di
    inc si
    cmp ax,1
    jne ne_mac_decoded_byte
    inc si
ne_mac_decoded_byte:
    loop ne_mac_decode
    lea si,[bx+CFG_MAC-NNET_BASE]
    cmp byte ptr [si],0
    je ne_mac_validate
    lea di,[bx+CFG_HWMAC-NNET_BASE]
    mov cx,6
ne_mac_override:
    call ne_mac_nibble
    jc ne_invalid_mac
    mov ah,al
    shl ah,1
    shl ah,1
    shl ah,1
    shl ah,1
    call ne_mac_nibble
    jc ne_invalid_mac
    or al,ah
    mov [di],al
    inc di
    dec cx
    jz ne_mac_override_end
    cmp byte ptr [si],':'
    jne ne_invalid_mac
    inc si
    jmp ne_mac_override
ne_mac_override_end:
    cmp byte ptr [si],0
    jne ne_invalid_mac
    mov word ptr [bx+CFG_MACSRC-NNET_BASE],1
ne_mac_validate:
    lea si,[bx+CFG_HWMAC-NNET_BASE]
    test byte ptr [si],1
    jnz ne_invalid_mac
    xor dl,dl
    mov cx,6
ne_mac_nonzero:
    or dl,[si]
    inc si
    loop ne_mac_nonzero
    or dl,dl
    jz ne_invalid_mac
    xor ax,ax
    clc
    ret
ne_invalid_mac:
    mov ax,5
    stc
    ret
ne_mac_nibble:
    lodsb
    cmp al,'0'
    jb ne_mac_bad_nibble
    cmp al,'9'
    jbe ne_mac_digit
    and al,0DFh
    cmp al,'A'
    jb ne_mac_bad_nibble
    cmp al,'F'
    ja ne_mac_bad_nibble
    sub al,'A'-10
    clc
    ret
ne_mac_digit:
    sub al,'0'
    clc
    ret
ne_mac_bad_nibble:
    stc
    ret

IFDEF LLFS_COMPACT
FSINIT ends
_TEXT segment word public 'CODE'
ENDIF
