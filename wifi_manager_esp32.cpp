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
    
    // Set up web server routes - SPA routes
    server.on("/spa.html", HTTP_GET, [this]() { 
        using namespace WiFiHTMLTemplates;
        this->server.send(200, "text/html", generateSPAHTML());
    });
    server.on("/spa.css", HTTP_GET, [this]() { 
        using namespace WiFiHTMLTemplates;
        this->server.send(200, "text/css", generateSPACSS());
    });
    server.on("/spa.js", HTTP_GET, [this]() { 
        using namespace WiFiHTMLTemplates;
        this->server.send(200, "application/javascript", generateSPAJS());
    });
    
    // API endpoints - JSON only
    server.on("/api/board", HTTP_GET, [this]() { this->handleBoard(); });
    server.on("/api/board-edit", HTTP_POST, [this]() { this->handleBoardEdit(); });
    server.on("/api/config", HTTP_GET, [this]() { this->handleGetConfig(); });
    server.on("/api/config", HTTP_POST, [this]() { this->handleConfigSubmit(); });
    server.on("/api/gameselect", HTTP_POST, [this]() { this->handleGameSelection(); });
    server.on("/api/pause-moves", HTTP_GET, [this]() { this->handleGetPauseState(); });
    server.on("/api/pause-moves", HTTP_POST, [this]() { this->handlePauseMoves(); });
    server.on("/api/undo-move", HTTP_POST, [this]() { this->handleUndoMove(); });
    
    // Legacy endpoints for backward compatibility (redirect to API)
    server.on("/board", HTTP_GET, [this]() { this->handleBoard(); });
    server.on("/board-edit", HTTP_POST, [this]() { this->handleBoardEdit(); });
    server.on("/submit", HTTP_POST, [this]() { this->handleConfigSubmit(); });
    server.on("/gameselect", HTTP_POST, [this]() { this->handleGameSelection(); });
    server.on("/pause-moves", HTTP_GET, [this]() { this->handleGetPauseState(); });
    server.on("/pause-moves", HTTP_POST, [this]() { this->handlePauseMoves(); });
    server.on("/undo-move", HTTP_POST, [this]() { this->handleUndoMove(); });
    
    // Legacy HTML pages - serve SPA instead
    server.on("/", HTTP_GET, [this]() { 
        using namespace WiFiHTMLTemplates;
        this->server.send(200, "text/html", generateSPAHTML());
    });
    server.on("/game", HTTP_GET, [this]() { 
        using namespace WiFiHTMLTemplates;
        this->server.send(200, "text/html", generateSPAHTML());
    });
    server.on("/board-view", HTTP_GET, [this]() { 
        using namespace WiFiHTMLTemplates;
        this->server.send(200, "text/html", generateSPAHTML());
    });
    server.on("/board-edit", HTTP_GET, [this]() { 
        using namespace WiFiHTMLTemplates;
        this->server.send(200, "text/html", generateSPAHTML());
    });
    
    // Other endpoints
    server.on("/connect-wifi", HTTP_POST, [this]() { this->handleConnectWiFi(); });
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
        // Serve SPA for all unknown routes (client-side routing)
        using namespace WiFiHTMLTemplates;
        this->server.send(200, "text/html", generateSPAHTML());
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

void WiFiManagerESP32::handleGetConfig() {
    String response = "{";
    response += "\"ssid\":\"" + wifiSSID + "\",";
    response += "\"token\":\"" + lichessToken + "\",";
    response += "\"gameMode\":\"" + gameMode + "\",";
    response += "\"startupType\":\"" + startupType + "\",";
    response += "\"connectionStatus\":\"" + getConnectionStatus() + "\"";
    response += "}";
    server.send(200, "application/json", response);
}

