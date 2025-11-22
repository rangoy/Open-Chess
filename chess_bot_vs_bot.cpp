#include "chess_bot_vs_bot.h"
#include <Arduino.h>

ChessBotVsBot::ChessBotVsBot(BoardDriver* boardDriver, ChessEngine* chessEngine, 
                               BotDifficulty whiteDiff, BotDifficulty blackDiff,
                               int moveDelayMs) {
    _boardDriver = boardDriver;
    _chessEngine = chessEngine;
    whiteDifficulty = whiteDiff;
    blackDifficulty = blackDiff;
    moveDelay = moveDelayMs;
    
    // Set difficulty settings
    switch(whiteDifficulty) {
        case BOT_EASY: whiteSettings = StockfishSettings::easy(); break;
        case BOT_MEDIUM: whiteSettings = StockfishSettings::medium(); break;
        case BOT_HARD: whiteSettings = StockfishSettings::hard(); break;
        case BOT_EXPERT: whiteSettings = StockfishSettings::expert(); break;
    }
    
    switch(blackDifficulty) {
        case BOT_EASY: blackSettings = StockfishSettings::easy(); break;
        case BOT_MEDIUM: blackSettings = StockfishSettings::medium(); break;
        case BOT_HARD: blackSettings = StockfishSettings::hard(); break;
        case BOT_EXPERT: blackSettings = StockfishSettings::expert(); break;
    }
    
    // White always moves first in chess
    isWhiteTurn = true;
    gameStarted = false;
    botThinking = false;
    wifiConnected = false;
    currentEvaluation = 0.0;
    lastMoveTime = 0;
    moveCompletionState = MOVE_COMPLETE;
    pendingFromRow = pendingFromCol = pendingToRow = pendingToCol = -1;
    lastBlinkTime = 0;
    blinkState = false;
}

void ChessBotVsBot::begin() {
    Serial.println("=== Starting Chess Bot vs Bot Mode ===");
    Serial.print("White AI Difficulty: ");
    switch(whiteDifficulty) {
        case BOT_EASY: Serial.println("Easy (Depth 6)"); break;
        case BOT_MEDIUM: Serial.println("Medium (Depth 10)"); break;
        case BOT_HARD: Serial.println("Hard (Depth 14)"); break;
        case BOT_EXPERT: Serial.println("Expert (Depth 16)"); break;
    }
    Serial.print("Black AI Difficulty: ");
    switch(blackDifficulty) {
        case BOT_EASY: Serial.println("Easy (Depth 6)"); break;
        case BOT_MEDIUM: Serial.println("Medium (Depth 10)"); break;
        case BOT_HARD: Serial.println("Hard (Depth 14)"); break;
        case BOT_EXPERT: Serial.println("Expert (Depth 16)"); break;
    }
    Serial.print("Move delay: ");
    Serial.print(moveDelay);
    Serial.println(" ms");
    
    _boardDriver->clearAllLEDs();
    _boardDriver->showLEDs();
    
    // Connect to WiFi
    Serial.println("Connecting to WiFi...");
    showConnectionStatus();
    
    if (connectToWiFi()) {
        Serial.println("WiFi connected! Bot vs Bot mode ready.");
        wifiConnected = true;
        
        // Show success animation
        for (int i = 0; i < 3; i++) {
            _boardDriver->clearAllLEDs();
            _boardDriver->showLEDs();
            delay(200);
            
            // Light up entire board green briefly
            for (int row = 0; row < 8; row++) {
                for (int col = 0; col < 8; col++) {
                    _boardDriver->setSquareLED(row, col, 0, 255, 0); // Green
                }
            }
            _boardDriver->showLEDs();
            delay(200);
        }
        
        initializeBoard();
        
        // Wait for board setup (optional - can skip if desired)
        Serial.println("Please set up the chess board in starting position...");
        Serial.println("(Or wait 5 seconds to skip setup check)");
        
        unsigned long setupStart = millis();
        bool setupComplete = false;
        
        while (!setupComplete && (millis() - setupStart < 5000)) {
            _boardDriver->readSensors();
            if (_boardDriver->checkInitialBoard(INITIAL_BOARD)) {
                setupComplete = true;
            } else {
                _boardDriver->updateSetupDisplay(INITIAL_BOARD);
                _boardDriver->showLEDs();
                delay(100);
            }
        }
        
        if (setupComplete) {
            Serial.println("Board setup complete!");
        } else {
            Serial.println("Skipping board setup check - game will start automatically.");
        }
        
        _boardDriver->fireworkAnimation();
        gameStarted = true;
        isWhiteTurn = true; // White moves first
        
        // Show initial board state
        printCurrentBoard();
        
        Serial.println("Game started! White AI will make the first move...");
        lastMoveTime = millis();
    } else {
        Serial.println("Failed to connect to WiFi. Bot vs Bot mode unavailable.");
        wifiConnected = false;
        
        // Show error animation (red flashing)
        for (int i = 0; i < 5; i++) {
            _boardDriver->clearAllLEDs();
            _boardDriver->showLEDs();
            delay(300);
            
            // Light up entire board red briefly
            for (int row = 0; row < 8; row++) {
                for (int col = 0; col < 8; col++) {
                    _boardDriver->setSquareLED(row, col, 255, 0, 0); // Red
                }
            }
            _boardDriver->showLEDs();
            delay(300);
        }
        
        _boardDriver->clearAllLEDs();
        _boardDriver->showLEDs();
    }
}

