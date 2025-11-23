#include "unified_chess_game.h"
#include <Arduino.h>
#include "arduino_secrets.h"

// WiFi includes - conditional based on board type
#if defined(ESP32) || defined(ESP8266)
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
#elif defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_NANO_RP2040_CONNECT)
  #include <WiFiNINA.h>
  #include <WiFiSSLClient.h>
#endif

// Expected initial configuration
// Note: With corrected mapping, row 0 = rank 1, row 7 = rank 8
// Code convention: uppercase = white, lowercase = black
const char UnifiedChessGame::INITIAL_BOARD[8][8] = {
  {'R', 'N', 'B', 'K', 'Q', 'B', 'N', 'R'},  // row 0 (rank 1 - white pieces at bottom)
  {'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'},  // row 1 (rank 2)
  {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},    // row 2 (rank 3)
  {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},    // row 3 (rank 4)
  {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},    // row 4 (rank 5)
  {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},    // row 5 (rank 6)
  {'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'},    // row 6 (rank 7)
  {'r', 'n', 'b', 'k', 'q', 'b', 'n', 'r'}     // row 7 (rank 8 - black pieces at top)
};

UnifiedChessGame::UnifiedChessGame(BoardDriver* boardDriver, ChessEngine* chessEngine) {
    _boardDriver = boardDriver;
    _chessEngine = chessEngine;
    _pgnTracker = new ChessPGN();
    
    whitePlayer = PLAYER_HUMAN;
    blackPlayer = PLAYER_HUMAN;
    isWhiteTurn = true;
    gameStarted = false;
    shouldReturnToSelection = false;
    botThinking = false;
    wifiConnected = false;
    currentEvaluation = 0.0;
    botState = BOT_IDLE;
    botMoveFromRow = botMoveFromCol = botMoveToRow = botMoveToCol = -1;
    lastBlinkTime = 0;
    blinkState = false;
    pendingStockfishResponse = "";
    stockfishRequestInProgress = false;
    stockfishClient = nullptr;
    
    initializeBoard();
}

UnifiedChessGame::~UnifiedChessGame() {
    if (_pgnTracker) {
        delete _pgnTracker;
    }
}

void UnifiedChessGame::begin(PlayerType white, PlayerType black) {
    whitePlayer = white;
    blackPlayer = black;
    
    Serial.println("=== Starting Unified Chess Game ===");
    Serial.print("White: ");
    switch(whitePlayer) {
        case PLAYER_HUMAN: Serial.println("Human"); break;
        case PLAYER_BOT_EASY: Serial.println("Easy AI"); break;
        case PLAYER_BOT_MEDIUM: Serial.println("Medium AI"); break;
        case PLAYER_BOT_HARD: Serial.println("Hard AI"); break;
    }
    Serial.print("Black: ");
    switch(blackPlayer) {
        case PLAYER_HUMAN: Serial.println("Human"); break;
        case PLAYER_BOT_EASY: Serial.println("Easy AI"); break;
        case PLAYER_BOT_MEDIUM: Serial.println("Medium AI"); break;
        case PLAYER_BOT_HARD: Serial.println("Hard AI"); break;
    }
    
    // Set bot difficulty settings
    switch(whitePlayer) {
        case PLAYER_BOT_EASY: whiteSettings = StockfishSettings::easy(); break;
        case PLAYER_BOT_MEDIUM: whiteSettings = StockfishSettings::medium(); break;
        case PLAYER_BOT_HARD: whiteSettings = StockfishSettings::hard(); break;
        default: break;
    }
    switch(blackPlayer) {
        case PLAYER_BOT_EASY: blackSettings = StockfishSettings::easy(); break;
        case PLAYER_BOT_MEDIUM: blackSettings = StockfishSettings::medium(); break;
        case PLAYER_BOT_HARD: blackSettings = StockfishSettings::hard(); break;
        default: break;
    }
    
    _boardDriver->clearAllLEDs();
    _boardDriver->showLEDs();
    
    // Connect to WiFi if any player is a bot
    if (whitePlayer != PLAYER_HUMAN || blackPlayer != PLAYER_HUMAN) {
        Serial.println("Connecting to WiFi...");
        showConnectionStatus();
        
        if (connectToWiFi()) {
            Serial.println("WiFi connected! Bot mode ready.");
            wifiConnected = true;
            
            // Show success animation
            for (int i = 0; i < 3; i++) {
                _boardDriver->clearAllLEDs();
                _boardDriver->showLEDs();
                delay(200);
                
                for (int row = 0; row < 8; row++) {
                    for (int col = 0; col < 8; col++) {
                        _boardDriver->setSquareLED(row, col, 0, 255, 0); // Green
                    }
                }
                _boardDriver->showLEDs();
                delay(200);
            }
        } else {
            Serial.println("Failed to connect to WiFi. Bot mode unavailable.");
            wifiConnected = false;
            
            // Show error animation
            for (int i = 0; i < 5; i++) {
                _boardDriver->clearAllLEDs();
                _boardDriver->showLEDs();
                delay(300);
                
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
    
    initializeBoard();
    waitForBoardSetup();
    
    Serial.println("Chess game ready to start!");
    _boardDriver->fireworkAnimation();
    
    _boardDriver->readSensors();
    _boardDriver->updateSensorPrev();
    
    gameStarted = true;
    isWhiteTurn = true;
}

void UnifiedChessGame::reset() {
    gameStarted = false;
    botThinking = false;
    botState = BOT_IDLE;
    botMoveFromRow = botMoveFromCol = botMoveToRow = botMoveToCol = -1;
    stockfishRequestInProgress = false;
    shouldReturnToSelection = false;
    if (stockfishClient != nullptr) {
#if defined(ESP32) || defined(ESP8266)
        WiFiClientSecure* client = (WiFiClientSecure*)stockfishClient;
        client->stop();
        delete client;
#elif defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_NANO_RP2040_CONNECT)
        WiFiSSLClient* client = (WiFiSSLClient*)stockfishClient;
        client->stop();
        delete client;
#endif
        stockfishClient = nullptr;
    }
    isWhiteTurn = true;
    if (_pgnTracker) {
        _pgnTracker->reset();
        _pgnTracker->updateBoardState(board);
    }
    initializeBoard();
}

void UnifiedChessGame::initializeBoard() {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            board[row][col] = INITIAL_BOARD[row][col];
        }
    }
    isWhiteTurn = true;
    if (_pgnTracker) {
        _pgnTracker->reset();
        _pgnTracker->updateBoardState(board);
    }
}

void UnifiedChessGame::waitForBoardSetup() {
    Serial.println("Waiting for board setup...");
    Serial.println("Please set up the chess board in starting position...");
    
    _boardDriver->clearAllLEDs();
    
    // Wait until all pieces are in the correct positions
    while (!_boardDriver->checkInitialBoard(INITIAL_BOARD)) {
        _boardDriver->readSensors();
        _boardDriver->updateSetupDisplay(INITIAL_BOARD);
        _boardDriver->showLEDs();
        delay(100);
    }
    
    Serial.println("Board setup complete! All pieces are in place.");
    _boardDriver->clearAllLEDs();
    _boardDriver->showLEDs();
}

void UnifiedChessGame::getBoardState(char boardState[8][8]) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            boardState[row][col] = board[row][col];
        }
    }
}

