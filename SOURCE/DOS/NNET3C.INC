; LiteLink native polled EtherLink III 3C509B, independently implemented from
; 3Com 09-0398-002B (August 1994), chapters 3,4,6,7,10. No packet-driver code.
; Raw ABI matches NNETNIC: BX=netstate+1920, DI=32 open/33,40 close/34 TX/
; 35 RX (AX points to requested/result dword)/39 poll. AX=0 or error.
; RX returns DS:SI/CX, reusing the same 1514-byte frame cache as the NE backend.
; Every adapter I/O instruction is byte-wide; paired registers are low then high.
; EEPROM commands here are READ only. No ISR, DOS callback, TSR, PIC or allocation.
C3_STAGE EQU CFG_HWSTAGE-1920
C3_STATUS EQU CFG_HWSTATUS-1920
raw_3c_start:
    push bp
    push di
    push dx
    cmp word ptr [bx+NE_GUARD],0
    jne c3_reentrant
    mov word ptr [bx+NE_GUARD],1
    cmp di,32
    je c3_open
    cmp di,33
    je c3_close
    cmp di,40
    je c3_close
    cmp di,34
    je c3_send
    cmp di,35
    je c3_receive
    cmp di,39
    je c3_poll
    mov ax,445
    jmp c3_return
c3_reentrant:
    mov ax,424
    jmp c3_return_noguard
; Paired 8-bit register access preserves DX and CPU interrupt state.
c3_readw:
    pushf
    cli
    in al,dx
    mov ah,al
    inc dx
    in al,dx
    dec dx
    xchg al,ah
    popf
    ret
c3_writew:
    pushf
    cli
    out dx,al
    inc dx
    xchg al,ah
    out dx,al
    xchg al,ah
    dec dx
    popf
    ret
; All commands are serialized. Timeout in BIOS ticks plus a finite loop bound.
c3_command:
    push cx
    push bp
    mov dx,[bx+CFG_NIC-1920]
    add dx,14
    call c3_writew
    call ip_ticks
    mov bp,ax
    mov cx,0FFFFh
c3_command_wait:
    mov dx,[bx+CFG_NIC-1920]
    add dx,14
    call c3_readw
    mov [bx+C3_STATUS],ax
    test ax,1000h
    jz c3_command_ok
    call ip_ticks
    sub ax,bp
    cmp ax,3
    jae c3_command_bad
    loop c3_command_wait
c3_command_bad:
    pop bp
    pop cx
    stc
    ret
c3_command_ok:
    pop bp
    pop cx
    clc
    ret
; A two-tick delay guarantees at least one full BIOS tick (55 ms) elapsed.
c3_settle:
    push ax
    push bp
    push cx
    mov cx,0FFFFh
    call ip_ticks
    mov bp,ax
c3_settle_loop:
    call ip_ticks
    sub ax,bp
    cmp ax,2
    jae c3_settle_done
    loop c3_settle_loop
c3_settle_done:
    pop cx
    pop bp
    pop ax
    ret
; Classic ISA identification. Does not change the EEPROM or configured base.
c3_id_sequence:
    push ax
    push cx
    mov dx,[bx+CFG_IDPORT-1920]
    xor al,al
    out dx,al
    out dx,al
    mov al,0FFh
    mov cx,255
c3_id_next:
    out dx,al
    shl al,1
    jnc c3_id_shifted
    xor al,0CFh
c3_id_shifted:
    loop c3_id_next
    pop cx
    pop ax
    ret
; AL=EEPROM word number, AX=result. Serial reads also perform contention.
c3_id_read:
    push cx
    push bp
    mov dx,[bx+CFG_IDPORT-1920]
    or al,80h
    out dx,al
    call c3_settle
    xor bp,bp
    mov cx,16
c3_id_bit:
    in al,dx
    and ax,1
    shl bp,1
    or bp,ax
    loop c3_id_bit
    mov ax,bp
    pop bp
    pop cx
    ret
