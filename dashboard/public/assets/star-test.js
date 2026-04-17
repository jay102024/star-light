const nightSkyToggleButton = document.getElementById('nightSkyToggleButton');
const nightSkyResetButton = document.getElementById('nightSkyResetButton');
const nightSkyModeValue = document.getElementById('nightSkyModeValue');
const nightSkyCountValue = document.getElementById('nightSkyCountValue');
const nightSkyCompletedValue = document.getElementById('nightSkyCompletedValue');
const nightSkyStatusValue = document.getElementById('nightSkyStatusValue');

let teams = [];
let mode = null;
let nightSkyTestTimer = null;

bootstrap();

nightSkyToggleButton.addEventListener('click', toggleNightSkyTest);
nightSkyResetButton.addEventListener('click', resetAllCounts);

async function bootstrap() {
  const response = await fetch('/api/bootstrap');
  const data = await response.json();
  teams = data.teams;
  mode = data.mode;
  renderSnapshot();

  const socket = io();
  socket.on('state', (payload) => {
    teams = payload.teams;
    mode = payload.mode;
    renderSnapshot();
  });
}

function renderSnapshot() {
  const totalCount = teams.reduce((sum, team) => sum + Number(team.count || 0), 0);
  const completedCount = teams.filter((team) => team.completed).length;

  nightSkyModeValue.textContent = mode === 'banquet' ? '圓滿餐會' : mode === 'scoring' ? '計分模式' : '未設定';
  nightSkyCountValue.textContent = String(totalCount);
  nightSkyCompletedValue.textContent = String(completedCount);

  if (nightSkyTestTimer !== null) {
    nightSkyStatusValue.textContent = '測試進行中';
    nightSkyToggleButton.textContent = '停止夜空測試';
  } else {
    nightSkyStatusValue.textContent = '待命中';
    nightSkyToggleButton.textContent = '開始夜空測試';
  }
}

async function ensureBanquetMode() {
  if (mode === 'banquet') {
    return;
  }

  const response = await fetch('/api/admin/mode', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ mode: 'banquet' })
  });

  if (!response.ok) {
    throw new Error('Set banquet mode failed');
  }

  mode = 'banquet';
}

async function resetAllCounts() {
  const response = await fetch('/api/admin/reset-all', { method: 'POST' });
  if (!response.ok) {
    throw new Error('Reset all failed');
  }
}

async function toggleNightSkyTest() {
  if (nightSkyTestTimer !== null) {
    window.clearInterval(nightSkyTestTimer);
    nightSkyTestTimer = null;
    renderSnapshot();
    return;
  }

  const confirmed = window.confirm('開始夜空測試前，會先切到圓滿餐會模式，並將所有桌次數字歸零。要繼續嗎？');
  if (!confirmed) {
    return;
  }

  nightSkyToggleButton.disabled = true;
  nightSkyResetButton.disabled = true;
  try {
    await ensureBanquetMode();
    await resetAllCounts();
  } catch (error) {
    console.error(error);
    window.alert('夜空測試初始化失敗，請稍後再試。');
    nightSkyToggleButton.disabled = false;
    nightSkyResetButton.disabled = false;
    return;
  }

  nightSkyToggleButton.disabled = false;
  nightSkyResetButton.disabled = false;

  nightSkyTestTimer = window.setInterval(async () => {
    const snapshot = [...teams];
    for (const team of snapshot) {
      const delta = Math.floor(Math.random() * 3);
      if (delta === 0) {
        continue;
      }

      try {
        await fetch(`/api/teams/${team.id}/count`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ delta })
        });
      } catch (error) {
        console.error(error);
      }
    }
  }, 1000);

  renderSnapshot();
}
