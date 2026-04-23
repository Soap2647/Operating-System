// app.cpp — AppManager implementasyonu
// OOP: Composition (HAS-A App[]), Polymorphism dispatch
#include "app.hpp"
#include "event.hpp"
#include "terminal_app.hpp"

AppManager*  g_app_manager  = nullptr;
EventQueue*  g_event_queue  = nullptr;
TerminalApp* g_terminal_app = nullptr;

// ============================================================
// AppManager constructor
// ============================================================
AppManager::AppManager()
    : m_count(0), m_focused(0)
{
    for (int i = 0; i < MAX_APPS; i++) m_apps[i] = nullptr;
}

// ============================================================
// launch — Uygulama kaydet
// ============================================================
void AppManager::launch(App* app)
{
    if (!app || m_count >= MAX_APPS) return;
    m_apps[m_count++] = app;
}

// ============================================================
// setFocus — Odağı değiştir
// ============================================================
void AppManager::setFocus(int idx)
{
    if (idx >= 0 && idx < m_count) m_focused = idx;
}

// ============================================================
// focusNext — Ctrl+Tab: sonraki uygulamaya geç
// ============================================================
void AppManager::focusNext()
{
    if (m_count == 0) return;
    m_focused = (m_focused + 1) % m_count;
}

// ============================================================
// focused — Odaklı uygulamayı döndür
// ============================================================
App* AppManager::focused()
{
    if (m_count == 0 || m_focused < 0 || m_focused >= m_count)
        return nullptr;
    return m_apps[m_focused];
}

// ============================================================
// get — Index ile uygulama al
// ============================================================
App* AppManager::get(int idx)
{
    if (idx < 0 || idx >= m_count) return nullptr;
    return m_apps[idx];
}

// ============================================================
// dispatchKey — Focused uygulamaya key event gönder
// Polymorphism: App* → virtual onKey()
// ============================================================
void AppManager::dispatchKey(KeyEvent& ke)
{
    App* app = focused();
    if (app && app->isOpen()) {
        // Pencere görünürse dispatch et
        if (app->window() && app->window()->visible()) {
            app->onKey(ke);
        }
    }
}

// ============================================================
// drawAll — Tüm açık uygulamaları çiz
// Polymorphism: App* → virtual onDraw()
// ============================================================
void AppManager::drawAll(FramebufferDriver* fb)
{
    if (!fb) return;
    for (int i = 0; i < m_count; i++) {
        App* app = m_apps[i];
        if (!app || !app->isOpen()) continue;
        Window* win = app->window();
        if (!win || !win->visible()) continue;
        // Önce pencere çerçevesini çiz (title bar, shadow vb.)
        win->draw(fb);
        // Sonra uygulama içeriğini çiz (Polymorphic call)
        app->onDraw(fb);
    }
}

// ============================================================
// bringToFront — idx'teki uygulamayı en üste getir (z-order)
// Dizideki son eleman en üstte çizilir
// ============================================================
void AppManager::bringToFront(int idx)
{
    if (idx < 0 || idx >= m_count) return;
    App* app = m_apps[idx];
    // idx'ten sonraki elemanları öne kaydır
    for (int i = idx; i < m_count - 1; i++)
        m_apps[i] = m_apps[i + 1];
    m_apps[m_count - 1] = app;
    m_focused = m_count - 1;
}

// ============================================================
// hitTest — Koordinatta hangi pencere var?
// Son'dan ilk'e arar (üstteki pencere önce)
// ============================================================
int AppManager::hitTest(int x, int y)
{
    for (int i = m_count - 1; i >= 0; i--) {
        App* app = m_apps[i];
        if (!app || !app->isOpen()) continue;
        Window* win = app->window();
        if (!win || !win->visible()) continue;
        int wx = win->x();
        int wy = win->y();
        int ww = win->width();
        int wh = win->height();
        if (x >= wx && x < wx + ww && y >= wy && y < wy + wh)
            return i;
    }
    return -1;
}
