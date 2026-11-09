; DOS adapter replacing Win16 serial/event/GUI runtime. No callbacks.
net_rs_start:
    ret
rs_start_clock:
    call ip_ticks
    mov [bx+342],ax
    clc
    ret
rs_expired:
    call ip_ticks
    sub ax,[bx+342]
    cmp ax,[bx+6]
    cmc
    ret
rs_set_connected:
    mov word ptr [bx+8],1
    ret
rs_set_disconnected:
    mov word ptr [bx+8],0
    ret
rs_done:
    pop es
    pop bp
    ret
rs_empty:
    xor cx,cx
    jmp rs_done
