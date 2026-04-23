// splash.hpp — Boot splash ekranı
// OOP: SplashScreen : public Widget (INHERITANCE)
#pragma once
#include "widget.hpp"
#include "theme.hpp"

class SplashScreen : public Widget {
public:
    SplashScreen() {}

    // Widget interface — POLYMORPHIC override
    void draw(FramebufferDriver* fb) override;

    // Animasyonlu açılış (blocking — timer tick ile senkronize)
    void animate(FramebufferDriver* fb);

private:
    void drawLogo(FramebufferDriver* fb, int cx, int cy);
    void drawLoadingBar(FramebufferDriver* fb, int cx, int cy, int progress, int total);
    void fadeOut(FramebufferDriver* fb);

    static void waitTick();   // Timer tick bekle
};