void ChessBotVsBot::update() {
    if (!wifiConnected || !gameStarted) {
        return;
    }
    
    // Update sensors
    _boardDriver->readSensors();
    
    // Handle move completion state machine (non-blocking)
    if (moveCompletionState != MOVE_COMPLETE) {
        updateMoveCompletion();
        _boardDriver->updateSensorPrev();
        return;  // Don't process new moves while waiting for physical move
    }
    
    // Check if enough time has passed since last move
    if (!botThinking && (millis() - lastMoveTime >= moveDelay)) {
        botThinking = true;
        makeBotMove();
    }
    
    if (botThinking) {
        showBotThinking();
    }
    
    _boardDriver->updateSensorPrev();
}

void ChessBotVsBot::makeBotMove() {
    Serial.println("=== AI MOVE CALCULATION ===");
    Serial.print("Current player: ");
    Serial.println(isWhiteTurn ? "White AI" : "Black AI");
    
    // Show thinking animation
    showBotThinking();
    
    String fen = boardToFEN();
    Serial.print("Sending FEN to Stockfish: ");
    Serial.println(fen);
    
    // Use appropriate difficulty for current player
    BotDifficulty currentDifficulty = isWhiteTurn ? whiteDifficulty : blackDifficulty;
    String response = makeStockfishRequest(fen, currentDifficulty);
    
    if (response.length() > 0) {
        String bestMove;
        float evaluation = 0.0;
        if (parseStockfishResponse(response, bestMove, evaluation)) {
            // Store and print evaluation
            currentEvaluation = evaluation;
            Serial.print("=== STOCKFISH EVALUATION ===");
            Serial.println();
            if (evaluation > 0) {
                Serial.print("White advantage: +");
                Serial.print(evaluation / 100.0, 2);
                Serial.println(" pawns");
            } else if (evaluation < 0) {
                Serial.print("Black advantage: ");
                Serial.print(evaluation / 100.0, 2);
                Serial.println(" pawns");
            } else {
                Serial.println("Position is equal (0.00 pawns)");
            }
            Serial.print("Evaluation in centipawns: ");
            Serial.println(evaluation);
            Serial.println("============================");
            
            int fromRow, fromCol, toRow, toCol;
            if (parseMove(bestMove, fromRow, fromCol, toRow, toCol)) {
                Serial.print("AI calculated move: ");
                Serial.println(bestMove);
                
                // Verify the move is from the correct color piece
                char piece = board[fromRow][fromCol];
                bool isCorrectPiece = (isWhiteTurn && piece >= 'A' && piece <= 'Z') || 
                                      (!isWhiteTurn && piece >= 'a' && piece <= 'z');
                
                if (!isCorrectPiece) {
                    Serial.print("ERROR: AI tried to move a ");
                    Serial.print((piece >= 'A' && piece <= 'Z') ? "WHITE" : "BLACK");
                    Serial.print(" piece, but it's ");
                    Serial.print(isWhiteTurn ? "WHITE" : "BLACK");
                    Serial.println("'s turn");
                    botThinking = false;
                    lastMoveTime = millis();
                    return;
                }
                
                if (piece == ' ') {
                    Serial.println("ERROR: AI tried to move from an empty square!");
                    botThinking = false;
                    lastMoveTime = millis();
                    return;
                }
                
                executeBotMove(fromRow, fromCol, toRow, toCol);
                
                // Don't switch turns yet - wait for physical move completion
                // The turn will switch in updateMoveCompletion() when move is complete
            } else {
                Serial.print("Failed to parse AI move: ");
                Serial.println(bestMove);
                botThinking = false;
                lastMoveTime = millis();
            }
        } else {
            Serial.println("Failed to parse Stockfish response");
            botThinking = false;
            lastMoveTime = millis();
        }
    } else {
        Serial.println("No response from Stockfish API after all retries");
        botThinking = false;
        lastMoveTime = millis();
    }
}

