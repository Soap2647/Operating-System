// message_queue.hpp — IPC Message Queue (Template)
// OOP: Template sınıf — midterm "template kullanımı" kriteri
// Kullanım: MessageQueue<KeyEvent, 64>, MessageQueue<int, 8> vb.

#pragma once
#include <stdint.h>

// ============================================================
// MessageQueue<T, Size> — Lock-free Ring Buffer IPC Kuyruğu
//
// Midterm OOP Bağlantısı:
//   - Template: aynı veri yapısı farklı mesaj tipleri için çalışır
//   - Encapsulation: buffer ve index'ler private
//   - RAII: constructor sıfırlama yapar
//
// NOT: Şu an single-producer single-consumer (SPSC) senaryosu için.
//      Multi-core'da spinlock gerekir — ileride eklenecek.
// ============================================================
template<typename T, int Size>
class MessageQueue {
    // Derleme zamanı kontrol (C++17 if constexpr alternatifi)
    static_assert(Size > 1, "Queue size must be > 1");
    static_assert((Size & (Size - 1)) == 0, "Queue size must be power of 2");
    // Power-of-2 constraint: modulo yerine bitwise AND → daha hızlı

public:
    using MessageType = T;
    static constexpr int CAPACITY = Size - 1; // Gerçek kapasite (bir slot boş kalır)

    // ---- Yapıcı ----
    MessageQueue() : m_head(0), m_tail(0) {}

    // ---- Mesaj Gönder ----
    // Kuyruk doluysa false döner (blocking değil)
    bool send(const T& msg) {
        int next_tail = advance(m_tail);
        if (next_tail == m_head) return false; // Dolu

        m_buffer[m_tail] = msg;
        // Memory barrier: derleyici optimizasyonu reorder etmesin
        asm volatile("" : : : "memory");
        m_tail = next_tail;
        return true;
    }

    // ---- Mesaj Al ----
    // Kuyruk boşsa false döner
    bool receive(T& out) {
        if (m_head == m_tail) return false; // Boş

        out = m_buffer[m_head];
        asm volatile("" : : : "memory");
        m_head = advance(m_head);
        return true;
    }

    // ---- Peek — Almadan Bak ----
    bool peek(T& out) const {
        if (isEmpty()) return false;
        out = m_buffer[m_head];
        return true;
    }

    // ---- Durum Sorguları ----
    bool isEmpty() const { return m_head == m_tail; }
    bool isFull()  const { return advance(m_tail) == m_head; }

    int count() const {
        int diff = m_tail - m_head;
        return (diff >= 0) ? diff : (Size + diff);
    }

    int capacity() const { return CAPACITY; }

    // ---- Kuyrugu Temizle ----
    void clear() { m_head = m_tail = 0; }

private:
    T            m_buffer[Size];
    volatile int m_head;  // Okuma pozisyonu
    volatile int m_tail;  // Yazma pozisyonu

    // Power-of-2 size ile hızlı modulo
    static int advance(int idx) {
        return (idx + 1) & (Size - 1);
    }
};

// ============================================================
// Önceden tanımlanmış kuyruk tipleri (kullanım kolaylığı)
// Template instantiation örnekleri — midterm için iyi gösterim
// ============================================================

// Forward declare (tam tanım ilgili header'dan gelecek)
struct KeyEvent;

// Klavye event kuyruğu: 64 event kapasiteli
using KeyEventQueue = MessageQueue<KeyEvent, 64>;

// Basit integer mesaj kuyruğu (process arası sinyal için)
using SignalQueue = MessageQueue<uint32_t, 16>;

// Byte kuyruğu (seri port / debug için)
using ByteQueue = MessageQueue<uint8_t, 256>;

// ============================================================
// IPCChannel — İki process arasında iki yönlü iletişim
// OOP: Composition (iki MessageQueue barındırır)
// ============================================================
template<typename T, int Size = 32>
class IPCChannel {
public:
    // A → B mesaj gönder
    bool sendAtoB(const T& msg) { return m_a_to_b.send(msg); }

    // B → A mesaj gönder
    bool sendBtoA(const T& msg) { return m_b_to_a.send(msg); }

    // B, A'dan mesaj al
    bool receiveFromA(T& out) { return m_a_to_b.receive(out); }

    // A, B'den mesaj al
    bool receiveFromB(T& out) { return m_b_to_a.receive(out); }

    bool isAtoB_Empty() const { return m_a_to_b.isEmpty(); }
    bool isBtoA_Empty() const { return m_b_to_a.isEmpty(); }

private:
    MessageQueue<T, Size> m_a_to_b;
    MessageQueue<T, Size> m_b_to_a;
};