void UnifiedChessGame::setBoardState(char newBoardState[8][8]) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            board[row][col] = newBoardState[row][col];
        }
    }
    if (_pgnTracker) {
        _pgnTracker->updateBoardState(board);
    }
}

String UnifiedChessGame::getPGN() {
    if (_pgnTracker) {
        return _pgnTracker->getPGN();
    }
    return "";
}

bool UnifiedChessGame::undoLastMove() {
    if (_pgnTracker && _pgnTracker->canUndo()) {
        bool success = _pgnTracker->undoLastMove(board);
        if (success) {
            isWhiteTurn = !isWhiteTurn; // Switch turn back
            return true;
        }
    }
    return false;
}

bool UnifiedChessGame::canUndo() {
    return _pgnTracker && _pgnTracker->canUndo();
}

void UnifiedChessGame::update() {
    if (!gameStarted) return;
    
    // Always read sensors first to get current state
    _boardDriver->readSensors();
    
    // Check if both kings are physically removed from the board (sensor-based detection)
    checkForBothKingsMissing();
    if (shouldReturnToSelection) {
        return; // Don't process moves if we're returning to selection
    }
    
    // Check if current player is a bot
    PlayerType currentPlayer = isWhiteTurn ? whitePlayer : blackPlayer;
    
    if (currentPlayer == PLAYER_HUMAN) {
        // Human player's turn - handle move detection
        
        // Look for piece pickup
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                if (_boardDriver->getSensorPrev(row, col) && !_boardDriver->getSensorState(row, col)) {
                    char piece = board[row][col];
                    
                    // Skip empty squares or wrong color pieces
                    if (piece == ' ') continue;
                    bool isWhitePiece = (piece >= 'A' && piece <= 'Z');
                    if ((isWhiteTurn && !isWhitePiece) || (!isWhiteTurn && isWhitePiece)) continue;
                    
                    // Generate possible moves
                    int moveCount = 0;
                    int moves[28][2];
                    _chessEngine->getPossibleMoves(board, row, col, moveCount, moves);
                    
                    // Light up current square and possible moves
                    _boardDriver->setSquareLED(row, col, 0, 0, 0, 100);
                    for (int i = 0; i < moveCount; i++) {
                        int r = moves[i][0];
                        int c = moves[i][1];
                        if (board[r][c] == ' ') {
                            _boardDriver->setSquareLED(r, c, 0, 0, 0, 50);
                        } else {
                            _boardDriver->setSquareLED(r, c, 255, 0, 0, 50);
                        }
                    }
                    _boardDriver->showLEDs();
                    
                    // Wait for piece placement
                    int targetRow = -1, targetCol = -1;
                    bool piecePlaced = false;
                    
                    while (!piecePlaced) {
                        _boardDriver->readSensors();
                        
                        if (_boardDriver->getSensorState(row, col)) {
                            // Piece returned
                            targetRow = row;
                            targetCol = col;
                            piecePlaced = true;
                            break;
                        }
                        
                        for (int r2 = 0; r2 < 8; r2++) {
                            for (int c2 = 0; c2 < 8; c2++) {
                                if (r2 == row && c2 == col) continue;
                                
                                bool isLegalMove = false;
                                for (int i = 0; i < moveCount; i++) {
                                    if (moves[i][0] == r2 && moves[i][1] == c2) {
                                        isLegalMove = true;
                                        break;
                                    }
                                }
                                
                                if (!isLegalMove) continue;
                                
                                // Check for capture
                                if (board[r2][c2] != ' ' && !_boardDriver->getSensorState(r2, c2) && _boardDriver->getSensorPrev(r2, c2)) {
                                    targetRow = r2;
                                    targetCol = c2;
                                    // Wait for capturing piece placement
                                    while (!_boardDriver->getSensorState(r2, c2)) {
                                        _boardDriver->readSensors();
                                        delay(50);
                                    }
                                    piecePlaced = true;
                                    break;
                                } else if (board[r2][c2] == ' ' && _boardDriver->getSensorState(r2, c2) && !_boardDriver->getSensorPrev(r2, c2)) {
                                    targetRow = r2;
                                    targetCol = c2;
                                    piecePlaced = true;
                                    break;
                                }
                            }
                            if (piecePlaced) break;
                        }
                        delay(50);
                    }
                    
                    // Check if returned to original position
                    if (targetRow == row && targetCol == col) {
                        _boardDriver->clearAllLEDs();
                        continue;
                    }
                    
                    // Validate move
                    bool legalMove = false;
                    for (int i = 0; i < moveCount; i++) {
                        if (moves[i][0] == targetRow && moves[i][1] == targetCol) {
                            legalMove = true;
                            break;
                        }
                    }
                    
                    if (legalMove) {
                        char capturedPiece = board[targetRow][targetCol];
                        char promotedPiece = '\0';
                        if (_chessEngine->isPawnPromotion(piece, targetRow)) {
                            promotedPiece = _chessEngine->getPromotedPiece(piece);
                        }
                        
                        processMove(row, col, targetRow, targetCol, piece);
                        
                        if (_pgnTracker) {
                            _pgnTracker->addMove(row, col, targetRow, targetCol, piece, capturedPiece, promotedPiece, isWhiteTurn, board);
                        }
                        
                        checkForPromotion(targetRow, targetCol, piece);
                        
                        if (_pgnTracker && promotedPiece != '\0') {
                            _pgnTracker->updateBoardState(board);
                        }
                        
                        _boardDriver->clearAllLEDs();
                        _boardDriver->setSquareLED(targetRow, targetCol, 0, 255, 0);
                        _boardDriver->showLEDs();
                        delay(300);
                        _boardDriver->clearAllLEDs();
                    } else {
                        Serial.println("Invalid move!");
                        _boardDriver->blinkSquare(targetRow, targetCol, 3);
                        _boardDriver->clearAllLEDs();
                    }
                }
            }
        }
        
        // Note: updateSensorPrev() is now called at end of update() for both human and bot
    } else {
        // Bot player's turn - use non-blocking state machine
        // Note: sensors already read at start of update()
        updateBotState();
    }
    
    // Update sensor previous state at the end of the update cycle
    _boardDriver->updateSensorPrev();
}

