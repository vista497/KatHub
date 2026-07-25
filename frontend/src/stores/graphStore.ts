import { defineStore } from 'pinia'
import { ref } from 'vue'

export interface GraphNode {
  id: string
  label: string
  type: 'note' | 'folder' | 'tag'
  folder?: string
  x?: number
  y?: number
  fx?: number | null
  fy?: number | null
}

export interface GraphLink {
  source: string
  target: string
  type: 'wikilink' | 'folder' | 'tag'
}

export const useGraphStore = defineStore('graph', () => {
  const nodes = ref<GraphNode[]>([])
  const links = ref<GraphLink[]>([])
  const loading = ref(false)
  const selectedNode = ref<string | null>(null)
  const error = ref<string | null>(null)

  async function fetchGraph() {
    loading.value = true
    error.value = null
    try {
      const res = await fetch('/api/vault/graph')
      if (!res.ok) throw new Error(`HTTP ${res.status}`)
      const data = await res.json()
      nodes.value = data.nodes || []
      links.value = data.links || []
    } catch (e: any) {
      error.value = e.message || 'Failed to load graph'
      console.error('Graph load error:', e)
    } finally {
      loading.value = false
    }
  }

  function selectNode(id: string | null) {
    selectedNode.value = id
  }

  function clearSelection() {
    selectedNode.value = null
  }

  return { nodes, links, loading, selectedNode, error, fetchGraph, selectNode, clearSelection }
})
