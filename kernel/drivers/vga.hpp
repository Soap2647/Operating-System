// vga.hpp — VGA Text Mode Driver
// OOP: Driver'dan inheritance, ekran yazma kapsüllenir
// Midterm: Inheritance + method override örneği

#pragma once
#include "driver.hpp"

// VGA renk kodları
enum class VGAColor : uint8_t {
    Black         = 0,  DarkBlue      = 1,
    DarkGreen     = 2,  DarkCyan      = 3,
    DarkRed       = 4,  Magenta       = 5,
    Brown         = 6,  LightGray     = 7,
    DarkGray      = 8,  Blue          = 9,
    Green         = 10, Cyan          = 11,
    Red           = 12, LightMagenta  = 13,
    Yellow        = 14, White         = 15,
};

// ============================================================
// VGADriver — VGA Text Mode (80x25)
// Midterm OOP:
//   - Driver'dan türetilmiş (Inheritance)
//   - initialize() ve getName() override (Polymorphism)
//   - Ekran state'i encapsulate edilmiş
// ============================================================
class VGADriver : public Driver {
public:
    static constexpr int WIDTH  = 80;
    static constexpr int HEIGHT = 25;
    static constexpr uint32_t VGA_MEMORY = 0xB8000;

    // Constructor — öncelik 0 (en yüksek, ilk başlatılır)
    VGADriver();

    // ---- Driver interface override'ları ----
    bool        initialize() override;
    const char* getName()    const override { return "VGA Text Mode Driver"; }
    void        shutdown()   override;

    // ---- Ekran işlemleri ----

    // Tek karakter yaz
    void putChar(char c);

    // Renk ile karakter yaz
    void putCharAt(char c, int col, int row, VGAColor fg, VGAColor bg);

    // String yaz
    void print(const char* str);

    // Renk ile string yaz
    void printColored(const char* str, VGAColor fg, VGAColor bg = VGAColor::Black);

    // Sayı yaz (decimal)
    void printDec(uint64_t n);

    // Sayı yaz (hex)
    void printHex(uint64_t n, bool prefix = true);

    // Ekranı temizle
    void clear(VGAColor bg = VGAColor::Black);

    // Yeni satır
    void newline();

    // İmleç konumu
    void setCursor(int col, int row);
    void getCursor(int& col, int& row) const;

    // Renk ayarla
    void setColor(VGAColor fg, VGAColor bg);

private:
    volatile uint16_t* m_buffer;   // VGA memory mapped buffer
    int                m_col;      // Mevcut sütun
    int                m_row;      // Mevcut satır
    uint8_t            m_color;    // Mevcut renk attribute

    // Renk attribute byte'ı oluştur
    uint8_t makeColor(VGAColor fg, VGAColor bg) const;

    // VGA entry (karakter + renk) oluştur
    uint16_t makeEntry(char c, uint8_t color) const;

    // Hardware cursor'ı güncelle (VGA register'ları)
    void updateHWCursor();

    // Scroll: ekran dolduğunda yukarı kaydır
    void scroll();

    // I/O port erişimi (inb/outb inline asm)
    void     outb(uint16_t port, uint8_t value);
    uint8_t  inb(uint16_t port);
};

// Global VGA driver instance — kernel genelinde erişim
extern VGADriver* g_vga;