void UnifiedChessGame::processMove(int fromRow, int fromCol, int toRow, int toCol, char piece) {
    char capturedPiece = board[toRow][toCol];
    board[toRow][toCol] = piece;
    board[fromRow][fromCol] = ' ';
    isWhiteTurn = !isWhiteTurn;
    
    if (capturedPiece != ' ') {
        _boardDriver->captureAnimation();
    }
}

void UnifiedChessGame::checkForPromotion(int targetRow, int targetCol, char piece) {
    if (_chessEngine->isPawnPromotion(piece, targetRow)) {
        char promotedPiece = _chessEngine->getPromotedPiece(piece);
        board[targetRow][targetCol] = promotedPiece;
        _boardDriver->promotionAnimation(targetCol);
    }
}

void UnifiedChessGame::handlePromotion(int targetRow, int targetCol, char piece) {
    checkForPromotion(targetRow, targetCol, piece);
}

void UnifiedChessGame::makeBotMove() {
    if (!wifiConnected) {
        Serial.println("ERROR: Bot cannot make move - WiFi not connected");
        botState = BOT_IDLE;
        botThinking = false;
        return;
    }
    
    Serial.println("=== BOT MOVE CALCULATION ===");
    Serial.print("Bot is playing as: ");
    Serial.println(isWhiteTurn ? "White" : "Black");
    
    botState = BOT_THINKING;
    botThinking = true;
    
    String fen = boardToFEN();
    Serial.print("Sending FEN to Stockfish: ");
    Serial.println(fen);
    
    BotDifficulty currentDifficulty = isWhiteTurn ? 
        (whitePlayer == PLAYER_BOT_EASY ? BOT_EASY : 
         whitePlayer == PLAYER_BOT_MEDIUM ? BOT_MEDIUM : BOT_HARD) :
        (blackPlayer == PLAYER_BOT_EASY ? BOT_EASY : 
         blackPlayer == PLAYER_BOT_MEDIUM ? BOT_MEDIUM : BOT_HARD);
    
    pendingDifficulty = currentDifficulty;
    pendingStockfishResponse = "";
    stockfishRequestStartTime = millis();
    stockfishRequestInProgress = true;
    
    // Start the API request (non-blocking)
    // The actual request will be handled in updateBotState()
}

