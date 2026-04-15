const lbEyebrow = document.getElementById('lbEyebrow');
const lbTitle = document.getElementById('lbTitle');
const lbContent = document.getElementById('lbContent');

let teams = [];
let mode = null;

bootstrap();

async function bootstrap() {
  const response = await fetch('/api/bootstrap');
  const data = await response.json();
  teams = data.teams;
  mode = data.mode;
  render();

  const socket = io();
  socket.on('state', (payload) => {
    teams = payload.teams;
    mode = payload.mode;
    render();
  });
}

function render() {
  if (mode === 'scoring') {
    lbEyebrow.textContent = '計分模式';
    lbTitle.textContent = '積分排行榜';
    renderScoring();
  } else if (mode === 'banquet') {
    lbEyebrow.textContent = '圓滿餐會';
    lbTitle.textContent = '各桌達標進度';
    renderBanquet();
  } else {
    lbContent.innerHTML = '<p class="lb-waiting">等待管理員設定活動模式…</p>';
  }
}

function renderScoring() {
  const ranked = [...teams].sort((a, b) => {
    if (b.scoreCount !== a.scoreCount) return b.scoreCount - a.scoreCount;
    return getTeamOrder(a) - getTeamOrder(b);
  });

  lbContent.innerHTML = ranked.map((team, index) => {
    const rankClass = index === 0 ? 'rank-1' : index === 1 ? 'rank-2' : index === 2 ? 'rank-3' : '';
    return `
      <article class="lb-row ${rankClass}">
        <div class="lb-rank">#${index + 1}</div>
        <div class="lb-meta">
          <strong>${escapeHtml(team.name)}</strong>
        </div>
        <div class="lb-score">${team.scoreCount}<span class="lb-unit"> 分</span></div>
      </article>
    `;
  }).join('');
}

function renderBanquet() {
  const sorted = [...teams].sort((a, b) => {
    const aDone = a.target > 0 && a.count >= a.target ? 1 : 0;
    const bDone = b.target > 0 && b.count >= b.target ? 1 : 0;
    if (bDone !== aDone) return bDone - aDone;
    return getTeamOrder(a) - getTeamOrder(b);
  });

  lbContent.innerHTML = sorted.map((team) => {
    const done = team.target > 0 && team.count >= team.target;
    const pct = team.target > 0 ? Math.min(100, Math.round(team.count / team.target * 100)) : 0;
    return `
      <article class="lb-row ${done ? 'lb-done' : ''}">
        <div class="lb-meta">
          <strong>${escapeHtml(team.name)}</strong>
        </div>
        <div class="lb-progress-wrap">
          <div class="lb-progress-bar" style="width:${pct}%"></div>
        </div>
        <div class="lb-score">${team.count}<span class="lb-unit">/${team.target > 0 ? team.target : '?'}</span></div>
      </article>
    `;
  }).join('');
}

function getTeamOrder(team) {
  const match = String(team.id).match(/(\d+)$/);
  return match ? Number(match[1]) : Number.MAX_SAFE_INTEGER;
}

function escapeHtml(value) {
  return String(value)
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#39;');
}
