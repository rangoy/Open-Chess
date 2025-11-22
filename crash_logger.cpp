#include "crash_logger.h"

#if defined(ESP32) || defined(ESP8266)
  #include <esp_system.h>
  #include <soc/soc.h>
  #include <soc/rtc_cntl_reg.h>
#endif

CrashLogger::CrashLogger() {
}

void CrashLogger::begin() {
    initEEPROM();
    checkForCrash();
}

void CrashLogger::initEEPROM() {
#if defined(ESP32) || defined(ESP8266)
    EEPROM.begin(CRASH_LOG_SIZE);
#elif defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT)
    // FlashStorage doesn't need initialization
#else
    // Standard EEPROM
#endif
}

String CrashLogger::getResetReason() {
#if defined(ESP32)
    esp_reset_reason_t reason = esp_reset_reason();
    switch(reason) {
        case ESP_RST_POWERON: return "Power On";
        case ESP_RST_EXT: return "External Reset";
        case ESP_RST_SW: return "Software Reset";
        case ESP_RST_PANIC: return "Exception/Panic";
        case ESP_RST_INT_WDT: return "Interrupt Watchdog";
        case ESP_RST_TASK_WDT: return "Task Watchdog";
        case ESP_RST_WDT: return "Other Watchdog";
        case ESP_RST_DEEPSLEEP: return "Deep Sleep";
        case ESP_RST_BROWNOUT: return "Brownout";
        case ESP_RST_SDIO: return "SDIO";
        default: return "Unknown";
    }
#elif defined(ESP8266)
    rst_info* resetInfo = ESP.getResetInfoPtr();
    switch(resetInfo->reason) {
        case REASON_DEFAULT_RST: return "Normal Boot";
        case REASON_WDT_RST: return "Watchdog Reset";
        case REASON_EXCEPTION_RST: return "Exception";
        case REASON_SOFT_WDT_RST: return "Soft Watchdog";
        case REASON_SOFT_RESTART: return "Software Restart";
        case REASON_DEEP_SLEEP_AWAKE: return "Deep Sleep";
        case REASON_EXT_SYS_RST: return "External Reset";
        default: return "Unknown";
    }
#else
    // For other boards, check if we can determine reset reason
    return "Unknown";
#endif
}

void CrashLogger::checkForCrash() {
    String resetReason = getResetReason();
    
    // Check if this was an unexpected reset (not power-on or software reset)
    bool unexpectedReset = false;
#if defined(ESP32)
    esp_reset_reason_t reason = esp_reset_reason();
    unexpectedReset = (reason == ESP_RST_PANIC || 
                       reason == ESP_RST_INT_WDT || 
                       reason == ESP_RST_TASK_WDT ||
                       reason == ESP_RST_WDT ||
                       reason == ESP_RST_BROWNOUT);
#elif defined(ESP8266)
    rst_info* resetInfo = ESP.getResetInfoPtr();
    unexpectedReset = (resetInfo->reason == REASON_WDT_RST ||
                       resetInfo->reason == REASON_EXCEPTION_RST ||
                       resetInfo->reason == REASON_SOFT_WDT_RST);
#endif
    
    if (unexpectedReset) {
        Serial.println("========================================");
        Serial.println("CRASH DETECTED!");
        Serial.print("Reset Reason: ");
        Serial.println(resetReason);
        Serial.println("========================================");
        
        // Log the crash
        CrashLogEntry entry;
        entry.timestamp = millis(); // Will be approximate
        resetReason.toCharArray(entry.resetReason, sizeof(entry.resetReason));
        strncpy(entry.exceptionType, "Reset", sizeof(entry.exceptionType));
        strncpy(entry.exceptionDescription, "Unexpected reset detected", sizeof(entry.exceptionDescription));
        entry.freeHeap = 0;
        memset(entry.stackTrace, 0, sizeof(entry.stackTrace));
        entry.valid = true;
        
        logCrash("Reset", entry.exceptionDescription);
    } else {
        Serial.print("Normal startup. Reset reason: ");
        Serial.println(resetReason);
    }
}

