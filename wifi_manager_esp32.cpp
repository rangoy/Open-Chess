#include "wifi_manager_esp32.h"
#include "wifi_html_templates.h"
#include "crash_logger.h"
#include <Arduino.h>
#include "arduino_secrets.h"

WiFiManagerESP32::WiFiManagerESP32() : server(AP_PORT) {
    apMode = true;
    clientConnected = false;
    wifiSSID = "";
    wifiPassword = "";
    lichessToken = "";
    gameMode = "None";
    startupType = "WiFi";
    boardStateValid = false;
    hasPendingEdit = false;
    boardEvaluation = 0.0;
    boardPGN = "";
    moveDetectionPaused = false;
    pendingUndoRequest = false;
    lastUndoSucceeded = false;
    selectedWhitePlayer = PLAYER_HUMAN;
    selectedBlackPlayer = PLAYER_HUMAN;
    playerSelectionReady = false;
    
    // Initialize board state to empty
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            boardState[row][col] = ' ';
            pendingBoardEdit[row][col] = ' ';
        }
    }
}

void WiFiManagerESP32::begin() {
    Serial.println("=== Starting OpenChess WiFi Manager (ESP32) ===");
    Serial.println("Debug: WiFi Manager begin() called");
    
    // ESP32 can run both AP and Station modes simultaneously
    // Start Access Point first (always available)
    Serial.print("Debug: Creating Access Point with SSID: ");
    Serial.println(AP_SSID);
    Serial.print("Debug: Using password: ");
    Serial.println(AP_PASSWORD);
    
    Serial.println("Debug: Calling WiFi.softAP()...");
    
    // Create Access Point
    bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD);
    
    if (!apStarted) {
        Serial.println("ERROR: Failed to create Access Point!");
        return;
    }
    
    Serial.println("Debug: Access Point created successfully");
    
    // Try to connect to existing WiFi (if credentials available)
    bool connected = false;
    
    if (wifiSSID.length() > 0 || strlen(SECRET_SSID) > 0) {
        String ssidToUse = wifiSSID.length() > 0 ? wifiSSID : String(SECRET_SSID);
        String passToUse = wifiPassword.length() > 0 ? wifiPassword : String(SECRET_PASS);
        
        Serial.println("=== Attempting to connect to WiFi network ===");
        Serial.print("SSID: ");
        Serial.println(ssidToUse);
        
        connected = connectToWiFi(ssidToUse, passToUse);
        
        if (connected) {
            Serial.println("Successfully connected to WiFi network!");
        } else {
            Serial.println("Failed to connect to WiFi. Access Point mode still available.");
        }
    }
    
    // Wait a moment for everything to stabilize
    delay(100);
    
    // Print connection information
    Serial.println("=== WiFi Connection Information ===");
    Serial.print("Access Point SSID: ");
    Serial.println(AP_SSID);
    Serial.print("Access Point IP: ");
    Serial.println(WiFi.softAPIP());
    if (connected) {
        Serial.print("Connected to WiFi: ");
        Serial.println(WiFi.SSID());
        Serial.print("Station IP: ");
        Serial.println(WiFi.localIP());
        Serial.print("Access board via: http://");
        Serial.println(WiFi.localIP());
    } else {
        Serial.print("Access board via: http://");
        Serial.println(WiFi.softAPIP());
    }
    Serial.print("MAC Address: ");
    Serial.println(WiFi.softAPmacAddress());
    Serial.println("=====================================");
    
    // Set up web server routes
    server.on("/", HTTP_GET, [this]() { this->handleRoot(); });
    server.on("/game", HTTP_GET, [this]() { 
        String gameSelectionPage = this->generateGameSelectionPage();
        this->server.send(200, "text/html", gameSelectionPage);
    });
    server.on("/board", HTTP_GET, [this]() { this->handleBoard(); });
    server.on("/board-view", HTTP_GET, [this]() { this->handleBoardView(); });
    server.on("/board-edit", HTTP_GET, [this]() { 
        String boardEditPage = this->generateBoardEditPage();
        this->server.send(200, "text/html", boardEditPage);
    });
    server.on("/board-edit", HTTP_POST, [this]() { this->handleBoardEdit(); });
    server.on("/connect-wifi", HTTP_POST, [this]() { this->handleConnectWiFi(); });
    server.on("/submit", HTTP_POST, [this]() { this->handleConfigSubmit(); });
    server.on("/gameselect", HTTP_POST, [this]() { this->handleGameSelection(); });
    server.on("/pause-moves", HTTP_GET, [this]() { this->handleGetPauseState(); });
    server.on("/pause-moves", HTTP_POST, [this]() { this->handlePauseMoves(); });
    server.on("/undo-move", HTTP_POST, [this]() { this->handleUndoMove(); });
    server.on("/crash-logs", HTTP_GET, [this]() { 
        CrashLogger* logger = getCrashLogger();
        if (logger && server.hasArg("clear") && server.arg("clear") == "1") {
            logger->clearLogs();
            this->server.send(200, "text/html", "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'><h2>Logs Cleared</h2><p><a href='/crash-logs' style='color:#ec8703;'>View Logs</a></p></body></html>");
        } else if (logger) {
            String logsPage = logger->generateCrashLogsHTML();
            this->server.send(200, "text/html", logsPage);
        } else {
            this->server.send(200, "text/html", "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'><h2>Crash Logger Not Available</h2></body></html>");
        }
    });
    server.onNotFound([this]() {
        String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
        response += "<h2>404 - Page Not Found</h2>";
        response += "<p><a href='/' style='color:#ec8703;'>Back to Home</a></p>";
        response += "</body></html>";
        this->sendResponse(response, "text/html");
    });
    
    // Start the web server
    Serial.println("Debug: Starting web server...");
    server.begin();
    Serial.println("Debug: Web server started on port 80");
    Serial.println("WiFi Manager initialization complete!");
}

