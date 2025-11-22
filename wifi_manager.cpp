#include "wifi_manager.h"
#include "wifi_html_templates.h"

// Only compile this file for WiFiNINA boards
#ifdef WIFI_MANAGER_WIFININA_ENABLED

#include <Arduino.h>
#include "arduino_secrets.h"

WiFiManager::WiFiManager() : server(AP_PORT) {
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

void WiFiManager::begin() {
    Serial.println("!!! WIFI MANAGER BEGIN FUNCTION CALLED !!!");
    Serial.println("=== Starting OpenChess WiFi Manager ===");
    Serial.println("Debug: WiFi Manager begin() called");
    
    // Check if WiFi is available
    Serial.println("Debug: Checking WiFi module...");
    
    // Try to get WiFi status - this often fails on incompatible boards
    Serial.println("Debug: Attempting to get WiFi status...");
    int initialStatus = WiFi.status();
    Serial.print("Debug: Initial WiFi status: ");
    Serial.println(initialStatus);
    
    // Initialize WiFi module
    Serial.println("Debug: Checking for WiFi module presence...");
    if (initialStatus == WL_NO_MODULE) {
        Serial.println("ERROR: WiFi module not detected!");
        Serial.println("Board type: Arduino Nano RP2040 - WiFi not supported with WiFiNINA");
        Serial.println("This is expected behavior for RP2040 boards.");
        Serial.println("Use physical board selectors for game mode selection.");
        return;
    }
    
    Serial.println("Debug: WiFi module appears to be present");
    
    Serial.println("Debug: WiFi module detected");
    
    // Check firmware version
    String fv = WiFi.firmwareVersion();
    Serial.print("Debug: WiFi firmware version: ");
    Serial.println(fv);
    
    // Try to connect to existing WiFi first (if credentials are available)
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
            apMode = false;
        } else {
            Serial.println("Failed to connect to WiFi. Starting Access Point mode...");
        }
    }
    
    // If not connected, start Access Point
    if (!connected) {
        startAccessPoint();
    }
    
    // Print connection information
    IPAddress ip = getIPAddress();
    Serial.println("=== WiFi Connection Information ===");
    if (apMode) {
        Serial.print("Mode: Access Point");
        Serial.print("SSID: ");
        Serial.println(AP_SSID);
        Serial.print("Password: ");
        Serial.println(AP_PASSWORD);
    } else {
        Serial.println("Mode: Connected to WiFi Network");
        Serial.print("Connected to: ");
        Serial.println(WiFi.SSID());
    }
    Serial.print("IP Address: ");
    Serial.println(ip);
    Serial.print("Web Interface: http://");
    Serial.println(ip);
    Serial.println("=====================================");
    
    // Start the web server
    Serial.println("Debug: Starting web server...");
    server.begin();
    Serial.println("Debug: Web server started on port 80");
    Serial.println("WiFi Manager initialization complete!");
}

void WiFiManager::handleClient() {
    WiFiClient client = server.available();
    
    if (client) {
        clientConnected = true;
        Serial.println("New client connected");
        
        String request = "";
        bool currentLineIsBlank = true;
        bool readingBody = false;
        String body = "";
        
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                
                if (!readingBody) {
                    request += c;
                    
                    if (c == '\n' && currentLineIsBlank) {
                        // Headers ended, now reading body if POST
                        if (request.indexOf("POST") >= 0) {
                            readingBody = true;
                        } else {
                            break; // GET request, no body
                        }
                    }
                    
                    if (c == '\n') {
                        currentLineIsBlank = true;
                    } else if (c != '\r') {
                        currentLineIsBlank = false;
                    }
                } else {
                    // Reading POST body
                    body += c;
                    if (body.length() > 1000) break; // Prevent overflow
                }
            }
        }
        
        // Handle the request
        if (request.indexOf("GET / ") >= 0) {
            // Main configuration page
            String webpage = generateWebPage();
            sendResponse(client, webpage);
        }
        else if (request.indexOf("GET /game") >= 0) {
            // Game selection page
            String gameSelectionPage = generateGameSelectionPage();
            sendResponse(client, gameSelectionPage);
        }
        else if (request.indexOf("GET /board") >= 0) {
            // Board state JSON API
            String boardJSON = generateBoardJSON();
            sendResponse(client, boardJSON, "application/json");
        }
        else if (request.indexOf("GET /board-view") >= 0) {
            // Board visual display page
            String boardViewPage = generateBoardViewPage();
            sendResponse(client, boardViewPage);
        }
        else if (request.indexOf("GET /board-edit") >= 0) {
            // Board edit page
            String boardEditPage = generateBoardEditPage();
            sendResponse(client, boardEditPage);
        }
        else if (request.indexOf("POST /board-edit") >= 0) {
            // Board edit submission
            handleBoardEdit(client, request, body);
        }
        else if (request.indexOf("POST /connect-wifi") >= 0) {
            // WiFi connection request
            handleConnectWiFi(client, request, body);
        }
        else if (request.indexOf("POST /submit") >= 0) {
            // Configuration form submission
            parseFormData(body);
            String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
            response += "<h2>Configuration Saved!</h2>";
            response += "<p>WiFi SSID: " + wifiSSID + "</p>";
            response += "<p>Game Mode: " + gameMode + "</p>";
            response += "<p>Startup Type: " + startupType + "</p>";
            response += "<p><a href='/game' style='color:#ec8703;'>Go to Game Selection</a></p>";
            response += "</body></html>";
            sendResponse(client, response);
        }
        else if (request.indexOf("POST /gameselect") >= 0) {
            // Game selection submission
            handleGameSelection(client, body);
        }
        else {
            // 404 Not Found
            String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
            response += "<h2>404 - Page Not Found</h2>";
            response += "<p><a href='/' style='color:#ec8703;'>Back to Home</a></p>";
            response += "</body></html>";
            sendResponse(client, response, "text/html");
        }
        
        delay(10);
        client.stop();
        Serial.println("Client disconnected");
        clientConnected = false;
    }
}

