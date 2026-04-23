// framebuffer.cpp — VESA Framebuffer Driver implementasyonu

#include "framebuffer.hpp"
#include "vga.hpp"
#include "../../libc/include/kstring.hpp"

FramebufferDriver* g_fb = nullptr;

// ============================================================
// Builtin 8x8 font — CP437 ASCII 32..127 (96 karakter)
// Her karakter 8 byte, her bit bir piksel (MSB = sol)
// ============================================================
const uint8_t FramebufferDriver::s_font[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // '!'
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // '"'
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, // '#'
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, // '$'
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, // '%'
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, // '&'
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, // '''
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, // '('
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, // ')'
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // '*'
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, // '+'
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, // ','
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, // '-'
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, // '.'
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, // '/'
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, // '0'
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, // '1'
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, // '2'
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, // '3'
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, // '4'
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, // '5'
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, // '6'
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, // '7'
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, // '8'
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, // '9'
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, // ':'
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06}, // ';'
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, // '<'
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, // '='
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // '>'
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, // '?'
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, // '@'
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, // 'A'
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, // 'B'
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, // 'C'
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, // 'D'
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, // 'E'
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, // 'F'
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, // 'G'
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, // 'H'
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 'I'
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, // 'J'
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, // 'K'
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, // 'L'
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, // 'M'
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, // 'N'
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, // 'O'
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, // 'P'
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, // 'Q'
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, // 'R'
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, // 'S'
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 'T'
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, // 'U'
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, // 'V'
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // 'W'
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, // 'X'
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, // 'Y'
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, // 'Z'
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00}, // '['
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, // '\'
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, // ']'
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, // '^'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // '_'
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, // '`'
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, // 'a'
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, // 'b'
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, // 'c'
    {0x38,0x30,0x30,0x3e,0x33,0x33,0x6E,0x00}, // 'd'
    {0x00,0x00,0x1E,0x33,0x3f,0x03,0x1E,0x00}, // 'e'
    {0x1C,0x36,0x06,0x0f,0x06,0x06,0x0F,0x00}, // 'f'
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, // 'g'
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, // 'h'
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, // 'i'
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, // 'j'
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, // 'k'
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 'l'
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, // 'm'
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, // 'n'
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, // 'o'
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, // 'p'
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, // 'q'
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, // 'r'
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, // 's'
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, // 't'
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, // 'u'
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, // 'v'
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, // 'w'
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, // 'x'
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, // 'y'
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, // 'z'
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, // '{'
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // '|'
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, // '}'
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, // '~'
    {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, // DEL (solid block)
};

// ============================================================
// Bochs VBE Extension (BGA) yardımcıları — port 0x01CE/0x01CF
// QEMU -vga std, VMware VGA BIOS katmanı, VirtualBox destekler
// ============================================================
static inline void bvbe_write(uint16_t idx, uint16_t val) {
    asm volatile("outw %0, %1" :: "a"(idx), "Nd"((uint16_t)0x01CE));
    asm volatile("outw %0, %1" :: "a"(val), "Nd"((uint16_t)0x01CF));
}
static inline uint16_t bvbe_read(uint16_t idx) {
    asm volatile("outw %0, %1" :: "a"(idx), "Nd"((uint16_t)0x01CE));
    uint16_t v;
    asm volatile("inw %1, %0" : "=a"(v) : "Nd"((uint16_t)0x01CF));
    return v;
}
// PCI configuration space okuma (VGA cihazının BAR0'ını bulmak için)
static inline uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off) {
    uint32_t addr = 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)dev << 11)
                  | ((uint32_t)fn << 8) | (off & 0xFC);
    asm volatile("outl %0, %1" :: "a"(addr),  "Nd"((uint16_t)0xCF8));
    uint32_t val;
    asm volatile("inl  %1, %0" : "=a"(val) :  "Nd"((uint16_t)0xCFC));
    return val;
}