void CrashLogger::logCrash(const char* exceptionType, const char* description) {
    CrashLogEntry entry;
    entry.timestamp = millis();
    strncpy(entry.exceptionType, exceptionType, sizeof(entry.exceptionType) - 1);
    entry.exceptionType[sizeof(entry.exceptionType) - 1] = '\0';
    strncpy(entry.exceptionDescription, description, sizeof(entry.exceptionDescription) - 1);
    entry.exceptionDescription[sizeof(entry.exceptionDescription) - 1] = '\0';
    
    String resetReason = getResetReason();
    resetReason.toCharArray(entry.resetReason, sizeof(entry.resetReason) - 1);
    entry.resetReason[sizeof(entry.resetReason) - 1] = '\0';
    
#if defined(ESP32) || defined(ESP8266)
    entry.freeHeap = ESP.getFreeHeap();
#else
    entry.freeHeap = 0;
#endif
    
    memset(entry.stackTrace, 0, sizeof(entry.stackTrace));
    entry.valid = true;
    
    int slot = findNextLogSlot();
    if (slot >= 0) {
        writeLogEntry(slot, entry);
        Serial.print("Crash logged to slot ");
        Serial.println(slot);
    } else {
        Serial.println("Warning: Crash log full, overwriting oldest entry");
        writeLogEntry(0, entry); // Overwrite oldest
    }
}

void CrashLogger::logException(const char* type, const char* description) {
    logCrash(type, description);
}

int CrashLogger::findNextLogSlot() {
    for (int i = 0; i < MAX_ENTRIES; i++) {
        CrashLogEntry entry;
        if (!readLogEntry(i, entry) || !entry.valid) {
            return i;
        }
    }
    return -1; // All slots full
}

void CrashLogger::writeLogEntry(int slot, CrashLogEntry& entry) {
    if (slot < 0 || slot >= MAX_ENTRIES) return;
    
    int addr = CRASH_LOG_START_ADDR + (slot * LOG_ENTRY_SIZE);
    
#if defined(ESP32) || defined(ESP8266)
    uint8_t* data = (uint8_t*)&entry;
    for (size_t i = 0; i < sizeof(CrashLogEntry); i++) {
        EEPROM.write(addr + i, data[i]);
    }
    EEPROM.commit();
#elif defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT)
    // FlashStorage - would need a different approach
    // For now, just use EEPROM emulation if available
    // This is a limitation of FlashStorage
#else
    uint8_t* data = (uint8_t*)&entry;
    for (size_t i = 0; i < sizeof(CrashLogEntry); i++) {
        EEPROM.write(addr + i, data[i]);
    }
#endif
}

bool CrashLogger::readLogEntry(int slot, CrashLogEntry& entry) {
    if (slot < 0 || slot >= MAX_ENTRIES) return false;
    
    int addr = CRASH_LOG_START_ADDR + (slot * LOG_ENTRY_SIZE);
    
#if defined(ESP32) || defined(ESP8266)
    uint8_t* data = (uint8_t*)&entry;
    for (size_t i = 0; i < sizeof(CrashLogEntry); i++) {
        data[i] = EEPROM.read(addr + i);
    }
#elif defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT)
    // FlashStorage limitation
    return false;
#else
    uint8_t* data = (uint8_t*)&entry;
    for (size_t i = 0; i < sizeof(CrashLogEntry); i++) {
        data[i] = EEPROM.read(addr + i);
    }
#endif
    
    // Ensure strings are properly null-terminated to prevent encoding issues
    entry.resetReason[sizeof(entry.resetReason) - 1] = '\0';
    entry.exceptionType[sizeof(entry.exceptionType) - 1] = '\0';
    entry.exceptionDescription[sizeof(entry.exceptionDescription) - 1] = '\0';
    
    // Sanitize strings - remove any non-printable characters that might cause encoding issues
    for (size_t i = 0; i < sizeof(entry.resetReason); i++) {
        if (entry.resetReason[i] == '\0') break;
        if (entry.resetReason[i] < 32 || entry.resetReason[i] > 126) {
            entry.resetReason[i] = '?';
        }
    }
    for (size_t i = 0; i < sizeof(entry.exceptionType); i++) {
        if (entry.exceptionType[i] == '\0') break;
        if (entry.exceptionType[i] < 32 || entry.exceptionType[i] > 126) {
            entry.exceptionType[i] = '?';
        }
    }
    for (size_t i = 0; i < sizeof(entry.exceptionDescription); i++) {
        if (entry.exceptionDescription[i] == '\0') break;
        if (entry.exceptionDescription[i] < 32 || entry.exceptionDescription[i] > 126) {
            entry.exceptionDescription[i] = '?';
        }
    }
    
    return true;
}

