const brightnessDeviceList = document.getElementById('brightnessDeviceList');
const brightnessStatus = document.getElementById('brightnessStatus');
const tabSwitch = document.getElementById('tabSwitch');
const tabFinal = document.getElementById('tabFinal');
const switchPanel = document.getElementById('switchPanel');
const finalPanel = document.getElementById('finalPanel');
const swatchGrid = document.getElementById('swatchGrid');
const switchBrightness = document.getElementById('switchBrightness');
const applySwitchButton = document.getElementById('applySwitchButton');
const finalMin = document.getElementById('finalMin');
const finalMax = document.getElementById('finalMax');
const finalPeriod = document.getElementById('finalPeriod');
const applyFinalButton = document.getElementById('applyFinalButton');

const PALETTE = [
  { name: 'Lavender', hex: '#E6E6FA' },
  { name: 'LightPink', hex: '#FFB6C1' },
  { name: 'PaleTurquoise', hex: '#AFEEEE' },
  { name: 'PowderBlue', hex: '#B0E0E6' },
  { name: 'Thistle', hex: '#D8BFD8' },
  { name: 'Violet', hex: '#EE82EE' },
  { name: 'CornflowerBlue', hex: '#6495ED' },
  { name: 'MediumAquamarine', hex: '#66CDAA' },
  { name: 'Orchid', hex: '#DA70D6' },
  { name: 'SteelBlue', hex: '#4682B4' },
  { name: 'MediumPurple', hex: '#9370DB' },
  { name: 'Coral', hex: '#FF7F50' },
  { name: 'SkyBlue', hex: '#87CEEB' },
  { name: 'LightSeaGreen', hex: '#20B2AA' },
  { name: 'MediumSlateBlue', hex: '#7B68EE' },
  { name: 'Aquamarine', hex: '#7FFFD4' },
  { name: 'Pink', hex: '#FFC0CB' },
  { name: 'Turquoise', hex: '#40E0D0' },
  { name: 'Plum', hex: '#DDA0DD' },
  { name: 'DarkTurquoise', hex: '#00CED1' }
];

let teams = [];
let selectedTeamId = '';
let selectedColorIdx = 0;

bootstrap();
buildSwatchGrid();

tabSwitch.addEventListener('click', () => switchTab('switch'));
tabFinal.addEventListener('click', () => switchTab('final'));
applySwitchButton.addEventListener('click', applySwitch);
applyFinalButton.addEventListener('click', applyFinal);

function setStatus(message) {
  brightnessStatus.textContent = message;
}

function switchTab(mode) {
  const isSwitch = mode === 'switch';
  switchPanel.classList.toggle('hidden', !isSwitch);
  finalPanel.classList.toggle('hidden', isSwitch);
  tabSwitch.classList.toggle('active', isSwitch);
  tabFinal.classList.toggle('active', !isSwitch);
}

function buildSwatchGrid() {
  swatchGrid.innerHTML = PALETTE.map((color, index) => `
    <button class="swatch ${index === selectedColorIdx ? 'selected' : ''}" type="button" data-color-index="${index}" title="${color.name}" style="background:${color.hex}"></button>
  `).join('');

  swatchGrid.querySelectorAll('[data-color-index]').forEach((button) => {
    button.addEventListener('click', () => {
      selectedColorIdx = Number(button.dataset.colorIndex || 0);
      buildSwatchGrid();
    });
  });
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
    brightnessDeviceList.innerHTML = '<p class="sound-empty">目前沒有在線機器。</p>';
    setStatus('沒有可用的在線機器');
    return;
  }

  brightnessDeviceList.innerHTML = onlineTeams.map((team) => `
    <button class="sound-device-card ${team.id === selectedTeamId ? 'selected' : ''}" type="button" data-team-id="${team.id}">
      <strong>${escapeHtml(team.name)}</strong>
      <span>${escapeHtml(team.deviceId || team.id)}</span>
    </button>
  `).join('');

  brightnessDeviceList.querySelectorAll('[data-team-id]').forEach((button) => {
    button.addEventListener('click', () => {
      selectedTeamId = button.dataset.teamId;
      renderDeviceList();
      const team = teams.find((item) => item.id === selectedTeamId);
      setStatus(`已選擇 ${team ? team.name : selectedTeamId}`);
    });
  });
}

async function postLightTestConfig(payload) {
  if (!selectedTeamId) {
    setStatus('請先選擇一台在線機器');
    return;
  }

  const response = await fetch(`/api/teams/${selectedTeamId}/test-light-config`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  });

  if (!response.ok) {
    throw new Error('Light test request failed');
  }
}

async function applySwitch() {
  const brightness = clampNumber(Number(switchBrightness.value), 0, 255, 80);

  try {
    await postLightTestConfig({
      mode: 'switch',
      colorIndex: selectedColorIdx,
      brightness
    });

    const team = teams.find((item) => item.id === selectedTeamId);
    setStatus(`已送出切換測試到 ${team ? team.name : selectedTeamId}`);
  } catch (error) {
    console.error(error);
    setStatus('送出切換測試失敗，請稍後再試。');
  }
}

async function applyFinal() {
  const minBr = clampNumber(Number(finalMin.value), 0, 254, 10);
  const maxBr = clampNumber(Number(finalMax.value), minBr + 1, 255, 225);
  const periodSeconds = clampNumber(Number(finalPeriod.value), 0.1, 60, 9);
  const finalPeriodMs = Math.round(periodSeconds * 1000);

  try {
    await postLightTestConfig({
      mode: 'final',
      finalMin: minBr,
      finalMax: maxBr,
      finalPeriodMs
    });

    const team = teams.find((item) => item.id === selectedTeamId);
    setStatus(`已送出最終測試到 ${team ? team.name : selectedTeamId}`);
  } catch (error) {
    console.error(error);
    setStatus('送出最終測試失敗，請稍後再試。');
  }
}

function clampNumber(value, min, max, fallback) {
  const safeValue = Number.isFinite(value) ? value : fallback;
  return Math.min(max, Math.max(min, safeValue));
}

function escapeHtml(value) {
  return String(value)
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#39;');
}