bool FramebufferDriver::initFromBochsVBE(int w, int h)
{
    // Bochs VBE ID kontrolü: 0xB0C0–0xB0C5 arası geçerli
    uint16_t id = bvbe_read(0);
    extern VGADriver* g_vga;
    if (g_vga) {
        g_vga->print("[BGA]  Bochs VBE ID=0x");
        // Basit hex yazdır
        const char* hex = "0123456789ABCDEF";
        char buf[6] = { hex[(id>>12)&0xF], hex[(id>>8)&0xF],
                        hex[(id>>4)&0xF],  hex[id&0xF], '\0', '\0' };
        g_vga->print(buf); g_vga->print("\n");
    }
    if (id < 0xB0C0 || id > 0xB0C5) return false;

    // Modu ayarla
    bvbe_write(4, 0);              // Disable
    bvbe_write(1, (uint16_t)w);    // XRES
    bvbe_write(2, (uint16_t)h);    // YRES
    bvbe_write(3, 32);             // BPP
    bvbe_write(6, (uint16_t)w);    // Virtual width
    bvbe_write(7, (uint16_t)h);    // Virtual height
    bvbe_write(4, 0x41);           // Enable + LFB (Linear Frame Buffer)

    // PCI taramasıyla framebuffer fiziksel adresini bul
    // Display controller class (0x03) veya Bochs VGA PCI ID (1234:1111) ara
    uint64_t fb_addr = 0;
    for (uint8_t d = 0; d < 32 && !fb_addr; d++) {
        uint32_t vid_did   = pci_read32(0, d, 0, 0x00);
        if (vid_did == 0xFFFFFFFFu) continue;
        uint32_t class_rev = pci_read32(0, d, 0, 0x08);
        uint8_t  cls       = (class_rev >> 24) & 0xFF;
        uint16_t vendor    = vid_did & 0xFFFF;
        // Display class (0x03) ya da Bochs VGA vendor 0x1234
        if (cls == 0x03 || vendor == 0x1234 || vendor == 0x15AD /*VMware*/) {
            uint32_t bar0 = pci_read32(0, d, 0, 0x10);
            if (!(bar0 & 0x01)) {          // Memory BAR (bit0=0)
                fb_addr = bar0 & 0xFFFFFFF0u;
            }
        }
    }

    // PCI taraması başarısız → bilinen adresler dene
    if (!fb_addr) fb_addr = 0xFD000000;   // QEMU default
    if (fb_addr  == 0xFD000000) {
        // Alternatif: 0xE0000000 (bazı VMware/QEMU konfigürasyonları)
        // Burada ilk pixel'i test et — eğer 0 değilse muhtemelen geçerlidir
        volatile uint32_t* test = reinterpret_cast<volatile uint32_t*>(fb_addr);
        (void)test; // sadece adres kontrolü
    }

    m_fb     = reinterpret_cast<uint32_t*>(fb_addr);
    m_width  = w;
    m_height = h;
    m_pitch  = w * 4;    // 32bpp → 4 byte/pixel
    m_col = 0;
    m_row = 0;
    setStatus(Status::Running);
    return true;
}

// ============================================================
// Driver initialize
// ============================================================
bool FramebufferDriver::initialize()
{
    // initFromTag() ile zaten başlatılmış olabilir
    setStatus(m_fb ? Status::Running : Status::Error);
    return m_fb != nullptr;
}

bool FramebufferDriver::initFromTag(const Mb2TagFramebuffer* tag)
{
    if (!tag) return false;
    // Debug: log fb_type and bpp via VGA before framebuffer is ready
    extern VGADriver* g_vga;
    if (g_vga) {
        g_vga->print("[FB]   tag fb_type=");
        g_vga->printDec(tag->fb_type);
        g_vga->print(" bpp=");
        g_vga->printDec(tag->bpp);
        g_vga->print(" ");
        g_vga->printDec(tag->width);
        g_vga->print("x");
        g_vga->printDec(tag->height);
        g_vga->print("\n");
    }
    // Multiboot2 spec: fb_type 1 = direct RGB color, bpp must be 32
    if (tag->fb_type != 1 || tag->bpp != 32) return false;

    m_fb     = reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(tag->address));
    m_width  = static_cast<int>(tag->width);
    m_height = static_cast<int>(tag->height);
    m_pitch  = static_cast<int>(tag->pitch);
    m_col    = 0;
    m_row    = 0;

    setStatus(Status::Running);
    return true;
}

