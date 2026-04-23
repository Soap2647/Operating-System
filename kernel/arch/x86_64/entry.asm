; entry.asm — MertOS Kernel Entry Point
; GRUB Multiboot2 header + 32-bit → 64-bit long mode geçişi
; NASM syntax, x86_64-elf hedef

bits 32                         ; GRUB bizi 32-bit protected mode'da başlatır

; ============================================================
; MULTIBOOT2 HEADER
; GRUB bu sihirli sayıyı arar ve kernel'i buna göre yükler
; ============================================================
section .multiboot2
align 8
mb2_header_start:
    dd  0xE85250D6              ; Multiboot2 magic number
    dd  0                       ; Architecture: i386 (protected mode)
    dd  mb2_header_end - mb2_header_start   ; Header uzunluğu
    ; Checksum: tüm alanların toplamı 0 olmalı (mod 2^32)
    dd  0x100000000 - (0xE85250D6 + 0 + (mb2_header_end - mb2_header_start))

    ; --- Framebuffer Tag (isteğe bağlı, şimdilik VGA text mode) ---
    ; Boş tag → header sonu
    dw  0                       ; Type: end tag
    dw  0                       ; Flags
    dd  8                       ; Size
mb2_header_end:

; ============================================================
; KERNEL STACK
; 16KB stack — .bss'e koyuyoruz (başlangıçta sıfır)
; ============================================================
section .bss
align 16
stack_bottom:
    resb 16384                  ; 16 KB
stack_top:

; Sayfa tablolarımız için yer (4KB hizalı)
align 4096
pml4_table:     resb 4096       ; Page Map Level 4
pdp_table:      resb 4096       ; Page Directory Pointer
pd_table_0:     resb 4096       ; PD: 0GB - 1GB
pd_table_1:     resb 4096       ; PD: 1GB - 2GB
pd_table_2:     resb 4096       ; PD: 2GB - 3GB
pd_table_3:     resb 4096       ; PD: 3GB - 4GB  (framebuffer buraya denk geliyor)

; ============================================================
; BOOT CODE — 32-bit
; ============================================================
section .text
global _start
extern kmain                    ; C++ kernel entry
extern __init_array_start       ; Global constructor tablosu
extern __init_array_end