; AL=EEPROM word number, AX=result. Window zero must already be selected.
c3_ee_read:
    push cx
    push bp
    push ax
    call ip_ticks
    mov bp,ax
    mov cx,0FFFFh
c3_ee_idle:
    mov dx,[bx+CFG_NIC-1920]
    add dx,10
    call c3_readw
    test ax,8000h
    jz c3_ee_issue
    call ip_ticks
    sub ax,bp
    cmp ax,3
    jae c3_ee_bad_pop
    loop c3_ee_idle
c3_ee_bad_pop:
    pop ax
    jmp c3_ee_bad
c3_ee_issue:
    pop ax
    xor ah,ah
    or al,80h
    call c3_writew
    mov cx,0FFFFh
c3_ee_wait:
    call c3_readw
    test ax,8000h
    jz c3_ee_ready
    call ip_ticks
    sub ax,bp
    cmp ax,3
    jae c3_ee_bad
    loop c3_ee_wait
c3_ee_bad:
    pop bp
    pop cx
    stc
    ret
c3_ee_ready:
    add dx,2
    call c3_readw
    pop bp
    pop cx
    clc
    ret
; Refuse to take hardware away from a resident Ethernet packet driver.
; Read IVT signatures only; do not call, terminate or replace another driver.
c3_driver_present:
    push ax
    push cx
    push dx
    push si
    push di
    push es
    mov di,60h*4
    mov cx,32
c3_driver_next:
    xor ax,ax
    mov es,ax
    mov si,es:[di]
    mov dx,es:[di+2]
    or dx,si
    jz c3_driver_skip
    mov dx,es:[di+2]
    mov es,dx
    cmp word ptr es:[si+3],4B50h ; PK
    jne c3_driver_skip
    cmp word ptr es:[si+5],2054h ; T space
    jne c3_driver_skip
    cmp word ptr es:[si+7],5244h ; DR
    jne c3_driver_skip
    cmp word ptr es:[si+9],5256h ; VR
    je c3_driver_found
c3_driver_skip:
    add di,4
    loop c3_driver_next
    clc
    jmp c3_driver_done
c3_driver_found:
    stc
c3_driver_done:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop ax
    ret
c3_open:
    cmp word ptr [bx+8],1
    je c3_success
    mov word ptr [bx+C3_STAGE],1
    call c3_driver_present
    jc c3_resident
    ; A stale PicoMEM MAC override is not silently used on the physical 3Com.
    cmp byte ptr [bx+CFG_MAC-1920],0
    jne c3_override
    mov word ptr [bx+C3_STAGE],2
    call c3_id_sequence
    mov dx,[bx+CFG_IDPORT-1920]
    mov al,0D0h                  ; clear volatile contention tags, not EEPROM
    out dx,al
    mov al,7
    call c3_id_read
    cmp ax,6D50h
    jne c3_missing
    ; Contend on the full station address before reading configuration.
    xor al,al
    call c3_id_read
    mov al,1
    call c3_id_read
    mov al,2
    call c3_id_read
    mov al,8
    call c3_id_read
    mov [bx+CFG_HWADDR-1920],ax
    and ax,1Fh
    mov cl,4
    shl ax,cl
    add ax,200h
    cmp ax,[bx+CFG_NIC-1920]
    jne c3_wrong_base
    mov dx,[bx+CFG_IDPORT-1920]
    mov al,0D1h
    out dx,al
    mov al,0FFh                 ; activate at EEPROM-selected base only
    out dx,al
    mov word ptr [bx+C3_STAGE],3
    mov ax,0800h                ; select window zero
    call c3_command
    jc c3_timeout
    mov dx,[bx+CFG_NIC-1920]
    call c3_readw
    cmp ax,6D50h
    jne c3_missing
    add dx,2
    call c3_readw
    mov [bx+CFG_PRODUCT-1920],ax
    and ax,0F0FFh
    cmp ax,9050h
    jne c3_wrong_card
    add dx,2
    call c3_readw
    test ax,8000h               ; B-generation marker; old 3C509 is not 8-bit
    jz c3_not_byte
    test ax,1
    jnz c3_resident
    mov word ptr [bx+NE_OWNER],1
    or ax,1
    call c3_writew
    add dx,2
    call c3_readw
    mov [bx+CFG_HWADDR-1920],ax
    test ax,80h                ; no silent media choice for AUTO SELECT policy
    jnz c3_auto_media
    mov cl,14
    shr ax,cl
    mov [bx+CFG_MEDIA-1920],ax
    cmp ax,2
    je c3_wrong_media
    add dx,2
    call c3_readw
    mov cl,12
    shr ax,cl
    mov [bx+CFG_HWIRQ-1920],ax
    mov word ptr [bx+C3_STAGE],4
    mov al,10h
    call c3_ee_read
    jc c3_timeout
    mov [bx+CFG_CAPS-1920],ax
    cmp ax,0FFFFh
    je c3_wrong_card
    test ax,2
    jz c3_not_byte
    mov al,14h
    call c3_ee_read
    jc c3_timeout
    mov [bx+CFG_REVISION-1920],ax
    mov al,0Dh
    call c3_ee_read
    jc c3_timeout
    mov [bx+CFG_SOFTINFO-1920],ax
    lea di,[bx+CFG_HWMAC-1920]
    xor si,si
