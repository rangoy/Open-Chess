// Update evaluation bar
if (data.evaluation !== undefined) {
const evalValue = data.evaluation;
const evalInPawns = (evalValue / 100).toFixed(2);
const maxEval = 1000; // Maximum evaluation to display (10 pawns)
const clampedEval = Math.max(-maxEval, Math.min(maxEval, evalValue));
const percentage = Math.abs(clampedEval) / maxEval * 50; // Max 50% on each side
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

