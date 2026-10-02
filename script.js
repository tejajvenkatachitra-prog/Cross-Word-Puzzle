// ==============================================================
// Crosswise — frontend controller
// ==============================================================
// No algorithm logic lives here. This file only:
//   1. Renders the grid layout as clickable cells
//   2. Sends the grid text + dictionary text to the C++ backend
//   3. Animates the solver's attempt log (trying / accepted / rejected)
// ==============================================================

const API_BASE = '';

const state = {
  rows: [],        // current grid as array of strings ('#' or '.')
  slots: [],        // last /api/grid or /api/state response's slots
  animSpeed: 55,
  isRunning: false,
};

const gridInput = document.getElementById('gridInput');
const dictInput = document.getElementById('dictInput');
const gridContainer = document.getElementById('gridContainer');
const gridLoading = document.getElementById('gridLoading');
const connDot = document.getElementById('connStatus');
const connText = document.getElementById('connText');
const speedInput = document.getElementById('speed');
const dictCount = document.getElementById('dictCount');
const slotListEl = document.getElementById('slotList');

async function apiGet(path) {
  const r = await fetch(API_BASE + path);
  if (!r.ok) throw new Error(`GET ${path} failed`);
  return r.json();
}
async function apiPost(path, body) {
  const r = await fetch(API_BASE + path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body || {})
  });
  if (!r.ok) throw new Error(`POST ${path} failed`);
  return r.json();
}

function parseGridText(text) {
  return text.split('\n').map(line => line.trimEnd()).filter(line => line.length > 0);
}

function parseDictText(text) {
  return text.split('\n').map(w => w.trim().toUpperCase()).filter(w => w.length > 0);
}

function renderGrid(stateData) {
  const { height, width, blocked, slots } = stateData;
  state.slots = slots;

  const blockedSet = new Set(blocked.map(([r, c]) => `${r},${c}`));

  gridContainer.style.gridTemplateColumns = `repeat(${width}, 40px)`;
  gridContainer.style.gridTemplateRows = `repeat(${height}, 40px)`;
  gridContainer.innerHTML = '';

  // Figure out which cells start a slot, for numbering
  const cellNumbers = {};
  let num = 1;
  const startsSlot = new Set();
  slots.forEach(s => startsSlot.add(`${s.row},${s.col}`));
  // number cells in reading order that start at least one slot
  for (let r = 0; r < height; r++) {
    for (let c = 0; c < width; c++) {
      if (startsSlot.has(`${r},${c}`)) {
        cellNumbers[`${r},${c}`] = num++;
      }
    }
  }

  const frag = document.createDocumentFragment();
  for (let r = 0; r < height; r++) {
    for (let c = 0; c < width; c++) {
      const div = document.createElement('div');
      div.className = 'cell';
      div.dataset.r = r;
      div.dataset.c = c;
      const isBlocked = blockedSet.has(`${r},${c}`);
      if (isBlocked) div.classList.add('blocked');

      const key = `${r},${c}`;
      if (cellNumbers[key] !== undefined) {
        const numEl = document.createElement('span');
        numEl.className = 'cell-number';
        numEl.textContent = cellNumbers[key];
        div.appendChild(numEl);
      }

      div.addEventListener('click', () => toggleCell(r, c));
      frag.appendChild(div);
    }
  }
  gridContainer.appendChild(frag);

  renderSlotList(slots);
}

function renderSlotList(slots) {
  if (!slots.length) {
    slotListEl.innerHTML = '<p class="slot-empty">No slots — grid needs open runs of 2+ cells.</p>';
    return;
  }
  slotListEl.innerHTML = '';
  slots.forEach(s => {
    const row = document.createElement('div');
    row.className = 'slot-row';
    row.innerHTML = `<span><span class="slot-dir">${s.dir}</span> #${s.id} (${s.length})</span>
                      <span class="slot-pattern">${s.pattern}</span>`;
    slotListEl.appendChild(row);
  });
}

function toggleCell(r, c) {
  if (state.isRunning) return;
  const rows = parseGridText(gridInput.value);
  if (r >= rows.length) return;
  const row = rows[r].split('');
  if (c >= row.length) return;
  row[c] = row[c] === '#' ? '.' : '#';
  rows[r] = row.join('');
  gridInput.value = rows.join('\n');
  applyGrid();
}

function cellEl(r, c) {
  return gridContainer.querySelector(`.cell[data-r="${r}"][data-c="${c}"]`);
}

