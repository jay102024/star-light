const soundForm = document.getElementById('soundForm');
const soundStatus = document.getElementById('soundStatus');
const soundDeviceList = document.getElementById('soundDeviceList');
const notesCountInput = document.getElementById('notesCount');
const sequenceInputsContainer = document.getElementById('sequenceInputs');

const soundPresets = Array.from(document.querySelectorAll('.sound-preset'));

let teams = [];
let selectedTeamId = '';

bootstrap();
renderSequenceInputs();
notesCountInput.addEventListener('change', renderSequenceInputs);

function setStatus(message) {
  soundStatus.textContent = message;
}

function renderSequenceInputs() {
  const count = Math.max(1, Math.min(16, Number(notesCountInput.value) || 2));
  notesCountInput.value = count;
  
  let html = '';
  for (let i = 0; i < count; i++) {
    const noteNum = i + 1;
    html += `
      <div style="margin-bottom: 12px; padding: 12px; background: #fafafa; border-left: 3px solid #4a90e2; border-radius: 4px;">
        <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 12px;">
          <div>
            <label for="freq${i}" style="font-size: 12px; color: #666;">音符 ${noteNum} - 頻率 (Hz) / 0=靜音</label>
            <input id="freq${i}" type="number" min="0" max="20000" step="1" value="${i === 0 ? 1319 : i === 1 ? 1568 : 0}" class="sequence-freq" style="width: 100%; padding: 6px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box;">
          </div>
          <div>
            <label for="dur${i}" style="font-size: 12px; color: #666;">音符 ${noteNum} - 時間 (ms)</label>
            <input id="dur${i}" type="number" min="10" max="5000" step="1" value="${i === 0 ? 100 : 150}" class="sequence-dur" style="width: 100%; padding: 6px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box;">
          </div>
        </div>
      </div>
    `;
  }
  sequenceInputsContainer.innerHTML = html;
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

async function sendRemoteSound(frequencyOne, durationOne, frequencyTwo, durationTwo) {
  if (!selectedTeamId) {
    setStatus('請先選擇一台在線機器');
    return;
  }

  const response = await fetch(`/api/teams/${selectedTeamId}/test-buzzer`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      frequencyOne,
      durationOne,
      frequencyTwo,
      durationTwo
    })
  });

  if (!response.ok) {
    throw new Error('Remote sound request failed');
  }

  const team = teams.find((item) => item.id === selectedTeamId);
  setStatus(`已送出到 ${team ? team.name : selectedTeamId}: ${frequencyOne}Hz-${durationOne}ms, ${frequencyTwo}Hz-${durationTwo}ms`);
}

async function sendRemoteSequence(frequencies, durations) {
  if (!selectedTeamId) {
    setStatus('請先選擇一台在線機器');
    return;
  }

  console.log('[sendRemoteSequence] Sending to server:', { frequencies, durations });

  const response = await fetch(`/api/teams/${selectedTeamId}/test-buzzer`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      frequencies,
      durations
    })
  });

  if (!response.ok) {
    throw new Error('Remote sequence request failed');
  }

  const team = teams.find((item) => item.id === selectedTeamId);
  const totalDuration = durations.reduce((sum, d) => sum + d, 0);
  setStatus(`已送出序列到 ${team ? team.name : selectedTeamId}（${frequencies.length} 個音符，共 ${totalDuration}ms）`);
}

async function playSequence(frequencies, durations) {
  if (!selectedTeamId) {
    setStatus('請先選擇一台在線機器');
    return;
  }

  const team = teams.find((item) => item.id === selectedTeamId);
  setStatus(`正在播放序列到 ${team ? team.name : selectedTeamId}...`);

  try {
    await sendRemoteSequence(frequencies, durations);
  } catch (error) {
    console.error(error);
    setStatus('播放序列失敗，請稍後再試。');
  }
}

soundPresets.forEach((button) => {
  button.addEventListener('click', async () => {
    // Check if this is a sequence preset
    if (button.dataset.sequence === 'yes') {
      try {
        const seqData = JSON.parse(button.dataset.sequenceJson);
        await playSequence(seqData.freqs, seqData.durs);
      } catch (error) {
        console.error(error);
        setStatus('播放序列失敗，請稍後再試。');
      }
    } else {
      // Legacy dual-tone preset
      const f1 = Number(button.dataset.f1);
      const d1 = Number(button.dataset.d1);
      const f2 = Number(button.dataset.f2);
      const d2 = Number(button.dataset.d2);
      try {
        await sendRemoteSound(f1, d1, f2, d2);
      } catch (error) {
        console.error(error);
        setStatus('送出音效失敗，請稍後再試。');
      }
    }
  });
});

soundForm.addEventListener('submit', async (event) => {
  event.preventDefault();

  const freqs = Array.from(document.querySelectorAll('.sequence-freq')).map(el => Number(el.value) || 0);
  const durs = Array.from(document.querySelectorAll('.sequence-dur')).map(el => Number(el.value) || 100);

  console.log('[soundForm] User input:', { freqs, durs });

  // Validate inputs
  for (let i = 0; i < freqs.length; i++) {
    if (freqs[i] < 0 || freqs[i] > 20000) {
      setStatus(`音符 ${i + 1} 頻率範圍錯誤（0-20000 Hz）`);
      return;
    }
    if (durs[i] < 10 || durs[i] > 5000) {
      setStatus(`音符 ${i + 1} 時間範圍錯誤（10-5000 ms）`);
      return;
    }
  }

  try {
    await playSequence(freqs, durs);
  } catch (error) {
    console.error(error);
    setStatus('送出音效失敗，請稍後再試。');
  }
});

function escapeHtml(value) {
  return String(value)
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#39;');
}