void WiFiManagerESP32::handleConfigSubmit() {
    // Check if request has JSON body
    String contentType = server.header("Content-Type");
    bool isJSON = contentType.indexOf("application/json") >= 0;
    
    if (isJSON) {
        String body = server.arg("plain");
        if (body.length() == 0) {
            body = server.arg("body");
        }
        if (body.length() > 0) {
            // Parse JSON: {"ssid":"...","password":"...","token":"...","gameMode":"...","startupType":"..."}
            int ssidIdx = body.indexOf("\"ssid\":\"");
            int passwordIdx = body.indexOf("\"password\":\"");
            int tokenIdx = body.indexOf("\"token\":\"");
            int gameModeIdx = body.indexOf("\"gameMode\":\"");
            int startupTypeIdx = body.indexOf("\"startupType\":\"");
            
            if (ssidIdx >= 0) {
                ssidIdx += 8;
                int ssidEnd = body.indexOf("\"", ssidIdx);
                if (ssidEnd > ssidIdx) wifiSSID = body.substring(ssidIdx, ssidEnd);
            }
            if (passwordIdx >= 0) {
                passwordIdx += 11;
                int passwordEnd = body.indexOf("\"", passwordIdx);
                if (passwordEnd > passwordIdx) wifiPassword = body.substring(passwordIdx, passwordEnd);
            }
            if (tokenIdx >= 0) {
                tokenIdx += 9;
                int tokenEnd = body.indexOf("\"", tokenIdx);
                if (tokenEnd > tokenIdx) lichessToken = body.substring(tokenIdx, tokenEnd);
            }
            if (gameModeIdx >= 0) {
                gameModeIdx += 11;
                int gameModeEnd = body.indexOf("\"", gameModeIdx);
                if (gameModeEnd > gameModeIdx) gameMode = body.substring(gameModeIdx, gameModeEnd);
            }
            if (startupTypeIdx >= 0) {
                startupTypeIdx += 14;
                int startupTypeEnd = body.indexOf("\"", startupTypeIdx);
                if (startupTypeEnd > startupTypeIdx) startupType = body.substring(startupTypeIdx, startupTypeEnd);
            }
        }
    } else {
        // Form data
        if (server.hasArg("plain")) {
            parseFormData(server.arg("plain"));
        } else {
            if (server.hasArg("ssid")) wifiSSID = server.arg("ssid");
            if (server.hasArg("password")) wifiPassword = server.arg("password");
            if (server.hasArg("token")) lichessToken = server.arg("token");
            if (server.hasArg("gameMode")) gameMode = server.arg("gameMode");
            if (server.hasArg("startupType")) startupType = server.arg("startupType");
        }
    }
    
    // Save configuration (you may want to persist this to EEPROM/Flash)
    // For now, just store in memory
    
    String response = "{\"status\":\"success\",\"message\":\"Configuration saved\",\"ssid\":\"" + wifiSSID + "\",\"gameMode\":\"" + gameMode + "\",\"startupType\":\"" + startupType + "\"}";
    server.send(200, "application/json", response);
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
    return generateSPAHTML();
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
    updateBoardState(newBoardState, evaluation, pgn, "");
}

void WiFiManagerESP32::updateBoardState(char newBoardState[8][8], float evaluation, String pgn, String fen) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            boardState[row][col] = newBoardState[row][col];
        }
    }
    boardStateValid = true;
    boardEvaluation = evaluation;
    boardPGN = pgn;
    boardFEN = fen;
}

String WiFiManagerESP32::generateBoardJSON() {
    String json = "{";
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
    if (boardFEN.length() > 0) {
        json += ",\"fen\":\"";
        // Escape quotes in FEN
        String escapedFEN = boardFEN;
        escapedFEN.replace("\"", "\\\"");
        json += escapedFEN;
        json += "\"";
    } else {
        json += ",\"fen\":\"\"";
    }
    json += "}";
    
    return json;
}

void WiFiManagerESP32::handleBoard() {
    String boardJSON = generateBoardJSON();
    sendResponse(boardJSON, "application/json");
}

void WiFiManagerESP32::handleBoardView() {
    using namespace WiFiHTMLTemplates;
    sendResponse(generateSPAHTML(), "text/html");
}

