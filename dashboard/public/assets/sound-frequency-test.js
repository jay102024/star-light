const soundStatus = document.getElementById('soundStatus');
const soundDeviceList = document.getElementById('soundDeviceList');
const coinSoundBtn = document.getElementById('coinSoundBtn');
const starSoundBtn = document.getElementById('starSoundBtn');
const customSoundBtn = document.getElementById('customSoundBtn');

let teams = [];
let selectedTeamId = '';

bootstrap();

function setStatus(message) {
  soundStatus.textContent = message;
}

async function bootstrap() {
  const response = await fetch('/api/bootstrap');
  const data = await response.json();
  teams = data.teams;
  selectDefaultTeam();
  renderDeviceList();

  const socket = io();
  socket.on('state', (payload) => {
    teams = payload.teams;
    if (!teams.some((team) => team.id === selectedTeamId && team.deviceOnline)) {
      selectDefaultTeam();
    }
    renderDeviceList();
  });
}

function selectDefaultTeam() {
  const onlineTeams = getOnlineTeams();
  selectedTeamId = onlineTeams[0] ? onlineTeams[0].id : '';
}

function getOnlineTeams() {
  return teams.filter((team) => team.deviceOnline);
}

function renderDeviceList() {
  const onlineTeams = getOnlineTeams();
  if (!onlineTeams.length) {
    soundDeviceList.innerHTML = '<p class="sound-empty">目前沒有在線機器。</p>';
    setStatus('沒有可用的在線機器');
    return;
  }

  soundDeviceList.innerHTML = onlineTeams.map((team) => `
    <button class="sound-device-card ${team.id === selectedTeamId ? 'selected' : ''}" type="button" data-team-id="${team.id}">
      <strong>${escapeHtml(team.name)}</strong>
      <span>${escapeHtml(team.deviceId || team.id)}</span>
    </button>
  `).join('');

  soundDeviceList.querySelectorAll('[data-team-id]').forEach((button) => {
    button.addEventListener('click', () => {
      selectedTeamId = button.dataset.teamId;
      renderDeviceList();
      const team = teams.find((item) => item.id === selectedTeamId);
      setStatus(`已選擇 ${team ? team.name : selectedTeamId}`);
    });
  });
}

async function sendSound(soundMode) {
  if (!selectedTeamId) {
    setStatus('請先選擇一台在線機器');
    return;
  }

  try {
    const response = await fetch(`/api/teams/${selectedTeamId}/test-buzzer`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ soundMode })
    });

    if (!response.ok) {
      throw new Error('Request failed');
    }

    const team = teams.find((item) => item.id === selectedTeamId);
    const soundNames = ['投幣音', '星光音', '鐵幣音'];
    setStatus(`已播放 ${soundNames[soundMode]} 到 ${team ? team.name : selectedTeamId}`);
  } catch (error) {
    console.error(error);
    setStatus('播放失敗，請稍後再試。');
  }
}

// 投幣音效 (mode 0)
coinSoundBtn.addEventListener('click', () => {
  sendSound(0);
});

// 星光音效 (mode 1)
starSoundBtn.addEventListener('click', () => {
  sendSound(1);
});

// 鐵幣音效 (mode 2)
customSoundBtn.addEventListener('click', () => {
  sendSound(2);
});

function escapeHtml(value) {
  return String(value)
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#39;');
}
