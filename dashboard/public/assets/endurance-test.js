const enduranceDeviceList = document.getElementById('enduranceDeviceList');
const enduranceStartButton = document.getElementById('enduranceStartButton');
const enduranceStopButton = document.getElementById('enduranceStopButton');
const enduranceClearButton = document.getElementById('enduranceClearButton');
const enduranceStatus = document.getElementById('enduranceStatus');
const enduranceStateValue = document.getElementById('enduranceStateValue');
const firstHeartbeatValue = document.getElementById('firstHeartbeatValue');
const lastHeartbeatValue = document.getElementById('lastHeartbeatValue');
const durationValue = document.getElementById('durationValue');
const lightCountValue = document.getElementById('lightCountValue');
const currentModeValue = document.getElementById('currentModeValue');

const SCORE_PULSE_INTERVAL_MS = 2000;

let teams = [];
let mode = null;
let selectedTeamId = '';
let deviceTimeoutMs = 15000;
let run = createEmptyRun();
let scorePulseTimer = null;
let clockTimer = null;

bootstrap();

enduranceStartButton.addEventListener('click', startRun);
enduranceStopButton.addEventListener('click', () => finishRun('手動停止'));
enduranceClearButton.addEventListener('click', clearRun);

async function bootstrap() {
  const response = await fetch('/api/bootstrap');
  const data = await response.json();
  teams = data.teams;
  mode = data.mode;
  deviceTimeoutMs = Number(data.deviceTimeoutMs || deviceTimeoutMs);
  selectDefaultTeam();
  render();

  const socket = io();
  socket.on('state', (payload) => {
    teams = payload.teams;
    mode = payload.mode;
    handleHeartbeatUpdate();
    if (!run.active && !teams.some((team) => team.id === selectedTeamId && team.deviceOnline)) {
      selectDefaultTeam();
    }
    render();
  });
}

function createEmptyRun() {
  return {
    active: false,
    startedAt: 0,
    mode: 'heartbeat',
    teamId: '',
    firstHeartbeatAt: 0,
    lastHeartbeatAt: 0,
    lastSeenValue: 0,
    lightCount: 0,
    finishReason: ''
  };
}

function getSelectedRunMode() {
  const checked = document.querySelector('input[name="enduranceMode"]:checked');
  return checked ? checked.value : 'heartbeat';
}

function getOnlineTeams() {
  return teams.filter((team) => team.deviceOnline);
}

function getSelectedTeam() {
  return teams.find((team) => team.id === selectedTeamId);
}

function getRunTeam() {
  return teams.find((team) => team.id === run.teamId);
}

function selectDefaultTeam() {
  const onlineTeams = getOnlineTeams();
  selectedTeamId = onlineTeams[0] ? onlineTeams[0].id : '';
}

function render() {
  renderDeviceList();
  renderStats();
  currentModeValue.textContent = formatMode(mode);
  enduranceStartButton.disabled = run.active || !selectedTeamId;
  enduranceStopButton.disabled = !run.active;
  document.querySelectorAll('input[name="enduranceMode"]').forEach((input) => {
    input.disabled = run.active;
  });
}

function renderDeviceList() {
  const onlineTeams = getOnlineTeams();
  const runTeam = getRunTeam();
  const listTeams = run.active && runTeam && !onlineTeams.some((team) => team.id === runTeam.id)
    ? [runTeam, ...onlineTeams]
    : onlineTeams;

  if (!listTeams.length) {
    enduranceDeviceList.innerHTML = '<p class="sound-empty">目前沒有在線機器。</p>';
    return;
  }

  enduranceDeviceList.innerHTML = listTeams.map((team) => {
    const isSelected = team.id === selectedTeamId || (run.active && team.id === run.teamId);
    const status = team.deviceOnline ? '在線' : '測試中離線';
    return `
      <button class="sound-device-card ${isSelected ? 'selected' : ''}" type="button" data-team-id="${team.id}" ${run.active ? 'disabled' : ''}>
        <strong>${escapeHtml(team.name)}</strong>
        <span>${escapeHtml(team.deviceId || team.id)} · ${status}</span>
      </button>
    `;
  }).join('');

  enduranceDeviceList.querySelectorAll('[data-team-id]').forEach((button) => {
    button.addEventListener('click', () => {
      selectedTeamId = button.dataset.teamId;
      const team = getSelectedTeam();
      setStatus(`已選擇 ${team ? team.name : selectedTeamId}`);
      render();
    });
  });
}

function renderStats() {
  const effectiveLast = run.lastHeartbeatAt || (run.active ? run.firstHeartbeatAt : 0);
  enduranceStateValue.textContent = run.active ? '測試中' : (run.finishReason || '待命中');
  firstHeartbeatValue.textContent = formatTime(run.firstHeartbeatAt);
  lastHeartbeatValue.textContent = formatTime(run.lastHeartbeatAt);
  durationValue.textContent = formatDuration(getDurationMs(run.firstHeartbeatAt, effectiveLast));
  lightCountValue.textContent = String(run.lightCount);
}

