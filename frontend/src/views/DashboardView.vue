<script setup lang="ts">
import { ref, onMounted, onUnmounted, computed, nextTick } from 'vue'

interface HermesInfo {
  alive: boolean
  url: string
  version: string
  model?: string
}
interface SystemData {
  status: string
  version: string
  uptime: number
  httpPort: number
  wsPort: number
  hermes: HermesInfo
}
interface HealthData {
  cpu_usage: number
  memory_total: number
  memory_used: number
  memory_percent: number
  disk_total: number
  disk_used: number
  disk_percent: number
  uptime: number
}
interface SessionInfo {
  id: string
  title?: string
  message_count?: number
  updated_at?: number | string
  last_message_id?: number
}
interface AgentInfo {
  name: string
  status: string
  model: string | null
  active: boolean
}
interface SpeechStatus {
  enabled: boolean
  streaming: boolean
  bufferSeconds?: number
  initialized?: boolean
  connected?: boolean
  text?: string
  lastText?: string
}

const AGENT_COLORS: Record<string, string> = {
  default: 'var(--brand-violet)',
  orchestrator: 'var(--brand-cyan)',
  analyst: 'var(--brand-mint)',
  writer: 'var(--brand-pink)',
  marketer: 'var(--brand-amber)',
  coder: 'var(--brand-magenta)',
}

// ── Состояние ────────────────────────────────────────────────────
const system = ref<SystemData | null>(null)
const health = ref<HealthData | null>(null)
const credits = ref<number | null>(null)
const creditsError = ref<string | null>(null)
const sessions = ref<SessionInfo[]>([])
const agents = ref<AgentInfo[]>([])
const loading = ref(false)
const error = ref<string | null>(null)

// ── Speech: распознавание речи (whisper STT) ───────────────────
const SPEECH_CAPTURE_SECONDS = 10
const speech = ref<{
  status: SpeechStatus | null
  loading: boolean
  lastText: string
  lastAt: string
  error: string | null
}>({ status: null, loading: false, lastText: '', lastAt: '', error: null })

async function loadSpeechStatus() {
  try {
    const resp = await fetch('/api/speech', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ command: 'status' }),
    })
    const data = await resp.json()
    speech.value.status = data as SpeechStatus
    speech.value.error = null
    // Живой текст из непрерывного стрима (lastText отдаёт backend)
    const live = (data.lastText || '').trim()
    if (live) {
      speech.value.lastText = live
      speech.value.lastAt = new Date().toLocaleTimeString('ru-RU', { hour: '2-digit', minute: '2-digit' })
    }
  } catch (e: any) {
    speech.value.error = e.message || 'STT недоступен'
  }
}

async function captureSpeech() {
  if (speech.value.loading) return
  speech.value.loading = true
  speech.value.error = null
  try {
    const resp = await fetch('/api/speech', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ command: 'capture', seconds: SPEECH_CAPTURE_SECONDS }),
    })
    const data = await resp.json()
    speech.value.status = data as SpeechStatus
    const text = (data.text || '').trim()
    if (text) {
      speech.value.lastText = text
      speech.value.lastAt = new Date().toLocaleTimeString('ru-RU', { hour: '2-digit', minute: '2-digit' })
    } else if (data.connected === false) {
      speech.value.error = 'STT-сервер не запущен'
    } else {
      speech.value.error = 'Речь не распознана (тишина в микрофоне?)'
    }
  } catch (e: any) {
    speech.value.error = e.message || 'Ошибка распознавания'
  } finally {
    speech.value.loading = false
  }
}

// Live Ops: текущая директива (циклично по сессиям)
const directiveText = ref('')
const directiveIdx = ref(0)
// Контекст: циклично по агентам
const contextName = ref('')
const contextPct = ref(0)
const contextLast = ref('')
const contextIdx = ref(0)

let pollTimer: ReturnType<typeof setInterval> | null = null
let speechPollTimer: ReturnType<typeof setInterval> | null = null
let directiveTimer: ReturnType<typeof setInterval> | null = null
let contextTimer: ReturnType<typeof setInterval> | null = null

// ── Computed ─────────────────────────────────────────────────────
const fmtBytes = (b: number) => {
  if (!b) return '—'
  const g = b / (1024 ** 3)
  if (g >= 1) return g.toFixed(1) + ' ГБ'
  const m = b / (1024 ** 2)
  if (m >= 1) return m.toFixed(0) + ' МБ'
  return b.toFixed(0) + ' Б'
}

const fmtUptime = (s: number) => {
  if (!s) return '—'
  const d = Math.floor(s / 86400)
  const h = Math.floor((s % 86400) / 3600)
  const m = Math.floor((s % 3600) / 60)
  const parts: string[] = []
  if (d > 0) parts.push(d + ' д')
  if (h > 0) parts.push(h + ' ч')
  if (m > 0 || parts.length === 0) parts.push(m + ' мин')
  return parts.join(' ')
}

