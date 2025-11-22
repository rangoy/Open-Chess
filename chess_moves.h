#ifndef CHESS_MOVES_H
#define CHESS_MOVES_H

#include "board_driver.h"
#include "chess_engine.h"
#include "chess_pgn.h"

// ---------------------------
// Chess Game Mode Class
// ---------------------------
class ChessMoves {
private:
    BoardDriver* boardDriver;
    ChessEngine* chessEngine;
    ChessPGN* pgnTracker;
    
    // Expected initial configuration
    static const char INITIAL_BOARD[8][8];
    
    // Internal board state for gameplay
    char board[8][8];
    bool isWhiteTurn;
    
    // Helper functions
    void initializeBoard();
    void waitForBoardSetup();
    void processMove(int fromRow, int fromCol, int toRow, int toCol, char piece);
    void checkForPromotion(int targetRow, int targetCol, char piece);
    void handlePromotion(int targetRow, int targetCol, char piece);

public:
    ChessMoves(BoardDriver* bd, ChessEngine* ce);
    ~ChessMoves();
    void begin();
    void update();
    bool isActive();
    void reset();
    
    // Get current board state for WiFi display
    void getBoardState(char boardState[8][8]);
    
    // Set board state for editing/corrections
    void setBoardState(char newBoardState[8][8]);
    
    // PGN and undo functionality
    String getPGN();
    bool undoLastMove();
    bool canUndo();
};

#endif // CHESS_MOVES_H
