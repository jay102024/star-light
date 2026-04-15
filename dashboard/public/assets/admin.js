const summaryGrid = document.getElementById('summaryGrid');
const teamGrid = document.getElementById('teamGrid');
const resetAllButton = document.getElementById('resetAllButton');

let teams = [];
let mode = null;
const targetDrafts = {};

bootstrap();

resetAllButton.addEventListener('click', resetAllCountsAndReload);

async function bootstrap() {
  const response = await fetch('/api/bootstrap');
  const data = await response.json();
  teams = data.teams;
  mode = data.mode;

  if (!mode) {
    showModeOverlay();
  }

  render();

  const socket = io();
  socket.on('state', (payload) => {
    teams = payload.teams;
    mode = payload.mode;
    render(isEditingTargetInput());
  });
}

function render(skipTeamGrid = false) {
  renderSummary();
  renderLeaderboard();

  if (skipTeamGrid) {
    return;
  }

  renderTeamGrid();
}

function renderSummary() {
  const onlineCount = teams.filter((team) => team.deviceOnline).length;
  const activeClientCount = teams.filter((team) => team.clientActive).length;

  if (mode === 'scoring') {
    const totalScore = teams.reduce((sum, team) => sum + team.scoreCount, 0);
    const topTeam = [...teams].sort((a, b) => b.scoreCount - a.scoreCount)[0];

    summaryGrid.innerHTML = `
      <article class="summary-card">
        <span class="muted">設備在線</span>
        <strong>${onlineCount}</strong>
      </article>
      <article class="summary-card">
        <span class="muted">目前有隊長操作</span>
        <strong>${activeClientCount}</strong>
      </article>
      <article class="summary-card">
        <span class="muted">全場總積分</span>
        <strong>${totalScore}</strong>
      </article>
      <article class="summary-card">
        <span class="muted">目前領先</span>
        <strong>${topTeam && topTeam.scoreCount > 0 ? escapeHtml(topTeam.name) : '—'}</strong>
      </article>
    `;
  } else {
    const completedCount = teams.filter((team) => team.completed).length;
    const totalCount = teams.reduce((sum, team) => sum + team.count, 0);

    summaryGrid.innerHTML = `
      <article class="summary-card">
        <span class="muted">設備在線</span>
        <strong>${onlineCount}</strong>
      </article>
      <article class="summary-card">
        <span class="muted">已達標桌次</span>
        <strong>${completedCount}</strong>
      </article>
      <article class="summary-card">
        <span class="muted">目前有隊長操作</span>
        <strong>${activeClientCount}</strong>
      </article>
      <article class="summary-card">
        <span class="muted">全場已計數</span>
        <strong>${totalCount}</strong>
      </article>
    `;
  }
}

function renderTeamGrid() {
  teamGrid.innerHTML = teams.map((team) => `
    <article class="team-card">
      <div class="team-top">
        <div>
          <p class="eyebrow">${escapeHtml(team.id)}</p>
          <h3>${escapeHtml(team.name)}</h3>
        </div>
        <div class="status-stack">
          <span class="pill ${team.deviceOnline ? 'online' : 'offline'}">${team.deviceOnline ? '在線' : '離線'}</span>
          <span class="pill ${team.clientActive ? 'online' : 'offline'}">${team.clientActive ? '隊長在線' : '隊長離線'}</span>
        </div>
      </div>
      <div class="team-count-wrap">
        <div class="team-count">${team.count}/${team.target}</div>
      </div>
      <div class="team-actions-row">
        <button class="ghost-button team-delta-button" type="button" data-team-delta-id="${team.id}" data-team-delta="-1">-1</button>
        <button class="brand-button team-delta-button" type="button" data-team-delta-id="${team.id}" data-team-delta="1">+1</button>
        <button class="danger-button team-test-button" type="button" data-team-test-id="${team.id}">試亮</button>
        <button class="danger-button team-reset-button" type="button" data-team-reset-id="${team.id}">歸零</button>
      </div>
      <form class="target-form" data-team-id="${team.id}">
        <input id="target-${team.id}" name="target" type="number" min="0" step="1" placeholder="目標人數" value="${escapeHtml(getDraftOrTeamTarget(team))}">
        <button class="brand-button" type="submit">更新</button>
      </form>
    </article>
  `).join('');

  teamGrid.querySelectorAll('form').forEach((form) => {
    const teamId = form.dataset.teamId;
    const input = form.querySelector('input[name="target"]');

    input.addEventListener('focus', () => { targetDrafts[teamId] = input.value; });
    input.addEventListener('input', () => { targetDrafts[teamId] = input.value; });

    form.addEventListener('submit', async (event) => {
      event.preventDefault();
      const payload = new FormData(form);
      const targetValue = targetDrafts[teamId] ?? payload.get('target');
      const response = await fetch(`/api/teams/${teamId}/target`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ target: targetValue })
      });

      const data = await response.json();
      if (response.ok && data.team) {
        teams = teams.map((team) => (team.id === teamId ? data.team : team));
        delete targetDrafts[teamId];
        render();
      }
    });
  });

  attachDeltaListeners();
  attachResetListeners();

  teamGrid.querySelectorAll('[data-team-test-id]').forEach((button) => {
    button.addEventListener('click', async () => {
      const teamId = button.dataset.teamTestId;
      button.disabled = true;
      try {
        const response = await fetch(`/api/teams/${teamId}/test-light`, { method: 'POST' });
        if (!response.ok) {
          throw new Error('Test light trigger failed');
        }
      } catch (error) {
        console.error(error);
        window.alert('試亮失敗，請稍後再試。');
      } finally {
        button.disabled = false;
      }
    });
  });
}

