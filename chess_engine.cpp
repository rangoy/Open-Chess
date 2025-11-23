#include "chess_engine.h"
#include <Arduino.h>

// ---------------------------
// ChessEngine Implementation
// ---------------------------

ChessEngine::ChessEngine() {
    // Constructor - nothing to initialize for now
}

// Main move generation function (with optional game state)
void ChessEngine::getPossibleMoves(const char board[8][8], int row, int col, int &moveCount, int moves[][2], const GameState* gameState) {
    moveCount = 0;
    char piece = board[row][col];
    
    if (piece == ' ') return; // Empty square
    
    char pieceColor = getPieceColor(piece);
    
    // Convert to uppercase for easier comparison
    piece = (piece >= 'a' && piece <= 'z') ? piece - 32 : piece;

    switch(piece) {
        case 'P': // Pawn
            addPawnMoves(board, row, col, pieceColor, moveCount, moves, gameState);
            break;
        case 'R': // Rook
            addRookMoves(board, row, col, pieceColor, moveCount, moves);
            break;
        case 'N': // Knight
            addKnightMoves(board, row, col, pieceColor, moveCount, moves);
            break;
        case 'B': // Bishop
            addBishopMoves(board, row, col, pieceColor, moveCount, moves);
            break;
        case 'Q': // Queen
            addQueenMoves(board, row, col, pieceColor, moveCount, moves);
            break;
        case 'K': // King
            addKingMoves(board, row, col, pieceColor, moveCount, moves, gameState);
            break;
    }
    
    // Filter out moves that would leave king in check
    if (gameState != nullptr) {
        int filteredCount = 0;
        int filteredMoves[28][2];
        
        for (int i = 0; i < moveCount; i++) {
            if (!wouldMoveLeaveKingInCheck(board, row, col, moves[i][0], moves[i][1], pieceColor, gameState)) {
                filteredMoves[filteredCount][0] = moves[i][0];
                filteredMoves[filteredCount][1] = moves[i][1];
                filteredCount++;
            }
        }
        
        // Copy filtered moves back
        moveCount = filteredCount;
        for (int i = 0; i < moveCount; i++) {
            moves[i][0] = filteredMoves[i][0];
            moves[i][1] = filteredMoves[i][1];
        }
    }
}

// Pawn move generation (with en passant support)
void ChessEngine::addPawnMoves(const char board[8][8], int row, int col, char pieceColor, int &moveCount, int moves[][2], const GameState* gameState) {
    int direction = (pieceColor == 'w') ? 1 : -1;
    
    // One square forward
    if (isValidSquare(row + direction, col) && isSquareEmpty(board, row + direction, col)) {
        moves[moveCount][0] = row + direction;
        moves[moveCount][1] = col;
        moveCount++;
        
        // Initial two-square move
        if ((pieceColor == 'w' && row == 1) || (pieceColor == 'b' && row == 6)) {
            if (isSquareEmpty(board, row + 2*direction, col)) {
                moves[moveCount][0] = row + 2*direction;
                moves[moveCount][1] = col;
                moveCount++;
            }
        }
    }
    
    // Diagonal captures
    int captureColumns[] = {col-1, col+1};
    for (int i = 0; i < 2; i++) {
        int captureRow = row + direction;
        int captureCol = captureColumns[i];
        
        if (isValidSquare(captureRow, captureCol)) {
            if (isSquareOccupiedByOpponent(board, captureRow, captureCol, pieceColor)) {
                moves[moveCount][0] = captureRow;
                moves[moveCount][1] = captureCol;
                moveCount++;
            }
            // En passant capture
            else if (gameState != nullptr && gameState->enPassantRow == captureRow && gameState->enPassantCol == captureCol) {
                // Check if en passant is available for this pawn
                if (pieceColor == 'w' && row == 4 && captureRow == 5) {
                    moves[moveCount][0] = captureRow;
                    moves[moveCount][1] = captureCol;
                    moveCount++;
                } else if (pieceColor == 'b' && row == 3 && captureRow == 2) {
                    moves[moveCount][0] = captureRow;
                    moves[moveCount][1] = captureCol;
                    moveCount++;
                }
            }
        }
    }
}

