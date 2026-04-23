// event.hpp — Desktop Event sistemi
// OOP: Template kullanımı — EventQueue = MessageQueue<Event,64>
#pragma once
#include "../drivers/keyboard.hpp"
#include "../ipc/message_queue.hpp"

// ============================================================
// MouseEvent — Fare hareket/tıklama eventi
// ============================================================
struct MouseEvent {
    int     x, y;
    uint8_t buttons;
    bool    clicked;
};

// ============================================================
// Event — Birleşik event tipi (keyboard veya mouse)
// ============================================================
struct Event {
    enum class Type : uint8_t {
        NONE        = 0,
        KEY         = 1,
        MOUSE_MOVE  = 2,
        MOUSE_CLICK = 3,
        TIMER       = 4
    } type;

    union {
        KeyEvent   key;
        MouseEvent mouse;
    };

    Event() : type(Type::NONE) { key = {}; }
};

// ============================================================
// EventQueue — Template instantiation (midterm kriteri)
// MessageQueue<Event, 64> → EventQueue
// ============================================================
using EventQueue = MessageQueue<Event, 64>;

extern EventQueue* g_event_queue;
