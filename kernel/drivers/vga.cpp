// vga.cpp — VGA Text Mode Driver implementasyonu
// 0xB8000 adresine memory-mapped yazıyoruz
// Her hücre: [attribute byte][character byte] = 2 byte

#include "vga.hpp"

VGADriver* g_vga = nullptr;

// ============================================================
// Constructor — RAII: nesne oluşturulunca buffer pointer hazır
// ============================================================
VGADriver::VGADriver()
    : Driver(Driver::Type::Display, 0),  // Tip: Display, öncelik: 0
      m_buffer(reinterpret_cast<volatile uint16_t*>(VGA_MEMORY)),
      m_col(0), m_row(0),
      m_color(makeColor(VGAColor::LightGray, VGAColor::Black))
{
}

// ============================================================
// Driver Interface Override'ları
// ============================================================
bool VGADriver::initialize()
{
    clear();
    setStatus(Status::Running);
    return true;
}

void VGADriver::shutdown()
{
    clear(VGAColor::Black);
    setStatus(Status::Disabled);
}

// ============================================================
// Yardımcı Fonksiyonlar
// ============================================================
uint8_t VGADriver::makeColor(VGAColor fg, VGAColor bg) const
{
    // VGA attribute byte: [bg(4 bit)][fg(4 bit)]
    return (static_cast<uint8_t>(bg) << 4) | static_cast<uint8_t>(fg);
}

uint16_t VGADriver::makeEntry(char c, uint8_t color) const
{
    // VGA cell: [attribute(8 bit)][character(8 bit)]
    return (static_cast<uint16_t>(color) << 8) | static_cast<uint8_t>(c);
}

void VGADriver::outb(uint16_t port, uint8_t value)
{
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t VGADriver::inb(uint16_t port)
{
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Hardware cursor (VGA CRTC register'ları)
void VGADriver::updateHWCursor()
{
    uint16_t pos = m_row * WIDTH + m_col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, pos & 0xFF);
    outb(0x3D4, 0x0E);
    outb(0x3D5, (pos >> 8) & 0xFF);
}

// ============================================================
// Ekran İşlemleri
// ============================================================
void VGADriver::clear(VGAColor bg)
{
    uint8_t clearColor = makeColor(VGAColor::LightGray, bg);
    uint16_t blank     = makeEntry(' ', clearColor);

    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        m_buffer[i] = blank;
    }
    m_col = 0;
    m_row = 0;
    updateHWCursor();
}

void VGADriver::putCharAt(char c, int col, int row, VGAColor fg, VGAColor bg)
{
    if (col < 0 || col >= WIDTH || row < 0 || row >= HEIGHT) return;
    m_buffer[row * WIDTH + col] = makeEntry(c, makeColor(fg, bg));
}

void VGADriver::scroll()
{
    // Her satırı bir üste kopyala
    for (int row = 1; row < HEIGHT; row++) {
        for (int col = 0; col < WIDTH; col++) {
            m_buffer[(row - 1) * WIDTH + col] = m_buffer[row * WIDTH + col];
        }
    }
    // Son satırı temizle
    uint16_t blank = makeEntry(' ', m_color);
    for (int col = 0; col < WIDTH; col++) {
        m_buffer[(HEIGHT - 1) * WIDTH + col] = blank;
    }
    m_row = HEIGHT - 1;
}

void VGADriver::newline()
{
    m_col = 0;
    if (++m_row >= HEIGHT) {
        scroll();
    }
    updateHWCursor();
}

void VGADriver::putChar(char c)
{
    if (c == '\n') {
        newline();
        return;
    }
    if (c == '\r') {
        m_col = 0;
        updateHWCursor();
        return;
    }
    if (c == '\t') {
        // Tab: 4 boşluk
        int spaces = 4 - (m_col % 4);
        for (int i = 0; i < spaces; i++) putChar(' ');
        return;
    }
    if (c == '\b') {
        // Backspace
        if (m_col > 0) {
            m_col--;
            m_buffer[m_row * WIDTH + m_col] = makeEntry(' ', m_color);
            updateHWCursor();
        }
        return;
    }

    m_buffer[m_row * WIDTH + m_col] = makeEntry(c, m_color);
    if (++m_col >= WIDTH) {
        newline();
    } else {
        updateHWCursor();
    }
}

void VGADriver::print(const char* str)
{
    if (!str) return;
    while (*str) putChar(*str++);
}

void VGADriver::printColored(const char* str, VGAColor fg, VGAColor bg)
{
    uint8_t savedColor = m_color;
    m_color = makeColor(fg, bg);
    print(str);
    m_color = savedColor;
}

void VGADriver::printDec(uint64_t n)
{
    if (n == 0) { putChar('0'); return; }

    char buf[20];
    int i = 0;
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    // Ters sırada yaz
    while (i-- > 0) putChar(buf[i]);
}

void VGADriver::printHex(uint64_t n, bool prefix)
{
    if (prefix) { putChar('0'); putChar('x'); }

    const char* hex = "0123456789ABCDEF";
    bool leading = true;

    for (int i = 60; i >= 0; i -= 4) {
        uint8_t nibble = (n >> i) & 0xF;
        if (nibble != 0) leading = false;
        if (!leading || i == 0) putChar(hex[nibble]);
    }
}

void VGADriver::setColor(VGAColor fg, VGAColor bg)
{
    m_color = makeColor(fg, bg);
}

void VGADriver::setCursor(int col, int row)
{
    m_col = col;
    m_row = row;
    updateHWCursor();
}

void VGADriver::getCursor(int& col, int& row) const
{
    col = m_col;
    row = m_row;
}