// Rook move generation
void ChessEngine::addRookMoves(const char board[8][8], int row, int col, char pieceColor, int &moveCount, int moves[][2]) {
    int directions[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
    
    for (int d = 0; d < 4; d++) {
        for (int step = 1; step < 8; step++) {
            int newRow = row + step * directions[d][0];
            int newCol = col + step * directions[d][1];
            
            if (!isValidSquare(newRow, newCol)) break;
            
            if (isSquareEmpty(board, newRow, newCol)) {
                moves[moveCount][0] = newRow;
                moves[moveCount][1] = newCol;
                moveCount++;
            } else {
                // Check if it's a capturable piece
                if (isSquareOccupiedByOpponent(board, newRow, newCol, pieceColor)) {
                    moves[moveCount][0] = newRow;
                    moves[moveCount][1] = newCol;
                    moveCount++;
                }
                break; // Can't move past any piece
            }
        }
    }
}

// Knight move generation
void ChessEngine::addKnightMoves(const char board[8][8], int row, int col, char pieceColor, int &moveCount, int moves[][2]) {
    int knightMoves[8][2] = {{2,1}, {1,2}, {-1,2}, {-2,1},
                             {-2,-1}, {-1,-2}, {1,-2}, {2,-1}};
    
    for (int i = 0; i < 8; i++) {
        int newRow = row + knightMoves[i][0];
        int newCol = col + knightMoves[i][1];
        
        if (isValidSquare(newRow, newCol)) {
            if (isSquareEmpty(board, newRow, newCol) || 
                isSquareOccupiedByOpponent(board, newRow, newCol, pieceColor)) {
                moves[moveCount][0] = newRow;
                moves[moveCount][1] = newCol;
                moveCount++;
            }
        }
    }
}

// Bishop move generation
void ChessEngine::addBishopMoves(const char board[8][8], int row, int col, char pieceColor, int &moveCount, int moves[][2]) {
    int directions[4][2] = {{1,1}, {1,-1}, {-1,1}, {-1,-1}};
    
    for (int d = 0; d < 4; d++) {
        for (int step = 1; step < 8; step++) {
            int newRow = row + step * directions[d][0];
            int newCol = col + step * directions[d][1];
            
            if (!isValidSquare(newRow, newCol)) break;
            
            if (isSquareEmpty(board, newRow, newCol)) {
                moves[moveCount][0] = newRow;
                moves[moveCount][1] = newCol;
                moveCount++;
            } else {
                // Check if it's a capturable piece
                if (isSquareOccupiedByOpponent(board, newRow, newCol, pieceColor)) {
                    moves[moveCount][0] = newRow;
                    moves[moveCount][1] = newCol;
                    moveCount++;
                }
                break; // Can't move past any piece
            }
        }
    }
}

// Queen move generation (combination of rook and bishop)
void ChessEngine::addQueenMoves(const char board[8][8], int row, int col, char pieceColor, int &moveCount, int moves[][2]) {
    addRookMoves(board, row, col, pieceColor, moveCount, moves);
    addBishopMoves(board, row, col, pieceColor, moveCount, moves);
}

// King move generation (with castling support)
void ChessEngine::addKingMoves(const char board[8][8], int row, int col, char pieceColor, int &moveCount, int moves[][2], const GameState* gameState) {
    int kingMoves[8][2] = {{1,0}, {-1,0}, {0,1}, {0,-1},
                           {1,1}, {1,-1}, {-1,1}, {-1,-1}};
    
    for (int i = 0; i < 8; i++) {
        int newRow = row + kingMoves[i][0];
        int newCol = col + kingMoves[i][1];
        
        if (isValidSquare(newRow, newCol)) {
            if (isSquareEmpty(board, newRow, newCol) || 
                isSquareOccupiedByOpponent(board, newRow, newCol, pieceColor)) {
                moves[moveCount][0] = newRow;
                moves[moveCount][1] = newCol;
                moveCount++;
            }
        }
    }
    
    // Add castling moves if game state is provided
    if (gameState != nullptr) {
        addCastlingMoves(board, row, col, pieceColor, moveCount, moves, gameState);
    }
}

// Helper function to check if a square is occupied by an opponent piece
bool ChessEngine::isSquareOccupiedByOpponent(const char board[8][8], int row, int col, char pieceColor) {
    char targetPiece = board[row][col];
    if (targetPiece == ' ') return false;
    
    char targetColor = getPieceColor(targetPiece);
    return targetColor != pieceColor;
}

// Helper function to check if a square is empty
bool ChessEngine::isSquareEmpty(const char board[8][8], int row, int col) {
    return board[row][col] == ' ';
}

// Helper function to check if coordinates are within board bounds
bool ChessEngine::isValidSquare(int row, int col) {
    return row >= 0 && row < 8 && col >= 0 && col < 8;
}

// Helper function to get piece color
char ChessEngine::getPieceColor(char piece) {
    return (piece >= 'a' && piece <= 'z') ? 'b' : 'w';
}

// Move validation (checks if move is legal, including check constraints)
bool ChessEngine::isValidMove(const char board[8][8], int fromRow, int fromCol, int toRow, int toCol, const GameState* gameState) {
    int moveCount = 0;
    int moves[28][2]; // Maximum possible moves for a queen
    
    getPossibleMoves(board, fromRow, fromCol, moveCount, moves, gameState);
    
    for (int i = 0; i < moveCount; i++) {
        if (moves[i][0] == toRow && moves[i][1] == toCol) {
            return true;
        }
    }
    return false;
}

// Check if a pawn move results in promotion
// With corrected mapping: row 0 = rank 1, row 7 = rank 8
bool ChessEngine::isPawnPromotion(char piece, int targetRow) {
    if (piece == 'P' && targetRow == 7) return true;  // White pawn reaches 8th rank (row 7)
    if (piece == 'p' && targetRow == 0) return true;  // Black pawn reaches 1st rank (row 0)
    return false;
}

// Get the promoted piece (always queen for now)
char ChessEngine::getPromotedPiece(char piece) {
    return (piece == 'P') ? 'Q' : 'q';
}

// Utility function to print a move in readable format
void ChessEngine::printMove(int fromRow, int fromCol, int toRow, int toCol) {
    // Columns are reversed: file = 'a' + (7 - col)
    // Rows are reversed: rank = 1 + row
    Serial.print((char)('a' + (7 - fromCol)));
    Serial.print(1 + fromRow);
    Serial.print(" to ");
    Serial.print((char)('a' + (7 - toCol)));
    Serial.println(1 + toRow);
}

// Convert algebraic notation file (a-h) to column index (0-7)
// Columns are reversed: col = 7 - (file - 'a')
char ChessEngine::algebraicToCol(char file) {
    return 7 - (file - 'a');
}

// Convert algebraic notation rank (1-8) to row index (0-7)
// Rows are reversed: row = rank - 1 (formula same, but row 0 = rank 1 now)
int ChessEngine::algebraicToRow(int rank) {
    return rank - 1;
}

// ============================================
// Advanced Chess Rules Implementation
// ============================================

// Check if a square is attacked by opponent pieces
bool ChessEngine::isSquareAttacked(const char board[8][8], int row, int col, char attackingColor) {
    // Check all opponent pieces and see if they can attack this square
    // We use nullptr for gameState to avoid filtering moves (we want all possible attacks)
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            char piece = board[r][c];
            if (piece == ' ') continue;
            
            char pieceColor = getPieceColor(piece);
            if (pieceColor != attackingColor) continue;
            
            // Check if this piece can attack the target square
            // Use nullptr gameState to get all possible moves without filtering
            int moveCount = 0;
            int moves[28][2];
            getPossibleMoves(board, r, c, moveCount, moves, nullptr);
            
            for (int i = 0; i < moveCount; i++) {
                if (moves[i][0] == row && moves[i][1] == col) {
                    return true;
                }
            }
        }
    }
    return false;
}