_start:
    ; GRUB bilgileri: EAX = magic (0x36D76289), EBX = multiboot info pointer
    ; BSS sıfırlamadan önce kaydet (GRUB stack'i hâlâ geçerli)
    push eax                    ; magic'i geçici stack'e kaydet
    mov  esi, ebx               ; info ptr → ESI (BSS zero'dan korunur)

    ; BSS'i 32-bit modda sıfırla — page table kurulumundan ÖNCE!
    ; (page table'lar .bss'te, sonra sıfırlanırsa mapping silinir)
    cld
    extern __bss_start
    extern __bss_end
    mov  edi, __bss_start
    mov  ecx, __bss_end
    sub  ecx, edi
    xor  eax, eax
    rep  stosb

    ; Kayıtlı değerleri geri yükle
    pop  eax                    ; magic → EAX
    mov  edi, eax               ; magic → EDI (kmain 1. parametre olacak)
    ; ESI zaten mb info ptr

    ; Stack kur (BSS artık sıfır, stack alanı temiz)
    mov esp, stack_top

    ; CPU özelliklerini kontrol et (Long Mode desteği var mı?)
    call check_cpuid
    call check_long_mode

    ; Sayfa tablolarını kur (identity mapping: virt == phys)
    call setup_page_tables

    ; CR3'e PML4 adresini yaz
    mov eax, pml4_table
    mov cr3, eax

    ; PAE (Physical Address Extension) aktif et
    mov eax, cr4
    or  eax, (1 << 5)           ; PAE bit
    mov cr4, eax

    ; Long Mode'u EFER MSR'de aktif et
    mov ecx, 0xC0000080         ; EFER MSR
    rdmsr
    or  eax, (1 << 8)           ; LME (Long Mode Enable)
    wrmsr

    ; Paging'i aktif et + Protection Enable
    mov eax, cr0
    or  eax, (1 << 31) | (1 << 0)  ; PG | PE
    mov cr0, eax

    ; GDT yükle (64-bit code segment)
    lgdt [gdt64.pointer]

    ; Far jump → 64-bit code segment'e gir
    jmp gdt64.code:long_mode_start

; ============================================================
; YARDIMCI FONKSIYONLAR (32-bit)
; ============================================================

; CPUID destekli mi kontrol et (EFLAGS ID bit)
check_cpuid:
    pushfd
    pop  eax
    mov  ecx, eax
    xor  eax, (1 << 21)        ; ID bit'i toggle et
    push eax
    popfd
    pushfd
    pop  eax
    push ecx
    popfd
    xor  eax, ecx
    jz   .no_cpuid
    ret
.no_cpuid:
    mov al, 'C'                 ; Hata kodu: CPUID yok
    jmp error_halt

; Long Mode destekli mi?
check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb  .no_long_mode
    mov eax, 0x80000001
    cpuid
    test edx, (1 << 29)        ; LM bit
    jz  .no_long_mode
    ret
.no_long_mode:
    mov al, 'L'
    jmp error_halt

; Sayfa tabloları: identity mapping (0 - 4GB)
; Framebuffer 0xE0000000 (3.5GB) bölgesini kapsamak için 4 PD tablosu
setup_page_tables:
    push edi                    ; EDI'yi koru (MB2 magic orada saklanıyor!)
    ; PML4[0] → PDP table
    mov eax, pdp_table
    or  eax, 0b11               ; Present | Writable
    mov [pml4_table], eax

    ; PDP[0] → pd_table_0  (0GB - 1GB)
    mov eax, pd_table_0
    or  eax, 0b11
    mov [pdp_table + 0*8], eax

    ; PDP[1] → pd_table_1  (1GB - 2GB)
    mov eax, pd_table_1
    or  eax, 0b11
    mov [pdp_table + 1*8], eax

    ; PDP[2] → pd_table_2  (2GB - 3GB)
    mov eax, pd_table_2
    or  eax, 0b11
    mov [pdp_table + 2*8], eax

    ; PDP[3] → pd_table_3  (3GB - 4GB) ← framebuffer burada
    mov eax, pd_table_3
    or  eax, 0b11
    mov [pdp_table + 3*8], eax

    ; 4 PD tablosu yan yana: 2048 adet 2MB entry
    ; Her entry: fiziksel adres = ecx * 2MB, 4 tablo ardışık → doğrudan indexlenir
    mov edi, pd_table_0
    mov ecx, 0
.map_pd:
    mov eax, 0x200000           ; 2MB
    mul ecx                     ; EAX = ecx * 2MB (max: 2047 * 2MB = 0xFFE00000, 32-bit'e sığar)
    or  eax, 0b10000011         ; Present | Writable | Huge Page
    mov [edi + ecx * 8], eax    ; 4 tablo ardışık → tek döngüyle tümü doldurulur
    inc ecx
    cmp ecx, 2048               ; 4 × 512 = 2048 entry
    jne .map_pd
    pop edi                     ; EDI'yi geri yükle
    ret

; Hata durumu: AL = hata kodu harfi, VGA'ya yaz ve dur
error_halt:
    mov dword [0xB8000], 0x4F00 | 0x4F45   ; 'E' kırmızı arka plan
    mov byte  [0xB8001], 0x4F              ; Attribute
    hlt

; ============================================================
; 64-BIT GDT (Global Descriptor Table)
; Long mode'da segment descriptor'ları minimal — paging önemli
; ============================================================
section .rodata
gdt64:
    ; Null descriptor (zorunlu)
    dq 0
.code: equ $ - gdt64
    ; Code segment: Execute/Read, 64-bit
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)
.data: equ $ - gdt64
    ; Data segment: Read/Write
    dq (1 << 41) | (1 << 44) | (1 << 47)
.pointer:
    dw $ - gdt64 - 1            ; Limit
    dq gdt64                    ; Base

; ============================================================
; LONG MODE ENTRY (64-bit)
; ============================================================
bits 64
section .text
long_mode_start:
    ; Data segment register'larını güncelle
    mov ax, gdt64.data
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; BSS 32-bit modda sıfırlandı — burada tekrar sıfırlama yok

    ; Global constructor'ları çağır (C++ static nesneler için)
    ; Bu RAII ile oluşturulan nesnelerin constructor'larını tetikler
    call call_constructors

    ; kmain'i çağır (RDI = multiboot magic, RSI = info pointer zaten set edildi)
    call kmain

    ; kmain dönerse → kernel panic
    cli
    hlt

; Global constructor'ları çağır
call_constructors:
    mov  rbx, __init_array_start
.loop:
    cmp  rbx, __init_array_end
    jge  .done
    call [rbx]                  ; Her constructor pointer'ı çağır
    add  rbx, 8
    jmp  .loop
.done:
    ret