void ChessBotVsBot::executeBotMove(int fromRow, int fromCol, int toRow, int toCol) {
    char piece = board[fromRow][fromCol];
    
    // Store move information for non-blocking completion
    pendingFromRow = fromRow;
    pendingFromCol = fromCol;
    pendingToRow = toRow;
    pendingToCol = toCol;
    moveCompletionState = WAITING_FOR_PICKUP;
    lastBlinkTime = millis();
    blinkState = false;
    
    Serial.print("AI wants to move piece from ");
    Serial.print((char)('a' + fromCol));
    Serial.print(8 - fromRow);
    Serial.print(" to ");
    Serial.print((char)('a' + toCol));
    Serial.println(8 - toRow);
    Serial.println("Please make this move on the physical board...");
    
    // Show the move that needs to be made
    showBotMoveIndicator(fromRow, fromCol, toRow, toCol);
}

void ChessBotVsBot::showBotMoveIndicator(int fromRow, int fromCol, int toRow, int toCol) {
    // Clear all LEDs first
    _boardDriver->clearAllLEDs();
    
    // Show source square (where to pick up from) - use color based on current player
    if (isWhiteTurn) {
        _boardDriver->setSquareLED(fromRow, fromCol, 255, 255, 255); // White
    } else {
        _boardDriver->setSquareLED(fromRow, fromCol, 0, 0, 255); // Blue for black
    }
    
    // Show destination square (where to place)
    _boardDriver->setSquareLED(toRow, toCol, 0, 0, 0, 255); // Bright white using W channel
    
    _boardDriver->showLEDs();
}

void ChessBotVsBot::updateMoveCompletion() {
    if (moveCompletionState == MOVE_COMPLETE) {
        return;
    }
    
    int fromRow = pendingFromRow;
    int fromCol = pendingFromCol;
    int toRow = pendingToRow;
    int toCol = pendingToCol;
    
    if (moveCompletionState == WAITING_FOR_PICKUP) {
        // Blink the source square
        if (millis() - lastBlinkTime > 500) {
            _boardDriver->clearAllLEDs();
            if (blinkState) {
                if (isWhiteTurn) {
                    _boardDriver->setSquareLED(fromRow, fromCol, 255, 255, 255); // White
                } else {
                    _boardDriver->setSquareLED(fromRow, fromCol, 0, 0, 255); // Blue
                }
            }
            _boardDriver->setSquareLED(toRow, toCol, 0, 0, 0, 255); // Always show destination
            _boardDriver->showLEDs();
            
            blinkState = !blinkState;
            lastBlinkTime = millis();
        }
        
        // Check if piece was picked up from source
        if (!_boardDriver->getSensorState(fromRow, fromCol)) {
            moveCompletionState = WAITING_FOR_PLACEMENT;
            Serial.println("AI piece picked up, now place it on the destination...");
            
            // Stop blinking source, just show destination
            _boardDriver->clearAllLEDs();
            _boardDriver->setSquareLED(toRow, toCol, 0, 0, 0, 255);
            _boardDriver->showLEDs();
        }
    } else if (moveCompletionState == WAITING_FOR_PLACEMENT) {
        // Check if piece was placed on destination
        if (_boardDriver->getSensorState(toRow, toCol)) {
            // Move completed!
            char piece = board[fromRow][fromCol];
            char capturedPiece = board[toRow][toCol];
            
            // Update board state
            board[toRow][toCol] = piece;
            board[fromRow][fromCol] = ' ';
            
            if (capturedPiece != ' ') {
                Serial.print("Piece captured: ");
                Serial.println(capturedPiece);
                _boardDriver->captureAnimation();
            }
            
            // Check for pawn promotion
            if (_chessEngine->isPawnPromotion(piece, toRow)) {
                char promotedPiece = _chessEngine->getPromotedPiece(piece);
                board[toRow][toCol] = promotedPiece;
                Serial.print("Pawn promoted to ");
                Serial.println(promotedPiece);
                _boardDriver->promotionAnimation(toCol);
            }
            
            // Flash confirmation
            confirmMoveCompletion();
            
            Serial.println("AI move completed on physical board!");
            
            // Reset state and switch turns
            moveCompletionState = MOVE_COMPLETE;
            pendingFromRow = pendingFromCol = pendingToRow = pendingToCol = -1;
            botThinking = false;
            isWhiteTurn = !isWhiteTurn;
            lastMoveTime = millis();
            
            Serial.print("Move completed. Now it's ");
            Serial.print(isWhiteTurn ? "White" : "Black");
            Serial.println(" AI's turn!");
        }
    }
}