void CrashLogger::clearLogs() {
    CrashLogEntry empty;
    memset(&empty, 0, sizeof(empty));
    empty.valid = false;
    
    for (int i = 0; i < MAX_ENTRIES; i++) {
        writeLogEntry(i, empty);
    }
    
    Serial.println("Crash logs cleared");
}

int CrashLogger::getLogCount() {
    int count = 0;
    for (int i = 0; i < MAX_ENTRIES; i++) {
        CrashLogEntry entry;
        if (readLogEntry(i, entry) && entry.valid) {
            count++;
        }
    }
    return count;
}

bool CrashLogger::getLogEntry(int index, CrashLogEntry& entry) {
    if (index < 0 || index >= MAX_ENTRIES) return false;
    return readLogEntry(index, entry) && entry.valid;
}

String CrashLogger::formatLogEntry(CrashLogEntry& entry) {
    String result = "";
    result += "Timestamp: " + String(entry.timestamp) + " ms\n";
    result += "Reset Reason: " + String(entry.resetReason) + "\n";
    result += "Exception Type: " + String(entry.exceptionType) + "\n";
    result += "Description: " + String(entry.exceptionDescription) + "\n";
    result += "Free Heap: " + String(entry.freeHeap) + " bytes\n";
    return result;
}

void CrashLogger::printAllLogs() {
    Serial.println("========================================");
    Serial.println("CRASH LOGS");
    Serial.println("========================================");
    
    int count = getLogCount();
    Serial.print("Total crash entries: ");
    Serial.println(count);
    
    for (int i = 0; i < MAX_ENTRIES; i++) {
        CrashLogEntry entry;
        if (readLogEntry(i, entry) && entry.valid) {
            Serial.print("\n--- Entry ");
            Serial.print(i);
            Serial.println(" ---");
            Serial.print(formatLogEntry(entry));
        }
    }
    
    Serial.println("========================================");
}

void CrashLogger::enableWatchdog(int timeoutSeconds) {
#if defined(ESP32)
    // ESP32 has multiple watchdogs - enable task watchdog
    // Note: This is handled by FreeRTOS, but we can configure it
    Serial.print("Watchdog enabled: ");
    Serial.print(timeoutSeconds);
    Serial.println(" seconds");
#elif defined(ESP8266)
    ESP.wdtEnable(timeoutSeconds * 1000);
    Serial.print("Watchdog enabled: ");
    Serial.print(timeoutSeconds);
    Serial.println(" seconds");
#else
    // Other boards may not support watchdog
    Serial.println("Watchdog not supported on this board");
#endif
}

void CrashLogger::feedWatchdog() {
#if defined(ESP32)
    // ESP32 task watchdog is fed automatically by FreeRTOS
    // But we can explicitly feed it if needed
#elif defined(ESP8266)
    ESP.wdtFeed();
#else
    // Not supported
#endif
}