void UnifiedChessGame::updateBotState() {
    if (botState == BOT_IDLE) {
        // Check if it's time to start bot move
        if (wifiConnected && !botThinking) {
            makeBotMove();
        }
        return;
    }
    
    // Debug: Log current bot state
    static unsigned long lastStateLog = 0;
    if (millis() - lastStateLog > 2000) {
        Serial.print("DEBUG: updateBotState() - botState=");
        Serial.print(botState);
        Serial.print(", botThinking=");
        Serial.print(botThinking);
        Serial.print(", wifiConnected=");
        Serial.println(wifiConnected);
        lastStateLog = millis();
    }
    
    if (botState == BOT_THINKING) {
        // Show thinking animation
        showBotThinking();
        
        // Check if we need to start the API request
        if (stockfishRequestInProgress && stockfishClient == nullptr) {
            // Start the request
            String fen = boardToFEN();
            
            StockfishSettings currentSettings;
            switch(pendingDifficulty) {
                case BOT_EASY: currentSettings = StockfishSettings::easy(); break;
                case BOT_MEDIUM: currentSettings = StockfishSettings::medium(); break;
                case BOT_HARD: currentSettings = StockfishSettings::hard(); break;
                case BOT_EXPERT: currentSettings = StockfishSettings::expert(); break;
            }
            
            String encodedFen = urlEncode(fen);
            String url = String(STOCKFISH_API_PATH) + "?fen=" + encodedFen + "&depth=" + String(currentSettings.depth);
            
#if defined(ESP32) || defined(ESP8266)
            WiFiClientSecure* client = new WiFiClientSecure();
            client->setInsecure();
            stockfishClient = (void*)client;
            
            if (client->connect(STOCKFISH_API_URL, STOCKFISH_API_PORT)) {
                client->println("GET " + url + " HTTP/1.1");
                client->println("Host: " + String(STOCKFISH_API_URL));
                client->println("Connection: close");
                client->println();
            } else {
                Serial.println("Failed to connect to Stockfish API");
                delete client;
                stockfishClient = nullptr;
                stockfishRequestInProgress = false;
                botState = BOT_IDLE;
                botThinking = false;
                return;
            }
#elif defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_NANO_RP2040_CONNECT)
            WiFiSSLClient* client = new WiFiSSLClient();
            client->setInsecure();
            stockfishClient = (void*)client;
            
            if (client->connect(STOCKFISH_API_URL, STOCKFISH_API_PORT)) {
                client->println("GET " + url + " HTTP/1.1");
                client->println("Host: " + String(STOCKFISH_API_URL));
                client->println("Connection: close");
                client->println();
            } else {
                Serial.println("Failed to connect to Stockfish API");
                delete client;
                stockfishClient = nullptr;
                stockfishRequestInProgress = false;
                botState = BOT_IDLE;
                botThinking = false;
                return;
            }
#else
            stockfishRequestInProgress = false;
            botState = BOT_IDLE;
            botThinking = false;
            return;
#endif
        }
        
        // Check for response (non-blocking)
        if (stockfishClient != nullptr) {
#if defined(ESP32) || defined(ESP8266)
            WiFiClientSecure* client = (WiFiClientSecure*)stockfishClient;
            if (client->connected()) {
                if (client->available()) {
                    pendingStockfishResponse = client->readString();
                    client->stop();
                    delete client;
                    stockfishClient = nullptr;
                    stockfishRequestInProgress = false;
                    
                    // Process the response
                    if (pendingStockfishResponse.length() > 0) {
                        String bestMove;
                        float evaluation = 0.0;
                        if (parseStockfishResponse(pendingStockfishResponse, bestMove, evaluation)) {
                            currentEvaluation = evaluation;
                            
                            int fromRow, fromCol, toRow, toCol;
                            if (parseMove(bestMove, fromRow, fromCol, toRow, toCol)) {
                                Serial.print("DEBUG: Parsed move coordinates - fromRow=");
                                Serial.print(fromRow);
                                Serial.print(", fromCol=");
                                Serial.print(fromCol);
                                Serial.print(", toRow=");
                                Serial.print(toRow);
                                Serial.print(", toCol=");
                                Serial.println(toCol);
                                
                                // Debug: Print board state around the source square
                                Serial.println("DEBUG: Board state around source:");
                                for (int r = fromRow - 1; r <= fromRow + 1; r++) {
                                    if (r >= 0 && r < 8) {
                                        Serial.print("  Row ");
                                        Serial.print(r);
                                        Serial.print(": ");
                                        for (int c = fromCol - 1; c <= fromCol + 1; c++) {
                                            if (c >= 0 && c < 8) {
                                                char p = board[r][c];
                                                if (p == ' ') p = '.';
                                                Serial.print(p);
                                                Serial.print(" ");
                                            }
                                        }
                                        Serial.println();
                                    }
                                }
                                
                                char piece = board[fromRow][fromCol];
                                Serial.print("DEBUG: Piece at source: '");
                                Serial.print(piece);
                                Serial.print("' (isWhiteTurn=");
                                Serial.print(isWhiteTurn);
                                Serial.print(", isWhite=");
                                Serial.print(piece >= 'A' && piece <= 'Z');
                                Serial.print(", isBlack=");
                                Serial.print(piece >= 'a' && piece <= 'z');
                                Serial.println(")");
                                
                                // Try to find the piece on the board
                                if (piece == ' ') {
                                    Serial.println("DEBUG: Piece not found at expected location, searching board...");
                                    char expectedPiece = isWhiteTurn ? 'P' : 'p'; // Default to pawn
                                    bool found = false;
                                    for (int r = 0; r < 8 && !found; r++) {
                                        for (int c = 0; c < 8 && !found; c++) {
                                            if (board[r][c] == expectedPiece) {
                                                Serial.print("DEBUG: Found ");
                                                Serial.print(expectedPiece);
                                                Serial.print(" at row=");
                                                Serial.print(r);
                                                Serial.print(", col=");
                                                Serial.print(c);
                                                Serial.print(" (file=");
                                                Serial.print((char)('a' + (7 - c)));
                                                Serial.print(", rank=");
                                                Serial.print(1 + r);
                                                Serial.println(")");
                                            }
                                        }
                                    }
                                }
                                
                                bool isCorrectPiece = (isWhiteTurn && piece >= 'A' && piece <= 'Z') || 
                                                      (!isWhiteTurn && piece >= 'a' && piece <= 'z');
                                
                                if (isCorrectPiece && piece != ' ') {
                                    Serial.println("DEBUG: Piece is correct, calling executeBotMove()");
                                    // Start move execution
                                    executeBotMove(fromRow, fromCol, toRow, toCol);
                                    Serial.print("DEBUG: executeBotMove() returned, botState=");
                                    Serial.println(botState);
                                    return;
                                } else {
                                    Serial.println("DEBUG: ERROR - Piece is not correct or empty!");
                                    Serial.print("  isCorrectPiece=");
                                    Serial.print(isCorrectPiece);
                                    Serial.print(", piece='");
                                    Serial.print(piece);
                                    Serial.println("'");
                                    // Don't reset state, let it try again
                                    botState = BOT_IDLE;
                                    botThinking = false;
                                }
                            } else {
                                Serial.println("DEBUG: ERROR - Failed to parse move!");
                                botState = BOT_IDLE;
                                botThinking = false;
                            }
                        }
                    }
                    
                    // If we get here, something failed
                    botState = BOT_IDLE;
                    botThinking = false;
                } else {
                    // Check timeout
                    StockfishSettings currentSettings;
                    switch(pendingDifficulty) {
                        case BOT_EASY: currentSettings = StockfishSettings::easy(); break;
                        case BOT_MEDIUM: currentSettings = StockfishSettings::medium(); break;
                        case BOT_HARD: currentSettings = StockfishSettings::hard(); break;
                        case BOT_EXPERT: currentSettings = StockfishSettings::expert(); break;
                    }
                    
                    if (millis() - stockfishRequestStartTime > currentSettings.timeoutMs) {
                        Serial.println("Stockfish API request timeout");
                        client->stop();
                        delete client;
                        stockfishClient = nullptr;
                        stockfishRequestInProgress = false;
                        botState = BOT_IDLE;
                        botThinking = false;
                    }
                }
            } else {
                // Connection lost
                delete client;
                stockfishClient = nullptr;
                stockfishRequestInProgress = false;
                botState = BOT_IDLE;
                botThinking = false;
            }
#elif defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_NANO_RP2040_CONNECT)
            WiFiSSLClient* client = (WiFiSSLClient*)stockfishClient;
            if (client->connected()) {
                if (client->available()) {
                    pendingStockfishResponse = client->readString();
                    client->stop();
                    delete client;
                    stockfishClient = nullptr;
                    stockfishRequestInProgress = false;
                    
                    // Process the response (same as ESP32 version)
                    if (pendingStockfishResponse.length() > 0) {
                        String bestMove;
                        float evaluation = 0.0;
                        if (parseStockfishResponse(pendingStockfishResponse, bestMove, evaluation)) {
                            currentEvaluation = evaluation;
                            
                            int fromRow, fromCol, toRow, toCol;
                            if (parseMove(bestMove, fromRow, fromCol, toRow, toCol)) {
                                char piece = board[fromRow][fromCol];
                                bool isCorrectPiece = (isWhiteTurn && piece >= 'A' && piece <= 'Z') || 
                                                      (!isWhiteTurn && piece >= 'a' && piece <= 'z');
                                
                                if (isCorrectPiece && piece != ' ') {
                                    executeBotMove(fromRow, fromCol, toRow, toCol);
                                    return;
                                }
                            }
                        }
                    }
                    
                    botState = BOT_IDLE;
                    botThinking = false;
                } else {
                    StockfishSettings currentSettings;
                    switch(pendingDifficulty) {
                        case BOT_EASY: currentSettings = StockfishSettings::easy(); break;
                        case BOT_MEDIUM: currentSettings = StockfishSettings::medium(); break;
                        case BOT_HARD: currentSettings = StockfishSettings::hard(); break;
                        case BOT_EXPERT: currentSettings = StockfishSettings::expert(); break;
                    }
                    
                    if (millis() - stockfishRequestStartTime > currentSettings.timeoutMs) {
                        Serial.println("Stockfish API request timeout");
                        client->stop();
                        delete client;
                        stockfishClient = nullptr;
                        stockfishRequestInProgress = false;
                        botState = BOT_IDLE;
                        botThinking = false;
                    }
                }
            } else {
                delete client;
                stockfishClient = nullptr;
                stockfishRequestInProgress = false;
                botState = BOT_IDLE;
                botThinking = false;
            }
#endif
        }
    } else if (botState == BOT_WAITING_FOR_PICKUP || botState == BOT_WAITING_FOR_PLACEMENT) {
        // Handle move completion (non-blocking)
        Serial.print("DEBUG: updateBotState() calling updateMoveCompletion(), botState=");
        Serial.println(botState);
        updateMoveCompletion();
    } else {
        Serial.print("DEBUG: updateBotState() - unexpected botState=");
        Serial.println(botState);
    }
}

