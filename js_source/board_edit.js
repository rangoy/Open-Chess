// Initialize board edit page
function initBoardEdit() {
// Fetch current board state
fetch('/board')
.then(response => response.json())
.then(data => {
if (data.valid) {
renderEditBoard(data.board);
} else {
// Empty board
renderEditBoard(Array(8).fill(null).map(() => Array(8).fill('')));
}
})
.catch(error => {
console.error('Error fetching board state:', error);
renderEditBoard(Array(8).fill(null).map(() => Array(8).fill('')));
});
}

// Render editable board
function renderEditBoard(board) {
const boardEl = document.getElementById('chess-board');
boardEl.innerHTML = '';

// Piece symbols for options (rendered in HTML)
const pieceOptions = [
{ value: '', label: '' },
{ value: 'R', label: '♖ R' },
{ value: 'N', label: '♘ N' },
{ value: 'B', label: '♗ B' },
{ value: 'Q', label: '♕ Q' },
{ value: 'K', label: '♔ K' },
{ value: 'P', label: '♙ P' },
{ value: 'r', label: '♜ r' },
{ value: 'n', label: '♞ n' },
{ value: 'b', label: '♝ b' },
{ value: 'q', label: '♛ q' },
{ value: 'k', label: '♚ k' },
{ value: 'p', label: '♟ p' }
];

for (let row = 0; row < 8; row++) {
for (let col = 0; col < 8; col++) {
const isLight = (row + col) % 2 === 0;
const square = document.createElement('div');
square.className = 'square ' + (isLight ? 'light' : 'dark');

const select = document.createElement('select');
select.name = 'r' + row + 'c' + col;
select.id = 'r' + row + 'c' + col;

// Get piece from JSON (simple string format)
const piece = board[row][col] || '';

pieceOptions.forEach(opt => {
const option = document.createElement('option');
option.value = opt.value;
option.textContent = opt.label;
if (opt.value === piece || (piece === ' ' && opt.value === '')) {
option.selected = true;
}
select.appendChild(option);
});

square.appendChild(select);
boardEl.appendChild(square);
}
}
}

// Initialize on page load
if (document.readyState === 'loading') {
document.addEventListener('DOMContentLoaded', initBoardEdit);
} else {
initBoardEdit();
}

