function selectGame(mode) {
if (mode === 1 || mode === 2 || mode === 3 || mode === 4) {
fetch('/gameselect', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: 'gamemode=' + mode })
.then(response => response.text())
.then(data => { alert('Game mode ' + mode + ' selected! Check your chess board.'); })
.catch(error => { console.error('Error:', error); });
} else { alert('This game mode is coming soon!'); }
}