void ChessBotVsBot::confirmMoveCompletion() {
    // Flash entire board green twice
    for (int flash = 0; flash < 2; flash++) {
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                _boardDriver->setSquareLED(r, c, 0, 255, 0); // Green flash
            }
        }
        _boardDriver->showLEDs();
        delay(150);
        
        _boardDriver->clearAllLEDs();
        _boardDriver->showLEDs();
        delay(150);
    }
}

void ChessBotVsBot::showBotThinking() {
    static unsigned long lastUpdate = 0;
    static int thinkingStep = 0;
    
    if (millis() - lastUpdate > 500) {
        // Animated thinking indicator - pulse the corners with player color
        _boardDriver->clearAllLEDs();
        
        uint8_t brightness = (sin(thinkingStep * 0.3) + 1) * 127;
        
        if (isWhiteTurn) {
            // White AI thinking - pulse white
            _boardDriver->setSquareLED(0, 0, brightness, brightness, brightness);
            _boardDriver->setSquareLED(0, 7, brightness, brightness, brightness);
            _boardDriver->setSquareLED(7, 0, brightness, brightness, brightness);
            _boardDriver->setSquareLED(7, 7, brightness, brightness, brightness);
        } else {
            // Black AI thinking - pulse blue
            _boardDriver->setSquareLED(0, 0, 0, 0, brightness);
            _boardDriver->setSquareLED(0, 7, 0, 0, brightness);
            _boardDriver->setSquareLED(7, 0, 0, 0, brightness);
            _boardDriver->setSquareLED(7, 7, 0, 0, brightness);
        }
        
        _boardDriver->showLEDs();
        
        thinkingStep++;
        lastUpdate = millis();
    }
}

void ChessBotVsBot::showConnectionStatus() {
    // Show WiFi connection attempt with animated LEDs
    for (int i = 0; i < 8; i++) {
        _boardDriver->setSquareLED(3, i, 0, 0, 255); // Blue row
        _boardDriver->showLEDs();
        delay(200);
    }
}

void ChessBotVsBot::initializeBoard() {
    // Copy initial board state
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            board[row][col] = INITIAL_BOARD[row][col];
        }
    }
}

String ChessBotVsBot::boardToFEN() {
    String fen = "";
    
    // Board position - FEN expects rank 8 (black pieces) first, rank 1 (white pieces) last
    for (int row = 7; row >= 0; row--) {
        int emptyCount = 0;
        for (int col = 0; col < 8; col++) {
            if (board[row][col] == ' ') {
                emptyCount++;
            } else {
                if (emptyCount > 0) {
                    fen += String(emptyCount);
                    emptyCount = 0;
                }
                fen += board[row][col];
            }
        }
        if (emptyCount > 0) {
            fen += String(emptyCount);
        }
        if (row > 0) fen += "/";
    }
    
    // Active color
    fen += isWhiteTurn ? " w" : " b";
    
    // Castling availability (simplified - assume all available initially)
    fen += " KQkq";
    
    // En passant target square (simplified - assume none)
    fen += " -";
    
    // Halfmove clock (simplified)
    fen += " 0";
    
    // Fullmove number (simplified)
    fen += " 1";
    
    Serial.print("Generated FEN: ");
    Serial.println(fen);
    Serial.print("Active color: ");
    Serial.println(isWhiteTurn ? "White" : "Black");
    
    return fen;
}

void ChessBotVsBot::fenToBoard(String fen) {
    // Parse FEN string and update board state
    int spacePos = fen.indexOf(' ');
    if (spacePos > 0) {
        fen = fen.substring(0, spacePos);
    }
    
    // Clear board
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            board[row][col] = ' ';
        }
    }
    
    // Parse FEN ranks
    int rank = 7;
    int col = 0;
    
    for (int i = 0; i < fen.length() && rank >= 0; i++) {
        char c = fen.charAt(i);
        
        if (c == '/') {
            rank--;
            col = 0;
        } else if (c >= '1' && c <= '8') {
            int emptyCount = c - '0';
            col += emptyCount;
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            if (rank >= 0 && rank < 8 && col >= 0 && col < 8) {
                board[rank][col] = c;
                col++;
            }
        }
    }
    
    Serial.println("Board updated from FEN");
    printCurrentBoard();
}

