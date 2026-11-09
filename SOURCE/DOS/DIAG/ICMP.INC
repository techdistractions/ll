; Called only after IPv4 checksum, bounds, source/destination and fragment checks.
diag_icmp_read:
 mov ax,[bx+IP_END]
 add ax,bx
 add ax,IP_FRAME
 sub ax,di
 cmp ax,40
 jb ip_pump_timer
 cmp word ptr [di],0
 jne ip_pump_timer
 cmp word ptr [di+4],5541h
 jne ip_pump_timer
 mov cx,_diag_ping_seq
 xchg cl,ch
 cmp [di+6],cx
 jne ip_pump_timer
 mov _diag_ping_bytes,ax
 mov cx,ax
 mov si,di
 call ip_sum
 or ax,ax
 jnz ip_pump_timer
 xor ax,ax
 mov al,[bx+IP_FRAME+22]
 mov _diag_ping_ttl,ax
 mov word ptr _diag_ping_reply,1
 jmp ip_pump_timer