void WiFiManagerESP32::handleClient() {
    server.handleClient();
}

void WiFiManagerESP32::handleRoot() {
    String webpage = generateWebPage();
    sendResponse(webpage);
}

void WiFiManagerESP32::handleConfigSubmit() {
    if (server.hasArg("plain")) {
        parseFormData(server.arg("plain"));
    } else {
        // Try to get form data from POST body
        String body = "";
        while (server.hasArg("ssid") || server.hasArg("password") || server.hasArg("token") || 
               server.hasArg("gameMode") || server.hasArg("startupType")) {
            if (server.hasArg("ssid")) wifiSSID = server.arg("ssid");
            if (server.hasArg("password")) wifiPassword = server.arg("password");
            if (server.hasArg("token")) lichessToken = server.arg("token");
            if (server.hasArg("gameMode")) gameMode = server.arg("gameMode");
            if (server.hasArg("startupType")) startupType = server.arg("startupType");
            break;
        }
    }
    
    String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
    response += "<h2>Configuration Saved!</h2>";
    response += "<p>WiFi SSID: " + wifiSSID + "</p>";
    response += "<p>Game Mode: " + gameMode + "</p>";
    response += "<p>Startup Type: " + startupType + "</p>";
    response += "<p><a href='/game' style='color:#ec8703;'>Go to Game Selection</a></p>";
    response += "</body></html>";
    sendResponse(response);
}

