// Initialize board edit page
let isPaused = false;

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

    // Setup pause toggle button
    const pauseToggle = document.getElementById('pause-toggle');
    if (pauseToggle) {
        pauseToggle.addEventListener('click', togglePause);
        // Check initial pause state
        checkPauseState();
    }

    // Setup form submission handler
    const boardForm = document.getElementById('boardForm');
    if (boardForm) {
        boardForm.addEventListener('submit', handleFormSubmit);
    }

    // Setup undo button
    const undoButton = document.getElementById('undo-button');
    if (undoButton) {
        undoButton.addEventListener('click', handleUndo);
    }
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


// Check current pause state from server
function checkPauseState() {
    fetch('/pause-moves')
        .then(response => response.json())
        .then(data => {
            isPaused = data.paused || false;
            updatePauseButton();
        })
        .catch(error => {
            console.error('Error checking pause state:', error);
        });
}

// Toggle pause state for move detection
function togglePause() {
    const newPausedState = !isPaused;
    const formData = new FormData();
    formData.append('paused', newPausedState ? 'true' : 'false');

    fetch('/pause-moves', {
        method: 'POST',
        body: formData
    })
        .then(response => response.json())
        .then(data => {
            isPaused = data.paused || false;
            updatePauseButton();
        })
        .catch(error => {
            console.error('Error toggling pause state:', error);
        });
}

// Update pause button appearance
function updatePauseButton() {
const pauseToggle = document.getElementById('pause-toggle');
const undoButton = document.getElementById('undo-button');
const statusEl = document.querySelector('.status');

if (pauseToggle) {
if (isPaused) {
pauseToggle.textContent = 'Resume Move Detection';
pauseToggle.classList.remove('secondary');
pauseToggle.classList.add('paused');
if (statusEl) {
statusEl.textContent = 'Move detection PAUSED - You can rearrange pieces freely. Click on any square to change the piece.';
}
// Show undo button when paused
if (undoButton) {
undoButton.style.display = 'inline-block';
}
} else {
pauseToggle.textContent = 'Pause Move Detection';
pauseToggle.classList.remove('paused');
pauseToggle.classList.add('secondary');
if (statusEl) {
statusEl.textContent = 'Click on any square to change the piece. Empty = no piece.';
}
// Hide undo button when not paused
if (undoButton) {
undoButton.style.display = 'none';
}
}
}
}

// Handle undo button click
function handleUndo() {
const undoButton = document.getElementById('undo-button');
if (undoButton) {
undoButton.disabled = true;
const originalText = undoButton.textContent;
undoButton.textContent = 'Undoing...';

console.log('Undo button clicked, sending request to /undo-move');

fetch('/undo-move', {
method: 'POST',
headers: {
'Content-Type': 'application/json'
}
})
.then(response => {
console.log('Undo response status:', response.status);
return response.json();
})
.then(data => {
console.log('Undo response data:', data);
if (data.success) {
// Show success message
const statusEl = document.querySelector('.status');
if (statusEl) {
statusEl.textContent = 'Move undone successfully! Reloading...';
statusEl.style.color = '#4CAF50';
}
// Reload the page to show updated board state
setTimeout(() => {
window.location.reload();
}, 1000);
} else {
alert('Failed to undo move: ' + (data.message || 'Unknown error'));
undoButton.disabled = false;
undoButton.textContent = originalText;
}
})
.catch(error => {
console.error('Error undoing move:', error);
alert('Error undoing move. Please try again.');
undoButton.disabled = false;
undoButton.textContent = originalText;
});
} else {
console.error('Undo button not found!');
}
}

// Handle form submission
function handleFormSubmit(event) {
    event.preventDefault(); // Prevent default form submission

    const form = event.target;

    // Collect board data into a 2D array
    const board = [];
    for (let row = 0; row < 8; row++) {
        const rowData = [];
        for (let col = 0; col < 8; col++) {
            const select = document.getElementById('r' + row + 'c' + col);
            if (select) {
                const value = select.value || '';
                rowData.push(value);
            } else {
                rowData.push('');
            }
        }
        board.push(rowData);
    }

    // Show loading state
    const submitButton = form.querySelector('button[type="submit"]');
    const originalText = submitButton.textContent;
    submitButton.textContent = 'Saving...';
    submitButton.disabled = true;

    // Submit as JSON
    fetch('/board-edit', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({ board: board })
    })
        .then(response => {
            if (response.ok) {
                // Parse HTML response to check for success
                return response.text();
            } else {
                throw new Error('Failed to save board state');
            }
        })
        .then(html => {
            // Check if response contains success message
            if (html.includes('Board Updated!')) {
                // Show success and reload page after a short delay
                const statusEl = document.querySelector('.status');
                if (statusEl) {
                    statusEl.textContent = 'Board state saved successfully! Reloading...';
                    statusEl.style.color = '#4CAF50';
                }
                setTimeout(() => {
                    window.location.reload();
                }, 1000);
            } else {
                throw new Error('Unexpected response from server');
            }
        })
        .catch(error => {
            console.error('Error saving board state:', error);
            const statusEl = document.querySelector('.status');
            if (statusEl) {
                statusEl.textContent = 'Error saving board state. Please try again.';
                statusEl.style.color = '#F44336';
            }
            submitButton.textContent = originalText;
            submitButton.disabled = false;
        });
}

// Initialize on page load
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initBoardEdit);
} else {
    initBoardEdit();
}