c3_mac_word:
    mov ax,si
    call c3_ee_read
    jc c3_timeout
    mov [di],ah
    mov [di+1],al
    add di,2
    inc si
    cmp si,3
    jb c3_mac_word
    lea si,[bx+CFG_HWMAC-1920]
    test byte ptr [si],1
    jnz c3_wrong_card
    xor al,al
    mov cx,6
c3_mac_check:
    or al,[si]
    inc si
    loop c3_mac_check
    or al,al
    jz c3_wrong_card
    mov word ptr [bx+CFG_MACSRC-1920],0
    mov word ptr [bx+CFG_PROMFMT-1920],3 ; EEPROM, not an NE PROM layout
    mov al,3                    ; leave Product ID in EEPROM data register
    call c3_ee_read
    jc c3_timeout
    mov word ptr [bx+C3_STAGE],5
    mov ax,7000h                ; interrupt mask = 0
    call c3_command
    jc c3_timeout
    mov ax,78FFh                ; status reasons readable, no IRQs enabled
    call c3_command
    jc c3_timeout
    mov ax,1800h                ; RX disable
    call c3_command
    jc c3_timeout
    mov ax,5000h                ; TX disable
    call c3_command
    jc c3_timeout
    mov ax,2800h                ; RX FIFO reset
    call c3_command
    jc c3_timeout
    mov ax,5800h                ; TX FIFO reset
    call c3_command
    jc c3_timeout
    mov ax,0802h                ; station address window
    call c3_command
    jc c3_timeout
    mov dx,[bx+CFG_NIC-1920]
    lea si,[bx+CFG_HWMAC-1920]
    mov cx,6
c3_station:
    lodsb
    out dx,al
    inc dx
    loop c3_station
    mov ax,0804h
    call c3_command
    jc c3_timeout
    mov dx,[bx+CFG_NIC-1920]
    add dx,10
    call c3_readw
    and ax,0FF33h               ; CRC strip on, media controls set below
    cmp word ptr [bx+CFG_MEDIA-1920],0
    jne c3_not_tp
    or ax,40h                   ; TP jabber protection
    test word ptr [bx+CFG_SOFTINFO-1920],4000h
    jnz c3_media_write
    or ax,80h                   ; respect EEPROM link-beat enable policy
    jmp c3_media_write
c3_not_tp:
    cmp word ptr [bx+CFG_MEDIA-1920],1
    jne c3_media_write
    or ax,8                     ; AUI SQE statistics