void WiFiManagerESP32::handleGameSelection() {
    // Parse unified game player selection (JSON format)
    PlayerType whitePlayer = PLAYER_HUMAN;
    PlayerType blackPlayer = PLAYER_HUMAN;
    
    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        // Try to parse JSON: {"white":0,"black":1} or form data: white=0&black=1
        int whiteStart = body.indexOf("\"white\":");
        int blackStart = body.indexOf("\"black\":");
        
        if (whiteStart >= 0 && blackStart >= 0) {
            // JSON format
            whiteStart += 8; // Skip "white":
            int whiteEnd = body.indexOf(",", whiteStart);
            if (whiteEnd < 0) whiteEnd = body.indexOf("}", whiteStart);
            String whiteStr = body.substring(whiteStart, whiteEnd);
            whiteStr.trim();
            whitePlayer = (PlayerType)whiteStr.toInt();
            
            blackStart += 8; // Skip "black":
            int blackEnd = body.indexOf("}", blackStart);
            if (blackEnd < 0) blackEnd = body.length();
            String blackStr = body.substring(blackStart, blackEnd);
            blackStr.trim();
            blackPlayer = (PlayerType)blackStr.toInt();
        } else {
            // Form data format: white=0&black=1 or legacy gamemode=X
            int whiteIdx = body.indexOf("white=");
            int blackIdx = body.indexOf("black=");
            int modeIdx = body.indexOf("gamemode=");
            
            if (whiteIdx >= 0 && blackIdx >= 0) {
                whiteIdx += 6;
                int whiteEnd = body.indexOf("&", whiteIdx);
                if (whiteEnd < 0) whiteEnd = body.length();
                whitePlayer = (PlayerType)body.substring(whiteIdx, whiteEnd).toInt();
                
                blackIdx += 6;
                int blackEnd = body.indexOf("&", blackIdx);
                if (blackEnd < 0) blackEnd = body.length();
                blackPlayer = (PlayerType)body.substring(blackIdx, blackEnd).toInt();
            } else if (modeIdx >= 0) {
                // Legacy mode selection - map to unified game
                modeIdx += 9;
                int modeEnd = body.indexOf("&", modeIdx);
                if (modeEnd < 0) modeEnd = body.length();
                int mode = body.substring(modeIdx, modeEnd).toInt();
                
                // Map old modes to unified game
                if (mode == 1) {
                    whitePlayer = PLAYER_HUMAN;
                    blackPlayer = PLAYER_HUMAN;
                } else if (mode == 2) {
                    whitePlayer = PLAYER_HUMAN;
                    blackPlayer = PLAYER_BOT_MEDIUM;
                } else if (mode == 3) {
                    whitePlayer = PLAYER_BOT_MEDIUM;
                    blackPlayer = PLAYER_HUMAN;
                } else if (mode == 5) {
                    whitePlayer = PLAYER_BOT_MEDIUM;
                    blackPlayer = PLAYER_BOT_MEDIUM;
                }
            }
        }
    } else if (server.hasArg("white") && server.hasArg("black")) {
        whitePlayer = (PlayerType)server.arg("white").toInt();
        blackPlayer = (PlayerType)server.arg("black").toInt();
    } else if (server.hasArg("gamemode")) {
        // Legacy mode selection
        int mode = server.arg("gamemode").toInt();
        if (mode == 1) {
            whitePlayer = PLAYER_HUMAN;
            blackPlayer = PLAYER_HUMAN;
        } else if (mode == 2) {
            whitePlayer = PLAYER_HUMAN;
            blackPlayer = PLAYER_BOT_MEDIUM;
        } else if (mode == 3) {
            whitePlayer = PLAYER_BOT_MEDIUM;
            blackPlayer = PLAYER_HUMAN;
        } else if (mode == 5) {
            whitePlayer = PLAYER_BOT_MEDIUM;
            blackPlayer = PLAYER_BOT_MEDIUM;
        }
    }
    
    Serial.print("Player selection via web - White: ");
    Serial.print(whitePlayer);
    Serial.print(", Black: ");
    Serial.println(blackPlayer);
    
    // Store the selected players
    setSelectedPlayers(whitePlayer, blackPlayer);
    
    String response = "{\"status\":\"success\",\"message\":\"Game players selected\",\"white\":" + String(whitePlayer) + ",\"black\":" + String(blackPlayer) + "}";
    sendResponse(response, "application/json");
}

void WiFiManagerESP32::sendResponse(String content, String contentType) {
    server.send(200, contentType, content);
}

String WiFiManagerESP32::generateWebPage() {
    using namespace WiFiHTMLTemplates;
    return generateConfigPage(wifiSSID, wifiPassword, lichessToken, gameMode, startupType, 
                              getConnectionStatus(), !isConnectedToWiFi() || wifiSSID.length() > 0);
}

