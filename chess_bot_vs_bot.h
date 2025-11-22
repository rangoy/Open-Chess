#ifndef CHESS_BOT_VS_BOT_H
#define CHESS_BOT_VS_BOT_H

#include "board_driver.h"
#include "chess_engine.h"
#include "stockfish_settings.h"
#include "arduino_secrets.h"

// Platform-specific WiFi includes
#if defined(ESP32) || defined(ESP8266)
  // ESP32/ESP8266 use built-in WiFi libraries
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  #define WiFiSSLClient WiFiClientSecure
#elif defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_NANO_RP2040_CONNECT)
  // Boards with WiFiNINA module
  #include <WiFiNINA.h>
  #include <WiFiSSLClient.h>
#else
  // Other boards - WiFi not supported
  #warning "WiFi not supported on this board - Chess Bot vs Bot will not work"
#endif

class ChessBotVsBot {
private:
    BoardDriver* _boardDriver;
    ChessEngine* _chessEngine;
    
    char board[8][8];
    const char INITIAL_BOARD[8][8] = {
        {'R','N','B','K','Q','B','N','R'},  // row 0 (rank 8 - black pieces at top)
        {'P','P','P','P','P','P','P','P'},  // row 1 (rank 7)
        {' ',' ',' ',' ',' ',' ',' ',' '},  // row 2 (rank 6)
        {' ',' ',' ',' ',' ',' ',' ',' '},  // row 3 (rank 5)
        {' ',' ',' ',' ',' ',' ',' ',' '},  // row 4 (rank 4)
        {' ',' ',' ',' ',' ',' ',' ',' '},  // row 5 (rank 3)
        {'p','p','p','p','p','p','p','p'},  // row 6 (rank 2)
        {'r','n','b','k','q','b','n','r'}   // row 7 (rank 1 - white pieces at bottom)
    };
    
    StockfishSettings whiteSettings;
    StockfishSettings blackSettings;
    BotDifficulty whiteDifficulty;
    BotDifficulty blackDifficulty;
    
    bool isWhiteTurn;
    bool gameStarted;
    bool botThinking;
    bool wifiConnected;
    float currentEvaluation;  // Stockfish evaluation (in centipawns, positive = white advantage)
    unsigned long lastMoveTime;
    int moveDelay;  // Delay between moves in milliseconds
    
    // Non-blocking move completion state
    enum MoveCompletionState {
        MOVE_COMPLETE = 0,
        WAITING_FOR_PICKUP = 1,
        WAITING_FOR_PLACEMENT = 2
    };
    MoveCompletionState moveCompletionState;
    int pendingFromRow, pendingFromCol, pendingToRow, pendingToCol;
    unsigned long lastBlinkTime;
    bool blinkState;
    
    // FEN notation handling
    String boardToFEN();
    void fenToBoard(String fen);
    
    // WiFi and API
    bool connectToWiFi();
    String makeStockfishRequest(String fen, BotDifficulty difficulty);
    bool parseStockfishResponse(String response, String &bestMove, float &evaluation);
    
    // Move handling
    bool parseMove(String move, int &fromRow, int &fromCol, int &toRow, int &toCol);
    void executeBotMove(int fromRow, int fromCol, int toRow, int toCol);
    void updateMoveCompletion();  // Non-blocking move completion check
    
    // URL encoding helper
    String urlEncode(String str);
    
    // Game flow
    void initializeBoard();
    void makeBotMove();
    void showBotThinking();
    void showConnectionStatus();
    void showBotMoveIndicator(int fromRow, int fromCol, int toRow, int toCol);
    void confirmMoveCompletion();
    void printCurrentBoard();
    
public:
    ChessBotVsBot(BoardDriver* boardDriver, ChessEngine* chessEngine, 
                   BotDifficulty whiteDiff = BOT_MEDIUM, BotDifficulty blackDiff = BOT_MEDIUM,
                   int moveDelayMs = 2000);
    void begin();
    void update();
    void setWhiteDifficulty(BotDifficulty diff);
    void setBlackDifficulty(BotDifficulty diff);
    void setMoveDelay(int delayMs);
    
    // Get current board state for WiFi display
    void getBoardState(char boardState[8][8]);
    
    // Set board state for editing/corrections
    void setBoardState(char newBoardState[8][8]);
    
    // Get current evaluation
    float getEvaluation() { return currentEvaluation; }
};

#endif // CHESS_BOT_VS_BOT_H

