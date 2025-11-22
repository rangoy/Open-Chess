#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

// This WiFi manager is only for boards with WiFiNINA module
#if !defined(ESP32) && !defined(ESP8266) && (defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_NANO_RP2040_CONNECT))
  #include <WiFiNINA.h>
  #include <WiFiUdp.h>
  
  // Only define the class for WiFiNINA boards
  #define WIFI_MANAGER_WIFININA_ENABLED
#endif

#ifdef WIFI_MANAGER_WIFININA_ENABLED

// ---------------------------
// WiFi Configuration
// ---------------------------
#define AP_SSID "OpenChessBoard"
#define AP_PASSWORD "chess123"
#define AP_PORT 80

// ---------------------------
// WiFi Manager Class
// ---------------------------
class WiFiManager {
private:
    WiFiServer server;
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
    void handleConfigSubmit(WiFiClient& client, String request);
    void handleGameSelection(WiFiClient& client, String request);
    void handleBoardEdit(WiFiClient& client, String request, String body);
    void handleConnectWiFi(WiFiClient& client, String request, String body);
    void sendResponse(WiFiClient& client, String content, String contentType = "text/html");
    void parseFormData(String data);
    void parseBoardEditData(String data);
    
public:
    WiFiManager();
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

#endif // WIFI_MANAGER_WIFININA_ENABLED

#endif // WIFI_MANAGER_H