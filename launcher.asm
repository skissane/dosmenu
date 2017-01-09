;; DOSMENU launcher program
;;
;; We use this to avoid keeping DOSMENU from using conventional memory.
;; This launcher tries to use as little memory as possible.

; Set origin correctly for COM file
org 0x100

; Interrupt vector DOSMENU uses to communicate with LAUNCHER
LAUNCHVEC	equ	0x88

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; PROCEDURE _main
;;   Main procedure.
_main:
	; Switch stacks
	mov ax, cs
	mov ss, ax
	mov sp, topOfStack

	; Free memory we don't need, this makes sure there is enough
	; free memory to run the programs we will launch
	call _freeUnusedMemory

	; Save current disk drive
	mov ah, 0x19
	int 0x21
	mov byte [_gDrive], al

	; Save current working dir
	mov dl, al
	inc dl	; Add 1 here since int 21,19 counts drive numbers from 0
		; i.e. 0=A,1=B,2=C, etc; but int 21,47 counts them from
		; 1 instead, i.e. 1=A,2=B,3=C,...
	mov ah, 0x47
	mov si, _gDir
	clc
	int 0x21
	jc .curDirError

	; Install our vector
	cli
	call _checkHandlerZero
	call _installVector
	sti

	; Main loop
.mainLoop:

	; 1) Clear next command in data area
	mov byte [_gExec], 0
	mov byte [_gArgs], 0
	mov byte [_gPause], 0

	; 2) Restore original drive (21,E)
	mov ah, 0xE
	mov dl, [_gDrive]
	int 0x21
	jc .errorChgDrive

	; 3) Restore original directory
	mov ah, 0x3B
	mov dx, _gSlash		; Initial backslash makes it absolute
	int 0x21
	jc .errorChgDir

	; 4) Run DOSMENU.COM
	mov dx, zDOSMENU
	mov bx, zARGS
	call _runCommand
	test ax, ax
	jz .dosmenuNoError
	mov dx, mDOSMENUFail
	mov ah, 9
	int 0x21
	jmp .endMainLoop
.dosmenuNoError:

	; 5) If no command set, exit loop
	cmp byte [_gExec], 0
	je .endMainLoop

	; 6) Run the set command
	mov dx, _gExec
	mov bx, _gArgs
	call _runCommand
	test ax, ax
	jz .cmdNoError

	; 6a) If running command failed, show an message
	mov dx, mCommandFail
	mov ah, 9
	int 0x21
	mov dx, mPause  ; say "Press any key to continue..."
	mov ah, 9
	int 0x21
	xor ax,ax	; wait for "any key"
	int 0x16
	mov dx, mEOL	; print end of line
	mov ah, 9
	int 0x21
.cmdNoError:

	; 7) If pause flag set, perform pause
	cmp byte [_gPause], 0
	jz .noPause
	mov dx, mPause  ; say "Press any key to continue..."
	mov ah, 9
	int 0x21
	xor ax,ax	; wait for "any key"
	int 0x16
	mov dx, mEOL	; print end of line
	mov ah, 9
	int 0x21
.noPause:

	; 8) Continue loop
	jmp .mainLoop
.endMainLoop:

	; Remove our vector
	cli
	call _checkHandlerOurs
	call _removeVector
	sti

	; Exit successfully
	mov ax, 0x4c00
	int 0x21
.curDirError:
	mov ax, mErrorGetCWD
	call _abortMsg
.errorChgDrive:
	mov ax, mErrorChgDrive
	call _abortMsg
.errorChgDir:
	mov ax, mErrorChgDir
	call _abortMsg

;;
;; END PROCEDURE _main
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; PROCEDURE _freeUnusedMemory
;;   Initially when MS-DOS starts us, it allocates for us all available
;;   memory. We need to tell MS-DOS that we don't need that much memory;
;;   if we tell it how much we actually need, it will only give us that
;;   much. This is necessary before we can start any child process.
_freeUnusedMemory:
	mov ax, cs
	mov es, ax
	mov bx, _endOfCode ; Last address in our program is also how
			   ; much memory we need
	shr bx, 4	   ; Divide by 16 since INT 21,4A expects
			   ; units of paragraphs
	inc bx		   ; Add one more for truncation of last paragraph
	mov ah, 0x4a
	int 0x21
	ret
;;
;; END PROCEDURE _freeUnusedMemory
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; PROCEDURE _strlen
;;   Find length of ASCIIZ string
;;   Inputs
;;     BX = string whose length to return
;;   Outputs
;;     AX = length of that string
_strlen:
	xor ax, ax
_strlenLoop:
	cmp byte [bx], 0
	jz _strlenDone
	inc ax
	inc bx
	jmp _strlenLoop
_strlenDone:
	ret