// Find king position on the board
void ChessEngine::findKingPosition(const char board[8][8], char kingColor, int &kingRow, int &kingCol) {
    char kingPiece = (kingColor == 'w') ? 'K' : 'k';
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (board[r][c] == kingPiece) {
                kingRow = r;
                kingCol = c;
                return;
            }
        }
    }
    // King not found (shouldn't happen in valid position)
    kingRow = -1;
    kingCol = -1;
}

// Check if a specific color's king is in check
bool ChessEngine::isKingInCheck(const char board[8][8], char kingColor) {
    int kingRow, kingCol;
    findKingPosition(board, kingColor, kingRow, kingCol);
    
    if (kingRow == -1 || kingCol == -1) return false; // King not found
    
    char opponentColor = (kingColor == 'w') ? 'b' : 'w';
    return isSquareAttacked(board, kingRow, kingCol, opponentColor);
}

// Check if a move would leave the king in check
bool ChessEngine::wouldMoveLeaveKingInCheck(const char board[8][8], int fromRow, int fromCol, int toRow, int toCol, char pieceColor, const GameState* gameState) {
    char piece = board[fromRow][fromCol];
    char pieceType = (piece >= 'a' && piece <= 'z') ? piece - 32 : piece;
    
    // Handle castling moves specially - they're already validated in addCastlingMoves
    if (pieceType == 'K' && gameState != nullptr) {
        if (isCastlingMove(fromRow, fromCol, toRow, toCol, piece)) {
            // Castling moves are already validated for legality in addCastlingMoves
            // They check that king is not in check and squares are not attacked
            // So if we get here, the castling move is legal and doesn't leave king in check
            return false;
        }
    }
    
    // Handle en passant moves specially
    if (pieceType == 'P' && gameState != nullptr) {
        if (isEnPassantMove(fromRow, fromCol, toRow, toCol, piece, gameState)) {
            // Create a temporary board with the en passant move made
            char tempBoard[8][8];
            for (int r = 0; r < 8; r++) {
                for (int c = 0; c < 8; c++) {
                    tempBoard[r][c] = board[r][c];
                }
            }
            
            // Make the en passant move
            tempBoard[toRow][toCol] = piece;
            tempBoard[fromRow][fromCol] = ' ';
            
            // Remove the captured pawn
            int capturedPawnRow, capturedPawnCol;
            executeEnPassant(fromRow, fromCol, toRow, toCol, pieceColor, capturedPawnRow, capturedPawnCol);
            tempBoard[capturedPawnRow][capturedPawnCol] = ' ';
            
            return isKingInCheck(tempBoard, pieceColor);
        }
    }
    
    // Regular moves
    // Create a temporary board with the move made
    char tempBoard[8][8];
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            tempBoard[r][c] = board[r][c];
        }
    }
    
    // Make the move on temporary board
    tempBoard[toRow][toCol] = piece;
    tempBoard[fromRow][fromCol] = ' ';
    
    // Check if king is in check after the move
    return isKingInCheck(tempBoard, pieceColor);
}

