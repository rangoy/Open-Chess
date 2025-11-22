let selectedWhite = null;
let selectedBlack = null;

function selectPlayer(side, playerType) {
    // Remove previous selection
    const options = document.querySelectorAll(`.player-selection:has(.player-option[onclick*="${side}"]) .player-option`);
    options.forEach(opt => opt.classList.remove('selected'));
    
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
    
    const data = {
        white: selectedWhite,
        black: selectedBlack
    };
    
    fetch('/gameselect', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(data)
    })
    .then(response => response.json())
    .then(data => {
        alert('Game started! White: ' + getPlayerName(selectedWhite) + ', Black: ' + getPlayerName(selectedBlack));
    })
    .catch(error => {
        console.error('Error:', error);
        // Fallback to form data if JSON fails
        const formData = 'white=' + selectedWhite + '&black=' + selectedBlack;
        fetch('/gameselect', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded'
            },
            body: formData
        })
        .then(response => response.text())
        .then(data => {
            alert('Game started! White: ' + getPlayerName(selectedWhite) + ', Black: ' + getPlayerName(selectedBlack));
        })
        .catch(err => {
            console.error('Error:', err);
            alert('Failed to start game');
        });
    });
}

function selectSensorTest() {
    fetch('/gameselect', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded'
        },
        body: 'gamemode=4'
    })
    .then(response => response.text())
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