String WiFiManagerESP32::generateGameSelectionPage() {
    using namespace WiFiHTMLTemplates;
    return WiFiHTMLTemplates::generateGameSelectionPage();
}

void WiFiManagerESP32::parseFormData(String data) {
    // Parse URL-encoded form data
    int ssidStart = data.indexOf("ssid=");
    if (ssidStart >= 0) {
        int ssidEnd = data.indexOf("&", ssidStart);
        if (ssidEnd < 0) ssidEnd = data.length();
        wifiSSID = data.substring(ssidStart + 5, ssidEnd);
        wifiSSID.replace("+", " ");
        wifiSSID.replace("%20", " ");
    }
    
    int passStart = data.indexOf("password=");
    if (passStart >= 0) {
        int passEnd = data.indexOf("&", passStart);
        if (passEnd < 0) passEnd = data.length();
        wifiPassword = data.substring(passStart + 9, passEnd);
        wifiPassword.replace("+", " ");
        wifiPassword.replace("%20", " ");
    }
    
    int tokenStart = data.indexOf("token=");
    if (tokenStart >= 0) {
        int tokenEnd = data.indexOf("&", tokenStart);
        if (tokenEnd < 0) tokenEnd = data.length();
        lichessToken = data.substring(tokenStart + 6, tokenEnd);
        lichessToken.replace("+", " ");
        lichessToken.replace("%20", " ");
    }
    
    int gameModeStart = data.indexOf("gameMode=");
    if (gameModeStart >= 0) {
        int gameModeEnd = data.indexOf("&", gameModeStart);
        if (gameModeEnd < 0) gameModeEnd = data.length();
        gameMode = data.substring(gameModeStart + 9, gameModeEnd);
        gameMode.replace("+", " ");
        gameMode.replace("%20", " ");
    }
    
    int startupStart = data.indexOf("startupType=");
    if (startupStart >= 0) {
        int startupEnd = data.indexOf("&", startupStart);
        if (startupEnd < 0) startupEnd = data.length();
        startupType = data.substring(startupStart + 12, startupEnd);
    }
    
    Serial.println("Configuration updated:");
    Serial.println("SSID: " + wifiSSID);
    Serial.println("Game Mode: " + gameMode);
    Serial.println("Startup Type: " + startupType);
}

bool WiFiManagerESP32::isClientConnected() {
    return WiFi.softAPgetStationNum() > 0;
}

int WiFiManagerESP32::getSelectedGameMode() {
    return gameMode.toInt();
}

void WiFiManagerESP32::resetGameSelection() {
    gameMode = "0";
    playerSelectionReady = false;
    selectedWhitePlayer = PLAYER_HUMAN;
    selectedBlackPlayer = PLAYER_HUMAN;
}

PlayerType WiFiManagerESP32::getSelectedWhitePlayer() {
    return playerSelectionReady ? selectedWhitePlayer : PLAYER_HUMAN;
}

PlayerType WiFiManagerESP32::getSelectedBlackPlayer() {
    return playerSelectionReady ? selectedBlackPlayer : PLAYER_HUMAN;
}

void WiFiManagerESP32::setSelectedPlayers(PlayerType white, PlayerType black) {
    selectedWhitePlayer = white;
    selectedBlackPlayer = black;
    playerSelectionReady = true;
}

void WiFiManagerESP32::updateBoardState(char newBoardState[8][8]) {
    updateBoardState(newBoardState, 0.0, "");
}

void WiFiManagerESP32::updateBoardState(char newBoardState[8][8], float evaluation) {
    updateBoardState(newBoardState, evaluation, "");
}

void WiFiManagerESP32::updateBoardState(char newBoardState[8][8], float evaluation, String pgn) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            boardState[row][col] = newBoardState[row][col];
        }
    }
    boardStateValid = true;
    boardEvaluation = evaluation;
    boardPGN = pgn;
}

