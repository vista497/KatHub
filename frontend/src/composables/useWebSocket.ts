import { ref, onUnmounted } from 'vue'
import { useEventsStore } from '@/stores/eventsStore'
import { useServerStore } from '@/stores/serverStore'

export type WsConnectionStatus = 'connected' | 'disconnected' | 'reconnecting'

interface WsMessage {
  type: string
  topic?: string
  data?: unknown
}

export function useWebSocket(wsUrl?: string) {
  const url = wsUrl ?? `ws://${window.location.host}/ws`
  const ws = ref<WebSocket | null>(null)
  const connected = ref(false)
  const status = ref<WsConnectionStatus>('disconnected')
  const handlers = new Map<string, Set<(data: unknown) => void>>()
  const wildcardHandlers = new Set<(data: unknown) => void>()
  let reconnectTimer: ReturnType<typeof setTimeout> | null = null
  let reconnectAttempts = 0
  let destroyed = false

  const eventsStore = useEventsStore()
  const serverStore = useServerStore()

  function updateStatus(newStatus: WsConnectionStatus) {
    status.value = newStatus
    serverStore.wsConnectionStatus = newStatus
  }

  function connect() {
    if (destroyed) return
    if (ws.value?.readyState === WebSocket.OPEN) return

    try {
      ws.value = new WebSocket(url)
    } catch {
      scheduleReconnect()
      return
    }

    ws.value.onopen = () => {
      connected.value = true
      reconnectAttempts = 0
      updateStatus('connected')
    }

    ws.value.onmessage = (event: MessageEvent) => {
      try {
        const msg: WsMessage = JSON.parse(event.data)
        eventsStore.addEvent({
          type: 'info',
          message: JSON.stringify(msg.data ?? msg),
          timestamp: Date.now(),
        })
        dispatch(msg)
      } catch {
        // ignore malformed messages
      }
    }

    ws.value.onclose = () => {
      connected.value = false
      ws.value = null
      updateStatus('disconnected')
      scheduleReconnect()
    }

    ws.value.onerror = () => {
      // onclose will fire after onerror
    }
  }

  function scheduleReconnect() {
    if (destroyed) return
    if (reconnectAttempts >= 10) return
    if (reconnectTimer) return

    updateStatus('reconnecting')
    reconnectAttempts++

    reconnectTimer = setTimeout(() => {
      reconnectTimer = null
      connect()
    }, 5000)
  }

  function dispatch(msg: WsMessage) {
    const payload = msg.data ?? msg
    if (msg.topic && handlers.has(msg.topic)) {
      handlers.get(msg.topic)!.forEach((fn) => fn(payload))
    }
    wildcardHandlers.forEach((fn) => fn(payload))
  }

  function subscribe(topic: string, handler: (data: unknown) => void) {
    if (!handlers.has(topic)) {
      handlers.set(topic, new Set())
    }
    handlers.get(topic)!.add(handler)

    if (ws.value?.readyState === WebSocket.OPEN) {
      ws.value.send(JSON.stringify({ type: 'subscribe', topic }))
    }

    return () => {
      handlers.get(topic)?.delete(handler)
    }
  }

  function onMessage(handler: (data: unknown) => void) {
    wildcardHandlers.add(handler)
    return () => {
      wildcardHandlers.delete(handler)
    }
  }

  function publish(topic: string, data: unknown) {
    if (ws.value?.readyState === WebSocket.OPEN) {
      ws.value.send(JSON.stringify({ type: 'publish', topic, data }))
    }
  }

  function disconnect() {
    destroyed = true
    if (reconnectTimer) {
      clearTimeout(reconnectTimer)
      reconnectTimer = null
    }
    if (ws.value) {
      ws.value.onclose = null
      ws.value.close()
      ws.value = null
    }
    connected.value = false
    updateStatus('disconnected')
  }

  // Start connecting immediately
  connect()

  onUnmounted(() => {
    disconnect()
  })

  return {
    connected,
    status,
    subscribe,
    onMessage,
    publish,
    disconnect,
    connect,
  }
}