void UnifiedChessGame::executeBotMove(int fromRow, int fromCol, int toRow, int toCol) {
    // Store move information for non-blocking completion
    botMoveFromRow = fromRow;
    botMoveFromCol = fromCol;
    botMoveToRow = toRow;
    botMoveToCol = toCol;
    botState = BOT_WAITING_FOR_PICKUP;
    lastBlinkTime = millis();
    blinkState = false;
    
    Serial.print("Bot wants to move piece from ");
    Serial.print((char)('a' + fromCol));
    Serial.print(8 - fromRow);
    Serial.print(" to ");
    Serial.print((char)('a' + toCol));
    Serial.println(8 - toRow);
    Serial.println("Please make this move on the physical board...");
    
    showBotMoveIndicator(fromRow, fromCol, toRow, toCol);
}

void UnifiedChessGame::updateMoveCompletion() {
    if (botState == BOT_IDLE || botState == BOT_THINKING) {
        return;
    }
    
    int fromRow = botMoveFromRow;
    int fromCol = botMoveFromCol;
    int toRow = botMoveToRow;
    int toCol = botMoveToCol;
    
    _boardDriver->readSensors();
    
    if (botState == BOT_WAITING_FOR_PICKUP) {
        // Blink the source square
        if (millis() - lastBlinkTime > 500) {
            _boardDriver->clearAllLEDs();
            if (blinkState) {
                _boardDriver->setSquareLED(fromRow, fromCol, 0, 0, 0, 255);
            }
            _boardDriver->setSquareLED(toRow, toCol, 0, 0, 0, 255);
            _boardDriver->showLEDs();
            
            blinkState = !blinkState;
            lastBlinkTime = millis();
        }
        
        // Check if piece was picked up from source
        if (!_boardDriver->getSensorState(fromRow, fromCol)) {
            botState = BOT_WAITING_FOR_PLACEMENT;
            Serial.println("Bot piece picked up, now place it on the destination...");
            
            _boardDriver->clearAllLEDs();
            _boardDriver->setSquareLED(toRow, toCol, 0, 0, 0, 255);
            _boardDriver->showLEDs();
        }
    } else if (botState == BOT_WAITING_FOR_PLACEMENT) {
        // Check if piece was placed on destination
        if (_boardDriver->getSensorState(toRow, toCol)) {
            // Move completed!
            char piece = board[fromRow][fromCol];
            char capturedPiece = board[toRow][toCol];
            char promotedPiece = '\0';
            
            if (_chessEngine->isPawnPromotion(piece, toRow)) {
                promotedPiece = _chessEngine->getPromotedPiece(piece);
            }
            
            // Update board state
            board[toRow][toCol] = piece;
            board[fromRow][fromCol] = ' ';
            
            if (promotedPiece != '\0') {
                board[toRow][toCol] = promotedPiece;
                _boardDriver->promotionAnimation(toCol);
            }
            
            if (capturedPiece != ' ') {
                Serial.print("Piece captured: ");
                Serial.println(capturedPiece);
                _boardDriver->captureAnimation();
            }
            
            if (_pgnTracker) {
                _pgnTracker->addMove(fromRow, fromCol, toRow, toCol, piece, capturedPiece, promotedPiece, isWhiteTurn, board);
            }
            
            // Flash confirmation
            _boardDriver->setSquareLED(toRow, toCol, 0, 255, 0);
            _boardDriver->showLEDs();
            delay(300);
            _boardDriver->clearAllLEDs();
            
            Serial.println("Bot move completed on physical board!");
            
            // Reset state and switch turns
            botState = BOT_IDLE;
            botMoveFromRow = botMoveFromCol = botMoveToRow = botMoveToCol = -1;
            botThinking = false;
            isWhiteTurn = !isWhiteTurn;
            
            Serial.print("Move completed. Now it's ");
            Serial.print(isWhiteTurn ? "White" : "Black");
            Serial.println("'s turn!");
        }
    }
    
    // Note: updateSensorPrev() is now called at end of update() for both human and bot
}