// Add castling moves to the move list
void ChessEngine::addCastlingMoves(const char board[8][8], int row, int col, char pieceColor, int &moveCount, int moves[][2], const GameState* gameState) {
    // King must be on starting square
    // Note: Columns are reversed - col 0 = h, col 7 = a
    // So e1 is at col 3, e8 is at col 3
    if (pieceColor == 'w') {
        if (row != 0 || col != 3) return; // White king starts at e1 (row 0, col 3)
        
        // Kingside castling (O-O)
        // King at e1 (col 3), moves to g1 (col 1), rook at h1 (col 0) moves to f1 (col 2)
        if (gameState->whiteCanCastleKingside) {
            // Check if squares between king and rook are empty (f1=col 2, g1=col 1)
            if (board[0][2] == ' ' && board[0][1] == ' ') {
                // Check if rook is in place (h1 = col 0)
                if (board[0][0] == 'R') {
                    // Check if king is not in check and squares are not attacked
                    if (!isKingInCheck(board, 'w') && 
                        !isSquareAttacked(board, 0, 2, 'b') && 
                        !isSquareAttacked(board, 0, 1, 'b')) {
                        moves[moveCount][0] = 0;
                        moves[moveCount][1] = 1; // g1
                        moveCount++;
                        Serial.println("White kingside castling added to moves");
                    }
                }
            }
        }
        
        // Queenside castling (O-O-O)
        // King at e1 (col 3), moves to c1 (col 5), rook at a1 (col 7) moves to d1 (col 4)
        if (gameState->whiteCanCastleQueenside) {
            // Check if squares between king and rook are empty (d1=col 4, c1=col 5, b1=col 6)
            if (board[0][4] == ' ' && board[0][5] == ' ' && board[0][6] == ' ') {
                // Check if rook is in place (a1 = col 7)
                if (board[0][7] == 'R') {
                    // Check if king is not in check and squares are not attacked
                    if (!isKingInCheck(board, 'w') && 
                        !isSquareAttacked(board, 0, 4, 'b') && 
                        !isSquareAttacked(board, 0, 5, 'b')) {
                        moves[moveCount][0] = 0;
                        moves[moveCount][1] = 5; // c1
                        moveCount++;
                        Serial.println("White queenside castling added to moves");
                    }
                }
            }
        }
    } else { // Black
        if (row != 7 || col != 3) return; // Black king starts at e8 (row 7, col 3)
        
        // Kingside castling (O-O)
        // King at e8 (col 3), moves to g8 (col 1), rook at h8 (col 0) moves to f8 (col 2)
        if (gameState->blackCanCastleKingside) {
            // Check if squares between king and rook are empty (f8=col 2, g8=col 1)
            if (board[7][2] == ' ' && board[7][1] == ' ') {
                // Check if rook is in place (h8 = col 0)
                if (board[7][0] == 'r') {
                    // Check if king is not in check and squares are not attacked
                    if (!isKingInCheck(board, 'b') && 
                        !isSquareAttacked(board, 7, 2, 'w') && 
                        !isSquareAttacked(board, 7, 1, 'w')) {
                        moves[moveCount][0] = 7;
                        moves[moveCount][1] = 1; // g8
                        moveCount++;
                        Serial.println("Black kingside castling added to moves");
                    }
                }
            }
        }
        
        // Queenside castling (O-O-O)
        // King at e8 (col 3), moves to c8 (col 5), rook at a8 (col 7) moves to d8 (col 4)
        if (gameState->blackCanCastleQueenside) {
            // Check if squares between king and rook are empty (d8=col 4, c8=col 5, b8=col 6)
            if (board[7][4] == ' ' && board[7][5] == ' ' && board[7][6] == ' ') {
                // Check if rook is in place (a8 = col 7)
                if (board[7][7] == 'r') {
                    // Check if king is not in check and squares are not attacked
                    if (!isKingInCheck(board, 'b') && 
                        !isSquareAttacked(board, 7, 4, 'w') && 
                        !isSquareAttacked(board, 7, 5, 'w')) {
                        moves[moveCount][0] = 7;
                        moves[moveCount][1] = 5; // c8
                        moveCount++;
                        Serial.println("Black queenside castling added to moves");
                    }
                }
            }
        }
    }
}

