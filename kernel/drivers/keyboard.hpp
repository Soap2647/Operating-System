// keyboard.hpp — PS/2 Keyboard Driver
// OOP: Driver'dan inheritance — Midterm polymorphism örneği #2
// handleInterrupt() override → IRQ1 geldiğinde KeyboardDriver::handleInterrupt()

#pragma once
#include "driver.hpp"

// ============================================================
// Tuş kodları — özel tuşlar için enum
// ============================================================
enum class KeyCode : uint8_t {
    NONE      = 0,
    ENTER     = '\n',
    BACKSPACE = '\b',
    TAB       = '\t',
    ESCAPE    = 0x1B,
    SPACE     = ' ',
    UP        = 0x80,
    DOWN      = 0x81,
    LEFT      = 0x82,
    RIGHT     = 0x83,
    F1        = 0x90,
    F2        = 0x91,
    F3        = 0x92,
    F4        = 0x93,
    F5        = 0x94,
    F12       = 0x9B,
    LSHIFT    = 0xA0,
    RSHIFT    = 0xA1,
    LCTRL     = 0xA2,
    LALT      = 0xA3,
    CAPS_LOCK = 0xA4,
};

// Tuş event'i
struct KeyEvent {
    char    ascii;      // ASCII karakter (0 = özel tuş)
    KeyCode code;       // Tuş kodu
    bool    pressed;    // true = basıldı, false = bırakıldı
    bool    shift;      // Shift basılı mı
    bool    ctrl;       // Ctrl basılı mı
    bool    alt;        // Alt basılı mı
};

// ============================================================
// KeyboardDriver — PS/2 Keyboard Controller
//
// Midterm OOP Bağlantısı:
//   - Inheritance: Driver'dan türetilmiş
//   - Polymorphism: initialize(), getName(), handleInterrupt() override
//   - Encapsulation: ring buffer, modifier state private
//   - Composition: KeyEvent ring buffer (kendi mini circular buffer'ımız)
// ============================================================
class KeyboardDriver : public Driver {
public:
    static constexpr int BUFFER_SIZE = 64;  // Ring buffer kapasitesi

    // Port sabitleri
    static constexpr uint16_t KB_DATA_PORT    = 0x60;
    static constexpr uint16_t KB_STATUS_PORT  = 0x64;
    static constexpr uint16_t KB_CMD_PORT     = 0x64;

    // ---- Yapıcı ----
    KeyboardDriver();

    // ---- Driver interface override'ları (Polymorphism!) ----
    bool        initialize()   override;
    const char* getName()      const override { return "PS/2 Keyboard Driver"; }
    void        shutdown()     override;
    void        handleInterrupt() override;  // IRQ1'den çağrılır

    // ---- Klavye Okuma ----

    // Buffer'da event var mı?
    bool hasEvent() const;

    // Sonraki event'i al (blocking değil)
    bool getEvent(KeyEvent& out);

    // Tek karakter bekle (busy-wait — shell için)
    char getChar();

    // ---- Modifier Durumu ----
    bool isShiftPressed() const { return m_shift; }
    bool isCtrlPressed()  const { return m_ctrl;  }
    bool isAltPressed()   const { return m_alt;   }
    bool isCapsLock()     const { return m_caps;  }

private:
    // ---- Ring Buffer (circular queue) ----
    // STL queue yerine kendi basit implementasyonumuz
    KeyEvent m_buffer[BUFFER_SIZE];
    volatile int m_head;    // Okuma pozisyonu
    volatile int m_tail;    // Yazma pozisyonu

    void pushEvent(const KeyEvent& ev);

    // ---- Modifier State ----
    bool m_shift;
    bool m_ctrl;
    bool m_alt;
    bool m_caps;

    // ---- Scancode Set 1 → ASCII Tablosu ----
    static const char s_scancode_table[128];
    static const char s_scancode_shift_table[128];

    // ---- PS/2 Yardımcıları ----
    uint8_t  readData();
    void     writeCmd(uint8_t cmd);
    bool     outputBufferFull() const;

    // Scancode'u KeyEvent'e çevir
    KeyEvent translateScancode(uint8_t scancode);

    // I/O port
    static void     outb(uint16_t port, uint8_t value);
    static uint8_t  inb(uint16_t port);
};

extern KeyboardDriver* g_keyboard;