c3_media_write:
    call c3_writew
    mov ax,0B800h               ; stop coax unless configured BNC
    call c3_command
    jc c3_timeout
    cmp word ptr [bx+CFG_MEDIA-1920],3
    jne c3_media_settle
    mov ax,1000h                ; start configured BNC DC converter
    call c3_command
    jc c3_timeout
c3_media_settle:
    call c3_settle
    mov dx,[bx+CFG_NIC-1920]
    add dx,10
    call c3_readw
    mov [bx+CFG_LINK-1920],ax
    mov ax,0801h                ; normal operating window
    call c3_command
    jc c3_timeout
    mov ax,9FFCh                ; TX starts only after complete frame loaded
    call c3_command
    jc c3_timeout
    mov ax,8FFCh                ; RX early disabled
    call c3_command
    jc c3_timeout
    mov ax,8005h                ; individual address and broadcasts only
    call c3_command
    jc c3_timeout
    mov ax,68FFh                ; clear stale interrupt-latch reasons
    call c3_command
    jc c3_timeout
    mov ax,4800h                ; TX enable
    call c3_command
    jc c3_timeout
    mov ax,2000h                ; RX enable
    call c3_command
    jc c3_timeout
    mov word ptr [bx+8],1
    mov word ptr [bx+NE_LEN],0
    mov word ptr [bx+322],0
    mov word ptr [bx+C3_STAGE],6
    jmp c3_success
c3_close:
    cmp word ptr [bx+NE_OWNER],0
    je c3_closed
    mov ax,7000h
    call c3_command
    mov ax,1800h
    call c3_command
    mov ax,5000h
    call c3_command
    mov ax,0B800h
    call c3_command
    mov ax,0800h
    call c3_command
    jc c3_closed               ; never touch +4 unless window zero is selected
    mov dx,[bx+CFG_NIC-1920]
    add dx,4
    call c3_readw
    and ax,0FFFEh
    call c3_writew
c3_closed:
    mov word ptr [bx+NE_OWNER],0
    mov word ptr [bx+8],0
    mov word ptr [bx+10],0
    mov word ptr [bx+14],0
    mov word ptr [bx+NE_LEN],0
    mov word ptr [bx+322],0
    jmp c3_success
c3_send:
    cmp word ptr [bx+8],1
    jne c3_not_open
    mov word ptr [bx+C3_STAGE],7
    cmp cx,14
    jb c3_bad_frame
    cmp cx,1514
    ja c3_bad_frame
    mov bp,cx
    cmp bp,60
    jae c3_tx_min
    mov bp,60
c3_tx_min:
    mov dx,[bx+CFG_NIC-1920]
    add dx,12
    call c3_readw
    mov di,bp
    add di,7
    and di,0FFFCh              ; padded packet plus 4-byte preamble
    cmp ax,di
    jb c3_busy
    mov dx,[bx+CFG_NIC-1920]
    mov ax,bp
    or ah,80h                  ; request a polled completion even on success
    out dx,al
    mov al,ah
    out dx,al
    xor al,al
    out dx,al
    out dx,al
    mov di,cx
c3_tx_bytes:
    lodsb
    out dx,al
    loop c3_tx_bytes
    mov cx,bp
    sub cx,di
    jz c3_tx_align
    xor al,al
c3_tx_pad:
    out dx,al
    loop c3_tx_pad
c3_tx_align:
    mov cx,bp
    neg cx
    and cx,3
    jz c3_tx_wait_start
    xor al,al
c3_tx_dword:
    out dx,al
    loop c3_tx_dword
c3_tx_wait_start:
    call ip_ticks
    mov bp,ax
    mov cx,0FFFFh
c3_tx_wait:
    mov dx,[bx+CFG_NIC-1920]
    add dx,11
    in al,dx
    test al,80h
    jnz c3_tx_complete
    call ip_ticks
    sub ax,bp
    cmp ax,4
    jae c3_tx_timeout
    loop c3_tx_wait
c3_tx_timeout:
    inc word ptr [bx+CFG_TXERR-1920]
    jmp c3_timeout
