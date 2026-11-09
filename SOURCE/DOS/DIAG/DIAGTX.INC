; Diagnostic-only hooks; production AUTOUAT's NIC source/binary are unchanged.
.data
extrn _diag_tx_reads:word,_diag_tx_matches:word,_diag_tx_mismatch:word,_diag_tx_expect:word,_diag_tx_actual:word
extrn _diag_tx_last_len:word,_diag_tx_data_len:word,_diag_tx_source:word,_diag_tx_done:word,_diag_tx_error:word,_diag_tx_timeout:word
extrn _diag_tx_isr:word,_diag_tx_tsr:word,_diag_tx_cr:word,_diag_tx_loop:word,_diag_loop_result:word,_diag_loop_rxbytes:word,_diag_arp_saved:word
extrn _diag_tx_ram:byte,_diag_loop_frame:byte,_diag_loop_rx:byte,_diag_arp_samples:byte,_diag_loop_length:dword
.code

; BX=raw NIC state. Verify all submitted bytes and zero padding before TXP.
diag_verify_tx:
 push ax
 push cx
 push dx
 push si
 push di
 push bp
 push es
 inc word ptr _diag_tx_reads
 mov _diag_tx_last_len,bp
 mov _diag_tx_data_len,di
 mov ax,si
 sub ax,di
 mov _diag_tx_source,ax
 mov word ptr _diag_tx_mismatch,0FFFFh
 mov cx,bp
 mov ax,4000h
 mov di,offset _diag_tx_ram
 call ne_read_dma
 jc diag_verify_error
 mov cx,_diag_tx_data_len
 mov si,_diag_tx_source
 mov di,offset _diag_tx_ram
 jcxz diag_verify_pad_start
diag_verify_byte:
 lodsb
 cmp al,[di]
 jne diag_verify_mismatch
 inc di
 loop diag_verify_byte
diag_verify_pad_start:
 mov cx,_diag_tx_last_len
 sub cx,_diag_tx_data_len
 jcxz diag_verify_good
 xor al,al
diag_verify_pad:
 cmp al,[di]
 jne diag_verify_mismatch
 inc di
 loop diag_verify_pad
diag_verify_good:
 inc word ptr _diag_tx_matches
 clc
 jmp diag_verify_return
diag_verify_mismatch:
 xor ah,ah
 mov _diag_tx_expect,ax
 mov al,[di]
 mov _diag_tx_actual,ax
 mov ax,di
 sub ax,offset _diag_tx_ram
 mov _diag_tx_mismatch,ax
diag_verify_error:
 inc word ptr _diag_tx_error
 stc
diag_verify_return:
 pop es
 pop bp
 pop di
 pop si
 pop dx
 pop cx
 pop ax
 ret

; BX=raw NIC state. Capture registers without reissuing TXP or restarting STP.
diag_capture_regs:
 push ax
 push cx
 push dx
 mov dx,[bx+CFG_NIC-1920]
 in al,dx
 xor ah,ah
 mov _diag_tx_cr,ax
 and al,3
 or al,20h
 mov cl,al
 add dx,7
 in al,dx
 mov _diag_tx_isr,ax
 sub dx,3
 in al,dx
 mov _diag_tx_tsr,ax
 mov dx,[bx+CFG_NIC-1920]
 mov al,cl
 or al,80h
 out dx,al
 add dx,0Dh
 in al,dx
 mov _diag_tx_loop,ax
 mov dx,[bx+CFG_NIC-1920]
 mov al,cl
 out dx,al
 pop dx
 pop cx
 pop ax
 ret

; PTX proves NIC completion only; firmware Wi-Fi transmit success is separate.
diag_wait_tx:
 push ax
 push dx
 push bp
 call ip_ticks
 mov bp,ax
 mov dx,[bx+CFG_NIC-1920]
 add dx,7
diag_wait_tx_poll:
 in al,dx
 test al,8
 jnz diag_wait_tx_error
 test al,2
 jnz diag_wait_tx_done
 call ip_ticks
 sub ax,bp
 cmp ax,36
 jb diag_wait_tx_poll
 inc word ptr _diag_tx_timeout
 jmp diag_wait_tx_finish
diag_wait_tx_error:
 inc word ptr _diag_tx_error
 jmp diag_wait_tx_finish
diag_wait_tx_done:
 inc word ptr _diag_tx_done
diag_wait_tx_finish:
 call diag_capture_regs
 pop bp
 pop dx
 pop ax
 ret

; Called on validated-length ARP frames; BX is the full NETWORK state.
diag_save_arp:
 push ax
 push cx
 push dx
 push si
 push di
 mov ax,_diag_arp_rx
 dec ax
 and ax,3
 mov dx,28
 mul dx
 mov di,offset _diag_arp_samples
 add di,ax
 lea si,[bx+IP_FRAME+14]
 mov cx,28
 call ip_copy
 cmp word ptr _diag_arp_saved,4
 jae diag_arp_saved
 inc word ptr _diag_arp_saved
diag_arp_saved:
 pop di
 pop si
 pop dx
 pop cx
 pop ax
 ret

public _diag_regs,_diag_loop_test
_diag_regs proc near
 DPROLOG
 add bx,1920
 call diag_capture_regs
 DEPILOG
_diag_regs endp
_diag_loop_test proc near
 DPROLOG
 add bx,1920
 mov word ptr _diag_loop_result,0
 mov word ptr _diag_loop_rxbytes,0
 call diag_capture_regs
 cmp word ptr _diag_tx_loop,0
 jne diag_loop_unsafe
 mov dx,[bx+CFG_NIC-1920]
 add dx,0Dh
 mov al,2
 out dx,al
 call diag_capture_regs
 cmp word ptr _diag_tx_loop,2
 jne diag_loop_unsafe
 mov si,offset _diag_loop_frame
 mov cx,60
 mov di,34
 call raw_ne_start
 or ax,ax
 jnz diag_loop_error
 call ip_ticks
 mov bp,ax
diag_loop_receive:
 mov word ptr _diag_loop_length,1536
 mov word ptr _diag_loop_length+2,0
 mov ax,offset _diag_loop_length
 mov di,35
 call raw_ne_start
 or ax,ax
 jnz diag_loop_error
 or cx,cx
 jnz diag_loop_received
 call ip_ticks
 sub ax,bp
 cmp ax,36
 jb diag_loop_receive
 mov ax,1460
 jmp diag_loop_error
diag_loop_received:
 mov _diag_loop_rxbytes,cx
 cmp cx,1536
 ja diag_loop_unsafe
 mov di,offset _diag_loop_rx
 push ds
 pop es
 rep movsb
 jmp diag_loop_restore
diag_loop_unsafe:
 mov ax,5
diag_loop_error:
 mov _diag_loop_result,ax
diag_loop_restore:
 mov dx,[bx+CFG_NIC-1920]
 add dx,0Dh
 xor al,al
 out dx,al
 call diag_capture_regs
 mov ax,_diag_loop_result
 DEPILOG
_diag_loop_test endp