void UnifiedChessGame::showBotThinking() {
    static unsigned long lastUpdate = 0;
    static int thinkingStep = 0;
    
    if (millis() - lastUpdate > 500) {
        _boardDriver->clearAllLEDs();
        uint8_t brightness = (sin(thinkingStep * 0.3) + 1) * 127;
        _boardDriver->setSquareLED(0, 0, 0, 0, brightness);
        _boardDriver->setSquareLED(0, 7, 0, 0, brightness);
        _boardDriver->setSquareLED(7, 0, 0, 0, brightness);
        _boardDriver->setSquareLED(7, 7, 0, 0, brightness);
        _boardDriver->showLEDs();
        thinkingStep++;
        lastUpdate = millis();
    }
}

void UnifiedChessGame::showConnectionStatus() {
    for (int i = 0; i < 8; i++) {
        _boardDriver->setSquareLED(3, i, 0, 0, 255);
        _boardDriver->showLEDs();
        delay(200);
    }
}

void UnifiedChessGame::showBotMoveIndicator(int fromRow, int fromCol, int toRow, int toCol) {
    _boardDriver->clearAllLEDs();
    _boardDriver->setSquareLED(fromRow, fromCol, 0, 0, 0, 255);
    _boardDriver->setSquareLED(toRow, toCol, 0, 0, 0, 255);
    _boardDriver->showLEDs();
}

// waitForBotMoveCompletion removed - now using non-blocking updateMoveCompletion()

String UnifiedChessGame::boardToFEN() {
    String fen = "";
    for (int row = 7; row >= 0; row--) {
        int emptyCount = 0;
        // Columns are reversed: col 0 = file 'h', col 7 = file 'a'
        // So we need to iterate from col 7 to col 0 to get files 'a' to 'h' in FEN
        for (int col = 7; col >= 0; col--) {
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
    fen += isWhiteTurn ? " w" : " b";
    fen += " KQkq - 0 1";
    return fen;
}

void UnifiedChessGame::fenToBoard(String fen) {
    int spacePos = fen.indexOf(' ');
    if (spacePos > 0) {
        fen = fen.substring(0, spacePos);
    }
    
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            board[row][col] = ' ';
        }
    }
    
    int rank = 7;
    int col = 0;
    
    for (int i = 0; i < fen.length() && rank >= 0; i++) {
        char c = fen.charAt(i);
        if (c == '/') {
            rank--;
            col = 0;
        } else if (c >= '1' && c <= '8') {
            col += (c - '0');
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            if (rank >= 0 && rank < 8 && col >= 0 && col < 8) {
                board[rank][col] = c;
                col++;
            }
        }
    }
}

bool UnifiedChessGame::connectToWiFi() {
#if defined(ESP32) || defined(ESP8266)
    WiFi.mode(WIFI_STA);
    WiFi.begin(SECRET_SSID, SECRET_PASS);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    
    return WiFi.status() == WL_CONNECTED;
#elif defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_NANO_RP2040_CONNECT)
    if (WiFi.status() == WL_NO_MODULE) {
        return false;
    }
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
        WiFi.begin(SECRET_SSID, SECRET_PASS);
        delay(5000);
        attempts++;
    }
    
    return WiFi.status() == WL_CONNECTED;
#else
    return false;
#endif
}