// ============================================================
// Temel çizim primitifleri
// ============================================================
void FramebufferDriver::putPixel(int x, int y, uint32_t color)
{
    if (!m_fb || x < 0 || y < 0 || x >= m_width || y >= m_height) return;
    uint32_t* row = reinterpret_cast<uint32_t*>(
        reinterpret_cast<uint8_t*>(m_fb) + y * m_pitch);
    row[x] = color;
}

uint32_t FramebufferDriver::getPixel(int x, int y) const
{
    if (!m_fb || x < 0 || y < 0 || x >= m_width || y >= m_height) return 0;
    const uint32_t* row = reinterpret_cast<const uint32_t*>(
        reinterpret_cast<const uint8_t*>(m_fb) + y * m_pitch);
    return row[x];
}

void FramebufferDriver::fillRect(int x, int y, int w, int h, uint32_t color)
{
    if (!m_fb) return;
    int x1 = x < 0 ? 0 : x;
    int y1 = y < 0 ? 0 : y;
    int x2 = (x + w) > m_width  ? m_width  : (x + w);
    int y2 = (y + h) > m_height ? m_height : (y + h);
    for (int cy = y1; cy < y2; cy++) {
        uint32_t* row = reinterpret_cast<uint32_t*>(
            reinterpret_cast<uint8_t*>(m_fb) + cy * m_pitch);
        for (int cx = x1; cx < x2; cx++)
            row[cx] = color;
    }
}

void FramebufferDriver::clear(uint32_t color)
{
    fillRect(0, 0, m_width, m_height, color);
}

void FramebufferDriver::drawHLine(int x, int y, int w, uint32_t color)
{
    fillRect(x, y, w, 1, color);
}

void FramebufferDriver::drawVLine(int x, int y, int h, uint32_t color)
{
    fillRect(x, y, 1, h, color);
}

void FramebufferDriver::drawRect(int x, int y, int w, int h, uint32_t color)
{
    drawHLine(x,     y,         w, color);
    drawHLine(x,     y + h - 1, w, color);
    drawVLine(x,     y,         h, color);
    drawVLine(x + w - 1, y,     h, color);
}

// ============================================================
// Font render
// ============================================================
void FramebufferDriver::drawChar(int x, int y, char c, uint32_t fg, uint32_t bg)
{
    if (!m_fb) return;
    int idx = (unsigned char)c - 32;
    if (idx < 0 || idx >= 96) idx = 0; // bilinmeyen → boşluk

    const uint8_t* glyph = s_font[idx];
    for (int row = 0; row < FONT_H; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < FONT_W; col++) {
            bool set = (bits >> col) & 1;  // font LSB = sol piksel
            putPixel(x + col, y + row, set ? fg : bg);
        }
    }
}

void FramebufferDriver::drawString(int x, int y, const char* s, uint32_t fg, uint32_t bg)
{
    if (!s) return;
    int cx = x;
    while (*s) {
        if (*s == '\n') { cx = x; y += FONT_H; }
        else { drawChar(cx, y, *s, fg, bg); cx += FONT_W; }
        s++;
    }
}

void FramebufferDriver::drawCharScaled(int x, int y, char c, uint32_t fg, uint32_t bg, int scale)
{
    if (!m_fb || scale < 1) return;
    int idx = (unsigned char)c - 32;
    if (idx < 0 || idx >= 96) idx = 0;
    const uint8_t* glyph = s_font[idx];
    for (int row = 0; row < FONT_H; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < FONT_W; col++) {
            bool set = (bits >> col) & 1;
            fillRect(x + col*scale, y + row*scale, scale, scale, set ? fg : bg);
        }
    }
}

void FramebufferDriver::drawStringScaled(int x, int y, const char* s, uint32_t fg, uint32_t bg, int scale)
{
    if (!s) return;
    int cx = x;
    while (*s) {
        if (*s == '\n') { cx = x; y += FONT_H * scale; }
        else { drawCharScaled(cx, y, *s, fg, bg, scale); cx += FONT_W * scale; }
        s++;
    }
}

