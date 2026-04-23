// driver.hpp — Abstract Driver Base Class
// OOP: Pure virtual interface — her driver bu sözleşmeyi uygular
// Midterm: Polymorphism'in merkezi dosyası!

#pragma once
#include <stdint.h>

// ============================================================
// Driver — Abstract Base Class
//
// Midterm OOP Bağlantısı:
//   - Abstract class (pure virtual metodlar)
//   - Polymorphism: Driver* pointer'ı farklı driver'ları tutar
//   - Inheritance: VGADriver, KeyboardDriver türetilir
//   - Virtual destructor: bellek sızıntısını önler (RAII)
// ============================================================
class Driver {
public:
    // Driver tipleri — tip güvenli enum
    enum class Type : uint8_t {
        Unknown  = 0,
        Display  = 1,
        Keyboard = 2,
        Mouse    = 3,
        Storage  = 4,
        Network  = 5,
        PCI      = 6,
    };

    // Driver durumu
    enum class Status : uint8_t {
        Uninitialized = 0,
        Running       = 1,
        Error         = 2,
        Disabled      = 3,
    };

    // Constructor: tip ve öncelik alır
    explicit Driver(Type type, uint8_t priority = 0)
        : m_type(type), m_priority(priority), m_status(Status::Uninitialized) {}

    // Virtual destructor — polymorphic delete için zorunlu
    virtual ~Driver() = default;

    // ---- Pure Virtual Interface ----
    // Her driver bu metodları implement ETMEK ZORUNDA

    // Driver'ı başlat — donanımı kur, register'ları ayarla
    virtual bool initialize() = 0;

    // Driver ismini döndür (debug/log için)
    virtual const char* getName() const = 0;

    // ---- Virtual (override edilebilir ama zorunlu değil) ----

    // Driver'ı durdur (graceful shutdown)
    virtual void shutdown() {}

    // Interrupt geldiğinde çağrılır (IRQ handler'dan)
    virtual void handleInterrupt() {}

    // ---- Non-virtual (ortak davranış) ----

    Type   getType()     const { return m_type; }
    Status getStatus()   const { return m_status; }
    uint8_t getPriority() const { return m_priority; }
    bool   isRunning()   const { return m_status == Status::Running; }

protected:
    // Alt sınıflar durumu değiştirebilir
    void setStatus(Status s) { m_status = s; }

private:
    Type    m_type;
    uint8_t m_priority;
    Status  m_status;
};

// ============================================================
// DriverManager — Tüm driver'ları yöneten sınıf
// OOP: Composition (Driver pointer'ları tutar)
// ============================================================
class DriverManager {
public:
    static constexpr int MAX_DRIVERS = 16;

    DriverManager() : m_count(0) {}

    // Driver kaydet ve initialize et
    bool registerDriver(Driver* driver);

    // Tip ile driver bul (polymorphic döndürür)
    Driver* findDriver(Driver::Type type);

    // Tüm driver'ları başlat
    void initializeAll();

    // Kayıtlı driver sayısı
    int count() const { return m_count; }

private:
    Driver* m_drivers[MAX_DRIVERS];
    int     m_count;
};

extern DriverManager* g_driverManager;