bool ChessBotVsBot::parseMove(String move, int &fromRow, int &fromCol, int &toRow, int &toCol) {
    if (move.length() < 4) {
        return false;
    }
    
    char fromFile = move.charAt(0);
    char fromRank = move.charAt(1);
    char toFile = move.charAt(2);
    char toRank = move.charAt(3);
    
    // Validate file and rank characters
    if (fromFile < 'a' || fromFile > 'h' || toFile < 'a' || toFile > 'h') {
        return false;
    }
    
    if (fromRank < '1' || fromRank > '8' || toRank < '1' || toRank > '8') {
        return false;
    }
    
    fromCol = fromFile - 'a';
    fromRow = (fromRank - '0') - 1;
    toCol = toFile - 'a';
    toRow = (toRank - '0') - 1;
    
    return (fromRow >= 0 && fromRow < 8 && fromCol >= 0 && fromCol < 8 &&
            toRow >= 0 && toRow < 8 && toCol >= 0 && toCol < 8);
}

String ChessBotVsBot::makeStockfishRequest(String fen, BotDifficulty difficulty) {
    WiFiSSLClient client;
    
#if defined(ESP32) || defined(ESP8266)
    client.setInsecure();
#endif
    
    // Get settings for this difficulty
    StockfishSettings currentSettings;
    switch(difficulty) {
        case BOT_EASY: currentSettings = StockfishSettings::easy(); break;
        case BOT_MEDIUM: currentSettings = StockfishSettings::medium(); break;
        case BOT_HARD: currentSettings = StockfishSettings::hard(); break;
        case BOT_EXPERT: currentSettings = StockfishSettings::expert(); break;
    }
    
    // Retry logic
    for (int attempt = 1; attempt <= currentSettings.maxRetries; attempt++) {
        if (client.connect(STOCKFISH_API_URL, STOCKFISH_API_PORT)) {
            String encodedFen = urlEncode(fen);
            String url = String(STOCKFISH_API_PATH) + "?fen=" + encodedFen + "&depth=" + String(currentSettings.depth);
            
            client.println("GET " + url + " HTTP/1.1");
            client.println("Host: " + String(STOCKFISH_API_URL));
            client.println("Connection: close");
            client.println();
            
            unsigned long startTime = millis();
            String response = "";
            bool gotResponse = false;
            
            while (client.connected() && (millis() - startTime < currentSettings.timeoutMs)) {
                if (client.available()) {
                    response = client.readString();
                    gotResponse = true;
                    break;
                }
                delay(10);
            }
            
            client.stop();
            
            if (gotResponse && response.length() > 0) {
                return response;
            } else if (attempt < currentSettings.maxRetries) {
                delay(1000);
            }
        } else if (attempt < currentSettings.maxRetries) {
            delay(1000);
        }
    }
    
    return "";
}

bool ChessBotVsBot::parseStockfishResponse(String response, String &bestMove, float &evaluation) {
    evaluation = 0.0;
    
    int jsonStart = response.indexOf("{");
    if (jsonStart == -1) {
        return false;
    }
    
    String json = response.substring(jsonStart);
    
    // Parse evaluation
    int evalStart = json.indexOf("\"evaluation\":");
    if (evalStart == -1) {
        evalStart = json.indexOf("\"score\":");
        if (evalStart == -1) {
            evalStart = json.indexOf("\"cp\":");
            if (evalStart >= 0) {
                evalStart += 5;
            }
        } else {
            evalStart += 8;
        }
    } else {
        evalStart += 14;
    }
    
    if (evalStart >= 0) {
        String evalStr = json.substring(evalStart);
        evalStr.trim();
        while (evalStr.length() > 0 && (evalStr[0] == ' ' || evalStr[0] == '"' || evalStr[0] == '\'')) {
            evalStr = evalStr.substring(1);
        }
        int evalEnd = evalStr.length();
        for (int i = 0; i < evalStr.length(); i++) {
            char c = evalStr[i];
            if (c == ',' || c == '}' || c == ' ' || c == '\n' || c == '\r') {
                evalEnd = i;
                break;
            }
        }
        evalStr = evalStr.substring(0, evalEnd);
        evalStr.trim();
        
        if (evalStr.length() > 0) {
            evaluation = evalStr.toFloat();
            if (evaluation > -10 && evaluation < 10) {
                evaluation = evaluation * 100.0;
            }
        }
    }
    
    // Parse bestmove
    int bestmoveStart = json.indexOf("\"bestmove\":\"");
    if (bestmoveStart == -1) {
        bestmoveStart = json.indexOf("\"move\":\"");
        if (bestmoveStart == -1) {
            return false;
        }
        bestmoveStart += 8;
    } else {
        bestmoveStart += 12;
    }
    
    int bestmoveEnd = json.indexOf("\"", bestmoveStart);
    if (bestmoveEnd == -1) {
        return false;
    }
    
    String fullMove = json.substring(bestmoveStart, bestmoveEnd);
    
    int moveStart = fullMove.indexOf("bestmove ");
    if (moveStart >= 0) {
        moveStart += 9;
        int moveEnd = fullMove.indexOf(" ", moveStart);
        if (moveEnd == -1) {
            bestMove = fullMove.substring(moveStart);
        } else {
            bestMove = fullMove.substring(moveStart, moveEnd);
        }
    } else {
        bestMove = fullMove;
    }
    
    bestMove.trim();
    
    if (bestMove.length() < 4 || bestMove.length() > 5) {
        return false;
    }
    
    return true;
}