String WiFiManager::generateWebPage() {
    using namespace WiFiHTMLTemplates;
    return generateConfigPage(wifiSSID, wifiPassword, lichessToken, gameMode, startupType, 
                              getConnectionStatus(), apMode);
}

String WiFiManager::generateGameSelectionPage() {
    using namespace WiFiHTMLTemplates;
    return generateGameSelectionPage();
}

void WiFiManager::handleGameSelection(WiFiClient& client, String body) {
    // Parse game mode selection
    int modeStart = body.indexOf("gamemode=");
    if (modeStart >= 0) {
        int modeEnd = body.indexOf("&", modeStart);
        if (modeEnd < 0) modeEnd = body.length();
        
        String selectedMode = body.substring(modeStart + 9, modeEnd);
        int mode = selectedMode.toInt();
        
        Serial.print("Game mode selected via web: ");
        Serial.println(mode);
        
        // Store the selected game mode (you'll access this from main code)
        gameMode = String(mode);
        
        String response = R"({"status":"success","message":"Game mode selected","mode":)" + String(mode) + "}";
        sendResponse(client, response, "application/json");
    }
}

void WiFiManager::sendResponse(WiFiClient& client, String content, String contentType) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: " + contentType);
    client.println("Connection: close");
    client.println();
    client.println(content);
}

void WiFiManager::parseFormData(String data) {
    // Parse URL-encoded form data
    int ssidStart = data.indexOf("ssid=");
    if (ssidStart >= 0) {
        int ssidEnd = data.indexOf("&", ssidStart);
        if (ssidEnd < 0) ssidEnd = data.length();
        wifiSSID = data.substring(ssidStart + 5, ssidEnd);
        wifiSSID.replace("+", " ");
    }
    
    int passStart = data.indexOf("password=");
    if (passStart >= 0) {
        int passEnd = data.indexOf("&", passStart);
        if (passEnd < 0) passEnd = data.length();
        wifiPassword = data.substring(passStart + 9, passEnd);
    }
    
    int tokenStart = data.indexOf("token=");
    if (tokenStart >= 0) {
        int tokenEnd = data.indexOf("&", tokenStart);
        if (tokenEnd < 0) tokenEnd = data.length();
        lichessToken = data.substring(tokenStart + 6, tokenEnd);
    }
    
    int gameModeStart = data.indexOf("gameMode=");
    if (gameModeStart >= 0) {
        int gameModeEnd = data.indexOf("&", gameModeStart);
        if (gameModeEnd < 0) gameModeEnd = data.length();
        gameMode = data.substring(gameModeStart + 9, gameModeEnd);
        gameMode.replace("+", " ");
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

bool WiFiManager::isClientConnected() {
    return clientConnected;
}

int WiFiManager::getSelectedGameMode() {
    return gameMode.toInt();
}

void WiFiManager::resetGameSelection() {
    gameMode = "0";
}

void WiFiManager::updateBoardState(char newBoardState[8][8]) {
    updateBoardState(newBoardState, 0.0);
}

void WiFiManager::updateBoardState(char newBoardState[8][8], float evaluation) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            boardState[row][col] = newBoardState[row][col];
        }
    }
    boardStateValid = true;
    boardEvaluation = evaluation;
}

