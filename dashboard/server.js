const express = require('express');
const fs = require('fs/promises');
const http = require('http');
const path = require('path');
const { Server } = require('socket.io');
require('dotenv').config();
const mysql = require('mysql2/promise');

const PORT = Number(process.env.PORT || 3000);
const ADMIN_PATH_SEGMENT = process.env.ADMIN_PATH_SEGMENT || 'admin-J2E13412';
const DEFAULT_TEAM_COUNT = Number(process.env.DEFAULT_TEAM_COUNT || 30);
const DEVICE_TIMEOUT_MS = Number(process.env.DEVICE_TIMEOUT_MS || 15000);
const CLIENT_TIMEOUT_MS = Number(process.env.CLIENT_TIMEOUT_MS || 60000);
const DATA_DIR = path.join(__dirname, 'data');
const DATA_FILE = path.join(DATA_DIR, 'state.json');

// MySQL 連接池
const pool = mysql.createPool({
  host: process.env.DB_HOST || 'localhost',
  user: process.env.DB_USER || 'counter_user',
  password: process.env.DB_PASSWORD || '',
  database: process.env.DB_NAME || 'starlight_db',
  port: process.env.DB_PORT || 3306,
  waitForConnections: true,
  connectionLimit: 10,
  queueLimit: 0
});

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

