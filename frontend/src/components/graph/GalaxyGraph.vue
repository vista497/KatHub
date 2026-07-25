<script setup lang="ts">
import { ref, onMounted, onUnmounted, watch, nextTick } from 'vue'
import * as d3 from 'd3'
import { useGraphStore } from '../../stores/graphStore'

const container = ref<HTMLDivElement>()
const svgRef = ref<SVGSVGElement>()
const graph = useGraphStore()

let simulation: d3.Simulation<any, any> | null = null
let currentZoom = 1
const LABEL_ZOOM_THRESHOLD = 2.0

let dragged = false

function initGraph() {
  if (!svgRef.value || !container.value || !graph.nodes.length) return

  const width = container.value.clientWidth
  const height = container.value.clientHeight

  d3.select(svgRef.value).selectAll('*').remove()

  const svg = d3.select(svgRef.value)
    .attr('width', width)
    .attr('height', height)

  svg.append('rect')
    .attr('width', width)
    .attr('height', height)
    .attr('fill', 'transparent')

  const g = svg.append('g')

  const zoom = d3.zoom<SVGSVGElement, unknown>()
    .scaleExtent([0.1, 5])
    .on('zoom', (event) => {
      currentZoom = event.transform.k
      g.attr('transform', event.transform)
      updateLabelVisibility()
    })

  svg.call(zoom)

  const defs = svg.append('defs')
  const filter = defs.append('filter').attr('id', 'glow')
  filter.append('feGaussianBlur').attr('stdDeviation', '2.5').attr('result', 'blur')
  const merge = filter.append('feMerge')
  merge.append('feMergeNode').attr('in', 'blur')
  merge.append('feMergeNode').attr('in', 'SourceGraphic')

  // Links
  const link = g.append('g')
    .selectAll('line')
    .data(graph.links)
    .join('line')
    .attr('stroke', 'rgba(124,92,255,0.12)')
    .attr('stroke-width', 0.4)

  // Nodes
  const node = g.append('g')
    .selectAll('g')
    .data(graph.nodes)
    .join('g')
    .attr('cursor', 'pointer')
    .attr('class', (d: any) => `node-${d.type}`)

  node.on('pointerdown', () => { dragged = false })

  const drag = d3.drag<any, any>()
    .on('start', (_event, d) => {
      if (!_event.active) simulation!.alphaTarget(0.3).restart()
      d.fx = d.x
      d.fy = d.y
    })
    .on('drag', (event, d) => {
      dragged = true
      d.fx = event.x
      d.fy = event.y
    })
    .on('end', (_event, d) => {
      if (!_event.active) simulation!.alphaTarget(0)
      d.fx = null
      d.fy = null
    })

  node.call(drag)

  // Circles
  const circles = node.append('circle')
    .attr('r', (d: any) => d.type === 'folder' ? 7 : 4)
    .attr('fill', (d: any) => {
      switch (d.type) {
        case 'folder': return '#ff6b9d'
        case 'tag': return '#5ce0ff'
        default: return '#7c5cff'
      }
    })
    .attr('opacity', 0.9)

  // Public function to update selection styling WITHOUT reinit
  function updateSelection(id: string | null) {
    circles
      .attr('stroke', (d: any) => d.id === id ? '#ffffff' : 'transparent')
      .attr('stroke-width', (d: any) => d.id === id ? 2 : 0)
      .attr('filter', (d: any) => d.id === id ? 'url(#glow)' : null)
  }
  updateSelection(graph.selectedNode)

  // Labels
  const labels = node.append('text')
    .text((d: any) => d.label)
    .attr('font-size', '6px')
    .attr('fill', '#ccccee')
    .attr('dx', (d: any) => d.type === 'folder' ? 10 : 6)
    .attr('dy', 3)
    .attr('opacity', 0)
    .attr('pointer-events', 'none')
    .style('text-shadow', '0 0 3px rgba(0,0,0,0.8)')

  function updateLabelVisibility() {
    const show = currentZoom >= LABEL_ZOOM_THRESHOLD
    labels.attr('opacity', show ? 0.8 : 0)
  }

  // Click — open file or select
  node.on('click', (_event: any, d: any) => {
    if (dragged) return
    const newSelection = d.id === graph.selectedNode ? null : d.id
    graph.selectNode(newSelection)
    updateSelection(newSelection)  // Visual only, no sim restart
    if (d.type === 'note') {
      graph.openFile(d.id)
    }
  })

  // Background click — deselect
  svg.on('click', (event: any) => {
    if (event.target === svgRef.value) {
      graph.clearSelection()
      updateSelection(null)
    }
  })

  // Simulation — clamps orphans with strong center + radial force
  simulation = d3.forceSimulation(graph.nodes)
    .force('link', d3.forceLink(graph.links).id((d: any) => d.id).distance(40).strength(0.3))
    .force('charge', d3.forceManyBody().strength(-10))
    .force('center', d3.forceCenter(width / 2, height / 2).strength(1.0))
    .force('collision', d3.forceCollide().radius(8))
    .force('radial', d3.forceRadial(80, width / 2, height / 2).strength(0.05))
    .alphaDecay(0.01)
    .alphaMin(0.001)
    .on('tick', () => {
      link
        .attr('x1', (d: any) => d.source.x)
        .attr('y1', (d: any) => d.source.y)
        .attr('x2', (d: any) => d.target.x)
        .attr('y2', (d: any) => d.target.y)
      node.attr('transform', (d: any) => `translate(${d.x},${d.y})`)
    })

  // Store updateSelection for the WATCH BELOW — no reinit
  ;(window as any).__updateGalaxySelection = updateSelection
}

