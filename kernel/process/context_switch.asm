; context_switch.asm — CPU Context Switch
; void switchContext(CpuContext* old_ctx, CpuContext* new_ctx)
; rdi = old_ctx, rsi = new_ctx
;
; NEDEN assembly? C++ calling convention register'ları kurtarmaz.
; Biz tüm callee-saved register'ları manuel kaydetmeliyiz.
;
; CpuContext struct layout (process.hpp'a göre):
;   +0:  r15
;   +8:  r14
;   +16: r13
;   +24: r12
;   +32: rbx
;   +40: rbp
;   +48: rsp
;   +56: cr3

bits 64
section .text
global switchContext

switchContext:
    ; ============================================================
    ; 1. Mevcut context'i 'old_ctx'a kaydet (rdi)
    ; ============================================================
    mov  [rdi + 0],  r15
    mov  [rdi + 8],  r14
    mov  [rdi + 16], r13
    mov  [rdi + 24], r12
    mov  [rdi + 32], rbx
    mov  [rdi + 40], rbp

    ; RSP kaydet (call'dan önceki stack pointer)
    ; NOT: 'call switchContext' RSP'yi -8 yaptı (return addr push)
    ; Onu da hesaba katmamız gerekiyor — dönerken return addr pop olur.
    mov  [rdi + 48], rsp

    ; CR3 kaydet (mevcut page table)
    mov  rax, cr3
    mov  [rdi + 56], rax

    ; ============================================================
    ; 2. Yeni context'i 'new_ctx'tan yükle (rsi)
    ; ============================================================
    mov  r15, [rsi + 0]
    mov  r14, [rsi + 8]
    mov  r13, [rsi + 16]
    mov  r12, [rsi + 24]
    mov  rbx, [rsi + 32]
    mov  rbp, [rsi + 40]

    ; CR3 yükle (gerekirse — aynı CR3 ise skip et, TLB flush önle)
    mov  rax, [rsi + 56]
    mov  rcx, cr3
    cmp  rax, rcx
    je   .skip_cr3
    mov  cr3, rax           ; Yeni address space aktif
.skip_cr3:

    ; RSP yükle — yeni process'in kernel stack'ine geç
    mov  rsp, [rsi + 48]

    ; RET: yeni process'in stack'indeki return adresine atla
    ; İlk kez çalışan process için bu adres setup sırasında konuldu
    ret

; ============================================================
; process_entry_wrapper — Yeni process ilk kez başladığında buraya gelir
; Kernel stack setup'ı bu adresi return address olarak koyar
; ============================================================
global process_entry_wrapper
process_entry_wrapper:
    ; rax'ta entry fonksiyon adresi var (setup sırasında konuldu)
    ; rbx'te argüman var (şimdilik 0)
    call rax

    ; Process'in entry fonksiyonu döndüyse → zombie yap
    ; (ProcessManager::killProcess benzerini çağır)
    ; Şimdilik sadece hlt döngüsü
.idle:
    cli
    hlt
    jmp .idle
