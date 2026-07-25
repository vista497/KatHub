import { ref, onUnmounted } from 'vue'

type MessageHandler = (data: unknown) => void

interface WsMessage {
  type: string
  topic?: string
  data?: unknown
}

export function useWebSocket(wsUrl?: string) {
  const url = wsUrl ?? `ws://${window.location.hostname}:8081/ws`
  const ws = ref<WebSocket | null>(null)
  const connected = ref(false)
  const handlers = new Map<string, Set<MessageHandler>>()
  const wildcardHandlers = new Set<MessageHandler>()
  let reconnectTimer: ReturnType<typeof setTimeout> | null = null
  let reconnectDelay = 1000
  const maxReconnectDelay = 30000
  let destroyed = false

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
      reconnectDelay = 1000
    }

    ws.value.onmessage = (event: MessageEvent) => {
      try {
        const msg: WsMessage = JSON.parse(event.data)
        dispatch(msg)
      } catch {
        // ignore malformed messages
      }
    }

    ws.value.onclose = () => {
      connected.value = false
      ws.value = null
      scheduleReconnect()
    }

    ws.value.onerror = () => {
      // onclose will fire after onerror
    }
  }

  function scheduleReconnect() {
    if (destroyed) return
    if (reconnectTimer) return
    reconnectTimer = setTimeout(() => {
      reconnectTimer = null
      connect()
    }, reconnectDelay)
    reconnectDelay = Math.min(reconnectDelay * 1.5, maxReconnectDelay)
  }

  function dispatch(msg: WsMessage) {
    if (msg.topic && handlers.has(msg.topic)) {
      handlers.get(msg.topic)!.forEach((fn) => fn(msg.data ?? msg))
    }
    wildcardHandlers.forEach((fn) => fn(msg.data ?? msg))
  }

  function subscribe(topic: string, handler: MessageHandler) {
    if (!handlers.has(topic)) {
      handlers.set(topic, new Set())
    }
    handlers.get(topic)!.add(handler)

    // Send subscribe to server
    if (ws.value?.readyState === WebSocket.OPEN) {
      ws.value.send(JSON.stringify({ type: 'subscribe', topic }))
    } else {
      // Will be sent on reconnect via the sendSubscriptions below
    }

    return () => {
      handlers.get(topic)?.delete(handler)
    }
  }

  function onMessage(handler: MessageHandler) {
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
  }

  // Start connecting immediately
  connect()

  onUnmounted(() => {
    disconnect()
  })

  return {
    connected,
    subscribe,
    onMessage,
    publish,
    disconnect,
    connect,
  }
}
