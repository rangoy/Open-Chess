// Initialize board view page
function initBoardView() {
    // Create empty board squares
    const board = document.getElementById('chess-board');
    board.innerHTML = '';
    for (let row = 0; row < 8; row++) {
        for (let col = 0; col < 8; col++) {
            const isLight = (row + col) % 2 === 0;
            const square = document.createElement('div');
            square.className = 'square ' + (isLight ? 'light' : 'dark');
            square.id = 'square-' + row + '-' + col;
            board.appendChild(square);
        }
    }
    // Initial board update
    updateBoard();
}

// Update board display from JSON
function updateBoardDisplay(data) {
    const statusEl = document.getElementById('board-status');
    const containerEl = document.getElementById('board-container');
    const noBoardEl = document.getElementById('no-board-message');

    if (data.valid) {
        statusEl.textContent = 'Board state: Active';
        containerEl.style.display = 'block';
        noBoardEl.style.display = 'none';

        // Update board squares - resolve symbols in JavaScript
        for (let row = 0; row < 8; row++) {
            for (let col = 0; col < 8; col++) {
                const piece = data.board[row][col];
                const square = document.getElementById('square-' + row + '-' + col);
                if (piece && piece !== '') {
                    const isWhite = piece === piece.toUpperCase();
                    const symbol = getPieceSymbol(piece);
                    square.innerHTML = '<span class="piece ' + (isWhite ? 'white' : 'black') + '">' + symbol + '</span>';
                } else {
                    square.innerHTML = '';
                }
            }
        }
    } else {
        statusEl.textContent = 'Board state: Not available';
        containerEl.style.display = 'none';
        noBoardEl.style.display = 'block';
    }
}

// Fetch and update board state
function updateBoard() {
    fetch('/board')
        .then(response => response.json())
        .then(data => {
            updateBoardDisplay(data);
            // Update evaluation bar
            if (data.evaluation !== undefined) {
                const evalValue = data.evaluation;
                const evalInPawns = (evalValue / 100).toFixed(2);
                const maxEval = 1000;
                const clampedEval = Math.max(-maxEval, Math.min(maxEval, evalValue));
                const percentage = Math.abs(clampedEval) / maxEval * 50;
                const bar = document.getElementById('eval-bar');
                const arrow = document.getElementById('eval-arrow');
                const text = document.getElementById('eval-text');
                let evalText = '';
                let arrowSymbol = '⬌';
                if (evalValue > 0) {
                    bar.style.left = '50%';
                    bar.style.width = percentage + '%';
                    bar.style.background = 'linear-gradient(to right, #ec8703 0%, #4CAF50 100%)';
                    arrow.style.left = (50 + percentage) + '%';
                    arrowSymbol = '→';
                    arrow.style.color = '#4CAF50';
                    evalText = '+' + evalInPawns + ' (White advantage)';
                } else if (evalValue < 0) {
                    bar.style.left = (50 - percentage) + '%';
                    bar.style.width = percentage + '%';
                    bar.style.background = 'linear-gradient(to right, #F44336 0%, #ec8703 100%)';
                    arrow.style.left = (50 - percentage) + '%';
                    arrowSymbol = '←';
                    arrow.style.color = '#F44336';
                    evalText = evalInPawns + ' (Black advantage)';
                } else {
                    bar.style.left = '50%';
                    bar.style.width = '0%';
                    bar.style.background = '#ec8703';
                    arrow.style.left = '50%';
                    arrowSymbol = '⬌';
                    arrow.style.color = '#ec8703';
                    evalText = '0.00 (Equal)';
                }
                arrow.textContent = arrowSymbol;
                text.textContent = evalText;
                text.style.color = arrow.style.color;
            }
            
            // Update PGN display
            const pgnSection = document.getElementById('pgn-section');
            const pgnDisplay = document.getElementById('pgn-display');
            if (data.pgn !== undefined && data.pgn && data.pgn.length > 0) {
                pgnSection.style.display = 'block';
                pgnDisplay.textContent = data.pgn;
            } else {
                pgnSection.style.display = 'none';
            }
        })
        .catch(error => {
            console.error('Error fetching board state:', error);
            document.getElementById('board-status').textContent = 'Error loading board state';
        });
}

// Initialize on page load
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initBoardView);
} else {
    initBoardView();
}

// Auto-refresh every 2 seconds
setInterval(updateBoard, 2000);