// Check if a move is a castling move
bool ChessEngine::isCastlingMove(int fromRow, int fromCol, int toRow, int toCol, char piece) {
    if (piece != 'K' && piece != 'k') return false;
    
    char pieceColor = getPieceColor(piece);
    
    if (pieceColor == 'w') {
        // White castling: king from e1 (0,3) to g1 (0,1) or c1 (0,5)
        return (fromRow == 0 && fromCol == 3) && 
               (toRow == 0 && (toCol == 1 || toCol == 5));
    } else {
        // Black castling: king from e8 (7,3) to g8 (7,1) or c8 (7,5)
        return (fromRow == 7 && fromCol == 3) && 
               (toRow == 7 && (toCol == 1 || toCol == 5));
    }
}

// Execute castling move (returns rook positions)
// Note: Columns are reversed - col 0 = h, col 7 = a
void ChessEngine::executeCastling(const char board[8][8], int kingFromRow, int kingFromCol, int kingToRow, int kingToCol, 
                                   int &rookFromRow, int &rookFromCol, int &rookToRow, int &rookToCol) {
    if (kingToCol == 1) { // Kingside (g1/g8)
        rookFromRow = kingFromRow;
        rookFromCol = 0; // h1/h8
        rookToRow = kingFromRow;
        rookToCol = 2; // f1/f8
    } else { // Queenside (c1/c8, toCol == 5)
        rookFromRow = kingFromRow;
        rookFromCol = 7; // a1/a8
        rookToRow = kingFromRow;
        rookToCol = 4; // d1/d8
    }
}