c3_tx_complete:
    xor ah,ah
    mov [bx+C3_STATUS],ax
    mov ah,al
    xor al,al
    out dx,al                  ; pop completion, never enable an interrupt
    test ah,3Ch
    jnz c3_tx_error
    inc word ptr [bx+CFG_TXOK-1920]
    mov word ptr [bx+14],0
    jmp c3_success
c3_tx_error:
    inc word ptr [bx+CFG_TXERR-1920]
    mov ax,5800h
    call c3_command
    mov ax,9FFCh
    call c3_command
    mov ax,4800h
    call c3_command
    mov ax,427
    jmp c3_return
c3_receive:
    mov bp,ax
    mov ax,ds:[bp]
    mov dx,ds:[bp+2]
    mov word ptr ds:[bp],0
    mov word ptr ds:[bp+2],0
    test dx,8000h
    jnz c3_bad_frame
    or dx,dx
    jnz c3_rx_limit
    cmp ax,1514
    jbe c3_rx_bounded
c3_rx_limit:
    mov ax,1514
c3_rx_bounded:
    mov [bx+324],ax
    call c3_rx_one
    jc c3_timeout
    mov cx,[bx+324]
    cmp cx,[bx+NE_LEN]
    jbe c3_rx_result
    mov cx,[bx+NE_LEN]
c3_rx_result:
    mov ds:[bp],cx
    lea si,[bx+NE_BUF]
    add si,[bx+322]
    sub [bx+NE_LEN],cx
    add [bx+322],cx
    mov ax,[bx+NE_LEN]
    mov [bx+10],ax
    jmp c3_success
c3_poll:
    call c3_rx_one
    jc c3_timeout
    jmp c3_success
c3_rx_one:
    cmp word ptr [bx+NE_LEN],0
    jne c3_rx_none
    cmp word ptr [bx+8],1
    jne c3_rx_none
    mov word ptr [bx+C3_STAGE],8
    mov dx,[bx+CFG_NIC-1920]
    add dx,8
    call c3_readw
    test ax,8000h
    jnz c3_rx_none
    mov [bx+C3_STATUS],ax
    test ax,4000h
    jnz c3_rx_drop
    and ax,7FFh
    cmp ax,14
    jb c3_rx_drop
    cmp ax,1514
    ja c3_rx_drop
    mov [bx+NE_LEN],ax
    mov [bx+10],ax
    mov word ptr [bx+322],0
    mov cx,ax
    lea di,[bx+NE_BUF]
    mov dx,[bx+CFG_NIC-1920]
c3_rx_bytes:
    in al,dx
    mov [di],al
    inc di
    loop c3_rx_bytes
    inc word ptr [bx+CFG_RXOK-1920]
    mov ax,4000h               ; discard remainder/padding, advance RX status
    call c3_command
    ret
c3_rx_drop:
    inc word ptr [bx+CFG_RXERR-1920]
    mov ax,4000h
    call c3_command
    ret
c3_rx_none:
    clc
    ret
c3_missing:
    mov ax,420
    jmp c3_return
c3_resident:
    mov ax,430
    jmp c3_return
c3_override:
    mov ax,432
    jmp c3_return
c3_not_byte:
    mov ax,433
    jmp c3_return
c3_wrong_base:
    mov ax,434
    jmp c3_return
c3_wrong_card:
    mov ax,435
    jmp c3_return
c3_wrong_media:
    mov ax,436
    jmp c3_return
c3_auto_media:
    mov ax,437
    jmp c3_return
c3_timeout:
    mov ax,429
    jmp c3_return
c3_not_open:
    mov ax,422
    jmp c3_return
c3_bad_frame:
    mov ax,5
    jmp c3_return
c3_busy:
    mov ax,428
    jmp c3_return
c3_success:
    xor ax,ax
c3_return:
    mov word ptr [bx+NE_GUARD],0
c3_return_noguard:
    pop dx
    pop di
    pop bp
    ret