const fmtDate = (ts: number | string | undefined) => {
  if (!ts) return '—'
  const ms = typeof ts === 'number'
    ? (ts < 1e12 ? ts * 1000 : ts)
    : new Date(ts).getTime()
  if (isNaN(ms)) return String(ts)
  const d = new Date(ms)
  const now = new Date()
  const sameDay = d.toDateString() === now.toDateString()
  const hm = d.toLocaleTimeString('ru-RU', { hour: '2-digit', minute: '2-digit' })
  if (sameDay) return 'сегодня ' + hm
  const yesterday = new Date(now); yesterday.setDate(now.getDate() - 1)
  if (d.toDateString() === yesterday.toDateString()) return 'вчера ' + hm
  return d.toLocaleDateString('ru-RU', { day: '2-digit', month: '2-digit' }) + ' ' + hm
}

const totalSessions = computed(() => sessions.value.length)
const totalMessages = computed(() =>
  sessions.value.reduce((a, s) => a + (s.message_count || 0), 0))
const activeAgents = computed(() => agents.value.filter(a => a.active))
const todaySessions = computed(() => {
  const now = new Date()
  return sessions.value.filter(s => {
    const ts = typeof s.updated_at === 'number' ? s.updated_at : Date.parse(String(s.updated_at || 0))
    if (isNaN(ts) || !ts) return false
    const d = new Date(ts < 1e12 ? ts * 1000 : ts)
    return d.toDateString() === now.toDateString()
  }).length
})

const recentSessions = computed(() =>
  [...sessions.value]
    .sort((a, b) => {
      const ta = typeof a.updated_at === 'number' ? a.updated_at : Date.parse(String(a.updated_at || 0))
      const tb = typeof b.updated_at === 'number' ? b.updated_at : Date.parse(String(b.updated_at || 0))
      return tb - ta
    })
    .slice(0, 8))

// Sparkline: активность по дням из сессий
const byDay = computed(() => {
  const map = new Map<string, number>()
  const now = new Date()
  for (let i = 6; i >= 0; i--) {
    const d = new Date(now); d.setDate(now.getDate() - i)
    map.set(d.toISOString().slice(0, 10), 0)
  }
  sessions.value.forEach(s => {
    const ts = typeof s.updated_at === 'number' ? s.updated_at : Date.parse(String(s.updated_at || 0))
    if (isNaN(ts) || !ts) return
    const d = new Date(ts < 1e12 ? ts * 1000 : ts)
    const key = d.toISOString().slice(0, 10)
    if (map.has(key)) map.set(key, (map.get(key) || 0) + (s.message_count || 1))
  })
  return [...map.entries()].map(([day, total]) => ({ day: day.slice(5), total }))
})

const peakDay = computed(() => {
  if (!byDay.value.length) return null
  return byDay.value.reduce((a, b) => b.total > a.total ? b : a, byDay.value[0])
})

const healthOk = computed(() => {
  if (!health.value) return 'unknown'
  if (health.value.cpu_usage > 90 || health.value.memory_percent > 90) return 'warn'
  return 'ok'
})

const radarDots = computed(() => {
  const cx = 70, cy = 70, maxR = 58
  const slice = (2 * Math.PI) / Math.max(agents.value.length, 1)
  return agents.value.map((a, i) => {
    const frac = a.active ? 0.85 : 0.35
    const r = Math.max(4, frac * maxR)
    const angle = -Math.PI / 2 + i * slice
    return {
      name: a.name,
      x: cx + r * Math.cos(angle),
      y: cy + r * Math.sin(angle),
      color: AGENT_COLORS[a.name] || 'var(--brand-cyan)',
      active: a.active,
    }
  })
})

// ── API ──────────────────────────────────────────────────────────
async function loadOverview(showSpinner = false) {
  if (showSpinner) loading.value = true
  error.value = null
  try {
    const [sysResp, healthResp, sessResp, agentsResp, creditsResp] = await Promise.all([
      fetch('/api/system').then(r => r.ok ? r.json() : null),
      fetch('/api/health').then(r => r.ok ? r.json() : null),
      fetch('/api/hermes/sessions').then(r => r.ok ? r.json() : null),
      fetch('/api/agents').then(r => r.ok ? r.json() : null),
      fetch('/api/credits').then(r => r.ok ? r.json() : null),
    ])
    if (sysResp) system.value = sysResp as SystemData
    if (healthResp) health.value = healthResp as HealthData
    if (creditsResp) {
      credits.value = typeof creditsResp.credits === 'number' ? creditsResp.credits : null
      creditsError.value = null
    } else {
      creditsError.value = 'недоступен'
    }
    if (sessResp) {
      const raw: any[] = Array.isArray(sessResp) ? sessResp : (sessResp.data || [])
      sessions.value = raw
        .filter((s: any) => (s.id || s.session_id) && (s.message_count === undefined || (s.message_count || 0) > 0))
        .map((s: any) => ({
          id: s.id || s.session_id,
          title: s.title || 'Без названия',
          message_count: s.message_count || 0,
          updated_at: s.updated_at ?? s.updatedAt ?? undefined,
          last_message_id: s.last_message_id || s.lastMessageId || 0,
        }))
    }
    if (agentsResp) {
      const raw: any[] = Array.isArray(agentsResp) ? agentsResp : (agentsResp.profiles || agentsResp.data || [])
      agents.value = raw.map((a: any) => ({
        name: a.name || a.id || String(a),
        status: a.status || 'unknown',
        model: a.model || a.current_model || null,
        active: a.active !== false && (a.status === 'running' || a.status === 'active'),
      }))
    }
    await nextTick()
    renderSparkline()
    await loadSpeechStatus()
  } catch (e: any) {
    error.value = e.message || 'Не удалось загрузить данные'
  } finally {
    if (showSpinner) loading.value = false
  }
}