app.get('/golden_dome.glb', (req, res) => {
  res.sendFile(path.join(__dirname, '..', 'golden dome 3d model.glb'));
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

app.get('/test-J2E13412/endurance-test', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'endurance-test.html'));
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

async function loadStateFromDB() {
  const conn = await pool.getConnection();
  try {
    await ensureDatabaseSchema(conn);

    const [rows] = await conn.query('SELECT mode, updated_at FROM global_state WHERE id = 1');
    const [teamRows] = await conn.query('SELECT * FROM teams ORDER BY id');
    
    const state = createDefaultState();
    if (rows && rows.length > 0) {
      state.mode = rows[0].mode || null;
    }
    
    if (teamRows && teamRows.length > 0) {
      state.teams = teamRows.map(row => ({
        id: row.id,
        name: row.name,
        count: row.count,
        scoreCount: row.score_count,
        target: row.target,
        testLightSeq: row.test_light_seq,
        testLightMode: row.test_light_mode,
        testLightColorIndex: row.test_light_color_index,
        testLightBrightness: row.test_light_brightness,
        testLightFinalMin: row.test_light_final_min,
        testLightFinalMax: row.test_light_final_max,
        testLightFinalPeriodMs: row.test_light_final_period_ms,
        deviceId: row.device_id || '',
        deviceLastSeenAt: row.device_last_seen_at,
        clientLastSeenAt: row.client_last_seen_at,
        updatedAt: row.updated_at
      }));
    }
    
    return state;
  } finally {
    conn.release();
  }
}

async function ensureDatabaseSchema(conn) {
  await conn.query(`
    CREATE TABLE IF NOT EXISTS global_state (
      id INT PRIMARY KEY,
      mode VARCHAR(20) NULL,
      updated_at BIGINT NOT NULL DEFAULT 0,
      created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
    )
  `);

  await conn.query(`
    CREATE TABLE IF NOT EXISTS teams (
      id VARCHAR(50) PRIMARY KEY,
      name VARCHAR(100) NOT NULL,
      count INT NOT NULL DEFAULT 0,
      score_count INT NOT NULL DEFAULT 0,
      target INT NOT NULL DEFAULT 0,
      test_light_seq INT NOT NULL DEFAULT 0,
      test_light_mode VARCHAR(20) NOT NULL DEFAULT 'classic',
      test_light_color_index INT NOT NULL DEFAULT 0,
      test_light_brightness INT NOT NULL DEFAULT 220,
      test_light_final_min INT NOT NULL DEFAULT 10,
      test_light_final_max INT NOT NULL DEFAULT 255,
      test_light_final_period_ms INT NOT NULL DEFAULT 9000,
      device_id VARCHAR(100) NOT NULL DEFAULT '',
      device_last_seen_at BIGINT NOT NULL DEFAULT 0,
      client_last_seen_at BIGINT NOT NULL DEFAULT 0,
      updated_at BIGINT NOT NULL DEFAULT 0,
      created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
      INDEX idx_updated_at (updated_at)
    )
  `);

  await conn.query(
    `INSERT INTO global_state (id, mode, updated_at)
     VALUES (1, NULL, ?)
     ON DUPLICATE KEY UPDATE id = id`,
    [Date.now()]
  );

  const now = Date.now();
  const defaults = createDefaultState();
  for (const team of defaults.teams) {
    await conn.query(
      `INSERT INTO teams (
        id, name, count, score_count, target,
        test_light_seq, test_light_mode, test_light_color_index,
        test_light_brightness, test_light_final_min, test_light_final_max,
        test_light_final_period_ms, device_id, device_last_seen_at,
        client_last_seen_at, updated_at
      ) VALUES (?, ?, 0, 0, 0, 0, 'classic', 0, 220, 10, 255, 9000, '', 0, 0, ?)
      ON DUPLICATE KEY UPDATE id = id`,
      [team.id, team.name, now]
    );
  }
}

async function loadState() {
  try {
    return await loadStateFromDB();
  } catch (error) {
    console.warn('Failed to load state from database, using defaults.');
    console.warn(error);
    const initial = createDefaultState();
    try {
      await persistState(initial);
    } catch (persistError) {
      console.warn('Also failed to persist defaults to database.');
    }
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

async function persistStateToDB(nextState) {
  const conn = await pool.getConnection();
  try {
    await conn.beginTransaction();

    await ensureDatabaseSchema(conn);
    
    // 更新全局狀態
    await conn.query(
      `INSERT INTO global_state (id, mode, updated_at)
       VALUES (1, ?, ?)
       ON DUPLICATE KEY UPDATE mode = VALUES(mode), updated_at = VALUES(updated_at)`,
      [nextState.mode, Date.now()]
    );
    
    // 更新團隊
    for (const team of nextState.teams) {
      await conn.query(
        `INSERT INTO teams (
          id, name, count, score_count, target,
          test_light_seq, test_light_mode, test_light_color_index,
          test_light_brightness, test_light_final_min, test_light_final_max,
          test_light_final_period_ms, device_id, device_last_seen_at,
          client_last_seen_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON DUPLICATE KEY UPDATE
          name = VALUES(name),
          count = VALUES(count),
          score_count = VALUES(score_count),
          target = VALUES(target),
          test_light_seq = VALUES(test_light_seq),
          test_light_mode = VALUES(test_light_mode),
          test_light_color_index = VALUES(test_light_color_index),
          test_light_brightness = VALUES(test_light_brightness),
          test_light_final_min = VALUES(test_light_final_min),
          test_light_final_max = VALUES(test_light_final_max),
          test_light_final_period_ms = VALUES(test_light_final_period_ms),
          device_id = VALUES(device_id),
          device_last_seen_at = VALUES(device_last_seen_at),
          client_last_seen_at = VALUES(client_last_seen_at),
          updated_at = VALUES(updated_at)`,
        [
          team.id, team.name, team.count, team.scoreCount, team.target,
          team.testLightSeq, team.testLightMode, team.testLightColorIndex,
          team.testLightBrightness, team.testLightFinalMin, team.testLightFinalMax,
          team.testLightFinalPeriodMs, team.deviceId, team.deviceLastSeenAt,
          team.clientLastSeenAt, team.updatedAt
        ]
      );
    }
    
    await conn.commit();
  } catch (error) {
    await conn.rollback();
    throw error;
  } finally {
    conn.release();
  }
}

async function persistState(nextState) {
  try {
    await persistStateToDB(nextState);
  } catch (error) {
    console.error('Failed to persist state to database');
    console.error(error);
  }
}
