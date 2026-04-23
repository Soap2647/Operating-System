// idt.hpp — Interrupt Descriptor Table (IDT)
// OOP: IDT sınıfı interrupt handler'ları kapsüller
// Midterm notu: Polymorphism'e zemin hazırlıyor
//   (InterruptHandler base class → override ile farklı davranış)

#pragma once
#include <stdint.h>

// IDT Gate Descriptor — 16 byte (x86_64)
struct IDTGate {
    uint16_t offset_low;        // Handler adresi [0:15]
    uint16_t selector;          // Code segment selector
    uint8_t  ist;               // Interrupt Stack Table offset
    uint8_t  type_attr;         // Type + DPL + Present
    uint16_t offset_mid;        // Handler adresi [16:31]
    uint32_t offset_high;       // Handler adresi [32:63]
    uint32_t zero;              // Reserved
} __attribute__((packed));

// IDTR register pointer
struct IDTPointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// CPU'nun interrupt sırasında stack'e ittiği frame
struct InterruptFrame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed));

// ============================================================
// IDT Sınıfı
// Midterm OOP Bağlantısı:
//   - Encapsulation: gate'ler private
//   - Polymorphism zemini: registerHandler() → farklı handler'lar
// ============================================================
class IDT {
public:
    static constexpr int IDT_ENTRIES = 256;

    // Standart exception vektörleri
    static constexpr int EX_DIVIDE_BY_ZERO   = 0;
    static constexpr int EX_DEBUG            = 1;
    static constexpr int EX_NMI              = 2;
    static constexpr int EX_BREAKPOINT       = 3;
    static constexpr int EX_OVERFLOW         = 4;
    static constexpr int EX_BOUND_RANGE      = 5;
    static constexpr int EX_INVALID_OPCODE   = 6;
    static constexpr int EX_DEVICE_UNAVAIL   = 7;
    static constexpr int EX_DOUBLE_FAULT     = 8;
    static constexpr int EX_INVALID_TSS      = 10;
    static constexpr int EX_SEGMENT_NP       = 11;
    static constexpr int EX_STACK_FAULT      = 12;
    static constexpr int EX_GENERAL_PROTECT  = 13;
    static constexpr int EX_PAGE_FAULT       = 14;
    static constexpr int EX_FPU_ERROR        = 16;
    static constexpr int EX_ALIGNMENT_CHECK  = 17;
    static constexpr int EX_MACHINE_CHECK    = 18;

    // IRQ vektörleri (PIC ile 0x20'den başlar)
    static constexpr int IRQ_BASE     = 0x20;
    static constexpr int IRQ_TIMER    = IRQ_BASE + 0;
    static constexpr int IRQ_KEYBOARD = IRQ_BASE + 1;
    static constexpr int IRQ_MOUSE    = 0x2C;  // Slave PIC: IRQ12 = 0x28 + 4

    using HandlerFunc = void (*)();

    IDT();

    // Belirli bir vektöre handler kaydet
    void registerHandler(uint8_t vector, HandlerFunc handler,
                         uint8_t dpl = 0);

    void load();

private:
    IDTGate    gates[IDT_ENTRIES];
    IDTPointer pointer;

    void setGate(uint8_t vector, uint64_t handler,
                 uint16_t selector, uint8_t type_attr);
};

extern IDT* g_idt;

// ============================================================
// ISR Stub Bildirimleri (entry.asm'de assembly wrapper'lar)
// Her ISR stub CPU context'i kaydedip C handler'ı çağırır
// ============================================================
extern "C" {
    void isr_stub_0();   // Divide by zero
    void isr_stub_1();   // Debug
    void isr_stub_2();   // NMI
    void isr_stub_3();   // Breakpoint
    void isr_stub_6();   // Invalid opcode
    void isr_stub_8();   // Double fault
    void isr_stub_13();  // General Protection Fault
    void isr_stub_14();  // Page Fault
    void irq_stub_0();    // Timer (IRQ0)
    void irq_stub_1();    // Keyboard (IRQ1)
    void irq_stub_12();   // Mouse (IRQ12)
}