uint32_t FramebufferDriver::blendColor(uint32_t fg, uint32_t bg, uint8_t alpha)
{
    uint32_t r = (((fg>>16)&0xFF)*alpha + ((bg>>16)&0xFF)*(255-alpha)) >> 8;
    uint32_t g = (((fg>>8)&0xFF)*alpha  + ((bg>>8)&0xFF)*(255-alpha))  >> 8;
    uint32_t b = ((fg&0xFF)*alpha        + (bg&0xFF)*(255-alpha))       >> 8;
    return (r<<16)|(g<<8)|b;
}

void FramebufferDriver::fillRectBlend(int x, int y, int w, int h, uint32_t color, uint8_t alpha)
{
    if (!m_fb) return;
    int x1 = x < 0 ? 0 : x;
    int y1 = y < 0 ? 0 : y;
    int x2 = (x+w) > m_width  ? m_width  : (x+w);
    int y2 = (y+h) > m_height ? m_height : (y+h);
    for (int cy = y1; cy < y2; cy++) {
        uint32_t* row = reinterpret_cast<uint32_t*>(
            reinterpret_cast<uint8_t*>(m_fb) + cy * m_pitch);
        for (int cx = x1; cx < x2; cx++)
            row[cx] = blendColor(color, row[cx], alpha);
    }
}

// ============================================================
// Text console (shell için)
// ============================================================
void FramebufferDriver::consoleClear()
{
    // Sadece console alanını temizle
    int cx = m_con_x, cy = m_con_y;
    int cw = (m_con_w > 0) ? m_con_w : m_width;
    int ch = (m_con_h > 0) ? m_con_h : m_height;
    fillRect(cx, cy, cw, ch, Color::Black);
    m_col = 0;
    m_row = 0;
}

void FramebufferDriver::consoleScroll()
{
    if (!m_fb) return;
    int totalRows = rows();
    int cx = m_con_x;
    int cy = m_con_y;
    int cw = (m_con_w > 0) ? m_con_w : m_width;
    int fh = FONT_H * m_scale;  // Scale dahil satır yüksekliği

    // Her karakter satırını bir yukarı kopyala
    for (int r = 1; r < totalRows; r++) {
        for (int line = 0; line < fh; line++) {
            int dstY = cy + (r - 1) * fh + line;
            int srcY = cy + r * fh + line;
            uint8_t* dst = reinterpret_cast<uint8_t*>(m_fb) + dstY * m_pitch + cx * 4;
            uint8_t* src = reinterpret_cast<uint8_t*>(m_fb) + srcY * m_pitch + cx * 4;
            kmemcpy(dst, src, static_cast<uint64_t>(cw) * 4);
        }
    }
    // Son satırı temizle
    fillRect(cx, cy + (totalRows - 1) * fh, cw, fh, Color::Black);
    m_row = totalRows - 1;
}

void FramebufferDriver::consoleNewLine()
{
    m_col = 0;
    m_row++;
    if (m_row >= rows()) consoleScroll();
}

void FramebufferDriver::consoleSetCursor(int col, int row)
{
    m_col = col;
    m_row = row;
}

void FramebufferDriver::consolePutChar(char c, uint32_t fg, uint32_t bg)
{
    if (!m_fb) return;
    if (c == '\n') { consoleNewLine(); return; }
    if (c == '\r') { m_col = 0; return; }
    if (c == '\b') {
        if (m_col > 0) {
            m_col--;
            int ox = (m_con_x > 0) ? m_con_x : 0;
            int oy = (m_con_y > 0) ? m_con_y : 0;
            drawCharScaled(ox + m_col * FONT_W * m_scale, oy + m_row * FONT_H * m_scale, ' ', fg, bg, m_scale);
        }
        return;
    }
    if (m_col >= cols()) consoleNewLine();
    // Console origin offset ile çiz
    int ox = (m_con_x > 0) ? m_con_x : 0;
    int oy = (m_con_y > 0) ? m_con_y : 0;
    drawCharScaled(ox + m_col * FONT_W * m_scale, oy + m_row * FONT_H * m_scale, c, fg, bg, m_scale);
    m_col++;
}

void FramebufferDriver::consolePrint(const char* s, uint32_t fg, uint32_t bg)
{
    while (s && *s) { consolePutChar(*s++, fg, bg); }
}

void FramebufferDriver::consolePrintColored(const char* s, uint32_t fg, uint32_t bg)
{
    consolePrint(s, fg, bg);
}
