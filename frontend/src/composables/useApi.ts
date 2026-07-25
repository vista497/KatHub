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
  }
}