// Get rook position for a castling move (for visual indication)
// Note: Columns are reversed - col 0 = h, col 7 = a
bool ChessEngine::getCastlingRookPosition(int kingFromRow, int kingFromCol, int kingToRow, int kingToCol, 
                                           int &rookRow, int &rookCol) {
    // Check if this looks like a castling move based on coordinates
    // White: from (0,3) to (0,1) or (0,5) - e1 to g1 or c1
    // Black: from (7,3) to (7,1) or (7,5) - e8 to g8 or c8
    bool isWhiteCastling = (kingFromRow == 0 && kingFromCol == 3) && (kingToRow == 0 && (kingToCol == 1 || kingToCol == 5));
    bool isBlackCastling = (kingFromRow == 7 && kingFromCol == 3) && (kingToRow == 7 && (kingToCol == 1 || kingToCol == 5));
    
    if (!isWhiteCastling && !isBlackCastling) {
        return false;
    }
    
    rookRow = kingFromRow;
    if (kingToCol == 1) { // Kingside (g1/g8) - rook at h1/h8 (col 0)
        rookCol = 0;
    } else if (kingToCol == 5) { // Queenside (c1/c8) - rook at a1/a8 (col 7)
        rookCol = 7;
    } else {
        return false; // Not a valid castling destination
    }
    return true;
}

// Check if a move is an en passant move
bool ChessEngine::isEnPassantMove(int fromRow, int fromCol, int toRow, int toCol, char piece, const GameState* gameState) {
    if (gameState == nullptr) return false;
    if (piece != 'P' && piece != 'p') return false;
    
    // En passant target must be set
    if (gameState->enPassantRow == -1 || gameState->enPassantCol == -1) return false;
    
    // Check if destination matches en passant target
    return (toRow == gameState->enPassantRow && toCol == gameState->enPassantCol);
}

// Execute en passant move (returns captured pawn position)
void ChessEngine::executeEnPassant(int fromRow, int fromCol, int toRow, int toCol, char pieceColor, 
                                    int &capturedPawnRow, int &capturedPawnCol) {
    // The captured pawn is on the same rank as the moving pawn, same file as destination
    capturedPawnRow = fromRow; // Same rank as starting position
    capturedPawnCol = toCol;   // Same file as destination
}

