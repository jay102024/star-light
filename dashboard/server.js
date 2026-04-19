const express = require('express');
const fs = require('fs/promises');
const http = require('http');
const path = require('path');
const { Server } = require('socket.io');

const PORT = Number(process.env.PORT || 3000);
const ADMIN_PATH_SEGMENT = process.env.ADMIN_PATH_SEGMENT || 'admin-J2E13412';
const DEFAULT_TEAM_COUNT = Number(process.env.DEFAULT_TEAM_COUNT || 20);
const DEVICE_TIMEOUT_MS = Number(process.env.DEVICE_TIMEOUT_MS || 15000);
const CLIENT_TIMEOUT_MS = Number(process.env.CLIENT_TIMEOUT_MS || 60000);
const DATA_DIR = path.join(__dirname, 'data');
const DATA_FILE = path.join(DATA_DIR, 'state.json');

const app = express();
const server = http.createServer(app);
const io = new Server(server);

let state = createDefaultState();
let persistTimer = null;

app.use(express.json());
app.use(express.urlencoded({ extended: false }));
app.use('/assets', express.static(path.join(__dirname, 'public', 'assets')));

app.get('/star_night.png', (req, res) => {
  res.sendFile(path.join(__dirname, '..', 'star_night.png'));
});

app.get('/big_star.png', (req, res) => {
  res.sendFile(path.join(__dirname, '..', 'big_star.png'));
});

app.get('/small_star.png', (req, res) => {
  res.sendFile(path.join(__dirname, '..', 'small_star.png'));
});

app.get('/favicon.ico', (req, res) => {
  res.status(204).end();
});

app.get('/test.mp4', (req, res) => {
  res.sendFile(path.join(__dirname, '..', 'test.mp4'), {
    acceptRanges: false,
    cacheControl: false,
    headers: {
      'Cache-Control': 'no-store'
    }
  });
});

app.get('/doraemon.mp4', (req, res) => {
  res.sendFile(path.join(__dirname, '..', 'doraemon.mp4'), {
    acceptRanges: false,
    cacheControl: false,
    headers: {
      'Cache-Control': 'no-store'
    }
  });
});

app.get('/God_Can_You_Hear_Me.mp4', (req, res) => {
  res.sendFile(path.join(__dirname, '..', 'God_Can_You_Hear_Me.mp4'), {
    acceptRanges: false,
    cacheControl: false,
    headers: {
      'Cache-Control': 'no-store'
    }
  });
});

app.get('/', (req, res) => {
  res.redirect('/client');
});

app.get('/client', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'client.html'));
});

app.get('/leaderboard', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'leaderboard.html'));
});

app.get('/star_night', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'star_night.html'));
});

app.get('/test-J2E13412', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'test-center.html'));
});

app.get('/test-J2E13412/star-test', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'star-test.html'));
});

app.get('/test-J2E13412/brightness-test', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'brightness-test.html'));
});

app.get(`/${ADMIN_PATH_SEGMENT}`, (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'admin.html'));
});

app.get('/api/bootstrap', (req, res) => {
  res.json({
    mode: state.mode,
    teams: serializeTeams(),
    adminPath: `/${ADMIN_PATH_SEGMENT}`,
    deviceTimeoutMs: DEVICE_TIMEOUT_MS,
    clientTimeoutMs: CLIENT_TIMEOUT_MS,
    serverTime: Date.now()
  });
});

app.post('/api/admin/mode', (req, res) => {
  const allowed = ['scoring', 'banquet'];
  const mode = req.body.mode;
  if (!allowed.includes(mode)) {
    return res.status(400).json({ error: 'Invalid mode' });
  }

  state.mode = mode;
  publishState();
  return res.json({ ok: true, mode: state.mode });
});

app.post('/api/teams/:teamId/target', (req, res) => {
  const team = findTeam(req.params.teamId);
  if (!team) {
    return res.status(404).json({ error: 'Team not found' });
  }

  const target = normalizeNumber(req.body.target);
  team.target = Math.max(0, target);
  team.count = clampBanquetCount(team, team.count);
  team.updatedAt = Date.now();
  publishState();
  return res.json({ team: serializeTeam(team) });
});

