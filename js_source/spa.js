// SPA Router
const router = {
    currentView: null,
    routes: {
        '/': 'view-config',
        '/game': 'view-game',
        '/board-view': 'view-board-view',
        '/board-edit': 'view-board-edit'
    },
    
    init() {
        // Handle browser back/forward
        window.addEventListener('popstate', () => this.navigate(window.location.pathname, false));
        
        // Initial navigation
        this.navigate(window.location.pathname || '/', false);
    },
    
    navigate(path, pushState = true) {
        const viewId = this.routes[path] || this.routes['/'];
        
        // Hide all views
        document.querySelectorAll('.view').forEach(view => {
            view.style.display = 'none';
        });
        
        // Show target view
        const targetView = document.getElementById(viewId);
        if (targetView) {
            targetView.style.display = 'block';
            this.currentView = viewId;
            
            // Initialize view-specific code
            this.initView(viewId);
        }
        
        // Show navigation
        document.getElementById('nav').style.display = 'flex';
        
        // Update URL without reload
        if (pushState) {
            window.history.pushState({}, '', path);
        }
    },
    
    initView(viewId) {
        switch(viewId) {
            case 'view-config':
                initConfig();
                break;
            case 'view-game':
                initGameSelection();
                break;
            case 'view-board-view':
                initBoardView();
                break;
            case 'view-board-edit':
                initBoardEdit();
                break;
        }
    }
};

// Piece Symbols
function getPieceSymbol(piece) {
    if (!piece) return '';
    const symbols = {
        'R': '♖', 'N': '♘', 'B': '♗', 'Q': '♕', 'K': '♔', 'P': '♙',
        'r': '♜', 'n': '♞', 'b': '♝', 'q': '♛', 'k': '♚', 'p': '♟'
    };
    return symbols[piece] || piece;
}

// Config View
function initConfig() {
    fetch('/api/config')
        .then(response => response.json())
        .then(data => {
            if (data.ssid) document.getElementById('ssid').value = data.ssid;
            if (data.token) document.getElementById('token').value = data.token;
            if (data.gameMode) document.getElementById('gameMode').value = data.gameMode;
            if (data.startupType) document.getElementById('startupType').value = data.startupType;
        })
        .catch(err => console.error('Error loading config:', err));
    
    document.getElementById('config-form').addEventListener('submit', (e) => {
        e.preventDefault();
        const formData = {
            ssid: document.getElementById('ssid').value,
            password: document.getElementById('password').value,
            token: document.getElementById('token').value,
            gameMode: document.getElementById('gameMode').value,
            startupType: document.getElementById('startupType').value
        };
        
        fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(formData)
        })
        .then(response => response.json())
        .then(data => {
            const status = document.getElementById('config-status');
            status.textContent = data.message || 'Configuration saved!';
            status.style.color = data.status === 'success' ? '#4CAF50' : '#F44336';
        })
        .catch(err => {
            console.error('Error saving config:', err);
            document.getElementById('config-status').textContent = 'Error saving configuration';
        });
    });
}

// Game Selection View
let selectedWhite = null;
let selectedBlack = null;

function initGameSelection() {
    selectedWhite = null;
    selectedBlack = null;
    document.querySelectorAll('.player-option').forEach(opt => opt.classList.remove('selected'));
    document.getElementById('start-game-btn').disabled = true;
}

function selectPlayer(side, playerType) {
    // Remove previous selection for this side
    const sideSelector = side === 'white' ? '.player-selection:first-child .player-option' : '.player-selection:last-child .player-option';
    document.querySelectorAll(sideSelector).forEach(opt => opt.classList.remove('selected'));
    
    // Add selection to clicked option
    event.target.closest('.player-option').classList.add('selected');
    
    if (side === 'white') {
        selectedWhite = playerType;
    } else {
        selectedBlack = playerType;
    }
    
    // Enable start button if both players are selected
    const startBtn = document.getElementById('start-game-btn');
    if (selectedWhite !== null && selectedBlack !== null) {
        startBtn.disabled = false;
    }
}

function startGame() {
    if (selectedWhite === null || selectedBlack === null) {
        alert('Please select both players');
        return;
    }
    
    fetch('/api/gameselect', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ white: selectedWhite, black: selectedBlack })
    })
    .then(response => response.json())
    .then(data => {
        alert('Game started! White: ' + getPlayerName(selectedWhite) + ', Black: ' + getPlayerName(selectedBlack));
    })
    .catch(error => {
        console.error('Error:', error);
        alert('Failed to start game');
    });
}

function selectSensorTest() {
    fetch('/api/gameselect', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ gamemode: 4 })
    })
    .then(response => response.json())
    .then(data => {
        alert('Sensor Test mode selected!');
    })
    .catch(error => {
        console.error('Error:', error);
        alert('Failed to select sensor test');
    });
}

function getPlayerName(playerType) {
    switch(playerType) {
        case 0: return 'Human';
        case 1: return 'Easy AI';
        case 2: return 'Medium AI';
        case 3: return 'Hard AI';
        default: return 'Unknown';
    }
}

// Board View
let boardViewInterval = null;

function initBoardView() {
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
    updateBoardView();
    
    // Clear any existing interval
    if (boardViewInterval) clearInterval(boardViewInterval);
    boardViewInterval = setInterval(updateBoardView, 2000);
}