async function startRun() {
  const team = getSelectedTeam();
  if (!team || !team.deviceOnline) {
    setStatus('請先選擇一台在線機器。');
    return;
  }

  const selectedMode = getSelectedRunMode();
  enduranceStartButton.disabled = true;
  enduranceClearButton.disabled = true;

  try {
    if (selectedMode === 'scoring') {
      await ensureScoringMode();
      await resetSelectedScore(team.id);
    }

    run = {
      ...createEmptyRun(),
      active: true,
      startedAt: Date.now(),
      mode: selectedMode,
      teamId: team.id,
      lastSeenValue: Number(team.deviceLastSeenAt || 0)
    };

    if (selectedMode === 'scoring') {
      scorePulseTimer = window.setInterval(sendScorePulse, SCORE_PULSE_INTERVAL_MS);
    }

    clockTimer = window.setInterval(renderStats, 1000);
    setStatus(`測試已開始：${team.name}。等待測試開始後的第一個新心跳。`);
  } catch (error) {
    console.error(error);
    setStatus('無法開始測試，請確認伺服器和設備連線狀態。');
  } finally {
    enduranceClearButton.disabled = false;
    render();
  }
}

async function ensureScoringMode() {
  if (mode === 'scoring') {
    return;
  }

  const response = await fetch('/api/admin/mode', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ mode: 'scoring' })
  });

  if (!response.ok) {
    throw new Error('Set scoring mode failed');
  }

  mode = 'scoring';
}

async function resetSelectedScore(teamId) {
  const response = await fetch(`/api/teams/${teamId}/score`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ score: 0 })
  });

  if (!response.ok) {
    throw new Error('Reset selected score failed');
  }
}

async function sendScorePulse() {
  if (!run.active || run.mode !== 'scoring') {
    return;
  }

  try {
    const response = await fetch(`/api/teams/${run.teamId}/count`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ delta: 1 })
    });

    if (!response.ok) {
      throw new Error('Score pulse failed');
    }

    renderStats();
  } catch (error) {
    console.error(error);
    setStatus('模擬加分失敗，測試仍會繼續記錄心跳。');
  }
}

function handleHeartbeatUpdate() {
  if (!run.active) {
    return;
  }

  const team = getRunTeam();
  if (!team) {
    return;
  }

  const seenAt = Number(team.deviceLastSeenAt || 0);
  if (seenAt > run.lastSeenValue && seenAt >= run.startedAt) {
    if (!run.firstHeartbeatAt) {
      run.firstHeartbeatAt = seenAt;
      setStatus('已收到第一個心跳，正在記錄續航。');
    }
    run.lastHeartbeatAt = seenAt;
    run.lastSeenValue = seenAt;
    if (run.mode === 'scoring') {
      run.lightCount = Math.max(run.lightCount, Number(team.scoreCount || 0));
    }
  }

  if (run.firstHeartbeatAt && !team.deviceOnline && Date.now() - run.lastHeartbeatAt > deviceTimeoutMs) {
    finishRun('設備離線，測試結束');
  }
}

function finishRun(reason) {
  if (!run.active) {
    return;
  }

  window.clearInterval(scorePulseTimer);
  window.clearInterval(clockTimer);
  scorePulseTimer = null;
  clockTimer = null;
  run.active = false;
  run.finishReason = reason;

  const duration = formatDuration(getDurationMs(run.firstHeartbeatAt, run.lastHeartbeatAt));
  if (!run.firstHeartbeatAt) {
    setStatus(`${reason}：尚未收到測試開始後的新心跳。`);
  } else if (run.mode === 'scoring') {
    setStatus(`${reason}：總共亮了 ${run.lightCount} 次，續航 ${duration}。`);
  } else {
    setStatus(`${reason}：續航 ${duration}。`);
  }

  render();
}

function clearRun() {
  if (run.active) {
    finishRun('手動停止');
  }
  run = createEmptyRun();
  setStatus('選擇一台在線機器後開始測試。');
  render();
}

function setStatus(message) {
  enduranceStatus.textContent = message;
}

function getDurationMs(firstAt, lastAt) {
  if (!firstAt || !lastAt || lastAt < firstAt) {
    return 0;
  }
  return lastAt - firstAt;
}

function formatDuration(ms) {
  const totalSeconds = Math.max(0, Math.floor(ms / 1000));
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;
  return [hours, minutes, seconds].map((value) => String(value).padStart(2, '0')).join(':');
}

function formatTime(timestamp) {
  if (!timestamp) {
    return '尚未收到';
  }

  return new Date(timestamp).toLocaleTimeString('zh-TW', {
    hour12: false,
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit'
  });
}

function formatMode(value) {
  if (value === 'scoring') {
    return '計分模式';
  }
  if (value === 'banquet') {
    return '圓滿餐會';
  }
  return '未設定';
}

function escapeHtml(value) {
  return String(value)
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#39;');
}