String WiFiManager::generateBoardJSON() {
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

String WiFiManager::generateBoardViewPage() {
    using namespace WiFiHTMLTemplates;
    return generateBoardViewPageTemplate();
}

String WiFiManager::generateBoardEditPage() {
    using namespace WiFiHTMLTemplates;
    return generateBoardEditPageTemplate();
}

void WiFiManager::handleBoardEdit(WiFiClient& client, String request, String body) {
    parseBoardEditData(body);
    
    String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
    response += "<h2>Board Updated!</h2>";
    response += "<p>Your board changes have been applied.</p>";
    response += "<p><a href='/board-view' style='color:#ec8703;'>View Board</a></p>";
    response += "<p><a href='/board-edit' style='color:#ec8703;'>Edit Again</a></p>";
    response += "<p><a href='/' style='color:#ec8703;'>Back to Home</a></p>";
    response += "</body></html>";
    sendResponse(client, response);
}

void WiFiManager::parseBoardEditData(String data) {
    // Parse the form data which contains r0c0, r0c1, etc.
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            String paramName = "r" + String(row) + "c" + String(col) + "=";
            int paramStart = data.indexOf(paramName);
            
            if (paramStart >= 0) {
                int valueStart = paramStart + paramName.length();
                int valueEnd = data.indexOf("&", valueStart);
                if (valueEnd < 0) valueEnd = data.length();
                
                String value = data.substring(valueStart, valueEnd);
                value.replace("+", " ");
                value.replace("%20", " ");
                
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

bool WiFiManager::getPendingBoardEdit(char editBoard[8][8]) {
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

void WiFiManager::clearPendingEdit() {
    hasPendingEdit = false;
}

bool WiFiManager::connectToWiFi(String ssid, String password) {
    Serial.println("=== Connecting to WiFi Network ===");
    Serial.print("SSID: ");
    Serial.println(ssid);
    
    int attempts = 0;
    WiFi.begin(ssid.c_str(), password.c_str());
    
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
        apMode = false;
        return true;
    } else {
        Serial.println("Failed to connect to WiFi");
        return false;
    }
}

bool WiFiManager::startAccessPoint() {
    Serial.println("=== Starting Access Point ===");
    Serial.print("SSID: ");
    Serial.println(AP_SSID);
    Serial.print("Password: ");
    Serial.println(AP_PASSWORD);
    
    int status = WiFi.beginAP(AP_SSID, AP_PASSWORD);
    
    if (status != WL_AP_LISTENING) {
        Serial.println("First attempt failed, trying with channel 6...");
        status = WiFi.beginAP(AP_SSID, AP_PASSWORD, 6);
    }
    
    if (status != WL_AP_LISTENING) {
        Serial.println("ERROR: Failed to create Access Point!");
        return false;
    }
    
    // Wait for AP to start
    for (int i = 0; i < 10; i++) {
        delay(1000);
        if (WiFi.status() == WL_AP_LISTENING) {
            Serial.println("AP is now listening!");
            break;
        }
    }
    
    apMode = true;
    return true;
}

IPAddress WiFiManager::getIPAddress() {
    if (apMode) {
        return WiFi.localIP(); // In AP mode, localIP() returns AP IP
    } else {
        return WiFi.localIP(); // In station mode, localIP() returns assigned IP
    }
}

bool WiFiManager::isConnectedToWiFi() {
    return !apMode && WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getConnectionStatus() {
    String status = "";
    if (apMode) {
        status = "Access Point Mode - SSID: " + String(AP_SSID);
    } else if (WiFi.status() == WL_CONNECTED) {
        status = "Connected to: " + WiFi.SSID() + " (IP: " + WiFi.localIP().toString() + ")";
    } else {
        status = "Not connected";
    }
    return status;
}

void WiFiManager::handleConnectWiFi(WiFiClient& client, String request, String body) {
    // Parse WiFi credentials from POST body
    parseFormData(body);
    
    if (wifiSSID.length() > 0) {
        Serial.println("Attempting to connect to WiFi from web interface...");
        bool connected = connectToWiFi(wifiSSID, wifiPassword);
        
        String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
        if (connected) {
            response += "<h2>WiFi Connected!</h2>";
            response += "<p>Successfully connected to: " + wifiSSID + "</p>";
            response += "<p>IP Address: " + WiFi.localIP().toString() + "</p>";
            response += "<p>You can now access the board at: http://" + WiFi.localIP().toString() + "</p>";
        } else {
            response += "<h2>WiFi Connection Failed</h2>";
            response += "<p>Could not connect to: " + wifiSSID + "</p>";
            response += "<p>Please check your credentials and try again.</p>";
            response += "<p>Access Point mode will remain active.</p>";
        }
        response += "<p><a href='/' style='color:#ec8703;'>Back to Configuration</a></p>";
        response += "</body></html>";
        sendResponse(client, response);
    } else {
        String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
        response += "<h2>Error</h2>";
        response += "<p>No WiFi SSID provided.</p>";
        response += "<p><a href='/' style='color:#ec8703;'>Back to Configuration</a></p>";
        response += "</body></html>";
        sendResponse(client, response);
    }
}

#endif // WIFI_MANAGER_WIFININA_ENABLED