function updateBoardView() {
    fetch('/api/board')
        .then(response => response.json())
        .then(data => {
            updateBoardDisplay(data);
        })
        .catch(error => {
            console.error('Error fetching board state:', error);
            document.getElementById('board-status').textContent = 'Error loading board state';
        });
}

function updateBoardDisplay(data) {
    const statusEl = document.getElementById('board-status');
    const containerEl = document.getElementById('board-container');
    const noBoardEl = document.getElementById('no-board-message');

    if (data.valid) {
        statusEl.textContent = 'Board state: Active';
        containerEl.style.display = 'block';
        noBoardEl.style.display = 'none';

        // Update board squares
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
        
        // Update evaluation bar
        if (data.evaluation !== undefined) {
            updateEvaluationBar(data.evaluation);
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
    } else {
        statusEl.textContent = 'Board state: Not available';
        containerEl.style.display = 'none';
        noBoardEl.style.display = 'block';
    }
}

function updateEvaluationBar(evalValue) {
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

// Board Edit View
let isPaused = false;
let editBoardInterval = null;

function initBoardEdit() {
    fetch('/api/board')
        .then(response => response.json())
        .then(data => {
            if (data.valid) {
                renderEditBoard(data.board);
            } else {
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
        checkPauseState();
    }

    // Setup undo button
    const undoButton = document.getElementById('undo-button');
    if (undoButton) {
        undoButton.addEventListener('click', handleUndo);
    }
    
    // Auto-refresh board state
    if (editBoardInterval) clearInterval(editBoardInterval);
    editBoardInterval = setInterval(() => {
        if (!isPaused) {
            fetch('/api/board')
                .then(response => response.json())
                .then(data => {
                    if (data.valid) {
                        updateEditBoard(data.board);
                    }
                })
                .catch(err => console.error('Error refreshing board:', err));
        }
    }, 2000);
}

function renderEditBoard(board) {
    const boardEl = document.getElementById('edit-chess-board');
    boardEl.innerHTML = '';

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

function updateEditBoard(board) {
    for (let row = 0; row < 8; row++) {
        for (let col = 0; col < 8; col++) {
            const select = document.getElementById('r' + row + 'c' + col);
            if (select) {
                const piece = board[row][col] || '';
                select.value = piece === ' ' ? '' : piece;
            }
        }
    }
}

function checkPauseState() {
    fetch('/api/pause-moves')
        .then(response => response.json())
        .then(data => {
            isPaused = data.paused || false;
            updatePauseButton();
        })
        .catch(error => {
            console.error('Error checking pause state:', error);
        });
}

function togglePause() {
    const newPausedState = !isPaused;
    fetch('/api/pause-moves', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ paused: newPausedState })
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

function updatePauseButton() {
    const pauseToggle = document.getElementById('pause-toggle');
    const undoButton = document.getElementById('undo-button');
    const statusEl = document.getElementById('edit-status');

    if (pauseToggle) {
        if (isPaused) {
            pauseToggle.textContent = 'Resume Move Detection';
            pauseToggle.classList.remove('secondary');
            pauseToggle.classList.add('paused');
            if (statusEl) {
                statusEl.textContent = 'Move detection PAUSED - You can rearrange pieces freely. Click on any square to change the piece.';
            }
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
            if (undoButton) {
                undoButton.style.display = 'none';
            }
        }
    }
}

function handleUndo() {
    const undoButton = document.getElementById('undo-button');
    if (undoButton) {
        undoButton.disabled = true;
        const originalText = undoButton.textContent;
        undoButton.textContent = 'Undoing...';

        fetch('/api/undo-move', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' }
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                const statusEl = document.getElementById('edit-status');
                if (statusEl) {
                    statusEl.textContent = 'Move undone successfully! Reloading...';
                    statusEl.style.color = '#4CAF50';
                }
                // Wait a bit longer for the server to update the board state
                setTimeout(() => {
                    initBoardEdit();
                }, 1500);
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

function saveBoard() {
    const board = [];
    let pieceCount = 0;
    
    for (let row = 0; row < 8; row++) {
        const rowData = [];
        for (let col = 0; col < 8; col++) {
            const select = document.getElementById('r' + row + 'c' + col);
            if (select) {
                const value = select.value || '';
                rowData.push(value);
                if (value && value.length > 0) {
                    pieceCount++;
                }
            } else {
                console.error('Select element not found for r' + row + 'c' + col);
                rowData.push('');
            }
        }
        board.push(rowData);
    }
    
    console.log('DEBUG: Saving board with', pieceCount, 'pieces');
    console.log('DEBUG: First row:', board[0]);

    const statusEl = document.getElementById('edit-status');
    statusEl.textContent = 'Saving...';
    statusEl.style.color = '#ec8703';

    fetch('/api/board-edit', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ board: board })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            statusEl.textContent = 'Board state saved successfully!';
            statusEl.style.color = '#4CAF50';
            // Don't reload immediately - wait a bit for the server to process
            setTimeout(() => {
                initBoardEdit();
            }, 1500);
        } else {
            statusEl.textContent = 'Error saving board state: ' + (data.message || 'Unknown error');
            statusEl.style.color = '#F44336';
        }
    })
    .catch(error => {
        console.error('Error saving board state:', error);
        statusEl.textContent = 'Error saving board state. Please try again.';
        statusEl.style.color = '#F44336';
    });
}

function resetBoard() {
    if (confirm('Reset board to current state from server?')) {
        initBoardEdit();
    }
}

// Initialize router on page load
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => router.init());
} else {
    router.init();
}