async function applyGrid() {
  const rows = parseGridText(gridInput.value);
  if (!rows.length) return;
  const data = await apiPost('/api/grid', { rows });
  renderGrid(data);
  clearOverlay();
  resetStats();
}

async function applyDictionary() {
  const words = parseDictText(dictInput.value);
  if (!words.length) return;
  const result = await apiPost('/api/dictionary', { words });
  dictCount.textContent = `${result.wordCount} words loaded`;
}

function clearOverlay() {
  gridContainer.querySelectorAll('.cell.trying, .cell.accepted, .cell.rejected').forEach(el => {
    el.classList.remove('trying', 'accepted', 'rejected');
  });
}

function resetStats() {
  document.getElementById('statStatus').textContent = '—';
  document.getElementById('statAttempts').textContent = '—';
  document.getElementById('statSlots').textContent = '—';
  document.getElementById('statTime').textContent = '—';
}

function delayForSpeed() {
  const t = state.animSpeed / 100;
  return Math.max(2, Math.round(70 * (1 - t) + 2));
}

// Highlights every cell belonging to a given slot with a class, briefly,
// to animate the solver "trying" a word before we know if it stuck.
function slotCells(slotId) {
  const slot = state.slots.find(s => s.id === slotId);
  if (!slot) return [];
  const cells = [];
  for (let i = 0; i < slot.length; i++) {
    const r = slot.dir === 'across' ? slot.row : slot.row + i;
    const c = slot.dir === 'across' ? slot.col + i : slot.col;
    cells.push([r, c]);
  }
  return cells;
}

function animateLog(log) {
  return new Promise(resolve => {
    const delay = delayForSpeed();
    let i = 0;
    function step() {
      if (i >= log.length) { resolve(); return; }
      const entry = log[i];
      const cells = slotCells(entry.slotId);
      cells.forEach(([r, c]) => {
        const el = cellEl(r, c);
        if (!el) return;
        el.classList.remove('trying', 'accepted', 'rejected');
        el.classList.add(entry.accepted ? 'accepted' : 'rejected');
      });
      i++;
      setTimeout(step, delay);
    }
    step();
  });
}

function paintFinalLetters(finalPatterns) {
  state.slots.forEach((slot, idx) => {
    const pattern = finalPatterns[idx];
    if (!pattern) return;
    for (let i = 0; i < slot.length; i++) {
      const r = slot.dir === 'across' ? slot.row : slot.row + i;
      const c = slot.dir === 'across' ? slot.col + i : slot.col;
      const el = cellEl(r, c);
      if (!el) continue;
      let letterSpan = el.querySelector('.cell-letter');
      if (!letterSpan) {
        letterSpan = document.createElement('span');
        letterSpan.className = 'cell-letter';
        el.appendChild(letterSpan);
      }
      const ch = pattern[i];
      if (ch && ch !== '.') letterSpan.textContent = ch;
    }
  });
}

async function solve() {
  if (state.isRunning) return;
  state.isRunning = true;
  clearOverlay();

  const btn = document.getElementById('solveBtn');
  btn.disabled = true;
  btn.textContent = 'Solving…';

  try {
    const result = await apiPost('/api/solve', {});
    await animateLog(result.log);
    if (result.success) paintFinalLetters(result.finalPatterns);

    document.getElementById('statStatus').textContent = result.success ? 'Solved' : 'No solution';
    document.getElementById('statAttempts').textContent = result.attemptsCount;
    document.getElementById('statSlots').textContent = state.slots.length;
    document.getElementById('statTime').textContent = `${result.microseconds} µs`;
  } catch (err) {
    connText.textContent = 'Error contacting backend';
    connDot.classList.remove('online');
  } finally {
    btn.disabled = false;
    btn.textContent = 'Solve ▸';
    state.isRunning = false;
  }
}

document.getElementById('applyGrid').addEventListener('click', applyGrid);
document.getElementById('applyDict').addEventListener('click', applyDictionary);
document.getElementById('clearOverlay').addEventListener('click', clearOverlay);
document.getElementById('solveBtn').addEventListener('click', solve);
speedInput.addEventListener('input', () => { state.animSpeed = Number(speedInput.value); });

async function boot() {
  try {
    await apiGet('/api/health');
    connDot.classList.add('online');
    connText.textContent = 'connected to C++ backend';

    const data = await apiGet('/api/state');
    gridLoading.style.display = 'none';
    renderGrid(data);

    // Load the default dictionary count for display
    dictCount.textContent = 'default dictionary active';
  } catch (err) {
    connText.textContent = 'backend unreachable';
    gridLoading.textContent = 'Could not reach backend.';
  }
}

boot();
