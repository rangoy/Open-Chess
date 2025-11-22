#ifndef CHESS_PGN_H
#define CHESS_PGN_H

#include <Arduino.h>

// Maximum number of moves to store in history
#define MAX_MOVE_HISTORY 200

struct MoveEntry {
    int fromRow;
    int fromCol;
    int toRow;
    int toCol;
    char piece;
    char capturedPiece;
    char promotedPiece;  // '\0' if no promotion
    bool isWhiteMove;
    bool valid;
};

class ChessPGN {
private:
    MoveEntry moveHistory[MAX_MOVE_HISTORY];
    int moveCount;
    char board[8][8];  // Current board state for PGN generation
    
    // Convert move to PGN notation
    String moveToPGN(int fromRow, int fromCol, int toRow, int toCol, char piece, char capturedPiece, char promotedPiece, const char board[8][8]);
    String getPieceSymbol(char piece);
    bool isAmbiguousMove(int fromRow, int fromCol, int toRow, int toCol, char piece, const char board[8][8]);
    String getDisambiguation(int fromRow, int fromCol, int toRow, int toCol, char piece, const char board[8][8]);
    
public:
    ChessPGN();
    void reset();
    void addMove(int fromRow, int fromCol, int toRow, int toCol, char piece, char capturedPiece, char promotedPiece, bool isWhiteMove, const char board[8][8]);
    bool undoLastMove(char board[8][8]);
    String getPGN();
    String getMoveHistory();
    int getMoveCount() { return moveCount; }
    bool canUndo() { return moveCount > 0; }
    void updateBoardState(const char newBoard[8][8]);
};

#endif // CHESS_PGN_H

