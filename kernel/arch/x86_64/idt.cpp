// idt.cpp — IDT implementasyonu + ISR handler'ları
// Tüm exception/interrupt handler'ları burada tanımlanır

#include "idt.hpp"
#include "../../drivers/vga.hpp"   // Hata mesajı için

IDT* g_idt = nullptr;

// ============================================================
// PIC (Programmable Interrupt Controller) Yardımcıları
// 8259A PIC'i yeniden programlıyoruz:
//   Master PIC: IRQ0-7  → vektör 0x20-0x27
//   Slave  PIC: IRQ8-15 → vektör 0x28-0x2F
// ============================================================
static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void io_wait() { outb(0x80, 0); }

static void pic_remap() {
    // Mevcut maskeleri kaydet
    uint8_t a1 = inb(0x21);
    uint8_t a2 = inb(0xA1);

    // Initialization command (ICW1)
    outb(0x20, 0x11); io_wait();
    outb(0xA0, 0x11); io_wait();

    // ICW2: Vector offset
    outb(0x21, 0x20); io_wait();   // Master → 0x20
    outb(0xA1, 0x28); io_wait();   // Slave  → 0x28

    // ICW3: Master/Slave cascade
    outb(0x21, 0x04); io_wait();   // Master: IRQ2'de slave var
    outb(0xA1, 0x02); io_wait();   // Slave: cascade identity = 2

    // ICW4: 8086 mode
    outb(0x21, 0x01); io_wait();
    outb(0xA1, 0x01); io_wait();

    // Maskeleri geri yükle
    outb(0x21, a1);
    outb(0xA1, a2);
}

// ============================================================
// IDT Gate Ayarla
// ============================================================
void IDT::setGate(uint8_t vector, uint64_t handler,
                  uint16_t selector, uint8_t type_attr)
{
    gates[vector].offset_low  = handler & 0xFFFF;
    gates[vector].offset_mid  = (handler >> 16) & 0xFFFF;
    gates[vector].offset_high = (handler >> 32) & 0xFFFFFFFF;
    gates[vector].selector    = selector;
    gates[vector].type_attr   = type_attr;
    gates[vector].ist         = 0;
    gates[vector].zero        = 0;
}

void IDT::registerHandler(uint8_t vector, HandlerFunc handler, uint8_t dpl)
{
    // type_attr: Present(1) | DPL | Type(0xE = 64-bit interrupt gate)
    uint8_t attr = 0x8E | ((dpl & 0x3) << 5);
    setGate(vector, reinterpret_cast<uint64_t>(handler), 0x08, attr);
}

IDT::IDT()
{
    // Tüm gate'leri sıfırla
    for (int i = 0; i < IDT_ENTRIES; i++) {
        gates[i] = {};
    }

    // PIC'i yeniden programla
    pic_remap();

    // Exception handler'larını kaydet
    registerHandler(EX_DIVIDE_BY_ZERO,  isr_stub_0);
    registerHandler(EX_DEBUG,           isr_stub_1);
    registerHandler(EX_NMI,             isr_stub_2);
    registerHandler(EX_BREAKPOINT,      isr_stub_3,  3); // Ring 3'ten çağrılabilir
    registerHandler(EX_INVALID_OPCODE,  isr_stub_6);
    registerHandler(EX_DOUBLE_FAULT,    isr_stub_8);
    registerHandler(EX_GENERAL_PROTECT, isr_stub_13);
    registerHandler(EX_PAGE_FAULT,      isr_stub_14);

    // IRQ handler'larını kaydet
    registerHandler(IRQ_TIMER,    irq_stub_0);
    registerHandler(IRQ_KEYBOARD, irq_stub_1);
    registerHandler(IRQ_MOUSE,    irq_stub_12);

    pointer.limit = sizeof(gates) - 1;
    pointer.base  = reinterpret_cast<uint64_t>(&gates[0]);

    load();
}

void IDT::load()
{
    asm volatile("lidt %0" : : "m"(pointer) : "memory");
    asm volatile("sti");   // Interrupt'ları aktif et
}

// ============================================================
// C++ Exception Handler'ları
// Her exception için burada işlem yapılır
// ============================================================
extern "C" {

// Genel exception handler — tüm ISR stub'lar buraya yönlendirir
void exception_handler(uint64_t vector, uint64_t error_code,
                        InterruptFrame* frame)
{
    // Kernel panic: ekranı kırmızıya boya ve dur
    // (VGA sınıfı henüz init olmamış olabilir, doğrudan memory erişimi)
    volatile uint16_t* vga = reinterpret_cast<uint16_t*>(0xB8000);
    const char* msgs[] = {
        "DIVIDE BY ZERO",    // 0
        "DEBUG",             // 1
        "NMI",               // 2
        "BREAKPOINT",        // 3
        "", "",
        "INVALID OPCODE",    // 6
        "",
        "DOUBLE FAULT",      // 8
        "", "", "", "",
        "GENERAL PROTECTION FAULT",  // 13
        "PAGE FAULT",        // 14
    };

    const char* msg = (vector < 15) ? msgs[vector] : "UNKNOWN EXCEPTION";

    // Kırmızı ekran
    for (int i = 0; i < 80 * 25; i++) {
        vga[i] = 0x4F20;    // Kırmızı bg, boşluk
    }

    // "KERNEL PANIC" yaz
    const char* panic = "!!! MERTOS KERNEL PANIC !!!";
    for (int i = 0; panic[i]; i++) {
        vga[i] = 0x4F00 | panic[i];
    }

    // Exception mesajı
    for (int i = 0; msg[i]; i++) {
        vga[80 + i] = 0x4F00 | msg[i];
    }

    // RIP değeri
    (void)frame;
    (void)error_code;

    asm volatile("cli; hlt");
    __builtin_unreachable();
}

// IRQ End-of-Interrupt gönder
static void pic_eoi(uint8_t irq) {
    if (irq >= 8) outb(0xA0, 0x20);  // Slave EOI
    outb(0x20, 0x20);                 // Master EOI
}

} // extern "C"
