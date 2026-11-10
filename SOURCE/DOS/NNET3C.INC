; LiteLink native polled EtherLink III 3C509B, independently implemented from
; 3Com 09-0398-002B (August 1994), chapters 3,4,6,7,10. No packet-driver code.
; Raw ABI matches NNETNIC: BX=netstate+1920, DI=32 open/33,40 close/34 TX/
; 35 RX (AX points to requested/result dword)/39 poll. AX=0 or error.
; RX returns DS:SI/CX, reusing the same 1514-byte frame cache as the NE backend.
; Every adapter I/O instruction is byte-wide; paired registers are low then high.
; EEPROM commands here are READ only. No ISR, DOS callback, TSR, PIC or allocation.
C3_STAGE EQU CFG_HWSTAGE-NNET_BASE
C3_STATUS EQU CFG_HWSTATUS-NNET_BASE
raw_3c_start:
    push bp
    push di
    push dx
    cmp word ptr [bx+NE_GUARD],0
    jne c3_reentrant
    mov word ptr [bx+NE_GUARD],1
    cmp di,32
IFDEF LLFS_COMPACT
    jne fs_skip_c3_open
    jmp c3_open
fs_skip_c3_open:
ELSE
    je c3_open
ENDIF
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
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,14
    call c3_writew
    call ip_ticks
    mov bp,ax
    mov cx,0FFFFh
c3_command_wait:
    mov dx,[bx+CFG_NIC-NNET_BASE]
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
IFDEF LLFS_COMPACT
_TEXT ends
FSINIT segment word public 'INITCODE'
ENDIF
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
    mov dx,[bx+CFG_IDPORT-NNET_BASE]
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
    mov dx,[bx+CFG_IDPORT-NNET_BASE]
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
    mov dx,[bx+CFG_NIC-NNET_BASE]
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
IFDEF LLFS_COMPACT
    jne fs_long_26
    jmp c3_success
fs_long_26:
ELSE
    je c3_success
ENDIF
    mov word ptr [bx+C3_STAGE],1
    call c3_driver_present
IFDEF LLFS_COMPACT
    jnc fs_long_27
    jmp c3_resident
fs_long_27:
ELSE
    jc c3_resident
ENDIF
    ; A stale PicoMEM MAC override is not silently used on the physical 3Com.
    cmp byte ptr [bx+CFG_MAC-NNET_BASE],0
IFDEF LLFS_COMPACT
    je fs_long_28
    jmp c3_override
fs_long_28:
ELSE
    jne c3_override
ENDIF
    mov word ptr [bx+C3_STAGE],2
    call c3_id_sequence
    mov dx,[bx+CFG_IDPORT-NNET_BASE]
    mov al,0D0h                  ; clear volatile contention tags, not EEPROM
    out dx,al
    mov al,7
    call c3_id_read
    cmp ax,6D50h
IFDEF LLFS_COMPACT
    je fs_long_29
    jmp c3_missing
fs_long_29:
ELSE
    jne c3_missing
ENDIF
    ; Contend on the full station address before reading configuration.
    xor al,al
    call c3_id_read
    mov al,1
    call c3_id_read
    mov al,2
    call c3_id_read
    mov al,8
    call c3_id_read
    mov [bx+CFG_HWADDR-NNET_BASE],ax
    and ax,1Fh
    mov cl,4
    shl ax,cl
    add ax,200h
    cmp ax,[bx+CFG_NIC-NNET_BASE]
IFDEF LLFS_COMPACT
    je fs_long_30
    jmp c3_wrong_base
fs_long_30:
ELSE
    jne c3_wrong_base
ENDIF
    mov dx,[bx+CFG_IDPORT-NNET_BASE]
    mov al,0D1h
    out dx,al
    mov al,0FFh                 ; activate at EEPROM-selected base only
    out dx,al
    mov word ptr [bx+C3_STAGE],3
    mov ax,0800h                ; select window zero
    call c3_command
IFDEF LLFS_COMPACT
    jnc fs_long_31
    jmp c3_timeout
