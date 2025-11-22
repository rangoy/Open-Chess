#ifndef UNIFIED_CHESS_GAME_H
#define UNIFIED_CHESS_GAME_H

#include "board_driver.h"
#include "chess_engine.h"
#include "chess_pgn.h"
#include "stockfish_settings.h"

// Player type for each side
enum PlayerType {
    PLAYER_HUMAN = 0,
    PLAYER_BOT_EASY = 1,
    PLAYER_BOT_MEDIUM = 2,
    PLAYER_BOT_HARD = 3
};

class UnifiedChessGame {
private:
    BoardDriver* _boardDriver;
    ChessEngine* _chessEngine;
    ChessPGN* _pgnTracker;
    
    char board[8][8];
    static const char INITIAL_BOARD[8][8];
    
    PlayerType whitePlayer;
    PlayerType blackPlayer;
    bool isWhiteTurn;
    bool gameStarted;
    
    // Bot-related
    bool botThinking;
    bool wifiConnected;
    float currentEvaluation;
    StockfishSettings whiteSettings;
    StockfishSettings blackSettings;
    
    // Move tracking for bot moves - non-blocking state machine
    enum BotState {
        BOT_IDLE = 0,
        BOT_THINKING = 1,
        BOT_WAITING_FOR_PICKUP = 2,
        BOT_WAITING_FOR_PLACEMENT = 3
    };
    BotState botState;
    int botMoveFromRow, botMoveFromCol, botMoveToRow, botMoveToCol;
    unsigned long lastBlinkTime;
    bool blinkState;
    String pendingStockfishResponse;
    BotDifficulty pendingDifficulty;
    unsigned long stockfishRequestStartTime;
    void* stockfishClient;  // WiFiClientSecure* or WiFiSSLClient* depending on platform
    bool stockfishRequestInProgress;
    
    // FEN and Stockfish
    String boardToFEN();
    void fenToBoard(String fen);
    bool connectToWiFi();
    String makeStockfishRequest(String fen, BotDifficulty difficulty);
    bool parseStockfishResponse(String response, String &bestMove, float &evaluation);
    bool parseMove(String move, int &fromRow, int &fromCol, int &toRow, int &toCol);
    void executeBotMove(int fromRow, int fromCol, int toRow, int toCol);
    String urlEncode(String str);
    
    // Game flow
    void initializeBoard();
    void waitForBoardSetup();
    void processMove(int fromRow, int fromCol, int toRow, int toCol, char piece);
    void checkForPromotion(int targetRow, int targetCol, char piece);
    void handlePromotion(int targetRow, int targetCol, char piece);
    void makeBotMove();
    void updateBotState();  // Non-blocking bot state machine
    void showBotThinking();
    void showConnectionStatus();
    void showBotMoveIndicator(int fromRow, int fromCol, int toRow, int toCol);
    void updateMoveCompletion();  // Non-blocking move completion check
    void printCurrentBoard();
    
public:
    UnifiedChessGame(BoardDriver* boardDriver, ChessEngine* chessEngine);
    ~UnifiedChessGame();
    
    void begin(PlayerType white, PlayerType black);
    void update();
    void reset();
    
    // Get current board state for WiFi display
    void getBoardState(char boardState[8][8]);
    
    // Set board state for editing/corrections
    void setBoardState(char newBoardState[8][8]);
    
    // PGN and undo functionality
    String getPGN();
    bool undoLastMove();
    bool canUndo();
    
    // Evaluation
    float getEvaluation() { return currentEvaluation; }
    
    // Player type getters
    PlayerType getWhitePlayer() { return whitePlayer; }
    PlayerType getBlackPlayer() { return blackPlayer; }
};

#endif // UNIFIED_CHESS_GAME_H