String UnifiedChessGame::makeStockfishRequest(String fen, BotDifficulty difficulty) {
#if defined(ESP32) || defined(ESP8266)
    WiFiClientSecure client;
    client.setInsecure();
#elif defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_NANO_RP2040_CONNECT)
    WiFiSSLClient client;
    client.setInsecure();
#else
    // No WiFi support
    Serial.println("ERROR: WiFi not supported on this board");
    return "";
#endif
    
    StockfishSettings currentSettings;
    switch(difficulty) {
        case BOT_EASY: currentSettings = StockfishSettings::easy(); break;
        case BOT_MEDIUM: currentSettings = StockfishSettings::medium(); break;
        case BOT_HARD: currentSettings = StockfishSettings::hard(); break;
        case BOT_EXPERT: currentSettings = StockfishSettings::expert(); break;
    }
    
    Serial.println("Making API request to Stockfish...");
    Serial.print("FEN: ");
    Serial.println(fen);
    Serial.print("Depth: ");
    Serial.println(currentSettings.depth);
    Serial.print("Timeout: ");
    Serial.print(currentSettings.timeoutMs);
    Serial.println(" ms");
    Serial.print("Max retries: ");
    Serial.println(currentSettings.maxRetries);
    
#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_NANO_RP2040_CONNECT)
    for (int attempt = 1; attempt <= currentSettings.maxRetries; attempt++) {
        Serial.print("Attempt ");
        Serial.print(attempt);
        Serial.print("/");
        Serial.println(currentSettings.maxRetries);
        
        if (client.connect(STOCKFISH_API_URL, STOCKFISH_API_PORT)) {
            Serial.println("Connected to Stockfish API");
            String encodedFen = urlEncode(fen);
            String url = String(STOCKFISH_API_PATH) + "?fen=" + encodedFen + "&depth=" + String(currentSettings.depth);
            
            Serial.print("Request URL: ");
            Serial.println(url);
            
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
                Serial.print("Got response, length: ");
                Serial.println(response.length());
                if (response.length() < 500) {
                    Serial.print("Response: ");
                    Serial.println(response);
                } else {
                    Serial.print("Response (first 500 chars): ");
                    Serial.println(response.substring(0, 500));
                }
                return response;
            } else {
                Serial.print("No response or empty. gotResponse=");
                Serial.print(gotResponse);
                Serial.print(", length=");
                Serial.println(response.length());
                if (attempt < currentSettings.maxRetries) {
                    Serial.println("Retrying...");
                    delay(1000);
                }
            }
        } else {
            Serial.println("Failed to connect to Stockfish API");
            if (attempt < currentSettings.maxRetries) {
                Serial.println("Retrying...");
                delay(1000);
            }
        }
    }
    
    Serial.println("All API request attempts failed");
#endif
    
    return "";
}

bool UnifiedChessGame::parseStockfishResponse(String response, String &bestMove, float &evaluation) {
    // Initialize evaluation to 0 (neutral)
    evaluation = 0.0;
    
    // Find JSON content
    int jsonStart = response.indexOf("{");
    if (jsonStart == -1) {
        Serial.println("No JSON found in response");
        Serial.print("Response was: ");
        if (response.length() < 500) {
            Serial.println(response);
        } else {
            Serial.println(response.substring(0, 500) + "... (truncated)");
        }
        return false;
    }
    
    String json = response.substring(jsonStart);
    Serial.print("Extracted JSON: ");
    Serial.println(json);
    
    // Check if request was successful (some APIs use "success":true, others just return the move)
    bool hasSuccess = json.indexOf("\"success\"") >= 0;
    if (hasSuccess && json.indexOf("\"success\":true") == -1) {
        Serial.println("API request was not successful");
        return false;
    }
    
    // Parse evaluation - try different possible field names
    // Format 1: "evaluation": 0.5 (in pawns)
    // Format 2: "evaluation": 50 (in centipawns)
    // Format 3: "score": 0.5
    // Format 4: "cp": 50 (centipawns)
    int evalStart = json.indexOf("\"evaluation\":");
    if (evalStart == -1) {
        evalStart = json.indexOf("\"score\":");
        if (evalStart == -1) {
            evalStart = json.indexOf("\"cp\":");
            if (evalStart >= 0) {
                evalStart += 5; // Skip "cp":
            }
        } else {
            evalStart += 8; // Skip "score":
        }
    } else {
        evalStart += 14; // Skip "evaluation":
    }
    
    if (evalStart >= 0) {
        // Find the number value
        String evalStr = json.substring(evalStart);
        evalStr.trim();
        // Remove any leading whitespace or quotes
        while (evalStr.length() > 0 && (evalStr[0] == ' ' || evalStr[0] == '"' || evalStr[0] == '\'')) {
            evalStr = evalStr.substring(1);
        }
        // Find the end of the number (comma, }, or whitespace)
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
            // If evaluation is small (< 10), assume it's in pawns, convert to centipawns
            // If evaluation is large (> 10), assume it's already in centipawns
            if (evaluation > -10 && evaluation < 10) {
                evaluation = evaluation * 100.0; // Convert pawns to centipawns
            }
            Serial.print("Parsed evaluation: ");
            Serial.print(evaluation);
            Serial.println(" centipawns");
        }
    } else {
        Serial.println("No evaluation field found in response");
    }
    
    // Parse bestmove field - try different possible formats
    // Format 1: "bestmove":"e2e4"
    // Format 2: "bestmove":"bestmove e2e4 ponder e7e5"
    // Format 3: {"move":"e2e4"}
    
    int bestmoveStart = json.indexOf("\"bestmove\":\"");
    if (bestmoveStart == -1) {
        // Try alternative format with just "move"
        bestmoveStart = json.indexOf("\"move\":\"");
        if (bestmoveStart == -1) {
            Serial.println("No bestmove or move field found in response");
            return false;
        }
        bestmoveStart += 8; // Skip "move":"
    } else {
        bestmoveStart += 12; // Skip "bestmove":"
    }
    
    int bestmoveEnd = json.indexOf("\"", bestmoveStart);
    if (bestmoveEnd == -1) {
        Serial.println("Invalid bestmove format - no closing quote");
        return false;
    }
    
    String fullMove = json.substring(bestmoveStart, bestmoveEnd);
    Serial.print("Full move string: ");
    Serial.println(fullMove);
    
    // Check if the move string contains "bestmove " prefix (some APIs include it)
    int moveStart = fullMove.indexOf("bestmove ");
    if (moveStart >= 0) {
        moveStart += 9; // Skip "bestmove "
        int moveEnd = fullMove.indexOf(" ", moveStart);
        if (moveEnd == -1) {
            // No ponder part, take rest of string
            bestMove = fullMove.substring(moveStart);
        } else {
            bestMove = fullMove.substring(moveStart, moveEnd);
        }
    } else {
        // Move is directly in the field value
        bestMove = fullMove;
    }
    
    // Clean up the move - remove any whitespace
    bestMove.trim();
    
    Serial.print("Parsed move: ");
    Serial.println(bestMove);
    
    // Validate move format (should be 4-5 characters like "e2e4" or "e7e8q")
    if (bestMove.length() < 4 || bestMove.length() > 5) {
        Serial.print("Invalid move length: ");
        Serial.println(bestMove.length());
        return false;
    }
    
    return true;
}

