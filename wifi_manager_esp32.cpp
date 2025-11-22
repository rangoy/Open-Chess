#include "wifi_manager_esp32.h"
#include "wifi_html_templates.h"
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
    // Parse game mode selection
    int mode = 0;
    
    if (server.hasArg("gamemode")) {
        mode = server.arg("gamemode").toInt();
    } else if (server.hasArg("plain")) {
        // Try to parse from plain body
        String body = server.arg("plain");
        int modeStart = body.indexOf("gamemode=");
        if (modeStart >= 0) {
            int modeEnd = body.indexOf("&", modeStart);
            if (modeEnd < 0) modeEnd = body.length();
            String selectedMode = body.substring(modeStart + 9, modeEnd);
            mode = selectedMode.toInt();
        }
    }
    
    Serial.print("Game mode selected via web: ");
    Serial.println(mode);
    
    // Store the selected game mode
    gameMode = String(mode);
    
    String response = "{\"status\":\"success\",\"message\":\"Game mode selected\",\"mode\":" + String(mode) + "}";
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
    return generateGameSelectionPage();
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
}

void WiFiManagerESP32::updateBoardState(char newBoardState[8][8]) {
    updateBoardState(newBoardState, 0.0);
}

void WiFiManagerESP32::updateBoardState(char newBoardState[8][8], float evaluation) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            boardState[row][col] = newBoardState[row][col];
        }
    }
    boardStateValid = true;
    boardEvaluation = evaluation;
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
    parseBoardEditData();
    
    String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
    response += "<h2>Board Updated!</h2>";
    response += "<p>Your board changes have been applied.</p>";
    response += "<p><a href='/board-view' style='color:#ec8703;'>View Board</a></p>";
    response += "<p><a href='/board-edit' style='color:#ec8703;'>Edit Again</a></p>";
    response += "<p><a href='/' style='color:#ec8703;'>Back to Home</a></p>";
    response += "</body></html>";
    sendResponse(response);
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