// ── Sparkline canvas ─────────────────────────────────────────────
function renderSparkline() {
  const canvas = document.getElementById('sparkline-canvas') as HTMLCanvasElement | null
  if (!canvas) return
  const vals = [...byDay.value].map(d => d.total)
  const w = canvas.clientWidth || 500
  const h = 90
  const dpr = window.devicePixelRatio || 1
  canvas.width = w * dpr
  canvas.height = h * dpr
  canvas.style.width = w + 'px'
  canvas.style.height = h + 'px'
  const ctx = canvas.getContext('2d')
  if (!ctx) return
  ctx.scale(dpr, dpr)
  ctx.clearRect(0, 0, w, h)

  if (!vals.length || vals.every(v => v === 0)) {
    ctx.fillStyle = 'rgba(255,255,255,0.06)'
    ctx.fillRect(0, 0, w, h)
    ctx.fillStyle = 'var(--text-muted)'
    ctx.font = '10px var(--font-mono)'
    ctx.textAlign = 'center'
    ctx.fillText('no data yet', w / 2, h / 2 + 4)
    return
  }

  const pad = 8
  const max = Math.max(...vals, 1)

  if (vals.length === 1) {
    const cx = w / 2
    const cy = h - pad - (vals[0] / max) * (h - pad * 2)
    ctx.beginPath()
    ctx.arc(cx, cy, 4, 0, 2 * Math.PI)
    ctx.fillStyle = 'var(--brand-cyan)'
    ctx.shadowColor = 'var(--brand-cyan)'
    ctx.shadowBlur = 12
    ctx.fill()
    ctx.shadowBlur = 0
    ctx.fillStyle = 'var(--text-muted)'
    ctx.font = '10px var(--font-mono)'
    ctx.textAlign = 'center'
    ctx.fillText(String(vals[0]), cx, cy - 10)
    return
  }

  // Grid lines
  ctx.strokeStyle = 'rgba(255,255,255,0.05)'
  ctx.lineWidth = 1
  for (let i = 1; i <= 3; i++) {
    const y = pad + (h - pad * 2) * (i / 4)
    ctx.beginPath()
    ctx.moveTo(pad, y)
    ctx.lineTo(w - pad, y)
    ctx.stroke()
  }

  const stepX = (w - pad * 2) / (vals.length - 1)
  const pts = vals.map((v, i) => ({
    x: pad + i * stepX,
    y: h - pad - (v / max) * (h - pad * 2),
  }))

  // Area fill
  const grad = ctx.createLinearGradient(0, pad, 0, h - pad)
  grad.addColorStop(0, 'rgba(125,211,252,0.25)')
  grad.addColorStop(1, 'rgba(125,211,252,0)')
  ctx.beginPath()
  ctx.moveTo(pts[0].x, h - pad)
  pts.forEach(p => ctx.lineTo(p.x, p.y))
  ctx.lineTo(pts[pts.length - 1].x, h - pad)
  ctx.closePath()
  ctx.fillStyle = grad
  ctx.fill()

  // Line
  ctx.beginPath()
  pts.forEach((p, i) => i === 0 ? ctx.moveTo(p.x, p.y) : ctx.lineTo(p.x, p.y))
  ctx.strokeStyle = 'var(--brand-cyan)'
  ctx.lineWidth = 2
  ctx.shadowColor = 'var(--brand-cyan)'
  ctx.shadowBlur = 8
  ctx.stroke()
  ctx.shadowBlur = 0

  // Dots
  pts.forEach(p => {
    ctx.beginPath()
    ctx.arc(p.x, p.y, 2.5, 0, 2 * Math.PI)
    ctx.fillStyle = 'var(--brand-cyan)'
    ctx.fill()
  })

  // Day labels
  ctx.fillStyle = 'var(--text-muted)'
  ctx.font = '9px var(--font-mono)'
  ctx.textAlign = 'center'
  const labelIdx = [0, Math.floor((vals.length - 1) / 2), vals.length - 1]
  labelIdx.forEach(i => {
    ctx.fillText(byDay.value[i]?.day || '', pts[i].x, h - 2)
  })
}

// ── Directive / Context cycling ──────────────────────────────────
function cycleDirective() {
  const recent = recentSessions.value
  if (!recent.length) {
    directiveText.value = system.value?.hermes?.alive
      ? 'SYSTEM · ожидание задач'
      : 'SYSTEM · hermes оффлайн'
    return
  }
  const entry = recent[directiveIdx.value % recent.length]
  directiveText.value = `${(entry.title || 'SESSION').toUpperCase()} · ${entry.message_count || 0} сообщ.`
  directiveIdx.value++
}

