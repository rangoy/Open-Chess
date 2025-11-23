#include "chess_pgn.h"
#include <Arduino.h>

ChessPGN::ChessPGN() {
    moveCount = 0;
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            board[row][col] = ' ';
        }
    }
}

void ChessPGN::reset() {
    moveCount = 0;
    for (int i = 0; i < MAX_MOVE_HISTORY; i++) {
        moveHistory[i].valid = false;
    }
}

void ChessPGN::updateBoardState(const char newBoard[8][8]) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            board[row][col] = newBoard[row][col];
        }
    }
}

String ChessPGN::getPieceSymbol(char piece) {
    switch (piece) {
        case 'R': case 'r': return "R";
        case 'N': case 'n': return "N";
        case 'B': case 'b': return "B";
        case 'Q': case 'q': return "Q";
        case 'K': case 'k': return "K";
        case 'P': case 'p': return "";
        default: return "";
    }
}

bool ChessPGN::isAmbiguousMove(int fromRow, int fromCol, int toRow, int toCol, char piece, const char board[8][8]) {
    // Check if there are other pieces of the same type that could move to the same square
    char pieceType = (piece >= 'a' && piece <= 'z') ? piece - 32 : piece;
    bool isWhite = (piece >= 'A' && piece <= 'Z');
    
    int count = 0;
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (row == fromRow && col == fromCol) continue;
            char bPiece = board[row][col];
            if (bPiece == ' ') continue;
            
            char bPieceType = (bPiece >= 'a' && bPiece <= 'z') ? bPiece - 32 : bPiece;
            bool bIsWhite = (bPiece >= 'A' && bPiece <= 'Z');
            
            if (bPieceType == pieceType && bIsWhite == isWhite) {
                // Check if this piece could also move to the target square
                // This is simplified - in a full implementation, we'd check actual move legality
                count++;
            }
        }
    }
    
    return count > 0;
}

String ChessPGN::getDisambiguation(int fromRow, int fromCol, int toRow, int toCol, char piece, const char board[8][8]) {
    // Simple disambiguation - use file (column) if ambiguous
    // In a full implementation, we'd check if file or rank disambiguation is needed
    // Columns are reversed: file = 'a' + (7 - col)
    return String((char)('a' + (7 - fromCol)));
}

String ChessPGN::moveToPGN(int fromRow, int fromCol, int toRow, int toCol, char piece, char capturedPiece, char promotedPiece, const char board[8][8]) {
    String pgn = "";
    
    char pieceType = (piece >= 'a' && piece <= 'z') ? piece - 32 : piece;
    bool isPawn = (pieceType == 'P');
    
    // Piece symbol (empty for pawns)
    if (!isPawn) {
        pgn += getPieceSymbol(piece);
    }
    
    // Disambiguation if needed (simplified)
    if (!isPawn && isAmbiguousMove(fromRow, fromCol, toRow, toCol, piece, board)) {
        pgn += getDisambiguation(fromRow, fromCol, toRow, toCol, piece, board);
    }
    
    // Capture indicator
    if (capturedPiece != ' ') {
        if (isPawn) {
            // Columns are reversed: file = 'a' + (7 - col)
            pgn += (char)('a' + (7 - fromCol));  // Source file for pawn captures
        }
        pgn += "x";
    }
    
    // Destination square
    // Columns are reversed: file = 'a' + (7 - col)
    // Rows are reversed: rank = 1 + row
    pgn += (char)('a' + (7 - toCol));
    pgn += (char)('0' + (1 + toRow));  // Convert row to rank (row 0 = rank 1, row 7 = rank 8)
    
    // Promotion
    if (promotedPiece != '\0' && promotedPiece != ' ') {
        pgn += "=";
        pgn += getPieceSymbol(promotedPiece);
    }
    
    return pgn;
}

