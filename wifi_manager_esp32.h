#ifndef WIFI_MANAGER_ESP32_H
#define WIFI_MANAGER_ESP32_H

#if !defined(ESP32) && !defined(ESP8266)
  #error "wifi_manager_esp32.h is only for ESP32/ESP8266 boards"
#endif

// Include Arduino.h first to set up ESP32 environment
#include <Arduino.h>

// ESP32 uses built-in WiFi library from the core
// Note: If you get WiFiNINA errors, ensure:
// 1. You're compiling for ESP32 board (Tools -> Board -> ESP32)
// 2. WiFiNINA library is not interfering (you may need to temporarily remove it)
#include <WiFi.h>
#include <WebServer.h>

// ---------------------------
// WiFi Configuration
// ---------------------------
#define AP_SSID "OpenChessBoard"
#define AP_PASSWORD "chess123"
#define AP_PORT 80

// ---------------------------
// WiFi Manager Class for ESP32
// ---------------------------
class WiFiManagerESP32 {
private:
    WebServer server;
    bool apMode;
    bool clientConnected;
    
    // Configuration variables
    String wifiSSID;
    String wifiPassword;
    String lichessToken;
    String gameMode;
    String startupType;
    
    // Board state storage
    char boardState[8][8];
    bool boardStateValid;
    float boardEvaluation;  // Stockfish evaluation (in centipawns)
    
    // Board edit storage (pending edits from web interface)
    char pendingBoardEdit[8][8];
    bool hasPendingEdit;
    
    // WiFi connection methods
    bool connectToWiFi(String ssid, String password);
    bool startAccessPoint();
    IPAddress getIPAddress();
    bool isConnectedToWiFi();
    
    // Web interface methods
    String generateWebPage();
    String generateGameSelectionPage();
    String generateBoardViewPage();
    String generateBoardEditPage();
    String generateBoardJSON();
    void handleRoot();
    void handleGameSelection();
    void handleConfigSubmit();
    void handleBoard();
    void handleBoardView();
    void handleBoardEdit();
    void handleConnectWiFi();
    void sendResponse(String content, String contentType = "text/html");
    void parseFormData(String data);
    void parseBoardEditData();
    
public:
    WiFiManagerESP32();
    void begin();
    void handleClient();
    bool isClientConnected();
    
    // Configuration getters
    String getWiFiSSID() { return wifiSSID; }
    String getWiFiPassword() { return wifiPassword; }
    String getLichessToken() { return lichessToken; }
    String getGameMode() { return gameMode; }
    String getStartupType() { return startupType; }
    
    // Game selection via web
    int getSelectedGameMode();
    void resetGameSelection();
    
    // Board state management
    void updateBoardState(char newBoardState[8][8]);
    void updateBoardState(char newBoardState[8][8], float evaluation);
    bool hasValidBoardState() { return boardStateValid; }
    float getEvaluation() { return boardEvaluation; }
    
    // Board edit management
    bool getPendingBoardEdit(char editBoard[8][8]);
    void clearPendingEdit();
    
    // WiFi status
    String getConnectionStatus();
};

#endif // WIFI_MANAGER_ESP32_H