app.post('/api/teams/:teamId/count', (req, res) => {
  const team = findTeam(req.params.teamId);
  if (!team) {
    return res.status(404).json({ error: 'Team not found' });
  }

  const delta = normalizeNumber(req.body.delta);
  if (state.mode === 'scoring') {
    team.scoreCount = Math.max(0, team.scoreCount + delta);
  } else {
    team.count = clampBanquetCount(team, team.count + delta);
  }
  team.clientLastSeenAt = Date.now();
  team.updatedAt = Date.now();
  publishState();
  return res.json({ team: serializeTeam(team) });
});

app.post('/api/teams/:teamId/score', (req, res) => {
  const team = findTeam(req.params.teamId);
  if (!team) {
    return res.status(404).json({ error: 'Team not found' });
  }

  team.scoreCount = Math.max(0, normalizeNumber(req.body.score));
  team.updatedAt = Date.now();
  publishState();
  return res.json({ team: serializeTeam(team) });
});

app.post('/api/teams/:teamId/reset', (req, res) => {
  const team = findTeam(req.params.teamId);
  if (!team) {
    return res.status(404).json({ error: 'Team not found' });
  }

  if (state.mode === 'scoring') {
    team.scoreCount = 0;
  } else {
    team.count = 0;
  }
  team.updatedAt = Date.now();
  publishState();
  return res.json({ team: serializeTeam(team) });
});

app.post('/api/teams/:teamId/test-light', (req, res) => {
  const team = findTeam(req.params.teamId);
  if (!team) {
    return res.status(404).json({ error: 'Team not found' });
  }

  team.testLightSeq = normalizeNumber(team.testLightSeq) + 1;
  team.testLightMode = 'classic';
  team.testLightColorIndex = 0;
  team.testLightBrightness = 220;
  team.testLightFinalMin = 10;
  team.testLightFinalMax = 255;
  team.testLightFinalPeriodMs = 9000;
  team.updatedAt = Date.now();
  publishState();
  return res.json({ team: serializeTeam(team) });
});

app.post('/api/teams/:teamId/test-light-config', (req, res) => {
  const team = findTeam(req.params.teamId);
  if (!team) {
    return res.status(404).json({ error: 'Team not found' });
  }

  const mode = String(req.body.mode || '').trim();
  if (!['switch', 'final'].includes(mode)) {
    return res.status(400).json({ error: 'Invalid light test mode' });
  }

  team.testLightMode = mode;
  team.testLightColorIndex = clampColorIndex(req.body.colorIndex);
  team.testLightBrightness = clampBrightness(req.body.brightness);
  team.testLightFinalMin = clampFinalBrightnessMin(req.body.finalMin);
  team.testLightFinalMax = clampFinalBrightnessMax(req.body.finalMax, team.testLightFinalMin);
  team.testLightFinalPeriodMs = clampFinalPeriodMs(req.body.finalPeriodMs);
  team.testLightSeq = normalizeNumber(team.testLightSeq) + 1;
  team.updatedAt = Date.now();

  publishState();
  return res.json({ team: serializeTeam(team) });
});

app.get('/api/teams/:teamId/state', (req, res) => {
  const team = findTeam(req.params.teamId);
  if (!team) {
    return res.status(404).json({ error: 'Team not found' });
  }

  const activeCount = state.mode === 'scoring' ? team.scoreCount : team.count;

  return res.json({
    id: team.id,
    mode: state.mode,
    count: activeCount,
    target: team.target,
    testLightSeq: normalizeNumber(team.testLightSeq),
    testLightMode: normalizeLightTestMode(team.testLightMode),
    testLightColorIndex: clampColorIndex(team.testLightColorIndex),
    testLightBrightness: clampBrightness(team.testLightBrightness),
    testLightFinalMin: clampFinalBrightnessMin(team.testLightFinalMin),
    testLightFinalMax: clampFinalBrightnessMax(team.testLightFinalMax, clampFinalBrightnessMin(team.testLightFinalMin)),
    testLightFinalPeriodMs: clampFinalPeriodMs(team.testLightFinalPeriodMs),
    updatedAt: team.updatedAt
  });
});

app.post('/api/admin/reset-all', (req, res) => {
  const now = Date.now();
  state.teams.forEach((team) => {
    if (state.mode === 'scoring') {
      team.scoreCount = 0;
    } else {
      team.count = 0;
      team.clientLastSeenAt = 0;
    }
    team.updatedAt = now;
  });

  publishState();
  return res.json({ ok: true, teams: serializeTeams() });
});

