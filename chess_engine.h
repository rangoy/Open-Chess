#ifndef CHESS_ENGINE_H
#define CHESS_ENGINE_H

// ---------------------------
// Chess Engine Class
// ---------------------------

// Game state structure for castling rights and en passant
struct GameState {
    bool whiteCanCastleKingside;
    bool whiteCanCastleQueenside;
    bool blackCanCastleKingside;
    bool blackCanCastleQueenside;
    int enPassantRow;  // -1 if no en passant available
    int enPassantCol;  // -1 if no en passant available
    bool isWhiteTurn;
    int halfmoveClock;  // Number of halfmoves since last capture or pawn move
    int fullmoveNumber;  // Fullmove counter (increments after black's move)
    
    GameState() {
        whiteCanCastleKingside = true;
        whiteCanCastleQueenside = true;
        blackCanCastleKingside = true;
        blackCanCastleQueenside = true;
        enPassantRow = -1;
        enPassantCol = -1;
        isWhiteTurn = true;
        halfmoveClock = 0;
        fullmoveNumber = 1;
    }
};

// Game result enumeration
enum GameResult {
    GAME_CONTINUING = 0,
    GAME_CHECK = 1,
    GAME_CHECKMATE = 2,
    GAME_STALEMATE = 3,
    GAME_DRAW = 4
};

class ChessEngine {
private:
    // Helper functions for move generation
    void addPawnMoves(const char board[8][8], int row, int col, char pieceColor, int &moveCount, int moves[][2], const GameState* gameState = nullptr);
    void addRookMoves(const char board[8][8], int row, int col, char pieceColor, int &moveCount, int moves[][2]);
    void addKnightMoves(const char board[8][8], int row, int col, char pieceColor, int &moveCount, int moves[][2]);
    void addBishopMoves(const char board[8][8], int row, int col, char pieceColor, int &moveCount, int moves[][2]);
    void addQueenMoves(const char board[8][8], int row, int col, char pieceColor, int &moveCount, int moves[][2]);
    void addKingMoves(const char board[8][8], int row, int col, char pieceColor, int &moveCount, int moves[][2], const GameState* gameState = nullptr);
    void addCastlingMoves(const char board[8][8], int row, int col, char pieceColor, int &moveCount, int moves[][2], const GameState* gameState);
    
    bool isSquareOccupiedByOpponent(const char board[8][8], int row, int col, char pieceColor);
    bool isSquareEmpty(const char board[8][8], int row, int col);
    bool isValidSquare(int row, int col);
    char getPieceColor(char piece);
    
    // Check detection functions
    bool isSquareAttacked(const char board[8][8], int row, int col, char attackingColor);
    bool isKingInCheck(const char board[8][8], char kingColor);
    void findKingPosition(const char board[8][8], char kingColor, int &kingRow, int &kingCol);
    
    // Move filtering
    bool wouldMoveLeaveKingInCheck(const char board[8][8], int fromRow, int fromCol, int toRow, int toCol, char pieceColor, const GameState* gameState = nullptr);

public:
    ChessEngine();
    
    // Main move generation function (with optional game state)
    void getPossibleMoves(const char board[8][8], int row, int col, int &moveCount, int moves[][2], const GameState* gameState = nullptr);
    
    // Move validation (checks if move is legal, including check constraints)
    bool isValidMove(const char board[8][8], int fromRow, int fromCol, int toRow, int toCol, const GameState* gameState = nullptr);
    
    // Check if a move is a castling move
    bool isCastlingMove(int fromRow, int fromCol, int toRow, int toCol, char piece);
    
    // Check if a move is an en passant move
    bool isEnPassantMove(int fromRow, int fromCol, int toRow, int toCol, char piece, const GameState* gameState);
    
    // Execute castling move (returns rook from/to positions)
    void executeCastling(const char board[8][8], int kingFromRow, int kingFromCol, int kingToRow, int kingToCol, 
                         int &rookFromRow, int &rookFromCol, int &rookToRow, int &rookToCol);
    
    // Get rook position for a castling move (for visual indication)
    bool getCastlingRookPosition(int kingFromRow, int kingFromCol, int kingToRow, int kingToCol, 
                                  int &rookRow, int &rookCol);
    
    // Execute en passant move (returns captured pawn position)
    void executeEnPassant(int fromRow, int fromCol, int toRow, int toCol, char pieceColor, 
                          int &capturedPawnRow, int &capturedPawnCol);
    
    // Update game state after a move
    void updateGameStateAfterMove(const char board[8][8], int fromRow, int fromCol, int toRow, int toCol, 
                                  char piece, char capturedPiece, GameState &gameState);
    
    // Check game result (check, checkmate, stalemate, etc.)
    GameResult getGameResult(const char board[8][8], const GameState* gameState);
    
    // Check if a specific color is in check
    bool isInCheck(const char board[8][8], char color, const GameState* gameState = nullptr);
    
    // Game state checks
    bool isPawnPromotion(char piece, int targetRow);
    char getPromotedPiece(char piece);
    
    // Utility functions
    void printMove(int fromRow, int fromCol, int toRow, int toCol);
    char algebraicToCol(char file);
    int algebraicToRow(int rank);
};

#endif // CHESS_ENGINE_H