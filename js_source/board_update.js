// Fetch board state via AJAX for smoother updates
function updateBoard() {
fetch('/board')
.then(response => response.json())
.then(data => {
if (data.valid) {
// Update board display
const squares = document.querySelectorAll('.square');
let index = 0;
for (let row = 0; row < 8; row++) {
for (let col = 0; col < 8; col++) {
const piece = data.board[row][col];
const square = squares[index];
if (piece && piece !== '') {
const isWhite = piece === piece.toUpperCase();
square.innerHTML = '<span class="piece ' + (isWhite ? 'white' : 'black') + '">' + getPieceSymbol(piece) + '</span>';
} else {
square.innerHTML = '';
}
index++;
}
}
}
});
}

