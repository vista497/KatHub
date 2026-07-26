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

  // File editor state
  const editingFile = ref<string | null>(null)   // node id (relative path)
  const editingTitle = ref<string>('')
  const fileContent = ref<string>('')
  const fileLoading = ref(false)

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
    editingFile.value = null
    fileContent.value = ''
  }

  async function openFile(nodeId: string) {
    editingFile.value = nodeId
    editingTitle.value = nodeId.split(/[\\/]/).pop()?.replace('.md', '') || nodeId
    fileLoading.value = true
    try {
      const res = await fetch(`/api/vault/file?path=${encodeURIComponent(nodeId)}`)
      if (!res.ok) throw new Error(`HTTP ${res.status}`)
      const data = await res.json()
      fileContent.value = data.content || ''
    } catch (e: any) {
      fileContent.value = `Error: ${e.message}`
    } finally {
      fileLoading.value = false
    }
  }

  function closeFile() {
    editingFile.value = null
    fileContent.value = ''
  }

  return {
    nodes, links, loading, selectedNode, error,
    editingFile, editingTitle, fileContent, fileLoading,
    fetchGraph, selectNode, clearSelection, openFile, closeFile,
  }
})
