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
  selected: null,        // {r, c} of the cell that receives typed letters
  appliedGridText: '',   // grid text the server currently has
  appliedDictText: '',   // dictionary text the server currently has
  renderSeq: 0,          // guards against out-of-order responses
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
  if (!r.ok) {
    let msg = `POST ${path} failed`;
    try { const e = await r.json(); if (e && e.error) msg = e.error; } catch (_) {}
    throw new Error(msg);
  }
  return r.json();
}

function parseGridText(text) {
  return text.split('\n').map(line => line.trimEnd()).filter(line => line.length > 0);
}

function parseDictText(text) {
  return text.split('\n').map(w => w.trim().toUpperCase()).filter(w => w.length > 0);
}

function renderGrid(stateData) {
  const { height, width, blocked, slots, rows } = stateData;
  state.slots = slots;
  state.rows = rows || [];

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

      // pre-filled hint letter (typed by the user)
      const hintCh = state.rows[r] ? state.rows[r][c] : '.';
      if (!isBlocked && hintCh && /[A-Za-z]/.test(hintCh)) {
        const letterEl = document.createElement('span');
        letterEl.className = 'cell-letter hint';
        letterEl.textContent = hintCh.toUpperCase();
        div.appendChild(letterEl);
      }
      if (state.selected && state.selected.r === r && state.selected.c === c) {
        div.classList.add('selected');
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

function writeCell(r, c, ch) {
  const rows = parseGridText(gridInput.value);
  if (r >= rows.length) return false;
  const row = rows[r].split('');
  if (c >= row.length) return false;
  row[c] = ch;
  rows[r] = row.join('');
  gridInput.value = rows.join('\n');
  return true;
}

function toggleCell(r, c) {
  if (state.isRunning) return;
  state.selected = { r, c };
  const rows = parseGridText(gridInput.value);
  if (r >= rows.length || c >= rows[r].length) return;
  // '#' -> open; open or letter -> '#'
  if (writeCell(r, c, rows[r][c] === '#' ? '.' : '#')) applyGrid();
}

// Click a cell to select it, then type a letter to pin it as a hint.
// Backspace / Delete / Space empties it again.
document.addEventListener('keydown', (e) => {
  if (state.isRunning || !state.selected) return;
  const tag = (e.target && e.target.tagName) || '';
  if (tag === 'TEXTAREA' || tag === 'INPUT') return;
  if (e.ctrlKey || e.metaKey || e.altKey) return;
  const { r, c } = state.selected;
  let ch = null;
  if (/^[a-zA-Z]$/.test(e.key)) ch = e.key.toUpperCase();
  else if (e.key === 'Backspace' || e.key === 'Delete' || e.key === ' ') ch = '.';
  if (ch === null) return;
  e.preventDefault();
  if (writeCell(r, c, ch)) applyGrid();
});

function cellEl(r, c) {
  return gridContainer.querySelector(`.cell[data-r="${r}"][data-c="${c}"]`);
}

async function applyGrid() {
  const rows = parseGridText(gridInput.value);
  if (!rows.length) return;
  const seq = ++state.renderSeq;
  try {
    const data = await apiPost('/api/grid', { rows });
    if (seq !== state.renderSeq) return; // a newer request already rendered
    // use the server's normalized version (upper-case letters, padded rows)
    gridInput.value = data.rows.join('\n');
    state.appliedGridText = gridInput.value;
    renderGrid(data);
    clearOverlay();
    resetStats();
  } catch (err) {
    connText.textContent = err.message || 'Could not apply grid';
  }
}

async function applyDictionary() {
  const words = parseDictText(dictInput.value);
  if (!words.length) return;
  try {
    const result = await apiPost('/api/dictionary', { words });
    dictCount.textContent = `${result.wordCount} words loaded`;
    state.appliedDictText = dictInput.value;
  } catch (err) {
    connText.textContent = err.message || 'Could not load dictionary';
  }
}

function clearOverlay() {
  gridContainer.querySelectorAll('.cell.trying, .cell.accepted, .cell.rejected').forEach(el => {
    el.classList.remove('trying', 'accepted', 'rejected');
  });
  gridContainer.querySelectorAll('.cell-letter.solved').forEach(el => el.remove());
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
      const ch = pattern[i];
      if (!ch || ch === '.') continue;
      let letterSpan = el.querySelector('.cell-letter');
      if (!letterSpan) {
        letterSpan = document.createElement('span');
        letterSpan.className = 'cell-letter solved';
        el.appendChild(letterSpan);
      }
      if (!letterSpan.classList.contains('hint')) letterSpan.textContent = ch;
    }
  });
}

async function solve() {
  if (state.isRunning) return;
  state.isRunning = true;

  const btn = document.getElementById('solveBtn');
  btn.disabled = true;
  btn.textContent = 'Solving…';

  try {
    // make sure the server is solving exactly what is on screen
    if (gridInput.value.trim() !== state.appliedGridText.trim()) await applyGrid();
    if (dictInput.value.trim() !== state.appliedDictText.trim()) await applyDictionary();
    clearOverlay();

    const result = await apiPost('/api/solve', {});
    await animateLog(result.log);
    if (result.success) paintFinalLetters(result.finalPatterns);

    document.getElementById('statStatus').textContent =
      result.success ? 'Solved' : (result.timedOut ? 'Timed out' : 'No solution');
    document.getElementById('statAttempts').textContent = result.attemptsCount;
    document.getElementById('statSlots').textContent = state.slots.length;
    document.getElementById('statTime').textContent = `${result.microseconds} µs`;
  } catch (err) {
    connText.textContent = (err && err.message) || 'Error contacting backend';
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
    gridInput.value = data.rows.join('\n');
    state.appliedGridText = gridInput.value;
    renderGrid(data);

    // Show the dictionary the server is actually using (single source of truth)
    const dict = await apiGet('/api/dictionary');
    dictInput.value = dict.words.join('\n');
    state.appliedDictText = dictInput.value;
    dictCount.textContent = `${dict.wordCount} words loaded`;
  } catch (err) {
    connText.textContent = 'backend unreachable';
    gridLoading.textContent = 'Could not reach backend.';
  }
}

boot();