void WiFiManagerESP32::handleBoardEdit() {
    // Check if request has JSON body
    // Try multiple ways to get Content-Type header
    String contentType = server.header("Content-Type");
    if (contentType.length() == 0) {
        contentType = server.header("content-type");
    }
    if (contentType.length() == 0) {
        contentType = server.header("CONTENT-TYPE");
    }
    
    Serial.print("DEBUG: handleBoardEdit - Content-Type: '");
    Serial.print(contentType);
    Serial.println("'");
    
    // Also check if the body looks like JSON (starts with '{')
    String body = "";
    bool looksLikeJSON = false;
    
    if (contentType.indexOf("application/json") >= 0) {
        looksLikeJSON = true;
    } else {
        // Try to peek at the body to see if it's JSON
        if (server.hasArg("plain")) {
            String peekBody = server.arg("plain");
            if (peekBody.length() > 0 && peekBody.charAt(0) == '{') {
                looksLikeJSON = true;
                Serial.println("DEBUG: Body looks like JSON (starts with '{')");
            }
        }
    }
    
    // Read the body - try "plain" argument first (ESP32 WebServer standard)
    if (body.length() == 0 && server.hasArg("plain")) {
        body = server.arg("plain");
        Serial.print("DEBUG: Body from 'plain' arg, length: ");
        Serial.println(body.length());
    }
    
    // Try alternative argument name
    if (body.length() == 0 && server.hasArg("body")) {
        body = server.arg("body");
        Serial.print("DEBUG: Body from 'body' arg, length: ");
        Serial.println(body.length());
    }
    
    // If still empty, try reading from the client stream
    if (body.length() == 0) {
        Serial.println("DEBUG: Reading body from client stream...");
        WiFiClient client = server.client();
        if (client.available()) {
            // Read Content-Length header if available
            int contentLength = 0;
            String contentLengthHeader = server.header("Content-Length");
            if (contentLengthHeader.length() == 0) {
                contentLengthHeader = server.header("content-length");
            }
            if (contentLengthHeader.length() > 0) {
                contentLength = contentLengthHeader.toInt();
                Serial.print("DEBUG: Content-Length: ");
                Serial.println(contentLength);
            }
            
            // Read the body
            unsigned long startTime = millis();
            while ((contentLength == 0 || body.length() < contentLength) && 
                   (millis() - startTime < 1000) && client.connected()) {
                if (client.available()) {
                    body += (char)client.read();
                } else {
                    delay(1);
                }
            }
            Serial.print("DEBUG: Body from stream, length: ");
            Serial.println(body.length());
        }
    }
    
    // Determine if this is JSON or form data
    if (body.length() > 0 && body.charAt(0) == '{') {
        // Body looks like JSON
        Serial.println("DEBUG: Body looks like JSON, parsing as JSON");
        Serial.print("DEBUG: Received JSON body (first 300 chars): ");
        Serial.println(body.substring(0, 300));
        parseBoardEditDataJSON(body);
    } else if (looksLikeJSON || contentType.indexOf("application/json") >= 0) {
        // Content-Type says JSON but body doesn't look like it
        Serial.println("ERROR: Content-Type says JSON but body doesn't look like JSON!");
        String response = "{\"success\":false,\"message\":\"Invalid JSON body\"}";
        server.send(400, "application/json", response);
        return;
    } else {
        // Parse form data (legacy support)
        Serial.println("DEBUG: Parsing as form data");
        parseBoardEditData();
    }
    
    // Return JSON response
    String response = "{\"success\":true,\"message\":\"Board updated successfully\"}";
    server.send(200, "application/json", response);
}

