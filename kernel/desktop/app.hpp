// app.hpp — Soyut App taban sınıfı + AppManager
// OOP: App soyut sınıf, AppManager COMPOSITION (HAS-A App[])
//      Polymorphism: App* → virtual onDraw/onKey/onMouse
#pragma once
#include "window.hpp"
#include "event.hpp"
#include "../drivers/framebuffer.hpp"

// Forward declaration (döngüsel include önlemek için)
class TerminalApp;

// ============================================================
// App — Soyut uygulama taban sınıfı
// Her uygulama bu sınıftan türer ve sanal metodları override eder
// ============================================================
class App {
public:
    // Composition: App HAS-A Window
    App(Window* w) : m_window(w), m_open(true) {}
    virtual ~App() = default;

    // ---- Pure virtual interface (POLYMORPHISM) ----
    virtual void        onDraw(FramebufferDriver* fb) = 0;
    virtual void        onKey(KeyEvent& ke)           = 0;
    virtual void        onMouse(MouseEvent& me)       = 0;
    virtual const char* getName()  const              = 0;
    // Uygulama kimlik bilgisi (title bar ikonu için)
    virtual uint32_t    getColor() const { return 0x0089B4FA; }  // Blue varsayılan
    virtual char        getSymbol() const { return '?'; }

    // ---- Accessors ----
    Window* window()  { return m_window; }
    bool    isOpen()  const { return m_open; }
    void    close()   { m_open = false; }

protected:
    Window* m_window;   // Composition: App HAS-A Window
    bool    m_open;     // Pencere açık mı
};

// ============================================================
// AppManager — Uygulama yöneticisi
// OOP: Composition (HAS-A App[])
// ============================================================
class AppManager {
public:
    static constexpr int MAX_APPS = 8;

    AppManager();

    // Uygulama başlat ve kaydet
    void launch(App* app);

    // Focus ver (index)
    void setFocus(int idx);

    // Sonraki uygulamaya odaklan (Ctrl+Tab)
    void focusNext();

    // Odaklı uygulamayı al
    App* focused();

    // Index ile uygulamayı al
    App* get(int idx);

    // Uygulama sayısı
    int count() const { return m_count; }

    // Focused uygulamaya key dispatch et
    void dispatchKey(KeyEvent& ke);

    // Tüm açık uygulamaları çiz
    void drawAll(FramebufferDriver* fb);

    // Verilen (x,y) koordinatında hangi app'in penceresi var?
    // Son'dan ilk'e arar (üstteki pencere önce)
    // Döner: app index (-1 = yok)
    int hitTest(int x, int y);

    // idx'teki uygulamayı en üste getir (z-order)
    void bringToFront(int idx);

private:
    App* m_apps[MAX_APPS];
    int  m_count;
    int  m_focused;
};

extern AppManager*  g_app_manager;
extern TerminalApp* g_terminal_app;  // Shell output routing için
