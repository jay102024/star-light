const teamPicker = document.getElementById('teamPicker');
const teamPickerGrid = document.getElementById('teamPickerGrid');
const counterPanel = document.getElementById('counterPanel');
const selectedTeamLabel = document.getElementById('selectedTeamLabel');
const selectedTeamName = document.getElementById('selectedTeamName');
const countValue = document.getElementById('countValue');
const targetValue = document.getElementById('targetValue');
const teamStatus = document.getElementById('teamStatus');
const incrementButton = document.getElementById('incrementButton');
const decrementButton = document.getElementById('decrementButton');
const changeTeamButton = document.getElementById('changeTeamButton');

const TEAM_STORAGE_KEY = 'counter:selected-team';

let teams = [];
let selectedTeamId = localStorage.getItem(TEAM_STORAGE_KEY) || '';
let presenceTimer = null;

bootstrap();

async function bootstrap() {
  const response = await fetch('/api/bootstrap');
  const data = await response.json();
  teams = data.teams;
  if (teams.some((team) => team.id === selectedTeamId)) {
    startPresencePing();
  } else {
    selectedTeamId = '';
    localStorage.removeItem(TEAM_STORAGE_KEY);
  }
  render();

  const socket = io();
  socket.on('state', (nextTeams) => {
    teams = nextTeams;
    render();
  });
}

incrementButton.addEventListener('click', () => updateCount(1));
decrementButton.addEventListener('click', () => updateCount(-1));

changeTeamButton.addEventListener('click', () => {
  selectedTeamId = '';
  localStorage.removeItem(TEAM_STORAGE_KEY);
  stopPresencePing();
  render();
});

function render() {
  teamPickerGrid.innerHTML = teams.map((team) => `
    <button class="team-card selectable" type="button" data-team-id="${team.id}">
      <div class="team-top">
        <div>
          <p class="eyebrow">${escapeHtml(team.id)}</p>
          <h3>${escapeHtml(team.name)}</h3>
        </div>
        <span class="pill ${team.deviceOnline ? 'online' : 'offline'}">${team.deviceOnline ? '設備在線' : '設備離線'}</span>
      </div>
      <div class="team-count">${team.count}/${team.target}</div>
      <p class="team-note">${team.target > 0 ? `距離目標還差 ${team.remaining} 人` : '尚未設定目標人數'}</p>
    </button>
  `).join('');

  teamPickerGrid.querySelectorAll('[data-team-id]').forEach((button) => {
    button.addEventListener('click', () => {
      selectedTeamId = button.dataset.teamId;
      localStorage.setItem(TEAM_STORAGE_KEY, selectedTeamId);
      startPresencePing();
      render();
    });
  });

  const activeTeam = teams.find((team) => team.id === selectedTeamId);
  const hasSelection = Boolean(activeTeam);

  teamPicker.classList.toggle('hidden', hasSelection);
  counterPanel.classList.toggle('hidden', !hasSelection);

  if (!activeTeam) {
    return;
  }

  selectedTeamLabel.textContent = activeTeam.id;
  selectedTeamName.textContent = activeTeam.name;
  countValue.textContent = activeTeam.count;
  targetValue.textContent = `/${activeTeam.target}`;

  if (activeTeam.target <= 0) {
    teamStatus.textContent = '管理員尚未設定目標人數';
  } else if (activeTeam.completed) {
    teamStatus.textContent = `已達標，超過 ${Math.max(0, activeTeam.count - activeTeam.target)} 人`;
  } else {
    teamStatus.textContent = `距離目標還差 ${activeTeam.remaining} 人`;
  }
}

async function updateCount(delta) {
  if (!selectedTeamId) {
    return;
  }

  await fetch(`/api/teams/${selectedTeamId}/count`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ delta })
  });
}

async function sendPresence() {
  if (!selectedTeamId) {
    return;
  }

  await fetch(`/api/teams/${selectedTeamId}/client-presence`, {
    method: 'POST'
  });
}

function startPresencePing() {
  stopPresencePing();
  sendPresence();
  presenceTimer = setInterval(sendPresence, 15000);
}

function stopPresencePing() {
  if (presenceTimer) {
    clearInterval(presenceTimer);
    presenceTimer = null;
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