fs_long_31:
ELSE
    jc c3_timeout
ENDIF
    mov dx,[bx+CFG_NIC-NNET_BASE]
    call c3_readw
    cmp ax,6D50h
IFDEF LLFS_COMPACT
    je fs_long_32
    jmp c3_missing
fs_long_32:
ELSE
    jne c3_missing
ENDIF
    add dx,2
    call c3_readw
    mov [bx+CFG_PRODUCT-NNET_BASE],ax
    and ax,0F0FFh
    cmp ax,9050h
IFDEF LLFS_COMPACT
    je fs_long_33
    jmp c3_wrong_card
fs_long_33:
ELSE
    jne c3_wrong_card
ENDIF
    add dx,2
    call c3_readw
    test ax,8000h               ; B-generation marker; old 3C509 is not 8-bit
IFDEF LLFS_COMPACT
    jnz fs_long_34
    jmp c3_not_byte
fs_long_34:
ELSE
    jz c3_not_byte
ENDIF
    test ax,1
IFDEF LLFS_COMPACT
    jz fs_long_35
    jmp c3_resident
fs_long_35:
ELSE
    jnz c3_resident
ENDIF
    mov word ptr [bx+NE_OWNER],1
    or ax,1
    call c3_writew
    add dx,2
    call c3_readw
    mov [bx+CFG_HWADDR-NNET_BASE],ax
    test ax,80h                ; no silent media choice for AUTO SELECT policy
IFDEF LLFS_COMPACT
    jz fs_long_36
    jmp c3_auto_media
fs_long_36:
ELSE
    jnz c3_auto_media
ENDIF
    mov cl,14
    shr ax,cl
    mov [bx+CFG_MEDIA-NNET_BASE],ax
    cmp ax,2
IFDEF LLFS_COMPACT
    jne fs_long_37
    jmp c3_wrong_media
fs_long_37:
ELSE
    je c3_wrong_media
ENDIF
    add dx,2
    call c3_readw
    mov cl,12
    shr ax,cl
    mov [bx+CFG_HWIRQ-NNET_BASE],ax
    mov word ptr [bx+C3_STAGE],4
    mov al,10h
    call c3_ee_read
IFDEF LLFS_COMPACT
    jnc fs_long_38
    jmp c3_timeout
fs_long_38:
ELSE
    jc c3_timeout
ENDIF
    mov [bx+CFG_CAPS-NNET_BASE],ax
    cmp ax,0FFFFh
IFDEF LLFS_COMPACT
    jne fs_long_39
    jmp c3_wrong_card
fs_long_39:
ELSE
    je c3_wrong_card
ENDIF
    test ax,2
IFDEF LLFS_COMPACT
    jnz fs_long_40
    jmp c3_not_byte
fs_long_40:
ELSE
    jz c3_not_byte
ENDIF
    mov al,14h
    call c3_ee_read
IFDEF LLFS_COMPACT
    jnc fs_long_41
    jmp c3_timeout
fs_long_41:
ELSE
    jc c3_timeout
ENDIF
    mov [bx+CFG_REVISION-NNET_BASE],ax
    mov al,0Dh
    call c3_ee_read
IFDEF LLFS_COMPACT
    jnc fs_long_42
    jmp c3_timeout
fs_long_42:
ELSE
    jc c3_timeout
ENDIF
    mov [bx+CFG_SOFTINFO-NNET_BASE],ax
    lea di,[bx+CFG_HWMAC-NNET_BASE]
    xor si,si
c3_mac_word:
    mov ax,si
    call c3_ee_read
IFDEF LLFS_COMPACT
    jnc fs_long_43
    jmp c3_timeout
fs_long_43:
ELSE
    jc c3_timeout
ENDIF
    mov [di],ah
    mov [di+1],al
    add di,2
    inc si
    cmp si,3
    jb c3_mac_word
    lea si,[bx+CFG_HWMAC-NNET_BASE]
    test byte ptr [si],1
IFDEF LLFS_COMPACT
    jz fs_long_44
    jmp c3_wrong_card
