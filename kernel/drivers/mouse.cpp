// mouse.cpp — PS/2 Mouse Driver implementasyonu
#include "mouse.hpp"

MouseDriver* g_mouse = nullptr;

MouseDriver::MouseDriver()
    : Driver(Type::Mouse, 12),
      m_x(512), m_y(384),
      m_maxX(1023), m_maxY(767),
      m_buttons(0), m_pkt_idx(0)
{
    for (int i = 0; i < 3; i++) m_packet[i] = 0;
}

// ============================================================
// 8042 yardımcıları
// ============================================================
void MouseDriver::waitWrite()
{
    for (int i = 0; i < 100000; i++) {
        if (!(inb(0x64) & 0x02)) return;
    }
}
void MouseDriver::waitRead()
{
    for (int i = 0; i < 100000; i++) {
        if (inb(0x64) & 0x01) return;
    }
}
void MouseDriver::writeMouse(uint8_t byte)
{
    waitWrite(); outb(0x64, 0xD4);  // 8042'ye: "fareye gönder"
    waitWrite(); outb(0x60, byte);
}

// ============================================================
// initialize() — PS/2 faresini etkinleştir
// ============================================================
bool MouseDriver::initialize()
{
    // 1. Auxiliary device (PS/2 port 2) etkinleştir
    waitWrite(); outb(0x64, 0xA8);

    // 2. IRQ12'yi etkinleştir (controller config byte'ındaki bit 1)
    waitWrite(); outb(0x64, 0x20);
    waitRead();
    uint8_t cfg = inb(0x60);
    cfg |=  0x02;   // Auxiliary interrupt etkinleştir
    cfg &= ~0x20;   // Auxiliary clock disable'ı kaldır
    waitWrite(); outb(0x64, 0x60);
    waitWrite(); outb(0x60, cfg);

    // 3. Fareye: varsayılan ayarları kullan
    writeMouse(0xF6);
    waitRead(); inb(0x60);  // ACK

    // 4. Fareye: veri akışını başlat
    writeMouse(0xF4);
    waitRead(); inb(0x60);  // ACK

    // 5. Buffer'ı temizle
    for (int i = 0; i < 8; i++) {
        if (inb(0x64) & 0x01) inb(0x60);
        else break;
    }

    m_pkt_idx = 0;
    setStatus(Status::Running);
    return true;
}

// ============================================================
// handleIRQ() — IRQ12 geldiğinde çağrılır
// 3 byte'lık PS/2 fare paketi biriktirir
// ============================================================
void MouseDriver::handleIRQ()
{
    uint8_t data = inb(0x60);
    m_packet[m_pkt_idx++] = data;

    if (m_pkt_idx < 3) return;  // Henüz tam paket yok
    m_pkt_idx = 0;

    // Byte 0: durum — bit 3 her zaman 1 olmalı (geçerlilik kontrolü)
    uint8_t status = m_packet[0];
    if (!(status & 0x08)) return;

    // Buton durumu (bit 0 = sol, bit 1 = sağ, bit 2 = orta)
    m_buttons = status & 0x07;

    // X ve Y delta değerleri (işaretli 9-bit)
    int dx = static_cast<int>(m_packet[1]);
    int dy = static_cast<int>(m_packet[2]);

    // İşaret bitleri (status byte'ın 4. ve 5. bitleri)
    if (status & 0x10) dx -= 256;   // X negatif
    if (status & 0x20) dy -= 256;   // Y negatif

    // Overflow bayrakları — geçersiz paket
    if ((status & 0x40) || (status & 0x80)) return;

    // Sensitivity: hareketi yarıya indir (çok hızlı delta'yı yumuşat)
    dx /= 2;
    dy /= 2;

    // Konumu güncelle
    // PS/2 standard: pozitif dx = sağ, pozitif dy = YUKARI (matematik koordinatı)
    // Ekran koordinatı: Y aşağı artar → m_y -= dy
    m_x += dx;
    m_y -= dy;

    // Sınırlar
    if (m_x < 0)      m_x = 0;
    if (m_y < 0)      m_y = 0;
    if (m_x > m_maxX) m_x = m_maxX;
    if (m_y > m_maxY) m_y = m_maxY;
}
