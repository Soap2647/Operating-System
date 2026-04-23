// window.hpp — Pencere widget'ı
// OOP: Window : public Widget (INHERITANCE)
//      draw() override → POLYMORPHISM
#pragma once
#include "widget.hpp"
#include "theme.hpp"

// ============================================================
// Window — Widget'tan türeyen pencere sınıfı
// Title bar (macOS tarzı trafik ışığı butonlar) + içerik alanı
// ============================================================
class Window : public Widget {
public:
    Window();

    // Widget interface — POLYMORPHIC override
    void draw(FramebufferDriver* fb) override;

    // Başlık çubuğu buton hit-test
    // Döner: -1=yok, 0=kapat(kırmızı), 1=küçült(sarı), 2=büyüt(yeşil)
    int hitButton(int mx, int my) const;

    // Başlık
    void        setTitle(const char* title);
    const char* getTitle() const { return m_title; }

    // İçerik alanı koordinatları (shell buraya yazacak)
    int contentX() const { return m_x + Theme::WIN_BORDER; }
    int contentY() const { return m_y + Theme::TITLEBAR_H; }
    int contentW() const { return m_w - 2 * Theme::WIN_BORDER; }
    int contentH() const { return m_h - Theme::TITLEBAR_H - Theme::WIN_BORDER; }

private:
    void drawTitleBar(FramebufferDriver* fb);
    void drawButton(FramebufferDriver* fb, int cx, int cy, uint32_t color);
    void drawTitleText(FramebufferDriver* fb);

    char m_title[64];
};