fs_long_44:
ELSE
    jnz c3_wrong_card
ENDIF
    xor al,al
    mov cx,6
c3_mac_check:
    or al,[si]
    inc si
    loop c3_mac_check
    or al,al
IFDEF LLFS_COMPACT
    jnz fs_long_45
    jmp c3_wrong_card
fs_long_45:
ELSE
    jz c3_wrong_card
ENDIF
    mov word ptr [bx+CFG_MACSRC-NNET_BASE],0
    mov word ptr [bx+CFG_PROMFMT-NNET_BASE],3 ; EEPROM, not an NE PROM layout
    mov al,3                    ; leave Product ID in EEPROM data register
    call c3_ee_read
IFDEF LLFS_COMPACT
    jnc fs_long_46
    jmp c3_timeout
fs_long_46:
ELSE
    jc c3_timeout
ENDIF
    mov word ptr [bx+C3_STAGE],5
    mov ax,7000h                ; interrupt mask = 0
    call c3_command
IFDEF LLFS_COMPACT
    jnc fs_long_47
    jmp c3_timeout
fs_long_47:
ELSE
    jc c3_timeout
ENDIF
    mov ax,78FFh                ; status reasons readable, no IRQs enabled
    call c3_command
IFDEF LLFS_COMPACT
    jnc fs_long_48
    jmp c3_timeout
fs_long_48:
ELSE
    jc c3_timeout
ENDIF
    mov ax,1800h                ; RX disable
    call c3_command
IFDEF LLFS_COMPACT
    jnc fs_long_49
    jmp c3_timeout
fs_long_49:
ELSE
    jc c3_timeout
ENDIF
    mov ax,5000h                ; TX disable
    call c3_command
IFDEF LLFS_COMPACT
    jnc fs_long_50
    jmp c3_timeout
fs_long_50:
ELSE
    jc c3_timeout
ENDIF
    mov ax,2800h                ; RX FIFO reset
    call c3_command
IFDEF LLFS_COMPACT
    jnc fs_long_51
    jmp c3_timeout
fs_long_51:
ELSE
    jc c3_timeout
ENDIF
    mov ax,5800h                ; TX FIFO reset
    call c3_command
IFDEF LLFS_COMPACT
    jnc fs_long_52
    jmp c3_timeout
fs_long_52:
ELSE
    jc c3_timeout
ENDIF
    mov ax,0802h                ; station address window
    call c3_command
IFDEF LLFS_COMPACT
    jnc fs_long_53
    jmp c3_timeout
fs_long_53:
ELSE
    jc c3_timeout
ENDIF
    mov dx,[bx+CFG_NIC-NNET_BASE]
    lea si,[bx+CFG_HWMAC-NNET_BASE]
    mov cx,6
c3_station:
    lodsb
    out dx,al
    inc dx
    loop c3_station
    mov ax,0804h
    call c3_command
IFDEF LLFS_COMPACT
    jnc fs_long_54
    jmp c3_timeout
fs_long_54:
ELSE
    jc c3_timeout
ENDIF
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,10
    call c3_readw
    and ax,0FF33h               ; CRC strip on, media controls set below
    cmp word ptr [bx+CFG_MEDIA-NNET_BASE],0
    jne c3_not_tp
    or ax,40h                   ; TP jabber protection
    test word ptr [bx+CFG_SOFTINFO-NNET_BASE],4000h
    jnz c3_media_write
    or ax,80h                   ; respect EEPROM link-beat enable policy
    jmp c3_media_write
c3_not_tp:
    cmp word ptr [bx+CFG_MEDIA-NNET_BASE],1
    jne c3_media_write
    or ax,8                     ; AUI SQE statistics
c3_media_write:
    call c3_writew
    mov ax,0B800h               ; stop coax unless configured BNC
    call c3_command
IFDEF LLFS_COMPACT
    jnc fs_long_55
    jmp c3_timeout
fs_long_55:
ELSE
    jc c3_timeout
