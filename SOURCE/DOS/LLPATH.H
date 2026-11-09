/* LiteLink source guide:
 * Shared DOS executable-relative master-INI lookup.
 * Read the DOS 3.3+ PSP environment trailer, prefer LL.INI beside the EXE, then CWD.
 * Callers supply LL_OPEN/LL_LEN/LL_COPY; this header is included, not linked.
 */
/* DOS 3.3+ executable path from the PSP environment trailer. No PATH changes. */
static int ll_exepath(char*);
#pragma aux ll_exepath = "push es" "mov ah,62h" "int 21h" "mov es,bx" "mov ax,es:[2ch]" "or ax,ax" "jz short EF" "mov es,ax" "xor si,si" "mov cx,32760" "E0:" "mov al,es:[si]" "inc si" "dec cx" "jz short EF" "or al,al" "jnz E0" "cmp byte ptr es:[si],0" "jne E0" "inc si" "cmp word ptr es:[si],0" "je short EF" "add si,2" "mov cx,127" "E1:" "mov al,es:[si]" "inc si" "mov [di],al" "inc di" "or al,al" "jz short EO" "loop E1" "mov byte ptr [di],0" "EF:" "xor ax,ax" "jmp short ED" "EO:" "mov ax,1" "ED:" "pop es" parm [di] value [ax] modify [ax bx cx si di];
static char ll_masterpath[128];
static int ll_open_master(void){unsigned n;int f;if(ll_exepath(ll_masterpath)){n=LL_LEN(ll_masterpath);while(n&&ll_masterpath[n-1]!='\\')n--;if(n&&n+7<sizeof(ll_masterpath)){LL_COPY(ll_masterpath+n,"LL.INI");f=LL_OPEN(ll_masterpath);if(f>=0)return f;}}LL_COPY(ll_masterpath,"LL.INI");return LL_OPEN(ll_masterpath);}
#undef LL_OPEN
#undef LL_LEN
#undef LL_COPY