app.post('/api/teams/:teamId/client-presence', (req, res) => {
  const team = findTeam(req.params.teamId);
  if (!team) {
    return res.status(404).json({ error: 'Team not found' });
  }

  team.clientLastSeenAt = Date.now();
  publishState(false);
  return res.json({ ok: true });
});

app.post('/api/devices/heartbeat', (req, res) => {
  const team = findTeam(req.body.teamId);
  if (!team) {
    return res.status(404).json({ error: 'Team not found' });
  }

  const now = Date.now();
  const deviceId = String(req.body.deviceId || '').trim();
  const nextCount = req.body.count;

  team.deviceId = deviceId || team.deviceId;
  team.deviceLastSeenAt = now;
  team.updatedAt = now;

  if (nextCount !== undefined) {
    const count = Math.max(0, normalizeNumber(nextCount));
    if (state.mode === 'scoring') {
      team.scoreCount = count;
    } else {
      team.count = clampBanquetCount(team, count);
    }
  }

  publishState();
  return res.json({ ok: true, team: serializeTeam(team) });
});

app.get('/healthz', (req, res) => {
  res.json({ ok: true });
});

io.on('connection', (socket) => {
  socket.emit('state', { teams: serializeTeams(), mode: state.mode });
});

setInterval(() => {
  io.emit('state', { teams: serializeTeams(), mode: state.mode });
}, 5000);

boot().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});

async function boot() {
  state = await loadState();
  server.listen(PORT, () => {
    console.log(`Dashboard server running at http://localhost:${PORT}/client`);
    console.log(`Hidden admin route: http://localhost:${PORT}/${ADMIN_PATH_SEGMENT}`);
  });
}

function createDefaultState() {
  return {
    mode: null,
    teams: Array.from({ length: DEFAULT_TEAM_COUNT }, (_, index) => ({
      id: `team-${index + 1}`,
      name: `第 ${index + 1} 桌`,
      count: 0,
      scoreCount: 0,
      target: 0,
      testLightSeq: 0,
      testLightMode: 'classic',
      testLightColorIndex: 0,
      testLightBrightness: 220,
      testLightFinalMin: 10,
      testLightFinalMax: 255,
      testLightFinalPeriodMs: 9000,
      deviceId: '',
      deviceLastSeenAt: 0,
      clientLastSeenAt: 0,
      updatedAt: Date.now()
    }))
  };
}

async function loadState() {
  try {
    const raw = await fs.readFile(DATA_FILE, 'utf8');
    const parsed = JSON.parse(raw);
    return normalizeState(parsed);
  } catch (error) {
    if (error.code !== 'ENOENT') {
      console.warn('Failed to load persisted state, using defaults.');
      console.warn(error);
    }

    const initial = createDefaultState();
    await persistState(initial);
    return initial;
  }
}

function normalizeState(input) {
  const base = createDefaultState();
  if (!input || !Array.isArray(input.teams)) {
    return base;
  }

  base.mode = ['scoring', 'banquet'].includes(input.mode) ? input.mode : null;

  const teams = input.teams.slice(0, DEFAULT_TEAM_COUNT).map((team, index) => ({
    id: typeof team.id === 'string' && team.id ? team.id : `team-${index + 1}`,
    name: typeof team.name === 'string' && team.name ? team.name : `第 ${index + 1} 桌`,
    count: Math.max(0, normalizeNumber(team.count)),
    scoreCount: Math.max(0, normalizeNumber(team.scoreCount)),
    target: Math.max(0, normalizeNumber(team.target)),
    testLightSeq: Math.max(0, normalizeNumber(team.testLightSeq)),
    testLightMode: normalizeLightTestMode(team.testLightMode),
    testLightColorIndex: clampColorIndex(team.testLightColorIndex),
    testLightBrightness: clampBrightness(team.testLightBrightness),
    testLightFinalMin: clampFinalBrightnessMin(team.testLightFinalMin),
    testLightFinalMax: clampFinalBrightnessMax(team.testLightFinalMax, clampFinalBrightnessMin(team.testLightFinalMin)),
    testLightFinalPeriodMs: clampFinalPeriodMs(team.testLightFinalPeriodMs),
    deviceId: typeof team.deviceId === 'string' ? team.deviceId : '',
    deviceLastSeenAt: normalizeNumber(team.deviceLastSeenAt),
    clientLastSeenAt: normalizeNumber(team.clientLastSeenAt),
    updatedAt: normalizeNumber(team.updatedAt) || Date.now()
  }));

  teams.forEach((team) => {
    team.count = clampBanquetCount(team, team.count);
  });

  while (teams.length < DEFAULT_TEAM_COUNT) {
    const nextIndex = teams.length + 1;
    teams.push({
      id: `team-${nextIndex}`,
      name: `第 ${nextIndex} 桌`,
      count: 0,
      scoreCount: 0,
      target: 0,
      testLightSeq: 0,
      testLightMode: 'classic',
      testLightColorIndex: 0,
      testLightBrightness: 220,
      testLightFinalMin: 10,
      testLightFinalMax: 255,
      testLightFinalPeriodMs: 9000,
      deviceId: '',
      deviceLastSeenAt: 0,
      clientLastSeenAt: 0,
      updatedAt: Date.now()
    });
  }

  base.teams = teams;
  return base;
}