;;
;; END PROCEDURE _strlen
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; PROCEDURE _runCommand
;;   Run command
;;   Inputs
;;         DS:DX = ASCIIZ name of program to run
;;	   DS:BX = ASCIIZ argument string
;;   Outputs
;;	   AX = 0 on success, non-zero on error
_runCommand:
	jmp .begin
.paramBlock:
	.pEnvSeg:	dw 0
	.pArgOff:	dw 0
	.pArgSeg:	dw 0
	.pFCB1Off:	dw 0
	.pFCB1Seg:	dw 0
	.pFCB2Off:	dw 0
	.pFCB2Seg:	dw 0
	.pArgCount:	db 0
	.pArgBytes:	times 127 db 0x0D
.begin:
	; Clear argument bytes (in case we used it before)
	push bx
	mov cx, 127
	mov bx, .pArgBytes
.clearLoop:
	test cx, cx
	jz .clearDone
	mov byte [bx], 0x0D
	inc bx
	dec cx
	jmp .clearLoop
.clearDone:
	pop bx

	; Find length of argument string
	push bx
	push dx
	call _strlen	; now AX=length of args
	pop dx
	pop bx

	; If AX > 126, reduce to 126
	cmp ax, 126
	jle .lengthReduced
	mov ax, 126
.lengthReduced:

	; Save AX in pArgCount
	inc al
	mov byte [.pArgCount], al
	dec al

	; If AX <> 0, skip arg copy
	test ax, ax
	jz .argCopyDone

	; Set first argument byte to space
	mov byte [.pArgBytes], 0x20

	; Copy argument bytes
	mov si, bx
	mov di, .pArgBytes
	inc di
.argCopyLoop:
	test ax, ax
	jz .argCopyDone
	mov cl, byte [si]
	mov byte [di], cl
	inc si
	inc di
	dec ax
	jmp .argCopyLoop
.argCopyDone:

	; Setup parameter block for call
	mov ax, ds
	mov word [.pArgSeg], ax
	mov word [.pArgOff], .pArgCount

	; Make the call
	mov es, ax
	mov ax, 0x4B00
	mov bx, .paramBlock
	int 0x21

	; Handle result
	jc .err
	xor ax, ax
.err:
	ret
;;
;; END PROCEDURE _runCommand
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; GLOBAL DATA AREA
;;
_gDrive:	db	0	; Save the disk drive. When we return
				; from program change disk back to this
_gSlash:	db 	"\"	; This is always a backslash
_gDir:		times 64 db 0	; 64 byte buffer to hold initial directory
_gExec:		times 128 db 0  ; Name of executable to run next
_gArgs:		times 128 db 0	; Command line arguments
_gPause:	db	0	; Pause flag
				; 1=pause after command is run
				; 0=don't pause
;;
;; END GLOBAL DATA AREA
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; MISCELLANEOUS CONSTANTS AREA
;;
zDOSMENU:	db "DOSMENU.COM", 0
zARGS:		db "",0
;;
;; END MISCELLANEOUS CONSTANTS AREA
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; PROCEDURE _installVector
;;   Install our interrupt handler
;; ASSUMPTIONS
;;   Interrupts disabled at call time (CLI)
;;
_installVector:
	push cs
	pop ds
	mov ah, 0x25
	mov al, LAUNCHVEC
	mov dx, _ourHandler
	int 0x21
	ret
;;
;; END PROCEDURE _installVector
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; PROCEDURE _removeVector
;;   Install our interrupt handler
;; ASSUMPTIONS
;;   Interrupts disabled at call time (CLI)
;;
_removeVector:
	push ds	; Save ds register
	xor ax, ax
	mov ds, ax
	mov ah, 0x25
	mov al, LAUNCHVEC
	xor dx, dx
	int 0x21
	pop ds	; Restore ds register
	ret
;;
;; END PROCEDURE _removeVector
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; PROCEDURE _checkHandlerZero
;;   Checks if our interrupt vector is 0000:0000. Bail out if something
;;   else.
;; ASSUMPTIONS
;;   Interrupts disabled at call time (CLI)
;;
_checkHandlerZero:

; Get current address of interrupt vector
	mov ah, 0x35
	mov al, LAUNCHVEC
	int 0x21
; Vector should be 0000:0000. Bail out if something else
	mov ax, es
	test ax, ax
	jnz _errorVECHookNZ
	test bx, bx
	jnz _errorVECHookNZ
; All is well, return to caller
	ret
; Error handling: someone else already hooked our interrupt
_errorVECHookNZ:
	; Turn interrupts back on
	sti
	mov ax, mErrorVecHook
	call _abortMsg
