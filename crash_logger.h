#ifndef CRASH_LOGGER_H
#define CRASH_LOGGER_H

#include <Arduino.h>

// Platform-specific includes
#if defined(ESP32) || defined(ESP8266)
  #include <EEPROM.h>
  #define CRASH_LOG_START_ADDR 0
  #define CRASH_LOG_SIZE 512
#elif defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT)
  #include <FlashStorage.h>
  #define CRASH_LOG_SIZE 512
#else
  #include <EEPROM.h>
  #define CRASH_LOG_START_ADDR 0
  #define CRASH_LOG_SIZE 256
#endif

struct CrashLogEntry {
    unsigned long timestamp;
    char resetReason[32];
    char exceptionType[32];
    char exceptionDescription[128];
    uint32_t freeHeap;
    uint32_t stackTrace[8];  // Store PC values if available
    bool valid;
};

class CrashLogger {
private:
    static const int MAX_ENTRIES = 5;
    static const int LOG_ENTRY_SIZE = sizeof(CrashLogEntry);
    
    void initEEPROM();
    int findNextLogSlot();
    void writeLogEntry(int slot, CrashLogEntry& entry);
    bool readLogEntry(int slot, CrashLogEntry& entry);
    String getResetReason();
    
public:
    CrashLogger();
    void begin();
    void logCrash(const char* exceptionType, const char* description);
    void logException(const char* type, const char* description);
    void checkForCrash();
    void clearLogs();
    int getLogCount();
    bool getLogEntry(int index, CrashLogEntry& entry);
    String formatLogEntry(CrashLogEntry& entry);
    void printAllLogs();
    
    // Watchdog timer
    void enableWatchdog(int timeoutSeconds = 8);
    void feedWatchdog();
    
    // Web interface helpers
    String generateCrashLogsHTML();
    String generateCrashLogsJSON();
};

// Global instance accessor (for WiFi managers)
CrashLogger* getCrashLogger();
void setCrashLogger(CrashLogger* logger);

#if defined(ESP32) || defined(ESP8266)
// ESP32/ESP8266 exception handler
void crashHandler();
#endif

#endif // CRASH_LOGGER_H