bool UnifiedChessGame::parseMove(String move, int &fromRow, int &fromCol, int &toRow, int &toCol) {
    if (move.length() < 4) return false;
    
    char fromFile = move.charAt(0);
    char fromRank = move.charAt(1);
    char toFile = move.charAt(2);
    char toRank = move.charAt(3);
    
    if (fromFile < 'a' || fromFile > 'h' || toFile < 'a' || toFile > 'h') return false;
    if (fromRank < '1' || fromRank > '8' || toRank < '1' || toRank > '8') return false;
    
    // Columns are reversed: col = 7 - (file - 'a')
    // Rows are reversed: row = rank - 1 (formula same, but row 0 = rank 1 now)
    fromCol = 7 - (fromFile - 'a');
    fromRow = (fromRank - '0') - 1;
    toCol = 7 - (toFile - 'a');
    toRow = (toRank - '0') - 1;
    
    return (fromRow >= 0 && fromRow < 8 && fromCol >= 0 && fromCol < 8 &&
            toRow >= 0 && toRow < 8 && toCol >= 0 && toCol < 8);
}

String UnifiedChessGame::urlEncode(String str) {
    String encoded = "";
    for (int i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        if (c == ' ') {
            encoded += "%20";
        } else if (c == '/') {
            encoded += "%2F";
        } else if (isalnum(c)) {
            encoded += c;
        } else {
            char code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) {
                code1 = (c & 0xf) - 10 + 'A';
            }
            char code0 = ((c >> 4) & 0xf) + '0';
            if (((c >> 4) & 0xf) > 9) {
                code0 = ((c >> 4) & 0xf) - 10 + 'A';
            }
            encoded += '%';
            encoded += code0;
            encoded += code1;
        }
    }
    return encoded;
}

void UnifiedChessGame::printCurrentBoard() {
    Serial.println("=== CURRENT BOARD STATE ===");
    Serial.println("  a b c d e f g h");
    for (int row = 0; row < 8; row++) {
        // Rows are reversed: rank = 1 + row
        Serial.print(1 + row);
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
        // Rows are reversed: rank = 1 + row
        Serial.println(1 + row);
    }
    Serial.println("  a b c d e f g h");
    Serial.println("========================");
}

void UnifiedChessGame::checkForBothKingsMissing() {
    // Find where the kings are on the board
    int whiteKingRow = -1, whiteKingCol = -1;
    int blackKingRow = -1, blackKingCol = -1;
    
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (board[row][col] == 'K') {
                whiteKingRow = row;
                whiteKingCol = col;
            }
            if (board[row][col] == 'k') {
                blackKingRow = row;
                blackKingCol = col;
            }
        }
    }
    
    // Debug: print king positions
    // if (whiteKingRow >= 0 && blackKingRow >= 0) {
    //     Serial.print("DEBUG: White king at row=");
    //     Serial.print(whiteKingRow);
    //     Serial.print(", col=");
    //     Serial.print(whiteKingCol);
    //     Serial.print(" (sensor=");
    //     Serial.print(_boardDriver->getSensorState(whiteKingRow, whiteKingCol) ? "present" : "missing");
    //     Serial.print("), Black king at row=");
    //     Serial.print(blackKingRow);
    //     Serial.print(", col=");
    //     Serial.print(blackKingCol);
    //     Serial.print(" (sensor=");
    //     Serial.print(_boardDriver->getSensorState(blackKingRow, blackKingCol) ? "present" : "missing");
    //     Serial.println(")");
    // } else {
    //     Serial.print("DEBUG: ");
    //     if (whiteKingRow < 0) Serial.print("White king not found on board. ");
    //     if (blackKingRow < 0) Serial.print("Black king not found on board. ");
    //     Serial.println();
    // }
    
    // If we found both kings, check if they're physically missing (sensors detect no piece)
    if (whiteKingRow >= 0 && blackKingRow >= 0) {
        bool whiteKingMissing = !_boardDriver->getSensorState(whiteKingRow, whiteKingCol);
        bool blackKingMissing = !_boardDriver->getSensorState(blackKingRow, blackKingCol);
        
        // If both kings are physically removed from the board, reset to game selection
        if (whiteKingMissing && blackKingMissing) {
            Serial.println("WARNING: Both kings are physically removed from the board!");
            Serial.println("Returning to game selection...");
            shouldReturnToSelection = true;
            _boardDriver->clearAllLEDs();
            // Flash all LEDs red to indicate reset
            for (int i = 0; i < 3; i++) {
                for (int row = 0; row < 8; row++) {
                    for (int col = 0; col < 8; col++) {
                        _boardDriver->setSquareLED(row, col, 255, 0, 0);
                    }
                }
                _boardDriver->showLEDs();
                delay(300);
                _boardDriver->clearAllLEDs();
                _boardDriver->showLEDs();
                delay(300);
            }
        }
    }
}