// Update game state after a move
void ChessEngine::updateGameStateAfterMove(const char board[8][8], int fromRow, int fromCol, int toRow, int toCol, 
                                            char piece, char capturedPiece, GameState &gameState) {
    char pieceColor = getPieceColor(piece);
    char pieceType = (piece >= 'a' && piece <= 'z') ? piece - 32 : piece;
    
    // Update castling rights
    if (pieceType == 'K') {
        // King moved - lose all castling rights
        if (pieceColor == 'w') {
            gameState.whiteCanCastleKingside = false;
            gameState.whiteCanCastleQueenside = false;
        } else {
            gameState.blackCanCastleKingside = false;
            gameState.blackCanCastleQueenside = false;
        }
    } else if (pieceType == 'R') {
        // Rook moved - lose castling right on that side
        // Note: Columns are reversed - col 0 = h, col 7 = a
        if (pieceColor == 'w') {
            if (fromRow == 0 && fromCol == 0) gameState.whiteCanCastleKingside = false; // h1
            if (fromRow == 0 && fromCol == 7) gameState.whiteCanCastleQueenside = false; // a1
        } else {
            if (fromRow == 7 && fromCol == 0) gameState.blackCanCastleKingside = false; // h8
            if (fromRow == 7 && fromCol == 7) gameState.blackCanCastleQueenside = false; // a8
        }
    }
    
    // Rook captured - lose castling right on that side
    // Note: Columns are reversed - col 0 = h, col 7 = a
    if (capturedPiece != ' ' && capturedPiece != '\0') {
        if (capturedPiece == 'R' && toRow == 0 && toCol == 0) gameState.whiteCanCastleKingside = false; // h1
        if (capturedPiece == 'R' && toRow == 0 && toCol == 7) gameState.whiteCanCastleQueenside = false; // a1
        if (capturedPiece == 'r' && toRow == 7 && toCol == 0) gameState.blackCanCastleKingside = false; // h8
        if (capturedPiece == 'r' && toRow == 7 && toCol == 7) gameState.blackCanCastleQueenside = false; // a8
    }
    
    // Update en passant target
    gameState.enPassantRow = -1;
    gameState.enPassantCol = -1;
    
    if (pieceType == 'P') {
        // Check if this is a two-square pawn move
        if ((pieceColor == 'w' && fromRow == 1 && toRow == 3) ||
            (pieceColor == 'b' && fromRow == 6 && toRow == 4)) {
            // Set en passant target square (the square behind the pawn)
            gameState.enPassantRow = (pieceColor == 'w') ? 2 : 5;
            gameState.enPassantCol = toCol;
        }
    }
    
    // Update halfmove clock
    // Reset to 0 if pawn move or capture, otherwise increment
    if (pieceType == 'P' || (capturedPiece != ' ' && capturedPiece != '\0')) {
        gameState.halfmoveClock = 0;
    } else {
        gameState.halfmoveClock++;
    }
    
    // Update fullmove number (increments after black's move)
    if (pieceColor == 'b') {
        gameState.fullmoveNumber++;
    }
    
    // Toggle turn
    gameState.isWhiteTurn = !gameState.isWhiteTurn;
}

// Check if a specific color is in check
bool ChessEngine::isInCheck(const char board[8][8], char color, const GameState* gameState) {
    return isKingInCheck(board, color);
}

// Check game result (check, checkmate, stalemate, etc.)
GameResult ChessEngine::getGameResult(const char board[8][8], const GameState* gameState) {
    if (gameState == nullptr) return GAME_CONTINUING;
    
    char currentColor = gameState->isWhiteTurn ? 'w' : 'b';
    bool inCheck = isInCheck(board, currentColor, gameState);
    
    // Generate all possible moves for current player
    int totalMoves = 0;
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            char piece = board[r][c];
            if (piece == ' ') continue;
            char pieceColor = getPieceColor(piece);
            if (pieceColor != currentColor) continue;
            
            int moveCount = 0;
            int moves[28][2];
            getPossibleMoves(board, r, c, moveCount, moves, gameState);
            totalMoves += moveCount;
        }
    }
    
    if (totalMoves == 0) {
        if (inCheck) {
            return GAME_CHECKMATE;
        } else {
            return GAME_STALEMATE;
        }
    } else if (inCheck) {
        return GAME_CHECK;
    }
    
    return GAME_CONTINUING;
}