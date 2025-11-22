function getPieceSymbol(piece) {
if (!piece) return '';
const symbols = {
'R': '♖', 'N': '♘', 'B': '♗', 'Q': '♕', 'K': '♔', 'P': '♙',
'r': '♜', 'n': '♞', 'b': '♝', 'q': '♛', 'k': '♚', 'p': '♟'
};
return symbols[piece] || piece;
}