function onResize() {
  initGraph()
}

const emit = defineEmits<{
  (e: 'open-file', path: string): void
}>()

watch(() => graph.editingFile, (val) => {
  if (val) emit('open-file', val)
})

onMounted(() => {
  window.addEventListener('resize', onResize)
  graph.fetchGraph()
})

onUnmounted(() => {
  window.removeEventListener('resize', onResize)
  simulation?.stop()
})

// Init graph when nodes arrive
watch(() => graph.nodes.length, async () => {
  if (graph.nodes.length) {
    await nextTick()
    initGraph()
  }
})

// Selection change — VISUAL UPDATE ONLY, no reinit
watch(() => graph.selectedNode, (newVal) => {
  const fn = (window as any).__updateGalaxySelection
  if (fn) fn(newVal)
})
</script>

<template>
  <div ref="container" class="graph-container">
    <div class="starfield"></div>

    <div v-if="graph.loading" class="overlay">
      <div class="loading-text">Loading galaxy...</div>
    </div>

    <div v-else-if="graph.error" class="overlay">
      <div class="error-text">⚠️ {{ graph.error }}</div>
      <button class="retry-btn" @click="graph.fetchGraph">Retry</button>
    </div>

    <svg ref="svgRef" class="graph-svg"></svg>

    <div v-if="graph.selectedNode" class="node-info">
      {{ graph.selectedNode }}
    </div>

    <div class="zoom-hint" :class="{ hidden: currentZoom >= LABEL_ZOOM_THRESHOLD }">
      🔍 Zoom in for labels
    </div>
  </div>
</template>

<style scoped>
.graph-container {
  position: relative;
  width: 100%;
  height: 100%;
  overflow: hidden;
  contain: strict;
}

.starfield {
  position: absolute;
  inset: 0;
  background:
    radial-gradient(1px 1px at 10% 20%, rgba(255,255,255,0.4), transparent),
    radial-gradient(1px 1px at 30% 60%, rgba(255,255,255,0.3), transparent),
    radial-gradient(1px 1px at 50% 10%, rgba(255,255,255,0.5), transparent),
    radial-gradient(1px 1px at 70% 40%, rgba(255,255,255,0.2), transparent),
    radial-gradient(1px 1px at 90% 80%, rgba(255,255,255,0.3), transparent),
    radial-gradient(1.5px 1.5px at 20% 80%, rgba(124,92,255,0.4), transparent),
    radial-gradient(1.5px 1.5px at 60% 30%, rgba(92,224,255,0.3), transparent),
    radial-gradient(1px 1px at 80% 55%, rgba(255,255,255,0.5), transparent),
    radial-gradient(1px 1px at 15% 45%, rgba(255,255,255,0.3), transparent),
    radial-gradient(1px 1px at 40% 85%, rgba(255,255,255,0.2), transparent),
    radial-gradient(1px 1px at 65% 15%, rgba(255,255,255,0.4), transparent),
    radial-gradient(1px 1px at 85% 35%, rgba(255,255,255,0.3), transparent);
  pointer-events: none;
}

.graph-svg {
  position: absolute;
  inset: 0;
  cursor: grab;
}

.graph-svg:active {
  cursor: grabbing;
}

.overlay {
  position: absolute;
  inset: 0;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: var(--space-4);
  background: var(--color-bg-primary);
  z-index: 10;
}

.loading-text {
  color: var(--color-text-secondary);
  font-size: var(--font-size-lg);
}

.error-text {
  color: var(--color-node-folder);
  font-size: var(--font-size-base);
}

.retry-btn {
  padding: var(--space-2) var(--space-4);
  background: var(--color-accent);
  color: white;
  border: none;
  border-radius: var(--radius-md);
  cursor: pointer;
  font-family: var(--font-sans);
  font-size: var(--font-size-sm);
}

.node-info {
  position: absolute;
  bottom: var(--space-4);
  left: 50%;
  transform: translateX(-50%);
  padding: var(--space-2) var(--space-4);
  background: var(--color-bg-tertiary);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-full);
  font-size: var(--font-size-sm);
  color: var(--color-text-secondary);
  pointer-events: none;
  max-width: 320px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.zoom-hint {
  position: absolute;
  bottom: 60px;
  left: 50%;
  transform: translateX(-50%);
  padding: var(--space-1) var(--space-3);
  background: rgba(0,0,0,0.6);
  border-radius: var(--radius-full);
  font-size: 11px;
  color: var(--color-text-secondary);
  pointer-events: none;
  transition: opacity 0.3s;
}

.zoom-hint.hidden {
  opacity: 0;
}
</style>