function cycleContext() {
  const list = agents.value
  if (!list.length) {
    contextName.value = ''
    contextPct.value = 0
    contextLast.value = 'нет данных'
    return
  }
  const a = list[contextIdx.value % list.length]
  contextName.value = a.name
  contextPct.value = a.active ? 82 : 12
  contextLast.value = a.active ? a.status : 'idle'
  contextIdx.value++
}

// ── Жизненный цикл ───────────────────────────────────────────────
onMounted(() => {
  loadOverview(true).then(() => {
    cycleDirective()
    cycleContext()
  })
  pollTimer = setInterval(() => loadOverview(), 5000)
  speechPollTimer = setInterval(() => loadSpeechStatus(), 2000)
  directiveTimer = setInterval(cycleDirective, 4000)
  contextTimer = setInterval(cycleContext, 3000)
  loadSpeechStatus()
})

onUnmounted(() => {
  if (pollTimer) clearInterval(pollTimer)
  if (speechPollTimer) clearInterval(speechPollTimer)
  if (directiveTimer) clearInterval(directiveTimer)
  if (contextTimer) clearInterval(contextTimer)
})
</script>

<template>
  <div class="dashboard">
    <div class="orb orb-violet"></div>
    <div class="orb orb-cyan"></div>
    <div class="orb orb-mint"></div>
    <div class="dash-content">
      <div class="dash-scroll">
        <!-- ═══ LIVE OPS CONSOLE ═══ -->
        <div class="live-ops">
          <!-- Радар -->
          <div class="ops-card radar-card">
            <div class="ops-card-header">
              <span class="ops-eyebrow">Live Ops</span>
              <span class="ops-title">Агентский радар</span>
            </div>
            <div class="radar-wrap">
              <svg viewBox="0 0 140 140" class="radar-svg">
                <defs>
                  <radialGradient id="radar-glow" cx="50%" cy="50%" r="50%">
                    <stop offset="0%" stop-color="rgba(139,92,246,0.18)"/>
                    <stop offset="100%" stop-color="rgba(139,92,246,0)"/>
                  </radialGradient>
                </defs>
                <circle cx="70" cy="70" r="60" fill="url(#radar-glow)"/>
                <circle cx="70" cy="70" r="40" fill="none" stroke="rgba(255,255,255,0.06)"/>
                <circle cx="70" cy="70" r="20" fill="none" stroke="rgba(255,255,255,0.06)"/>
                <line x1="70" y1="10" x2="70" y2="130" stroke="rgba(255,255,255,0.05)"/>
                <line x1="10" y1="70" x2="130" y2="70" stroke="rgba(255,255,255,0.05)"/>
                <circle
                  v-for="(d, i) in radarDots"
                  :key="d.name + i"
                  :cx="d.x" :cy="d.y" r="4.5"
                  :fill="d.color"
                  :class="{ 'radar-dot-idle': !d.active }"
                  class="radar-dot"
                >
                  <title>{{ d.name }}: {{ d.active ? 'активен' : 'ожидает' }}</title>
                </circle>
              </svg>
            </div>
            <div class="radar-legend">
              <span v-for="d in radarDots" :key="'l' + d.name" class="radar-legend-item">
                <span class="radar-legend-dot" :style="{ background: d.color }"></span>
                {{ d.name }}
              </span>
            </div>
          </div>

          <!-- Directive + Context -->
          <div class="ops-card">
            <div class="ops-card-header">
              <span class="ops-eyebrow">Directive</span>
              <span class="ops-title">Текущее направление</span>
            </div>
            <div class="directive-text">{{ directiveText }}</div>

            <div class="context-window">
              <div class="context-label">Контекст агентов</div>
              <div class="context-row">
                <span class="context-agent-name">{{ contextName || '—' }}</span>
                <div class="context-bar-wrap">
                  <div
                    class="context-bar"
                    :style="{ width: contextPct + '%', background: AGENT_COLORS[contextName] || 'var(--brand-cyan)' }"
                  ></div>
                </div>
                <span class="context-last">{{ contextLast }}</span>
              </div>
            </div>
          </div>

          <!-- VPS Health -->
          <div class="ops-card">
            <div class="ops-card-header">
              <span class="ops-eyebrow">VPS Health</span>
              <span class="ops-title">Состояние машины</span>
            </div>
            <div v-if="health" class="vps-health">
              <div class="vps-row">
                <span class="vps-label">CPU</span>
                <div class="vps-track"><div class="vps-fill" :style="{ width: health.cpu_usage + '%', background: 'linear-gradient(90deg, var(--brand-violet), var(--brand-cyan))' }"></div></div>
                <span class="vps-pct">{{ Math.round(health.cpu_usage) }}%</span>
              </div>
              <div class="vps-row">
                <span class="vps-label">RAM</span>
                <div class="vps-track"><div class="vps-fill" :style="{ width: health.memory_percent + '%', background: 'linear-gradient(90deg, var(--brand-cyan), var(--brand-mint))' }"></div></div>
                <span class="vps-pct">{{ Math.round(health.memory_percent) }}%</span>
              </div>
              <div class="vps-row">
                <span class="vps-label">DISK</span>
                <div class="vps-track"><div class="vps-fill" :style="{ width: health.disk_percent + '%', background: 'linear-gradient(90deg, var(--brand-amber), var(--brand-red))' }"></div></div>
                <span class="vps-pct">{{ Math.round(health.disk_percent) }}%</span>
              </div>
              <div class="vps-detail">
                RAM {{ fmtBytes(health.memory_used) }} / {{ fmtBytes(health.memory_total) }}
                <span class="vps-sep">·</span>
                DISK {{ fmtBytes(health.disk_used) }} / {{ fmtBytes(health.disk_total) }}
              </div>
            </div>
            <div v-else class="vps-health vps-empty">
              <div class="vps-row"><span class="vps-label">CPU</span><div class="vps-track"><div class="vps-fill" style="width:0%"></div></div><span class="vps-pct">—</span></div>
              <div class="vps-row"><span class="vps-label">RAM</span><div class="vps-track"><div class="vps-fill" style="width:0%"></div></div><span class="vps-pct">—</span></div>
              <div class="vps-row"><span class="vps-label">DISK</span><div class="vps-track"><div class="vps-fill" style="width:0%"></div></div><span class="vps-pct">—</span></div>
              <div class="vps-detail vps-detail-empty">health недоступен (пересобери backend)</div>
            </div>
          </div>

          <!-- AI Credits -->
          <div class="ops-card">
            <div class="ops-card-header">
              <span class="ops-eyebrow">AI Balance</span>
              <span class="ops-title">Баланс нейросетей</span>
            </div>
            <div v-if="credits !== null" class="vps-health">
              <div class="credits-row">
                <span class="credits-label">RouterAI</span>
                <span class="credits-value">{{ credits.toLocaleString('ru-RU', { maximumFractionDigits: 2 }) }}</span>
              </div>
              <div class="credits-hint">обновляется с пингом Hermes</div>
            </div>
            <div v-else class="vps-health vps-empty">
              <div class="vps-row"><span class="vps-label">Credits</span><div class="vps-track"><div class="vps-fill" style="width:0%"></div></div><span class="vps-pct">—</span></div>
              <div class="vps-detail vps-detail-empty">{{ creditsError || 'баланс недоступен' }}</div>
            </div>
          </div>

          <!-- Speech Recognition -->
          <div class="ops-card">
            <div class="ops-card-header">
              <span class="ops-eyebrow">Speech</span>
              <span class="ops-title">Распознавание речи</span>
            </div>
            <div class="speech-status-row">
              <span class="speech-indicator" :class="speech.status?.connected ? 'on' : 'off'"></span>
              <span class="speech-status-text">
                {{ speech.status?.connected ? 'STT слушает (whisper)' : 'STT не подключён' }}
              </span>
              <span v-if="speech.status?.bufferSeconds" class="speech-buffer">{{ speech.status.bufferSeconds }} сек буфер</span>
            </div>
            <button
              class="speech-capture-btn"
              @click="captureSpeech"
              :disabled="speech.loading"
            >
              <span v-if="speech.loading" class="speech-spinner">●</span>
              <span v-else>🎤</span>
              {{ speech.loading ? 'Распознаю…' : 'Распознать речь' }}
            </button>
            <div v-if="speech.lastText" class="speech-result">
              <div class="speech-result-header">
                <span class="speech-result-label">Последнее распознанное</span>
                <span class="speech-result-time">{{ speech.lastAt }}</span>
              </div>
              <div class="speech-result-text">{{ speech.lastText }}</div>
            </div>
            <div v-if="speech.error && !speech.lastText" class="speech-error-line">
              ⚠ {{ speech.error }}
            </div>
          </div>

          <!-- Ops Footer -->
          <div class="ops-footer">
            <div class="ops-foot-cell">
              <span class="ops-foot-num">{{ system ? (system.status === 'ok' ? 0 : '—') : '—' }}</span>
              <span class="ops-foot-label">Очередь</span>
            </div>
            <div class="ops-foot-cell">
              <span class="ops-foot-num">{{ totalSessions }}</span>
              <span class="ops-foot-label">Сессии</span>
            </div>
            <div class="ops-foot-cell">
              <span class="ops-foot-num">0</span>
              <span class="ops-foot-label">Ошибки</span>
            </div>
            <div class="ops-foot-cell">
              <span class="ops-foot-num">{{ todaySessions }}</span>
              <span class="ops-foot-label">Сегодня</span>
            </div>
            <div class="ops-foot-cell">
              <span class="ops-foot-num">{{ fmtUptime(system?.uptime || 0) }}</span>
              <span class="ops-foot-label">Аптайм</span>
            </div>
          </div>
        </div>

        <!-- ═══ STATS STRIP ═══ -->
        <div class="stats-strip">
          <div class="stat-card2 accent-violet">
            <div class="stat2-label">Hermes</div>
            <div class="stat2-value">{{ system && system.hermes.alive ? 'ОНЛАЙН' : 'ОФФЛАЙН' }}</div>
            <div class="stat2-sub">{{ system?.hermes.model || '—' }}</div>
          </div>
          <div class="stat-card2 accent-cyan">
            <div class="stat2-label">Сессии</div>
            <div class="stat2-value">{{ totalSessions }}</div>
            <div class="stat2-sub">сообщений: {{ totalMessages }}</div>
          </div>
          <div class="stat-card2 accent-mint">
            <div class="stat2-label">Агенты</div>
            <div class="stat2-value">{{ activeAgents.length }} / {{ agents.length }}</div>
            <div class="stat2-sub">активных из всех</div>
          </div>
          <div class="stat-card2 accent-amber">
            <div class="stat2-label">Нагрузка</div>
            <div class="stat2-value">
              {{ health?.cpu_usage != null ? Math.round(health.cpu_usage) + '%' : '—' }}
            </div>
            <div class="stat2-sub">
              {{ healthOk === 'ok' ? 'НОРМА' : (healthOk === 'warn' ? 'НАГРУЗКА' : '—') }}
            </div>
          </div>
          <div class="stat-card2 accent-pink">
            <div class="stat2-label">Аптайм</div>
            <div class="stat2-value">{{ fmtUptime(system?.uptime || 0) }}</div>
            <div class="stat2-sub">KatHub v{{ system?.version || '—' }}</div>
          </div>
        </div>

        <!-- ═══ BOTTOM SECTION: THROUGHPUT + ACTIVITY ═══ -->
        <div class="bottom-section">
          <div class="glass-card">
            <div class="ops-card-header">
              <span class="ops-eyebrow">Throughput</span>
              <span class="ops-title">Активность за 7 дней</span>
            </div>
            <canvas id="sparkline-canvas" class="sparkline"></canvas>
            <div class="sparkline-peak" v-if="peakDay">
              Пик: {{ peakDay.day }} ({{ peakDay.total }} {{ peakDay.total === 1 ? 'сообщение' : 'сообщений' }})
            </div>
          </div>

          <div class="glass-card">
            <div class="ops-card-header">
              <span class="ops-eyebrow">Activity</span>
              <span class="ops-title">Последняя активность</span>
            </div>
            <div v-if="loading" class="empty">Загрузка…</div>
            <div v-else-if="error" class="empty error-text">{{ error }}</div>
            <div v-else-if="recentSessions.length === 0" class="empty">Пока нет сессий</div>
            <div v-else class="activity-list">
              <div v-for="s in recentSessions" :key="s.id" class="activity-row">
                <span class="activity-dot"></span>
                <span class="activity-title">{{ s.title }}</span>
                <span class="activity-count" v-if="s.message_count !== undefined">{{ s.message_count }} сообщ.</span>
                <span class="activity-count" v-else>CLI</span>
                <span class="stamp">{{ fmtDate(s.updated_at) }}</span>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.dashboard {
  display: flex;
  flex-direction: column;
  width: 100%;
  height: 100%;
  background: var(--color-bg-primary);
  color: var(--text-primary, #F4F4F8);
  overflow: hidden;
  position: relative;
}
.orb {
  position: absolute;
  border-radius: 50%;
  filter: blur(90px);
  opacity: 0.16;
  pointer-events: none;
  z-index: 0;
}
.orb-violet { width: 480px; height: 480px; background: #8B5CF6; top: -160px; left: -120px; }
.orb-cyan { width: 420px; height: 420px; background: #7DD3FC; top: 30%; right: -180px; }
.orb-mint { width: 360px; height: 360px; background: #5EE2B5; bottom: -120px; left: 30%; }
.dash-content { flex: 1; overflow: hidden; position: relative; z-index: 1; }
.dash-scroll { height: 100%; overflow-y: auto; padding: var(--space-4); display: flex; flex-direction: column; gap: var(--space-4); }

/* ═══ Glass card base ═══ */
.glass-card, .ops-card {
  background: var(--bg-glass);
  backdrop-filter: var(--blur-heavy);
  -webkit-backdrop-filter: var(--blur-heavy);
  border: 1px solid var(--border-glass);
  border-radius: var(--radius-lg);
  padding: var(--space-4);
}

/* ═══ Live Ops grid ═══ */
.live-ops {
  display: grid;
  grid-template-columns: 1fr 1fr 1fr;
  gap: var(--space-3);
}
@media (max-width: 1100px) { .live-ops { grid-template-columns: 1fr 1fr; } .radar-card { display: none; } .ops-footer { grid-column: 1 / -1; } }
@media (max-width: 768px) { .live-ops { grid-template-columns: 1fr; } .ops-footer { grid-template-columns: repeat(3, 1fr); } }

.ops-card { display: flex; flex-direction: column; gap: var(--space-3); }
.ops-card-header { display: flex; flex-direction: column; gap: 2px; }
.ops-eyebrow {
  font: 500 var(--font-size-xs) var(--font-mono);
  color: var(--text-muted);
  letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase;
}
.ops-title { font-family: var(--font-display); font-size: var(--font-size-md); font-weight: 600; }

/* Radar */
.radar-wrap { display: flex; justify-content: center; padding: var(--space-2) 0; }
.radar-svg { width: 200px; height: 200px; }
.radar-dot { transition: r 0.3s ease; }
.radar-dot-idle { opacity: 0.45; }
.radar-legend { display: flex; flex-wrap: wrap; gap: var(--space-2); justify-content: center; }
.radar-legend-item {
  display: flex; align-items: center; gap: 4px;
  font: 400 10px var(--font-mono); color: var(--text-muted);
  text-transform: uppercase; letter-spacing: 0.04em;
}
.radar-legend-dot { width: 6px; height: 6px; border-radius: var(--radius-full); }

/* Directive */
.directive-text {
  font: 500 var(--font-size-sm) var(--font-mono);
  color: var(--text-primary);
  background: rgba(255, 255, 255, 0.04);
  border: 1px solid var(--border-glass);
  border-radius: var(--radius-md);
  padding: var(--space-2) var(--space-3);
  min-height: 38px;
  transition: opacity 0.35s ease;
}

/* Context window */
.context-window { display: flex; flex-direction: column; gap: var(--space-2); margin-top: var(--space-1); }
.context-label {
  font: 500 var(--font-size-xs) var(--font-mono);
  color: var(--text-muted); letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase;
}
.context-row { display: flex; align-items: center; gap: var(--space-2); }
.context-agent-name {
  font: 600 var(--font-size-xs) var(--font-mono);
  width: 90px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
}
.context-bar-wrap { flex: 1; height: 6px; background: rgba(255,255,255,0.06); border-radius: var(--radius-full); overflow: hidden; }
.context-bar { height: 100%; border-radius: var(--radius-full); transition: width 0.5s ease; }
.context-last {
  font: 400 10px var(--font-mono); color: var(--text-muted);
  width: 70px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; text-align: right;
}

/* VPS */
.vps-health { display: flex; flex-direction: column; gap: var(--space-2); }
.vps-row { display: flex; align-items: center; gap: var(--space-2); }
.vps-label {
  width: 46px; font: 500 10px var(--font-mono);
  color: var(--text-muted); letter-spacing: var(--letter-spacing-wide);
}
.vps-track { flex: 1; height: 8px; background: rgba(255,255,255,0.06); border-radius: var(--radius-full); overflow: hidden; }
.vps-fill { height: 100%; border-radius: var(--radius-full); transition: width 0.5s ease; }
.vps-pct { width: 42px; text-align: right; font: 500 var(--font-size-xs) var(--font-mono); color: var(--text-primary); font-variant-numeric: tabular-nums; }
.vps-detail { font: 400 10px var(--font-mono); color: var(--text-muted); padding-top: var(--space-1); border-top: 1px solid var(--border-glass); }
.vps-sep { opacity: 0.4; margin: 0 var(--space-1); }
.vps-detail-empty { opacity: 0.5; }

/* AI Credits */
.credits-row { display: flex; align-items: center; justify-content: space-between; gap: var(--space-2); }
.credits-label {
  font: 500 10px var(--font-mono);
  color: var(--text-muted); letter-spacing: var(--letter-spacing-wide);
}
.credits-value {
  font: 600 var(--font-size-lg) var(--font-mono);
  color: var(--brand-mint);
  font-variant-numeric: tabular-nums;
}
.credits-hint { font: 400 10px var(--font-mono); color: var(--text-muted); opacity: 0.6; }

/* Ops footer */
.ops-footer {
  grid-column: 1 / -1;
  display: grid;
  grid-template-columns: repeat(5, 1fr);
  background: var(--bg-glass);
  backdrop-filter: var(--blur-heavy);
  -webkit-backdrop-filter: var(--blur-heavy);
  border: 1px solid var(--border-glass);
  border-radius: var(--radius-lg);
  padding: var(--space-3) var(--space-4);
  gap: var(--space-2);
}
.ops-foot-cell { display: flex; flex-direction: column; align-items: center; gap: 2px; }
.ops-foot-num { font: 700 var(--font-size-md) var(--font-display); color: var(--text-primary); line-height: 1.1; }
.ops-foot-label {
  font: 400 9px var(--font-mono); color: var(--text-muted);
  letter-spacing: var(--letter-spacing-wide); text-transform: uppercase;
}

/* ═══ Stats strip ═══ */
.stats-strip {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(170px, 1fr));
  gap: var(--space-3);
}
.stat-card2 {
  background: var(--bg-glass);
  backdrop-filter: var(--blur-heavy);
  -webkit-backdrop-filter: var(--blur-heavy);
  border: 1px solid var(--border-glass);
  border-radius: var(--radius-lg);
  padding: var(--space-3) var(--space-4);
  position: relative;
  overflow: hidden;
  display: flex;
  flex-direction: column;
  gap: 2px;
}
.stat-card2::before {
  content: '';
  position: absolute; top: 0; left: 0; right: 0; height: 2px;
}
.stat-card2.accent-violet::before { background: var(--brand-violet); }
.stat-card2.accent-cyan::before { background: var(--brand-cyan); }
.stat-card2.accent-mint::before { background: var(--brand-mint); }
.stat-card2.accent-amber::before { background: var(--brand-amber); }
.stat-card2.accent-pink::before { background: var(--brand-pink); }
.stat2-label {
  font: 500 9px var(--font-mono); color: var(--text-muted);
  letter-spacing: var(--letter-spacing-wide); text-transform: uppercase;
}
.stat2-value { font: 700 var(--font-size-lg) var(--font-display); color: var(--text-primary); line-height: 1.1; }
.stat2-sub { font: 400 var(--font-size-xs) var(--font-mono); color: var(--text-muted); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }

/* ═══ Bottom section ═══ */
.bottom-section {
  display: grid;
  grid-template-columns: 1fr 1.2fr;
  gap: var(--space-3);
}
@media (max-width: 900px) { .bottom-section { grid-template-columns: 1fr; } }

.sparkline { width: 100%; display: block; }
.sparkline-peak {
  font: 400 var(--font-size-xs) var(--font-mono);
  color: var(--brand-cyan); padding-top: var(--space-2);
  border-top: 1px solid var(--border-glass); margin-top: var(--space-1);
}

/* Activity */
.activity-list { display: flex; flex-direction: column; }
.activity-row {
  display: flex; align-items: center; gap: var(--space-2);
  padding: var(--space-2) var(--space-1);
  border-bottom: 1px solid rgba(255,255,255,0.04);
  font-size: var(--font-size-sm);
}
.activity-row:hover { background: rgba(255,255,255,0.04); border-radius: var(--radius-sm); }
.activity-dot { width: 7px; height: 7px; border-radius: 50%; background: var(--brand-mint); flex-shrink: 0; }
.activity-title { flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; color: var(--text-primary); }
.activity-count { font-size: var(--font-size-xs); color: var(--text-muted); }
.stamp { font-size: var(--font-size-xs); color: var(--text-muted); flex-shrink: 0; }

.empty { padding: var(--space-4); color: var(--text-muted); font-size: var(--font-size-sm); text-align: center; }
.error-text { color: var(--brand-red); }

/* ═══ Speech recognition (whisper STT) ═══ */
.speech-status-row {
  display: flex; align-items: center; gap: var(--space-2);
  padding: var(--space-2) 0;
  font-size: var(--font-size-sm);
}
.speech-indicator {
  width: 9px; height: 9px; border-radius: 50%; flex-shrink: 0;
  background: var(--brand-red); opacity: 0.5;
}
.speech-indicator.on {
  background: var(--brand-mint);
  box-shadow: 0 0 8px var(--brand-mint);
  opacity: 1;
  animation: speech-pulse 2s infinite;
}
@keyframes speech-pulse {
  0%, 100% { box-shadow: 0 0 4px var(--brand-mint); }
  50%      { box-shadow: 0 0 12px var(--brand-mint); }
}
.speech-status-text { color: var(--text-primary); flex: 1; }
.speech-buffer {
  font: 400 var(--font-size-xs) var(--font-mono);
  color: var(--text-muted);
  background: rgba(255,255,255,0.05);
  border-radius: var(--radius-sm);
  padding: 2px 8px;
}
.speech-capture-btn {
  display: flex; align-items: center; justify-content: center; gap: 8px;
  width: 100%;
  padding: 10px 12px;
  border-radius: var(--radius-md);
  border: 1px solid var(--border-glass);
  background: linear-gradient(135deg, rgba(124,92,255,0.18), rgba(94,226,181,0.10));
  color: var(--text-primary);
  font: 600 var(--font-size-sm) var(--font-display);
  cursor: pointer;
  transition: all var(--transition-fast);
  margin: var(--space-1) 0;
}
.speech-capture-btn:hover:not(:disabled) {
  border-color: var(--brand-violet);
  box-shadow: 0 0 16px rgba(124,92,255,0.25);
}
.speech-capture-btn:disabled { opacity: 0.6; cursor: default; }
.speech-spinner { color: var(--brand-cyan); animation: speech-pulse 1s infinite; }
.speech-result {
  margin-top: var(--space-2);
  padding: var(--space-2) var(--space-3);
  background: rgba(124,92,255,0.08);
  border: 1px solid rgba(124,92,255,0.25);
  border-radius: var(--radius-md);
}
.speech-result-header {
  display: flex; justify-content: space-between; align-items: center;
  margin-bottom: 4px;
}
.speech-result-label {
  font: 500 9px var(--font-mono); color: var(--text-muted);
  letter-spacing: var(--letter-spacing-wide); text-transform: uppercase;
}
.speech-result-time { font: 400 10px var(--font-mono); color: var(--text-muted); }
.speech-result-text {
  font-size: var(--font-size-sm); color: var(--text-primary);
  line-height: 1.5; word-break: break-word;
}
.speech-error-line {
  margin-top: var(--space-2);
  font-size: var(--font-size-xs); color: var(--brand-red);
}
</style>