String WiFiManagerESP32::generateBoardJSON() {
    String json = "{";
    json += "\"board\":[";
    
    for (int row = 0; row < 8; row++) {
        json += "[";
        for (int col = 0; col < 8; col++) {
            char piece = boardState[row][col];
            if (piece == ' ') {
                json += "\"\"";
            } else {
                json += "\"";
                json += String(piece);
                json += "\"";
            }
            if (col < 7) json += ",";
        }
        json += "]";
        if (row < 7) json += ",";
    }
    
    json += "],";
    json += "\"valid\":" + String(boardStateValid ? "true" : "false");
    json += ",\"evaluation\":" + String(boardEvaluation, 2);
    if (boardPGN.length() > 0) {
        json += ",\"pgn\":\"";
        // Escape quotes in PGN
        String escapedPGN = boardPGN;
        escapedPGN.replace("\"", "\\\"");
        json += escapedPGN;
        json += "\"";
    } else {
        json += ",\"pgn\":\"\"";
    }
    json += "}";
    
    return json;
}

void WiFiManagerESP32::handleBoard() {
    String boardJSON = generateBoardJSON();
    sendResponse(boardJSON, "application/json");
}

void WiFiManagerESP32::handleBoardView() {
    String boardViewPage = generateBoardViewPage();
    sendResponse(boardViewPage, "text/html");
}

String WiFiManagerESP32::generateBoardViewPage() {
    using namespace WiFiHTMLTemplates;
    return generateBoardViewPageTemplate();
}

String WiFiManagerESP32::generateBoardEditPage() {
    using namespace WiFiHTMLTemplates;
    return generateBoardEditPageTemplate();
}

void WiFiManagerESP32::handleBoardEdit() {
    // Check if request has JSON body
    String contentType = server.header("Content-Type");
    if (contentType.indexOf("application/json") >= 0) {
        // Read JSON body - ESP32 WebServer stores body in "plain" argument
        String body = server.arg("plain");
        if (body.length() == 0) {
            // Try alternative method
            body = server.arg("body");
        }
        if (body.length() > 0) {
            Serial.print("Received JSON body: ");
            Serial.println(body);
            parseBoardEditDataJSON(body);
        } else {
            Serial.println("Warning: JSON Content-Type but no body data found, falling back to form data");
            parseBoardEditData();
        }
    } else {
        // Parse form data (legacy support)
        parseBoardEditData();
    }
    
    String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
    response += "<h2>Board Updated!</h2>";
    response += "<p>Your board changes have been applied.</p>";
    response += "<p><a href='/board-view' style='color:#ec8703;'>View Board</a></p>";
    response += "<p><a href='/board-edit' style='color:#ec8703;'>Edit Again</a></p>";
    response += "<p><a href='/' style='color:#ec8703;'>Back to Home</a></p>";
    response += "</body></html>";
    sendResponse(response);
}