function attachResetListeners() {
  teamGrid.querySelectorAll('[data-team-reset-id]').forEach((button) => {
    button.addEventListener('click', async () => {
      const teamId = button.dataset.teamResetId;
      const label = mode === 'scoring' ? '積分' : '人數';
      const confirmed = window.confirm(`確定要把 ${teamId} 目前${label}歸零嗎？`);
      if (!confirmed) { return; }

      button.disabled = true;
      try {
        const response = await fetch(`/api/teams/${teamId}/reset`, { method: 'POST' });
        if (!response.ok) { throw new Error('Reset team failed'); }
      } catch (error) {
        console.error(error);
        window.alert('此組歸零失敗，請稍後再試。');
      } finally {
        button.disabled = false;
      }
    });
  });
}

function attachDeltaListeners() {
  teamGrid.querySelectorAll('[data-team-delta-id]').forEach((button) => {
    button.addEventListener('click', async () => {
      const teamId = button.dataset.teamDeltaId;
      const delta = Number(button.dataset.teamDelta || 0);
      button.disabled = true;
      try {
        const response = await fetch(`/api/teams/${teamId}/count`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ delta })
        });
        if (!response.ok) { throw new Error('Team count update failed'); }
      } catch (error) {
        console.error(error);
        window.alert('加減分數失敗，請稍後再試。');
      } finally {
        button.disabled = false;
      }
    });
  });
}

function isEditingTargetInput() {
  return document.activeElement && document.activeElement.matches('input[name="target"]');
}

function getDraftOrTeamTarget(team) {
  return Object.prototype.hasOwnProperty.call(targetDrafts, team.id)
    ? targetDrafts[team.id]
    : String(team.target);
}

async function resetAllCountsAndReload() {
  const label = mode === 'scoring' ? '積分' : '人數';
  const confirmed = window.confirm(`確定要全部歸零嗎？這會把所有桌次目前${label}清成 0。`);
  if (!confirmed) {
    return;
  }

  resetAllButton.disabled = true;
  try {
    const response = await fetch('/api/admin/reset-all', { method: 'POST' });
    if (!response.ok) {
      throw new Error('Reset all failed');
    }

    window.location.reload();
  } catch (error) {
    console.error(error);
    resetAllButton.disabled = false;
    window.alert('全部歸零失敗，請稍後再試。');
  }
}

function escapeHtml(value) {
  return String(value)
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#39;');
}

function getTeamsInDisplayOrder() {
  return [...teams].sort((left, right) => getTeamOrder(left) - getTeamOrder(right));
}

function getLeaderboardTeams() {
  return [...teams].sort((left, right) => {
    if (right.scoreCount !== left.scoreCount) {
      return right.scoreCount - left.scoreCount;
    }

    return getTeamOrder(left) - getTeamOrder(right);
  });
}

function getTeamOrder(team) {
  const match = String(team.id).match(/(\d+)$/);
  return match ? Number(match[1]) : Number.MAX_SAFE_INTEGER;
}