ENDIF
    cmp word ptr [bx+CFG_MEDIA-NNET_BASE],3
    jne c3_media_settle
    mov ax,1000h                ; start configured BNC DC converter
    call c3_command
IFDEF LLFS_COMPACT
    jnc fs_long_56
    jmp c3_timeout
fs_long_56:
ELSE
    jc c3_timeout
ENDIF
c3_media_settle:
    call c3_settle
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,10
    call c3_readw
    mov [bx+CFG_LINK-NNET_BASE],ax
    mov ax,0801h                ; normal operating window
    call c3_command
IFDEF LLFS_COMPACT
    jnc fs_long_57
    jmp c3_timeout
fs_long_57:
ELSE
    jc c3_timeout
ENDIF
    mov ax,9FFCh                ; TX starts only after complete frame loaded
    call c3_command
IFDEF LLFS_COMPACT
    jnc fs_long_58
    jmp c3_timeout
fs_long_58:
ELSE
    jc c3_timeout
ENDIF
    mov ax,8FFCh                ; RX early disabled
    call c3_command
IFDEF LLFS_COMPACT
    jnc fs_long_59
    jmp c3_timeout
fs_long_59:
ELSE
    jc c3_timeout
ENDIF
    mov ax,8005h                ; individual address and broadcasts only
    call c3_command
IFDEF LLFS_COMPACT
    jnc fs_long_60
    jmp c3_timeout
fs_long_60:
ELSE
    jc c3_timeout
ENDIF
    mov ax,68FFh                ; clear stale interrupt-latch reasons
    call c3_command
IFDEF LLFS_COMPACT
    jnc fs_long_61
    jmp c3_timeout
fs_long_61:
ELSE
    jc c3_timeout
ENDIF
    mov ax,4800h                ; TX enable
    call c3_command
IFDEF LLFS_COMPACT
    jnc fs_long_62
    jmp c3_timeout
fs_long_62:
ELSE
    jc c3_timeout
ENDIF
    mov ax,2000h                ; RX enable
    call c3_command
IFDEF LLFS_COMPACT
    jnc fs_long_63
    jmp c3_timeout
fs_long_63:
ELSE
    jc c3_timeout
ENDIF
    mov word ptr [bx+8],1
    mov word ptr [bx+NE_LEN],0
    mov word ptr [bx+322],0
    mov word ptr [bx+C3_STAGE],6
    jmp c3_success
IFDEF LLFS_COMPACT
FSINIT ends
_TEXT segment word public 'CODE'
ENDIF
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
    mov dx,[bx+CFG_NIC-NNET_BASE]
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
    mov dx,[bx+CFG_NIC-NNET_BASE]
    add dx,12
    call c3_readw
    mov di,bp
    add di,7
    and di,0FFFCh              ; padded packet plus 4-byte preamble
    cmp ax,di
    jb c3_busy
    mov dx,[bx+CFG_NIC-NNET_BASE]
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
    mov dx,[bx+CFG_NIC-NNET_BASE]
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
    inc word ptr [bx+CFG_TXERR-NNET_BASE]
    jmp c3_timeout
c3_tx_complete:
    xor ah,ah
    mov [bx+C3_STATUS],ax
    mov ah,al
    xor al,al
    out dx,al                  ; pop completion, never enable an interrupt
    test ah,3Ch
    jnz c3_tx_error
    inc word ptr [bx+CFG_TXOK-NNET_BASE]
    mov word ptr [bx+14],0
    jmp c3_success
c3_tx_error:
    inc word ptr [bx+CFG_TXERR-NNET_BASE]
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
    mov dx,[bx+CFG_NIC-NNET_BASE]
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
    mov dx,[bx+CFG_NIC-NNET_BASE]
c3_rx_bytes:
    in al,dx
    mov [di],al
    inc di
    loop c3_rx_bytes
    inc word ptr [bx+CFG_RXOK-NNET_BASE]
    mov ax,4000h               ; discard remainder/padding, advance RX status
    call c3_command
    ret
c3_rx_drop:
    inc word ptr [bx+CFG_RXERR-NNET_BASE]
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
