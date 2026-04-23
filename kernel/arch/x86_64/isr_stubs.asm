; isr_stubs.asm — Interrupt Service Routine Assembly Stub'ları
; Her interrupt öncesi tüm register'ları kaydeder,
; C++ handler'ı çağırır, sonra geri yükler.
bits 64

; C++ handler bildirimleri
extern exception_handler
extern timer_handler
extern keyboard_handler
extern mouse_handler

; ============================================================
; MACRO: Register kaydet/yükle
; ============================================================
%macro SAVE_REGS 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro RESTORE_REGS 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

; ============================================================
; MACRO: Error code olmayan exception stub
; vector numarasını ve 0 error_code ile exception_handler'a iletir
; ============================================================
%macro ISR_NOERRCODE 1
global isr_stub_%1
isr_stub_%1:
    push qword 0            ; Dummy error code
    push qword %1           ; Vector numarası
    jmp  isr_common
%endmacro

; ============================================================
; MACRO: Error code olan exception stub
; CPU zaten error code'u stack'e itmişti
; ============================================================
%macro ISR_ERRCODE 1
global isr_stub_%1
isr_stub_%1:
    push qword %1           ; Vector numarası (error code zaten stack'te)
    jmp  isr_common
%endmacro

; Exception stub'ları
ISR_NOERRCODE 0    ; Divide by zero
ISR_NOERRCODE 1    ; Debug
ISR_NOERRCODE 2    ; NMI
ISR_NOERRCODE 3    ; Breakpoint
ISR_NOERRCODE 6    ; Invalid opcode
ISR_ERRCODE   8    ; Double fault (error code var)
ISR_ERRCODE   13   ; General Protection Fault
ISR_ERRCODE   14   ; Page Fault

; ============================================================
; Ortak exception işleyici
; Stack düzeni: [rsp] = vector, [rsp+8] = error_code, sonra InterruptFrame
; ============================================================
isr_common:
    SAVE_REGS

    ; exception_handler(vector, error_code, frame*)
    ; Linux x86_64 calling convention: rdi, rsi, rdx
    mov  rdi, [rsp + 15*8]      ; vector (15 register push'tan sonra)
    mov  rsi, [rsp + 15*8 + 8]  ; error_code
    lea  rdx, [rsp + 15*8 + 16] ; InterruptFrame pointer

    call exception_handler

    RESTORE_REGS
    add  rsp, 16                ; vector + error_code'u temizle
    iretq

; ============================================================
; IRQ Stub'ları
; ============================================================
%macro IRQ_STUB 2               ; %1 = IRQ numarası, %2 = C handler
global irq_stub_%1
irq_stub_%1:
    SAVE_REGS
    ; handler(frame*)
    lea  rdi, [rsp + 15*8]      ; InterruptFrame pointer
    call %2
    RESTORE_REGS
    iretq
%endmacro

IRQ_STUB 0, timer_handler
IRQ_STUB 1, keyboard_handler
IRQ_STUB 12, mouse_handler
