// mouse.hpp — PS/2 Mouse Driver
// OOP: MouseDriver : public Driver (INHERITANCE)
//      IRQ12 → handleIRQ() → konum güncelleme
#pragma once
#include "driver.hpp"

// ============================================================
// MouseDriver — PS/2 fare sürücüsü
// ============================================================
class MouseDriver : public Driver {
public:
    MouseDriver();

    // Driver interface
    bool        initialize() override;
    const char* getName() const override { return "PS/2 Mouse Driver"; }

    // IRQ12 handler tarafından çağrılır
    void handleIRQ();

    // Fare durumu
    int     x()       const { return m_x; }
    int     y()       const { return m_y; }
    uint8_t buttons() const { return m_buttons; }
    bool    leftBtn() const { return (m_buttons & 0x01) != 0; }
    bool    rightBtn()const { return (m_buttons & 0x02) != 0; }

    // Sınır ayarla (ekran boyutu)
    void setBounds(int maxX, int maxY) {
        m_maxX = maxX - 1; m_maxY = maxY - 1;
        m_x = maxX / 2;    m_y = maxY / 2;   // Ekranın tam merkezinden başla
    }

private:
    static inline void outb(uint16_t port, uint8_t val) {
        asm volatile("outb %0, %1" :: "a"(val), "Nd"(port));
    }
    static inline uint8_t inb(uint16_t port) {
        uint8_t v;
        asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
        return v;
    }
    static inline void io_wait() { outb(0x80, 0); }

    void waitWrite();    // 8042 yazma için hazır bekle
    void waitRead();     // 8042 okuma için hazır bekle
    void writeMouse(uint8_t byte);  // Fareye komut gönder

    int     m_x, m_y;           // Fare konumu (piksel)
    int     m_maxX, m_maxY;     // Ekran sınırları
    uint8_t m_buttons;          // Buton durumu
    uint8_t m_packet[3];        // 3-byte fare paketi buffer
    int     m_pkt_idx;          // Paket byte sayacı
};

extern MouseDriver* g_mouse;
