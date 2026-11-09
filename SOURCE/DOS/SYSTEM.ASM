
.8086
.model small
.data
supkey db 'AUSUP=1',0
.code
public _dos_supervised
_dos_supervised proc near
 push bx
 push cx
 push dx
 push si
 push di
 push es
 mov ah,62h
 int 21h
 mov es,bx
 mov ax,es:[2ch]
 or ax,ax
 jz absent
 mov es,ax
 xor si,si
 mov cx,32768
nextvar:
 cmp byte ptr es:[si],0
 je absent
 push si
 mov di,offset supkey
compare:
 mov al,es:[si]
 cmp al,[di]
 jne skip_saved
 inc si
 inc di
 dec cx
 jz absent_saved
 or al,al
 jnz compare
 pop si
 mov ax,1
 jmp done
skip_saved:
 pop si
skipvar:
 mov al,es:[si]
 inc si
 dec cx
 jz absent
 or al,al
 jnz skipvar
 jmp nextvar
absent_saved:
 pop si
absent:
 xor ax,ax
done:
 pop es
 pop di
 pop si
 pop dx
 pop cx
 pop bx
 ret
_dos_supervised endp
end
