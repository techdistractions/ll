.8086
.model tiny
.code
org 100h
start:
 push cs
 pop ds
 push cs
 pop es
 cli
 mov ax,cs
 mov ss,ax
 mov sp,offset mon_stack_top
 sti
 mov bx,offset mon_end
 add bx,15
 mov cl,4
 shr bx,cl
 mov ah,4ah
 int 21h
 jc failure
 mov word ptr hdr_self,cs
 mov ax,ds:[16h]
 mov hdr_parent,ax
 mov ax,ds:[2ch]
 mov hdr_env,ax
 mov ax,cs
 dec ax
 mov es,ax
 mov ax,es:[3]
 mov hdr_ownblock,ax
 push cs
 pop es
 int 12h
 mov hdr_conventional,ax
 mov ah,30h
 int 21h
 mov hdr_dos,ax
 ; Parse one bounded 8.3-path output argument from the ordinary PSP tail.
 xor cx,cx
 mov cl,ds:[80h]
 mov si,81h
skip_spaces:
 or cx,cx
 jz failure
 lodsb
 dec cx
 cmp al,' '
 je skip_spaces
 dec si
 inc cx
 mov di,offset outpath
 xor bx,bx
copy_argument:
 jcxz argument_done
 lodsb
 cmp al,' '
 je argument_done
 cmp al,13
 je argument_done
 mov [di],al
 inc di
 inc bx
 cmp bx,63
 jae failure
 dec cx
 jmp copy_argument
argument_done:
 mov byte ptr [di],0
 or bx,bx
 jz failure
 ; AUPSP.DAT lives alongside the measurement output, not at the job cwd.
 mov si,offset outpath
 mov di,offset markerpath
 mov bx,di
marker_copy:
 lodsb
 or al,al
 jz marker_name
 mov [di],al
 inc di
 cmp al,'\'
 jne marker_copy
 mov bx,di
 jmp marker_copy
marker_name:
 mov di,bx
 mov si,offset markerleaf
marker_leaf:
 lodsb
 mov [di],al
 inc di
 or al,al
 jnz marker_leaf
 mov dx,offset markerpath
 mov ax,3d00h
 int 21h
 jc marker_done
 mov bx,ax
 mov dx,offset prior_psp
 mov cx,8
 mov ah,3fh
 int 21h
 push ax
 mov ah,3eh
 int 21h
 pop ax
 cmp ax,8
 jne marker_done
 cmp word ptr prior_psp,5350h
 jne marker_done
 cmp word ptr prior_psp+2,3350h
 jne marker_done
 mov ax,word ptr prior_psp+4
 mov hdr_prior,ax
 cmp ax,hdr_self
 jne marker_not_reused
 or word ptr hdr_flags,4
marker_not_reused:
 mov ax,word ptr prior_psp+6
 mov hdr_priorblock,ax
marker_done:
 mov ax,3508h
 int 21h
 mov word ptr hdr_irq8,bx
 mov word ptr hdr_irq8+2,es
 mov ax,3521h
 int 21h
 mov word ptr hdr_irq21,bx
 mov word ptr hdr_irq21+2,es
 mov ah,52h
 int 21h
 mov ax,es:[bx-2]
 mov hdr_first,ax
 mov scan_seg,ax
scan_loop:
 cmp word ptr hdr_count,128
 jae failure
 call get_record
 or ax,ax
 jz failure
 inc word ptr hdr_count
 mov ax,word ptr mcb_record+4
 or ax,ax
 jnz allocated
 mov ax,word ptr mcb_record+6
 add hdr_free,ax
 jmp scan_next
allocated:
 cmp ax,hdr_self
 jne not_own
 mov ax,word ptr mcb_record+6
 inc ax
 add hdr_ownparas,ax
 jmp scan_next
not_own:
 cmp ax,hdr_prior
 jne name_check
 inc word ptr hdr_oldowners
name_check:
 cmp word ptr mcb_record+8,5541h
 jne scan_next
 cmp word ptr mcb_record+10,4f54h
 jne scan_next
 cmp word ptr mcb_record+12,4155h
 jne scan_next
 cmp byte ptr mcb_record+14,'T'
 jne scan_next
 inc word ptr hdr_agentnames
scan_next:
 cmp byte ptr mcb_record+2,'Z'
 je scan_complete
 mov ax,next_seg
 mov scan_seg,ax
 jmp scan_loop
scan_complete:
 or word ptr hdr_flags,3
 mov bx,0ffffh
 mov ah,48h
 int 21h
 jnc unexpected_allocate
 mov hdr_largest,bx
 jmp write_result
unexpected_allocate:
 mov es,ax
 mov ah,49h
 int 21h
 jmp failure
write_result:
 mov dx,offset outpath
 xor cx,cx
 mov ah,3ch
 int 21h
 jc failure
 mov handle,ax
 mov bx,ax
 mov dx,offset mem_header
 mov cx,48
 mov ah,40h
 int 21h
 jc write_failure
 cmp ax,48
 jne write_failure
 mov ax,hdr_first
 mov scan_seg,ax
write_loop:
 call get_record
 or ax,ax
 jz write_failure
 mov bx,handle
 mov dx,offset mcb_record
 mov cx,16
 mov ah,40h
 int 21h
 jc write_failure
 cmp ax,16
 jne write_failure
 cmp byte ptr mcb_record+2,'Z'
 je successful
 mov ax,next_seg
 mov scan_seg,ax
 jmp write_loop
successful:
 mov bx,handle
 mov ah,3eh
 int 21h
 jc failure
 mov ax,4c00h
 int 21h
write_failure:
 mov bx,handle
 mov ah,3eh
 int 21h
failure:
 mov ax,4c02h
 int 21h
get_record proc near
 mov ax,scan_seg
 cmp ax,0a000h
 jae bad_record
 mov es,ax
 mov word ptr mcb_record,ax
 mov al,es:[0]
 cmp al,'M'
 je type_ok
 cmp al,'Z'
 jne bad_record
type_ok:
 mov byte ptr mcb_record+2,al
 mov ax,es:[1]
 mov word ptr mcb_record+4,ax
 mov ax,es:[3]
 mov word ptr mcb_record+6,ax
 inc ax
 add ax,scan_seg
 jc bad_record
 cmp ax,scan_seg
 jbe bad_record
 mov next_seg,ax
 mov si,8
 mov di,offset mcb_record+8
 mov cx,8
copy_name:
 mov al,es:[si]
 mov [di],al
 inc si
 inc di
 loop copy_name
 mov ax,1
 ret
bad_record:
 xor ax,ax
 ret
get_record endp
mem_header db 'MEM3'
 dw 3
hdr_self dw 0
hdr_parent dw 0
hdr_first dw 0
hdr_prior dw 0
hdr_priorblock dw 0
hdr_ownblock dw 0
hdr_free dw 0
hdr_largest dw 0
hdr_oldowners dw 0
hdr_count dw 0
hdr_flags dw 0
hdr_irq8 dd 0
hdr_irq21 dd 0
hdr_env dw 0
hdr_ownparas dw 0
hdr_conventional dw 0
hdr_dos dw 0
hdr_agentnames dw 0
 dw 0
scan_seg dw 0
next_seg dw 0
handle dw 0
mcb_record db 16 dup(0)
outpath db 64 dup(0)
markerpath db 80 dup(0)
markerleaf db 'AUPSP.DAT',0
prior_psp db 8 dup(0)
mon_stack db 512 dup(0)
mon_stack_top label byte
mon_end label byte
end start