void WiFiManagerESP32::parseBoardEditDataJSON(String jsonData) {
    // Parse JSON: {"board":[["R","N","B",...],["r","n","b",...],...]}
    // Initialize all to empty
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            pendingBoardEdit[row][col] = ' ';
        }
    }
    
    // Find the board array start
    int boardArrayStart = jsonData.indexOf("\"board\":[");
    if (boardArrayStart < 0) {
        Serial.println("Error: Invalid JSON - no 'board' key found");
        return;
    }
    
    // Find the opening bracket of the board array
    int arrayStart = jsonData.indexOf('[', boardArrayStart + 8);
    if (arrayStart < 0) {
        Serial.println("Error: Invalid JSON - no board array found");
        return;
    }
    
    int pos = arrayStart + 1; // Position after '['
    int row = 0;
    
    // Parse each row
    while (row < 8 && pos < jsonData.length()) {
        // Skip whitespace
        while (pos < jsonData.length() && (jsonData.charAt(pos) == ' ' || jsonData.charAt(pos) == '\n' || jsonData.charAt(pos) == '\r' || jsonData.charAt(pos) == '\t')) {
            pos++;
        }
        
        if (pos >= jsonData.length()) break;
        
        // Check if this is a row array
        if (jsonData.charAt(pos) == '[') {
            pos++; // Skip '['
            int col = 0;
            
            // Parse row values
            while (col < 8 && pos < jsonData.length()) {
                // Skip whitespace
                while (pos < jsonData.length() && (jsonData.charAt(pos) == ' ' || jsonData.charAt(pos) == '\n' || jsonData.charAt(pos) == '\r' || jsonData.charAt(pos) == '\t')) {
                    pos++;
                }
                
                if (pos >= jsonData.length()) break;
                
                char piece = ' ';
                
                // Check for string value
                if (jsonData.charAt(pos) == '"') {
                    pos++; // Skip opening quote
                    if (pos < jsonData.length() && jsonData.charAt(pos) != '"') {
                        piece = jsonData.charAt(pos);
                        pos++;
                    }
                    // Find closing quote
                    while (pos < jsonData.length() && jsonData.charAt(pos) != '"') {
                        pos++;
                    }
                    if (pos < jsonData.length()) pos++; // Skip closing quote
                } else if (jsonData.charAt(pos) == 'n') {
                    // Check for "null"
                    if (jsonData.substring(pos, pos + 4) == "null") {
                        piece = ' ';
                        pos += 4;
                    }
                }
                
                pendingBoardEdit[row][col] = piece;
                col++;
                
                // Skip to next value (comma or closing bracket)
                while (pos < jsonData.length() && jsonData.charAt(pos) != ',' && jsonData.charAt(pos) != ']') {
                    pos++;
                }
                if (pos < jsonData.length() && jsonData.charAt(pos) == ',') {
                    pos++; // Skip comma
                }
            }
            
            row++;
            
            // Skip to next row or closing bracket
            while (pos < jsonData.length() && jsonData.charAt(pos) != '[' && jsonData.charAt(pos) != ']') {
                pos++;
            }
            if (pos < jsonData.length() && jsonData.charAt(pos) == ']') {
                pos++; // Skip row closing bracket
                if (pos < jsonData.length() && jsonData.charAt(pos) == ',') {
                    pos++; // Skip comma before next row
                }
            }
        } else {
            break; // Not a row array, exit
        }
    }
    
    hasPendingEdit = true;
    Serial.println("Board edit received and stored (JSON)");
}

void WiFiManagerESP32::parseBoardEditData() {
    // Parse the form data which contains r0c0, r0c1, etc.
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            String paramName = "r" + String(row) + "c" + String(col);
            
            if (server.hasArg(paramName)) {
                String value = server.arg(paramName);
                if (value.length() > 0) {
                    pendingBoardEdit[row][col] = value.charAt(0);
                } else {
                    pendingBoardEdit[row][col] = ' ';
                }
            } else {
                pendingBoardEdit[row][col] = ' ';
            }
        }
    }
    
    hasPendingEdit = true;
    Serial.println("Board edit received and stored");
}

bool WiFiManagerESP32::getPendingBoardEdit(char editBoard[8][8]) {
    if (hasPendingEdit) {
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                editBoard[row][col] = pendingBoardEdit[row][col];
            }
        }
        return true;
    }
    return false;
}

void WiFiManagerESP32::clearPendingEdit() {
    hasPendingEdit = false;
}

void WiFiManagerESP32::handleGetPauseState() {
    // Return current pause state without changing it
    String response = "{";
    response += "\"paused\":" + String(moveDetectionPaused ? "true" : "false");
    response += "}";
    server.send(200, "application/json", response);
}

void WiFiManagerESP32::handlePauseMoves() {
    // Set pause state based on request
    if (server.hasArg("paused")) {
        String pausedArg = server.arg("paused");
        moveDetectionPaused = (pausedArg == "true" || pausedArg == "1");
    } else {
        // If no argument, toggle
        moveDetectionPaused = !moveDetectionPaused;
    }
    
    Serial.print("Move detection ");
    Serial.println(moveDetectionPaused ? "PAUSED" : "RESUMED");
    
    // Return JSON response
    String response = "{";
    response += "\"paused\":" + String(moveDetectionPaused ? "true" : "false");
    response += "}";
    server.send(200, "application/json", response);
}