void WiFiManagerESP32::parseBoardEditDataJSON(String jsonData) {
    Serial.print("DEBUG: parseBoardEditDataJSON - JSON length: ");
    Serial.println(jsonData.length());
    Serial.print("DEBUG: First 200 chars: ");
    Serial.println(jsonData.substring(0, 200));
    
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
    
    // The '[' is part of "\"board\":[" - it's at position boardArrayStart + 8
    int arrayStart = boardArrayStart + 8; // Position of '[' in "\"board\":["
    if (arrayStart >= jsonData.length() || jsonData.charAt(arrayStart) != '[') {
        Serial.println("Error: Invalid JSON - board array bracket not found");
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
                
                char piece = ' '; // Default to empty
                
                // Check for string value
                if (jsonData.charAt(pos) == '"') {
                    pos++; // Skip opening quote
                    // Check if this is an empty string ""
                    if (pos < jsonData.length() && jsonData.charAt(pos) == '"') {
                        // Empty string - piece stays as ' '
                        pos++; // Skip closing quote
                    } else if (pos < jsonData.length()) {
                        // Non-empty string - read the character(s)
                        int charStart = pos;
                        // Find closing quote
                        while (pos < jsonData.length() && jsonData.charAt(pos) != '"') {
                            pos++;
                        }
                        // Extract the string content
                        if (pos > charStart) {
                            String pieceStr = jsonData.substring(charStart, pos);
                            if (pieceStr.length() > 0) {
                                piece = pieceStr.charAt(0);
                                // Debug for first few pieces
                                if (row == 0 && col < 4) {
                                    Serial.print("DEBUG: Parsing [");
                                    Serial.print(row);
                                    Serial.print("][");
                                    Serial.print(col);
                                    Serial.print("] - pieceStr='");
                                    Serial.print(pieceStr);
                                    Serial.print("', piece='");
                                    Serial.print(piece);
                                    Serial.println("'");
                                }
                            }
                        }
                        if (pos < jsonData.length()) pos++; // Skip closing quote
                    }
                } else if (pos < jsonData.length() && jsonData.charAt(pos) == 'n') {
                    // Check for "null"
                    if (jsonData.substring(pos, pos + 4) == "null") {
                        piece = ' ';
                        pos += 4;
                    }
                } else {
                    // Unexpected character - debug
                    if (row == 0 && col < 4) {
                        Serial.print("DEBUG: Unexpected char at pos ");
                        Serial.print(pos);
                        Serial.print(": '");
                        Serial.print(jsonData.charAt(pos));
                        Serial.println("'");
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
    
    // Debug: Print parsed board
    Serial.println("DEBUG: Parsed board (all rows):");
    for (int r = 0; r < 8; r++) {
        Serial.print("  Row ");
        Serial.print(r);
        Serial.print(": ");
        for (int c = 0; c < 8; c++) {
            char p = pendingBoardEdit[r][c];
            if (p == ' ') {
                Serial.print(".");
            } else {
                Serial.print(p);
            }
            Serial.print(" ");
        }
        Serial.println();
    }
    
    // Count non-empty squares
    int pieceCount = 0;
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (pendingBoardEdit[r][c] != ' ') {
                pieceCount++;
            }
        }
    }
    Serial.print("DEBUG: Total pieces parsed: ");
    Serial.println(pieceCount);
    
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
    // Check if request has JSON body
    String contentType = server.header("Content-Type");
    bool isJSON = contentType.indexOf("application/json") >= 0;
    
    if (isJSON) {
        String body = server.arg("plain");
        if (body.length() == 0) {
            body = server.arg("body");
        }
        if (body.length() > 0) {
            int pausedIdx = body.indexOf("\"paused\":");
            if (pausedIdx >= 0) {
                pausedIdx += 9;
                int pausedEnd = body.indexOf(",", pausedIdx);
                if (pausedEnd < 0) pausedEnd = body.indexOf("}", pausedIdx);
                if (pausedEnd < 0) pausedEnd = body.length();
                String pausedStr = body.substring(pausedIdx, pausedEnd);
                moveDetectionPaused = (pausedStr.indexOf("true") >= 0 || pausedStr == "1");
            }
        }
    } else {
        // Form data
        if (server.hasArg("paused")) {
            String pausedArg = server.arg("paused");
            moveDetectionPaused = (pausedArg == "true" || pausedArg == "1");
        } else {
            // If no argument, toggle
            moveDetectionPaused = !moveDetectionPaused;
        }
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