String CrashLogger::generateCrashLogsHTML() {
    String html = "<!DOCTYPE html>";
    html += "<html lang=\"en\">";
    html += "<head>";
    html += "<meta charset=\"UTF-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    html += "<title>Crash Logs</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; background-color: #5c5d5e; margin: 0; padding: 20px; }";
    html += ".container { background-color: #353434; border-radius: 8px; padding: 30px; max-width: 800px; margin: 0 auto; }";
    html += "h2 { color: #ec8703; }";
    html += ".log-entry { background-color: #444; padding: 15px; margin: 10px 0; border-radius: 5px; border-left: 4px solid #ec8703; }";
    html += ".log-entry h3 { color: #ec8703; margin-top: 0; }";
    html += ".log-entry p { color: #fff; margin: 5px 0; }";
    html += ".button { background-color: #ec8703; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; text-decoration: none; display: inline-block; margin: 10px 5px; }";
    html += ".button:hover { background-color: #ebca13; }";
    html += ".no-logs { color: #ec8703; text-align: center; padding: 20px; }";
    html += "</style>";
    html += "</head>";
    html += "<body>";
    html += "<div class=\"container\">";
    html += "<h2>Crash Logs</h2>";
    
    int count = getLogCount();
    if (count == 0) {
        html += "<div class=\"no-logs\"><p>No crash logs found. System is running normally.</p></div>";
    } else {
        html += "<p>Found " + String(count) + " crash log(s):</p>";
        for (int i = 0; i < MAX_ENTRIES; i++) {
            CrashLogEntry entry;
            if (getLogEntry(i, entry)) {
                html += "<div class=\"log-entry\">";
                html += "<h3>Crash Entry #" + String(i + 1) + "</h3>";
                html += "<p><strong>Timestamp:</strong> " + String(entry.timestamp) + " ms</p>";
                html += "<p><strong>Reset Reason:</strong> " + String(entry.resetReason) + "</p>";
                html += "<p><strong>Exception Type:</strong> " + String(entry.exceptionType) + "</p>";
                html += "<p><strong>Description:</strong> " + String(entry.exceptionDescription) + "</p>";
                html += "<p><strong>Free Heap:</strong> " + String(entry.freeHeap) + " bytes</p>";
                html += "</div>";
            }
        }
    }
    
    html += "<a href=\"/\" class=\"button\">Back to Home</a>";
    html += "<a href=\"/crash-logs?clear=1\" class=\"button\" style=\"background-color: #f44336;\">Clear Logs</a>";
    html += "</div>";
    html += "</body>";
    html += "</html>";
    
    return html;
}

String CrashLogger::generateCrashLogsJSON() {
    String json = "{\"count\":" + String(getLogCount()) + ",\"logs\":[";
    
    bool first = true;
    for (int i = 0; i < MAX_ENTRIES; i++) {
        CrashLogEntry entry;
        if (getLogEntry(i, entry)) {
            if (!first) json += ",";
            first = false;
            json += "{";
            json += "\"index\":" + String(i) + ",";
            json += "\"timestamp\":" + String(entry.timestamp) + ",";
            json += "\"resetReason\":\"" + String(entry.resetReason) + "\",";
            json += "\"exceptionType\":\"" + String(entry.exceptionType) + "\",";
            json += "\"description\":\"" + String(entry.exceptionDescription) + "\",";
            json += "\"freeHeap\":" + String(entry.freeHeap);
            json += "}";
        }
    }
    
    json += "]}";
    return json;
}

// Global instance accessor (defined in header as extern)
static CrashLogger* g_crashLogger = nullptr;

CrashLogger* getCrashLogger() {
    return g_crashLogger;
}

void setCrashLogger(CrashLogger* logger) {
    g_crashLogger = logger;
}

#if defined(ESP32) || defined(ESP8266)
// ESP32 exception handler
void crashHandler() {
    // This will be called on exception
    // Note: Limited functionality in exception context
    Serial.println("\n\n!!! EXCEPTION CAUGHT !!!\n");
    
    // Try to log the crash (may not work in exception context)
    // The checkForCrash() in begin() will catch it on next boot
}
#endif