function findTeam(teamId) {
  return state.teams.find((team) => team.id === teamId);
}

function clampBanquetCount(team, nextCount) {
  const safeCount = Math.max(0, normalizeNumber(nextCount));
  const limit = Math.max(0, normalizeNumber(team.target));
  if (limit <= 0) {
    return safeCount;
  }

  return Math.min(safeCount, limit);
}

function serializeTeams() {
  return state.teams.map(serializeTeam);
}

function serializeTeam(team) {
  const now = Date.now();
  const deviceOnline = now - team.deviceLastSeenAt <= DEVICE_TIMEOUT_MS;
  const clientActive = now - team.clientLastSeenAt <= CLIENT_TIMEOUT_MS;
  const completed = team.target > 0 && team.count >= team.target;

  return {
    id: team.id,
    name: team.name,
    count: team.count,
    scoreCount: team.scoreCount,
    target: team.target,
    testLightSeq: normalizeNumber(team.testLightSeq),
    testLightMode: normalizeLightTestMode(team.testLightMode),
    testLightColorIndex: clampColorIndex(team.testLightColorIndex),
    testLightBrightness: clampBrightness(team.testLightBrightness),
    testLightFinalMin: clampFinalBrightnessMin(team.testLightFinalMin),
    testLightFinalMax: clampFinalBrightnessMax(team.testLightFinalMax, clampFinalBrightnessMin(team.testLightFinalMin)),
    testLightFinalPeriodMs: clampFinalPeriodMs(team.testLightFinalPeriodMs),
    deviceId: team.deviceId,
    deviceOnline,
    clientActive,
    completed,
    remaining: Math.max(0, team.target - team.count),
    updatedAt: team.updatedAt,
    deviceLastSeenAt: team.deviceLastSeenAt
  };
}

function normalizeNumber(value) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : 0;
}

function normalizeLightTestMode(value) {
  return value === 'switch' || value === 'final' ? value : 'classic';
}

function clampColorIndex(value) {
  const normalized = normalizeNumber(value);
  return Math.min(19, Math.max(0, normalized || 0));
}

function clampBrightness(value) {
  const normalized = normalizeNumber(value);
  return Math.min(255, Math.max(0, normalized || 220));
}

function clampFinalBrightnessMin(value) {
  const normalized = normalizeNumber(value);
  return Math.min(254, Math.max(0, normalized || 0));
}

function clampFinalBrightnessMax(value, minValue = 0) {
  const normalized = normalizeNumber(value);
  const minBound = Math.min(254, Math.max(0, minValue));
  return Math.min(255, Math.max(minBound + 1, normalized || 225));
}

function clampFinalPeriodMs(value) {
  const normalized = normalizeNumber(value);
  return Math.min(60000, Math.max(100, normalized || 9000));
}

function publishState(shouldPersist = true) {
  io.emit('state', { teams: serializeTeams(), mode: state.mode });
  if (shouldPersist) {
    schedulePersist();
  }
}

function schedulePersist() {
  clearTimeout(persistTimer);
  persistTimer = setTimeout(() => {
    persistState(state).catch((error) => {
      console.error('Failed to persist dashboard state');
      console.error(error);
    });
  }, 150);
}

async function persistState(nextState) {
  await fs.mkdir(DATA_DIR, { recursive: true });
  await fs.writeFile(DATA_FILE, JSON.stringify(nextState, null, 2), 'utf8');
}