void ChessPGN::addMove(int fromRow, int fromCol, int toRow, int toCol, char piece, char capturedPiece, char promotedPiece, bool isWhiteMove, const char board[8][8]) {
    if (moveCount >= MAX_MOVE_HISTORY) {
        // Shift history (remove oldest moves)
        for (int i = 0; i < MAX_MOVE_HISTORY - 1; i++) {
            moveHistory[i] = moveHistory[i + 1];
        }
        moveCount = MAX_MOVE_HISTORY - 1;
    }
    
    MoveEntry& entry = moveHistory[moveCount];
    entry.fromRow = fromRow;
    entry.fromCol = fromCol;
    entry.toRow = toRow;
    entry.toCol = toCol;
    entry.piece = piece;
    entry.capturedPiece = capturedPiece;
    entry.promotedPiece = promotedPiece;
    entry.isWhiteMove = isWhiteMove;
    entry.valid = true;
    
    moveCount++;
    
    // Update internal board state
    updateBoardState(board);
}

bool ChessPGN::undoLastMove(char board[8][8]) {
    if (moveCount == 0) return false;
    
    moveCount--;
    MoveEntry& entry = moveHistory[moveCount];
    
    if (!entry.valid) {
        moveCount++;
        return false;
    }
    
    // Handle promotion undo - if it was a promotion, restore original pawn
    if (entry.promotedPiece != '\0' && entry.promotedPiece != ' ') {
        // It was a promotion - restore the original pawn at the from position
        char originalPawn = entry.isWhiteMove ? 'P' : 'p';
        board[entry.fromRow][entry.fromCol] = originalPawn;
    } else {
        // Normal move - restore the piece at the from position
        board[entry.fromRow][entry.fromCol] = entry.piece;
    }
    
    // Restore what was at the destination (captured piece or empty)
    board[entry.toRow][entry.toCol] = entry.capturedPiece;
    
    entry.valid = false;
    return true;
}

String ChessPGN::getPGN() {
    String pgn = "";
    int moveNumber = 1;
    
    for (int i = 0; i < moveCount; i++) {
        MoveEntry& entry = moveHistory[i];
        if (!entry.valid) continue;
        
        if (entry.isWhiteMove) {
            if (moveNumber > 1) pgn += " ";
            pgn += String(moveNumber);
            pgn += ".";
        } else {
            pgn += " ";
        }
        
        // Create a temporary board state for PGN generation
        // We need to reconstruct the board state at the time of this move
        char tempBoard[8][8];
        // For simplicity, we'll use the current board state
        // A full implementation would reconstruct the board from the move history
        updateBoardState(board);
        
        pgn += moveToPGN(entry.fromRow, entry.fromCol, entry.toRow, entry.toCol, 
                        entry.piece, entry.capturedPiece, entry.promotedPiece, board);
        
        if (!entry.isWhiteMove) {
            moveNumber++;
        }
    }
    
    return pgn;
}

String ChessPGN::getMoveHistory() {
    String history = "";
    int moveNumber = 1;
    
    for (int i = 0; i < moveCount; i++) {
        MoveEntry& entry = moveHistory[i];
        if (!entry.valid) continue;
        
        if (entry.isWhiteMove) {
            if (moveNumber > 1) history += " ";
            history += String(moveNumber);
            history += ".";
        } else {
            history += " ";
        }
        
        // Simple move notation: from-to (e.g., "e2e4")
        // Columns are reversed: file = 'a' + (7 - col)
        // Rows are reversed: rank = 1 + row
        history += (char)('a' + (7 - entry.fromCol));
        history += (char)('0' + (1 + entry.fromRow));
        history += (char)('a' + (7 - entry.toCol));
        history += (char)('0' + (1 + entry.toRow));
        
        if (entry.promotedPiece != '\0' && entry.promotedPiece != ' ') {
            history += "=";
            history += getPieceSymbol(entry.promotedPiece);
        }
        
        if (!entry.isWhiteMove) {
            moveNumber++;
        }
    }
    
    return history;
}

