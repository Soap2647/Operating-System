// driver.cpp — DriverManager implementasyonu

#include "driver.hpp"

DriverManager* g_driverManager = nullptr;

bool DriverManager::registerDriver(Driver* driver)
{
    if (!driver || m_count >= MAX_DRIVERS) return false;
    m_drivers[m_count++] = driver;
    return true;
}

Driver* DriverManager::findDriver(Driver::Type type)
{
    for (int i = 0; i < m_count; i++) {
        if (m_drivers[i]->getType() == type) {
            return m_drivers[i];
        }
    }
    return nullptr;
}

void DriverManager::initializeAll()
{
    // Öncelik sırasına göre sırala (bubble sort — STL yok)
    for (int i = 0; i < m_count - 1; i++) {
        for (int j = 0; j < m_count - i - 1; j++) {
            if (m_drivers[j]->getPriority() > m_drivers[j+1]->getPriority()) {
                Driver* tmp = m_drivers[j];
                m_drivers[j] = m_drivers[j+1];
                m_drivers[j+1] = tmp;
            }
        }
    }

    // Sırayla başlat
    for (int i = 0; i < m_count; i++) {
        m_drivers[i]->initialize();
    }
}
