.8086
.model small
.data
extrn _fs_regs:word,_fs_done:word,_fs_stack:byte,_fs_psp:word,_fs_info:word,_fs_vector:word,_fs_arg:word
.code
public _fs_install,_fs_control,_fs_restore
extrn _fs_dispatch:near
fs_ds dw 0
fs_ss dw 0
fs_sp dw 0
fs_old_off dw 0
fs_old_seg dw 0
fs_active db 0
fs_handler proc far
 cmp ah,11h
 je fs_enter
 cmp ah,0d7h
 je fs_enter
 jmp dword ptr cs:fs_old_off
fs_enter:
 cmp cs:fs_active,0
 je fs_available
 jmp dword ptr cs:fs_old_off
fs_available:
 mov cs:fs_active,1
 push ds
 push es
 push di
 push si
 push bp
 push dx
 push cx
 push bx
 push ax
 mov bp,sp
 mov ax,cs:fs_ds
 mov ds,ax
 mov ax,ss
 mov cs:fs_ss,ax
 mov cs:fs_sp,sp
 push ds
 pop es
 cld
 mov di,offset _fs_regs
 xor si,si
 mov cx,12
fs_copy_in:
 mov ax,ss:[bp+si]
 stosw
 add si,2
 loop fs_copy_in
 mov ax,ss:[bp+24]
 mov _fs_arg,ax
 cli
 mov ax,ds
 mov ss,ax
 mov sp,offset _fs_stack+510
 sti
 cld
 call _fs_dispatch
 cli
 mov ax,cs:fs_ss
 mov ss,ax
 mov sp,cs:fs_sp
 cmp _fs_done,0
 je fs_chain
 mov bp,sp
 xor si,si
 mov di,offset _fs_regs
 mov cx,12
fs_copy_out:
 mov ax,[di]
 mov ss:[bp+si],ax
 add di,2
 add si,2
 loop fs_copy_out
 mov cs:fs_active,0
 pop ax
 pop bx
 pop cx
 pop dx
 pop bp
 pop si
 pop di
 pop es
 pop ds
 iret
fs_chain:
 mov cs:fs_active,0
 pop ax
 pop bx
 pop cx
 pop dx
 pop bp
 pop si
 pop di
 pop es
 pop ds
 jmp dword ptr cs:fs_old_off
fs_handler endp
_TEXT ends
FSINIT segment word public 'INITCODE'
_fs_install proc near
 push ax
 push bx
 push dx
 push es
 mov ax,ds
 mov cs:fs_ds,ax
 mov ax,352fh
 int 21h
 mov cs:fs_old_off,bx
 mov cs:fs_old_seg,es
 mov _fs_vector,bx
 mov _fs_vector+2,es
 mov _fs_vector+4,offset fs_handler
 mov ax,cs
 mov _fs_vector+6,ax
 push ds
 push cs
 pop ds
 mov dx,offset fs_handler
 mov ax,252fh
 int 21h
 pop ds
 pop es
 pop dx
 pop bx
 pop ax
 ret
_fs_install endp
; AX=0 probe, AX=1 stop. Results are written into the transient instance's DS.
_fs_control proc near
 push bx
 push cx
 push dx
 push si
 push di
 push bp
 push es
 add ax,0d700h
 xor bx,bx
 xor cx,cx
 cmp ax,0d700h
 je fs_ctl_probe
 mov bx,4c4ch
 mov cx,4653h
fs_ctl_probe:
 int 2fh
 mov _fs_info,ax
 mov _fs_info+2,bx
 mov _fs_info+4,cx
 mov _fs_info+6,dx
 mov _fs_info+8,si
 mov _fs_info+10,di
 mov _fs_info+12,bp
 mov _fs_info+14,es
 pop es
 pop bp
 pop di
 pop si
 pop dx
 pop cx
 pop bx
 ret
_fs_control endp
_fs_restore proc near
 push ax
 push dx
 push ds
 mov dx,_fs_info+8
 mov ax,_fs_info+10
 mov ds,ax
 mov ax,252fh
 int 21h
 pop ds
 pop dx
 pop ax
 ret
_fs_restore endp
FSINIT ends
_TEXT segment word public 'CODE'
end