;;
;; END PROCEDURE _checkHandlerZero
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; PROCEDURE _checkHandlerOurs
;;   Checks if our interrupt vector is hooked by our handler.
;;   Bail out if something else.
;; ASSUMPTIONS
;;   Interrupts disabled at call time (CLI)
;;
_checkHandlerOurs:

; Get current address of interrupt vector
	mov ah, 0x35
	mov al, LAUNCHVEC
	int 0x21
; Vector should be CS:_ourHandler. Bail out if something else
	mov ax, es
	mov cx, cs
	cmp ax, cx
	jne _errorVECHookNE
	cmp bx, _ourHandler
	jne _errorVECHookNE
; All is well, return to caller
	ret
; Error handling: someone has tampered with our interrupt
_errorVECHookNE:
	; Turn interrupts back on
	sti
	mov ax, mErrorVecHookTamper
	call _abortMsg
;;
;; END PROCEDURE _checkHandlerOurs
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; PROCEDURE _copyString
;;   Copies ASCIIZ string
;;   Inputs:
;;	ES:SI = source ASCIIZ string
;;	DS:DI = destination ASCIIZ string
;;
_copyString:
	mov byte al, [es:si]
	mov byte [ds:di], al
	test al, al
	jz .end
	inc si
	inc di
	jmp _copyString
.end:
	ret
;;
;; END PROCEDURE _copyString
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; INTERRUPT HANDLER _ourHandler
;;   This is the interrupt handler DOSMENU calls to control us.
;;   Inputs:
;;	  AX = 0x1369 (magic signature)
;;     DS:BX = ASCIIZ command string (max 128 bytes)
;;     DS:SI = ASCIIZ argument string (max 128 bytes)
;;	  CX = pause flag (1=pause,0=don't pause)
;;   Outputs:
;;     AX = 0 on success, non-zero on failure
_ourHandler:
	push ds		; Save caller DS
	push es		; Save caller ES
	push cx		; Save CX argument

	; Check signature. This is to reduce the probability of
	; something going wrong if we are unexpectedly called by something
	; random.
	cmp ax, 0x1369
	jne .error

	; Move input DS to ES (ES=caller data segment)
	mov ax, ds
	mov es, ax

	; Move CS to DS (DS=our data segment)
	mov ax, cs
	mov ds, ax

	; Find length of command string
	push bx		; Save BX since _strlen trashes it
	call _strlen
	pop bx
	cmp ax, 128	; Error if longer than 128 bytes
	jg .error

	; Find length of argument string
	push bx
	mov bx, si
	call _strlen
	pop bx
	cmp ax, 128	; Error if longer than 128 bytes
	jg .error

	; Copy ES:BX to DS:_gExec
	push si
	mov si, bx
	mov di, _gExec
	call _copyString

	; Copy ES:SI to DS:_gArgs
	pop si
	mov di, _gArgs
	call _copyString

	; Save CX in _gPause
	pop cx
	mov byte [_gPause], cl

	; Return AX=0 for success
	xor ax, ax
	jmp .end

.error:	; Return AX=1 for error
	pop cx		; Forget saved CX argument
	mov ax, 0x1
.end:
	pop es		; Restore caller ES
	pop ds		; Restore caller DS
	iret
;;
;; END INTERRUPT HANDLER _ourHandler
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; PROCEDURE _abortMsg
;;   Aborts with an error message. Expects error message in AX.
;;
_abortMsg:
	; Print "ERROR: " prefix
	push ax
	mov dx, mERROR
	mov ah, 9
	int 0x21

	; Print error message
	pop dx
	mov ah, 9
	int 0x21

	; Abort
	mov ax, 4cffh
	int 0x21
;;
;; END PROCEDURE _abortMsg
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; ERROR MESSAGE AREA
;;   Stores text of error messages.
;;
mERROR:		db "ERROR: $"
mEOL:		db 13,10,'$'
mPause:		db "Press any key to continue...$"

mDOSMENUFail:	db "running DOSMENU.COM failed",13,10,'$'
mCommandFail:   db "running menu item failed",13,10,'$'
mErrorVecHook:  db "Launch vector already hooked",13,10,'$'
mErrorVecHookTamper:
		db "Launch vector has been tampered with",13,10,'$'
mErrorGetCWD:	db "could not determine current directory",13,10,'$'
mErrorChgDrive:	db "could not restore original drive",13,10,'$'
mErrorChgDir:	db "could not restore original directory",13,10,'$'

;;
;; END ERROR MESSAGE AREA
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; STACK AREA
;;   This data area is reserved for the stack.
;;
STACKSIZE	equ 256		; Hopefully this is enough
stackArea:	times STACKSIZE db 0
topOfStack:

;;
;; END STACK AREA
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

_endOfCode:
	; This must point to end of code

;; vim: cc=75 ft=nasm
