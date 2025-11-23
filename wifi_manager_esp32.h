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
#include "unified_chess_game.h"

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
    String boardPGN;  // PGN notation of the game
    String boardFEN;  // FEN notation of the game
    
    // Board edit storage (pending edits from web interface)
    char pendingBoardEdit[8][8];
    bool hasPendingEdit;
    
    // Move detection pause state
    bool moveDetectionPaused;
    
    // Undo request flag
    bool pendingUndoRequest;
    bool lastUndoSucceeded;
    
    // Unified game player selection
    PlayerType selectedWhitePlayer;
    PlayerType selectedBlackPlayer;
    bool playerSelectionReady;
    
    // WiFi connection methods
    bool connectToWiFi(String ssid, String password);
    bool startAccessPoint();
    IPAddress getIPAddress();
    bool isConnectedToWiFi();
    
    // Web interface methods
    String generateWebPage();
    void handleGetConfig();
    String generateBoardJSON();
    void handleRoot();
    void handleGameSelection();
    void handleConfigSubmit();
    void handleBoard();
    void handleBoardView();
    void handleBoardEdit();
    void handleGetPauseState();
    void handlePauseMoves();
    void handleUndoMove();
    void handleConnectWiFi();
    void sendResponse(String content, String contentType = "text/html");
    void parseFormData(String data);
    void parseBoardEditData();
    void parseBoardEditDataJSON(String jsonData);
    
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
    
    // Game selection via web (legacy - for compatibility)
    int getSelectedGameMode();
    void resetGameSelection();
    
    // Unified game selection via web
    bool hasPlayerSelection() { return playerSelectionReady; }
    PlayerType getSelectedWhitePlayer();
    PlayerType getSelectedBlackPlayer();
    void setSelectedPlayers(PlayerType white, PlayerType black);
    
    // Board state management
    void updateBoardState(char newBoardState[8][8]);
    void updateBoardState(char newBoardState[8][8], float evaluation);
    void updateBoardState(char newBoardState[8][8], float evaluation, String pgn);
    void updateBoardState(char newBoardState[8][8], float evaluation, String pgn, String fen);
    bool hasValidBoardState() { return boardStateValid; }
    float getEvaluation() { return boardEvaluation; }
    String getPGN() { return boardPGN; }
    
    // Board edit management
    bool getPendingBoardEdit(char editBoard[8][8]);
    void clearPendingEdit();
    
    // Move detection pause control
    bool isMoveDetectionPaused() { return moveDetectionPaused; }
    void setMoveDetectionPaused(bool paused) { moveDetectionPaused = paused; }
    
    // Undo request control
    bool hasPendingUndoRequest() { return pendingUndoRequest; }
    void clearUndoRequest() { pendingUndoRequest = false; }
    void setUndoResult(bool success) { lastUndoSucceeded = success; }
    bool getLastUndoResult() { return lastUndoSucceeded; }
    
    // WiFi status
    String getConnectionStatus();
};

#endif // WIFI_MANAGER_ESP32_H