String ChessBotVsBot::urlEncode(String str) {
    String encoded = "";
    char c;
    char code0;
    char code1;
    
    for (int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (c == ' ') {
            encoded += "%20";
        } else if (c == '/') {
            encoded += "%2F";
        } else if (isalnum(c)) {
            encoded += c;
        } else {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) {
                code1 = (c & 0xf) - 10 + 'A';
            }
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9) {
                code0 = c - 10 + 'A';
            }
            encoded += '%';
            encoded += code0;
            encoded += code1;
        }
    }
    return encoded;
}

bool ChessBotVsBot::connectToWiFi() {
#if defined(ESP32) || defined(ESP8266)
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SECRET_SSID);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(SECRET_SSID, SECRET_PASS);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        return true;
    } else {
        return false;
    }
#elif defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_NANO_RP2040_CONNECT)
    if (WiFi.status() == WL_NO_MODULE) {
        return false;
    }
    
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SECRET_SSID);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
        WiFi.begin(SECRET_SSID, SECRET_PASS);
        delay(5000);
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        return true;
    } else {
        return false;
    }
#else
    return false;
#endif
}

void ChessBotVsBot::printCurrentBoard() {
    Serial.println("=== CURRENT BOARD STATE ===");
    Serial.println("  a b c d e f g h");
    for (int row = 0; row < 8; row++) {
        Serial.print(8 - row);
        Serial.print(" ");
        for (int col = 0; col < 8; col++) {
            char piece = board[row][col];
            if (piece == ' ') {
                Serial.print(". ");
            } else {
                Serial.print(piece);
                Serial.print(" ");
            }
        }
        Serial.print(" ");
        Serial.println(8 - row);
    }
    Serial.println("  a b c d e f g h");
    Serial.println("========================");
}

void ChessBotVsBot::setWhiteDifficulty(BotDifficulty diff) {
    whiteDifficulty = diff;
    switch(whiteDifficulty) {
        case BOT_EASY: whiteSettings = StockfishSettings::easy(); break;
        case BOT_MEDIUM: whiteSettings = StockfishSettings::medium(); break;
        case BOT_HARD: whiteSettings = StockfishSettings::hard(); break;
        case BOT_EXPERT: whiteSettings = StockfishSettings::expert(); break;
    }
}

void ChessBotVsBot::setBlackDifficulty(BotDifficulty diff) {
    blackDifficulty = diff;
    switch(blackDifficulty) {
        case BOT_EASY: blackSettings = StockfishSettings::easy(); break;
        case BOT_MEDIUM: blackSettings = StockfishSettings::medium(); break;
        case BOT_HARD: blackSettings = StockfishSettings::hard(); break;
        case BOT_EXPERT: blackSettings = StockfishSettings::expert(); break;
    }
}

void ChessBotVsBot::setMoveDelay(int delayMs) {
    moveDelay = delayMs;
}

void ChessBotVsBot::getBoardState(char boardState[8][8]) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            boardState[row][col] = board[row][col];
        }
    }
}

void ChessBotVsBot::setBoardState(char newBoardState[8][8]) {
    Serial.println("Board state updated via WiFi edit");
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            board[row][col] = newBoardState[row][col];
        }
    }
    _boardDriver->readSensors();
}

