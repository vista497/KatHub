import { useServerStore } from '@/stores/serverStore'
import { usePluginsStore } from '@/stores/pluginsStore'

const BASE_URL = '/api'

interface RequestOptions extends RequestInit {
  params?: Record<string, string>
}

async function request<T = unknown>(
  endpoint: string,
  options: RequestOptions = {}
): Promise<T> {
  const { params, ...fetchOptions } = options

  let url = `${BASE_URL}${endpoint}`
  if (params) {
    const qs = new URLSearchParams(params).toString()
    url += `?${qs}`
  }

  const headers: Record<string, string> = {
    'Content-Type': 'application/json',
    ...(fetchOptions.headers as Record<string, string> | undefined),
  }

  const response = await fetch(url, {
    ...fetchOptions,
    headers,
  })

  if (!response.ok) {
    const text = await response.text()
    throw new Error(`HTTP ${response.status}: ${text}`)
  }

  return response.json() as Promise<T>
}

interface ServerStatusResponse {
  status: string
  version: string
  uptime: number
}

interface PluginResponse {
  name: string
  version: string
  status: string
}

async function fetchServerStatus(): Promise<ServerStatusResponse> {
  const serverStore = useServerStore()
  serverStore.loading = true
  serverStore.error = null
  try {
    const data = await request<ServerStatusResponse>('/status')
    serverStore.serverStatus = {
      status: data.status,
      version: data.version,
      uptime: data.uptime,
    }
    return data
  } catch (e) {
    serverStore.error = e instanceof Error ? e.message : String(e)
    throw e
  } finally {
    serverStore.loading = false
  }
}

async function fetchPlugins(): Promise<PluginResponse[]> {
  const pluginsStore = usePluginsStore()
  pluginsStore.loading = true
  pluginsStore.error = null
  try {
    const data = await request<PluginResponse[]>('/plugins')
    pluginsStore.plugins = data.map((p) => ({
      name: p.name,
      version: p.version,
      enabled: p.status === 'loaded',
      description: undefined,
    }))
    return data
  } catch (e) {
    pluginsStore.error = e instanceof Error ? e.message : String(e)
    throw e
  } finally {
    pluginsStore.loading = false
  }
}

export function useApi() {
  return {
    get<T = unknown>(endpoint: string, params?: Record<string, string>) {
      return request<T>(endpoint, { method: 'GET', params })
    },
    post<T = unknown>(endpoint: string, body?: unknown) {
      return request<T>(endpoint, {
        method: 'POST',
        body: body ? JSON.stringify(body) : undefined,
      })
    },
    put<T = unknown>(endpoint: string, body?: unknown) {
      return request<T>(endpoint, {
        method: 'PUT',
        body: body ? JSON.stringify(body) : undefined,
      })
    },
    delete<T = unknown>(endpoint: string) {
      return request<T>(endpoint, { method: 'DELETE' })
    },
    fetchServerStatus,
    fetchPlugins,
  }
}