void WiFiManagerESP32::handleUndoMove() {
    // Set flag to request undo from main loop
    // The main loop will process this on the next iteration
    pendingUndoRequest = true;
    lastUndoSucceeded = false;  // Reset result
    
    Serial.println("Undo move requested via web interface");
    
    // Return success - the actual undo will happen in the main loop
    // The page reload will show the result
    String response = "{";
    response += "\"success\":true";
    response += "}";
    server.send(200, "application/json", response);
}

bool WiFiManagerESP32::connectToWiFi(String ssid, String password) {
    Serial.println("=== Connecting to WiFi Network ===");
    Serial.print("SSID: ");
    Serial.println(ssid);
    
    // ESP32 can run both AP and Station modes simultaneously
    WiFi.mode(WIFI_AP_STA); // Enable both AP and Station modes
    
    WiFi.begin(ssid.c_str(), password.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
        Serial.print("Connection attempt ");
        Serial.print(attempts);
        Serial.print("/20 - Status: ");
        Serial.println(WiFi.status());
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        apMode = false; // We're connected, but AP is still running
        return true;
    } else {
        Serial.println("Failed to connect to WiFi");
        // AP mode is still available
        return false;
    }
}

bool WiFiManagerESP32::startAccessPoint() {
    // AP is already started in begin(), this is just for compatibility
    return WiFi.softAP(AP_SSID, AP_PASSWORD);
}

IPAddress WiFiManagerESP32::getIPAddress() {
    // Return station IP if connected, otherwise AP IP
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP();
    } else {
        return WiFi.softAPIP();
    }
}

bool WiFiManagerESP32::isConnectedToWiFi() {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManagerESP32::getConnectionStatus() {
    String status = "";
    if (WiFi.status() == WL_CONNECTED) {
        status = "Connected to: " + WiFi.SSID() + " (IP: " + WiFi.localIP().toString() + ")";
        status += " | AP also available at: " + WiFi.softAPIP().toString();
    } else {
        status = "Access Point Mode - SSID: " + String(AP_SSID) + " (IP: " + WiFi.softAPIP().toString() + ")";
    }
    return status;
}

void WiFiManagerESP32::handleConnectWiFi() {
    // Parse WiFi credentials from POST
    if (server.hasArg("ssid")) {
        wifiSSID = server.arg("ssid");
    }
    if (server.hasArg("password")) {
        wifiPassword = server.arg("password");
    }
    
    if (wifiSSID.length() > 0) {
        Serial.println("Attempting to connect to WiFi from web interface...");
        bool connected = connectToWiFi(wifiSSID, wifiPassword);
        
        String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
        if (connected) {
            response += "<h2>WiFi Connected!</h2>";
            response += "<p>Successfully connected to: " + wifiSSID + "</p>";
            response += "<p>Station IP Address: " + WiFi.localIP().toString() + "</p>";
            response += "<p>Access Point still available at: " + WiFi.softAPIP().toString() + "</p>";
            response += "<p>You can access the board at either IP address.</p>";
        } else {
            response += "<h2>WiFi Connection Failed</h2>";
            response += "<p>Could not connect to: " + wifiSSID + "</p>";
            response += "<p>Please check your credentials and try again.</p>";
            response += "<p>Access Point mode is still available at: " + WiFi.softAPIP().toString() + "</p>";
        }
        response += "<p><a href='/' style='color:#ec8703;'>Back to Configuration</a></p>";
        response += "</body></html>";
        sendResponse(response);
    } else {
        String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
        response += "<h2>Error</h2>";
        response += "<p>No WiFi SSID provided.</p>";
        response += "<p><a href='/' style='color:#ec8703;'>Back to Configuration</a></p>";
        response += "</body></html>";
        sendResponse(response);
    }
}

