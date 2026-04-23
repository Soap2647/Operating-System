// keyboard.cpp — PS/2 Keyboard Driver implementasyonu
// PS/2 kontrolleri: IRQ1 → scancode okuma → KeyEvent üretme

#include "keyboard.hpp"
#include "vga.hpp"

KeyboardDriver* g_keyboard = nullptr;

// ============================================================
// Scancode Set 1 → ASCII tablosu
// Index = scancode (make code), değer = ASCII karakter
// ============================================================
const char KeyboardDriver::s_scancode_table[128] = {
//  0     1     2     3     4     5     6     7
    0,    0x1B, '1',  '2',  '3',  '4',  '5',  '6',   // 0x00
    '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',   // 0x08
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',    // 0x10
    'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',    // 0x18
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',    // 0x20
    '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',    // 0x28
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',    // 0x30
    0,    ' ',  0,    0,    0,    0,    0,    0,       // 0x38
    0,    0,    0,    0,    0,    0,    0,    '7',     // 0x40
    '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',    // 0x48
    '2',  '3',  '0',  '.',  0,    0,    0,    0,       // 0x50
    0,    0,    0,    0,    0,    0,    0,    0,       // 0x58
    0,    0,    0,    0,    0,    0,    0,    0,       // 0x60
    0,    0,    0,    0,    0,    0,    0,    0,       // 0x68
    0,    0,    0,    0,    0,    0,    0,    0,       // 0x70
    0,    0,    0,    0,    0,    0,    0,    0,       // 0x78
};

// Shift ile yazılan karakterler
const char KeyboardDriver::s_scancode_shift_table[128] = {
    0,    0x1B, '!',  '@',  '#',  '$',  '%',  '^',
    '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',
    'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',
    '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',
    'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',
    0,    ' ',  0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
};

// ============================================================
// I/O Port Yardımcıları
// ============================================================
void KeyboardDriver::outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t KeyboardDriver::inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

bool KeyboardDriver::outputBufferFull() const {
    return (inb(KB_STATUS_PORT) & 0x01) != 0;
}

uint8_t KeyboardDriver::readData() {
    return inb(KB_DATA_PORT);
}

void KeyboardDriver::writeCmd(uint8_t cmd) {
    // Kontrollör hazır olana kadar bekle
    while (inb(KB_STATUS_PORT) & 0x02);
    outb(KB_CMD_PORT, cmd);
}

// ============================================================
// Ring Buffer İşlemleri
// ============================================================
void KeyboardDriver::pushEvent(const KeyEvent& ev) {
    int next = (m_tail + 1) % BUFFER_SIZE;
    if (next == m_head) return; // Buffer dolu — event drop
    m_buffer[m_tail] = ev;
    m_tail = next;
}

bool KeyboardDriver::hasEvent() const {
    return m_head != m_tail;
}

bool KeyboardDriver::getEvent(KeyEvent& out) {
    if (!hasEvent()) return false;
    out    = m_buffer[m_head];
    m_head = (m_head + 1) % BUFFER_SIZE;
    return true;
}

// Blocking okuma — interrupt'ların açık olması gerekiyor
char KeyboardDriver::getChar() {
    KeyEvent ev;
    while (true) {
        if (getEvent(ev) && ev.pressed && ev.ascii != 0) {
            return ev.ascii;
        }
        asm volatile("hlt"); // Interrupt bekle, CPU'yu boş döndürme
    }
}

// ============================================================
// Yapıcı — RAII
// ============================================================
KeyboardDriver::KeyboardDriver()
    : Driver(Driver::Type::Keyboard, 1),  // Öncelik: 1 (VGA'dan sonra)
      m_head(0), m_tail(0),
      m_shift(false), m_ctrl(false), m_alt(false), m_caps(false)
{
}

// ============================================================
// initialize() — Driver override
// PS/2 klavye kontrolörünü başlat
// ============================================================
bool KeyboardDriver::initialize()
{
    // Output buffer'ı temizle (kalan veri varsa at)
    for (int i = 0; i < 16; i++) {
        if (!(inb(KB_STATUS_PORT) & 0x01)) break;
        inb(KB_DATA_PORT);
    }

    // PS/2 port 1'i etkinleştir (IRQ1 aktif olur)
    // BIOS/GRUB scancode set 1 translation'ı zaten kurmuş, dokunmuyoruz
    writeCmd(0xAE);

    // Ring buffer'ı temizle
    m_head = m_tail = 0;

    setStatus(Status::Running);
    return true;
}

void KeyboardDriver::shutdown()
{
    // Klavye interface'ini kapat
    writeCmd(0xAD);
    setStatus(Status::Disabled);
}

// ============================================================
// translateScancode — Scancode → KeyEvent
// ============================================================
KeyEvent KeyboardDriver::translateScancode(uint8_t scancode)
{
    KeyEvent ev = {};
    ev.shift = m_shift;
    ev.ctrl  = m_ctrl;
    ev.alt   = m_alt;

    // Break code (key release): bit 7 set
    if (scancode & 0x80) {
        ev.pressed = false;
        uint8_t make = scancode & 0x7F;

        // Modifier release
        if (make == 0x2A || make == 0x36) m_shift = false; // Shift
        if (make == 0x1D)                 m_ctrl  = false; // Ctrl
        if (make == 0x38)                 m_alt   = false; // Alt

        return ev;
    }

    // Make code (key press)
    ev.pressed = true;

    switch (scancode) {
        case 0x2A: case 0x36: m_shift = true;  return ev; // LShift, RShift
        case 0x1D:            m_ctrl  = true;  return ev; // LCtrl
        case 0x38:            m_alt   = true;  return ev; // LAlt
        case 0x3A:            m_caps  = !m_caps; return ev; // CapsLock
        case 0x48: ev.code = KeyCode::UP;    return ev;
        case 0x50: ev.code = KeyCode::DOWN;  return ev;
        case 0x4B: ev.code = KeyCode::LEFT;  return ev;
        case 0x4D: ev.code = KeyCode::RIGHT; return ev;
        default:
            if (scancode < 128) {
                bool upper = m_shift ^ m_caps;
                char c = upper ? s_scancode_shift_table[scancode]
                               : s_scancode_table[scancode];
                ev.ascii = c;
                ev.code  = static_cast<KeyCode>(c ? c : 0);
            }
    }
    return ev;
}

// ============================================================
// handleInterrupt() — Driver override (Polymorphism!)
// IRQ1 geldiğinde idt.cpp tarafından çağrılır
// ============================================================
void KeyboardDriver::handleInterrupt()
{
    if (!outputBufferFull()) return;

    uint8_t  scancode = readData();
    KeyEvent ev       = translateScancode(scancode);

    if (ev.pressed || (!ev.pressed && (ev.shift || ev.ctrl || ev.alt))) {
        pushEvent(ev);
    }
}
