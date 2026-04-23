// widget.hpp — Soyut Widget taban sınıfı
// OOP: Inheritance ve Polymorphism için temel
// Her UI elemanı Widget'tan türer ve draw() override eder
#pragma once
#include "../drivers/framebuffer.hpp"

// ============================================================
// Widget — Soyut temel sınıf (Abstract Base Class)
// Midterm kriteri: pure virtual → zorunlu override
// ============================================================
class Widget {
public:
    Widget() : m_x(0), m_y(0), m_w(0), m_h(0), m_visible(true) {}
    virtual ~Widget() = default;

    // Pure virtual — her alt sınıf kendi çizimini tanımlar (POLYMORPHISM)
    virtual void draw(FramebufferDriver* fb) = 0;

    // Boyut / konum
    void setPos(int x, int y)    { m_x = x; m_y = y; }
    void setSize(int w, int h)   { m_w = w; m_h = h; }
    int  x()       const { return m_x; }
    int  y()       const { return m_y; }
    int  width()   const { return m_w; }
    int  height()  const { return m_h; }
    bool visible() const { return m_visible; }
    void setVisible(bool v) { m_visible = v; }

protected:
    int  m_x, m_y;    // Ekrandaki konum
    int  m_w, m_h;    // Boyut (piksel)
    bool m_visible;   // Görünürlük
};
