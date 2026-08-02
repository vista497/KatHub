<script setup lang="ts">
import { ref, onMounted, onUnmounted, watch, nextTick } from 'vue'
import * as d3 from 'd3'
import { useGraphStore } from '../../stores/graphStore'

const container = ref<HTMLDivElement>()
const svgRef = ref<SVGSVGElement>()
const graph = useGraphStore()

let simulation: d3.Simulation<any, any> | null = null
let currentZoom = 1
const isTouchDevice = ('ontouchstart' in window) || (navigator.maxTouchPoints > 0)
const labelZoomThreshold = ref(isTouchDevice ? 1.0 : 2.0)

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
    .attr('stroke', 'rgba(139,92,246,0.14)')
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

  // Circles — much larger on touch, with invisible hit area
  const nodeR = (d: any) => {
    const base = d.type === 'folder' ? 7 : 4
    return isTouchDevice ? base * 6 : base  // 24px/42px on phone
  }
  const hitR = (d: any) => nodeR(d) + (isTouchDevice ? 12 : 4)  // extra 12px touch slop

  // Invisible hit circle (larger touch target)
  node.append('circle')
    .attr('r', hitR)
    .attr('fill', 'transparent')
    .attr('stroke', 'none')

  const circles = node.append('circle')
    .attr('r', nodeR)
    .attr('fill', (d: any) => {
      switch (d.type) {
        case 'folder': return '#f472b6'
        case 'tag': return '#7dd3fc'
        default: return '#8b5cf6'
      }
    })
    .attr('opacity', 0.9)

  // Glow on touch devices
  if (isTouchDevice) {
    circles.attr('filter', 'url(#glow)')
  }

  // Public function to update selection styling WITHOUT reinit
  function updateSelection(id: string | null) {
    circles
      .attr('stroke', (d: any) => d.id === id ? '#ffffff' : 'transparent')
      .attr('stroke-width', (d: any) => d.id === id ? 2 : 0)
      .attr('filter', (d: any) => d.id === id ? 'url(#glow)' : null)
  }
  updateSelection(graph.selectedNode)

  // Labels — larger on touch, visible at lower zoom
  const labelSize = isTouchDevice ? '13px' : '6px'
  const labelDx = (d: any) => {
    const r = nodeR(d)
    return d.type === 'folder' ? r + 6 : r + 4
  }
  const labels = node.append('text')
    .text((d: any) => d.label)
    .attr('font-size', labelSize)
    .attr('fill', '#e8e8f5')
    .attr('dx', labelDx)
    .attr('dy', 3)
    .attr('opacity', 0)
    .attr('pointer-events', 'none')
    .style('text-shadow', '0 0 3px rgba(0,0,0,0.8)')

  function updateLabelVisibility() {
    const show = currentZoom >= labelZoomThreshold.value
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
      <div class="loading-text">Загрузка галактики…</div>
    </div>

    <div v-else-if="graph.error" class="overlay">
      <div class="error-text">⚠ {{ graph.error }}</div>
      <button class="retry-btn" @click="graph.fetchGraph">Повторить</button>
    </div>

    <svg ref="svgRef" class="graph-svg"></svg>

    <div v-if="graph.selectedNode" class="node-info">
      {{ graph.selectedNode }}
    </div>

    <div class="zoom-hint" :class="{ hidden: currentZoom >= labelZoomThreshold }">
      {{ isTouchDevice ? 'Щипок — масштаб' : 'Приблизьте для подписей' }}
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
    /* Deep space purple-black gradient */
    radial-gradient(ellipse at 30% 20%, rgba(30, 10, 60, 1) 0%, transparent 70%),
    radial-gradient(ellipse at 70% 80%, rgba(20, 5, 50, 1) 0%, transparent 70%),
    radial-gradient(ellipse at 50% 50%, rgba(10, 5, 30, 1) 0%, transparent 100%),
    /* Nebula wisps */
    radial-gradient(ellipse at 20% 40%, rgba(80, 40, 140, 0.15) 0%, transparent 40%),
    radial-gradient(ellipse at 75% 30%, rgba(60, 20, 120, 0.12) 0%, transparent 35%),
    radial-gradient(ellipse at 50% 75%, rgba(100, 50, 160, 0.1) 0%, transparent 35%),
    /* Bright stars — large */
    radial-gradient(2px 2px at 12% 8%, rgba(255,255,255,0.9), transparent),
    radial-gradient(2.5px 2.5px at 85% 15%, rgba(200,220,255,0.8), transparent),
    radial-gradient(2px 2px at 45% 5%, rgba(255,255,255,0.7), transparent),
    radial-gradient(3px 3px at 68% 12%, rgba(255,240,255,0.9), transparent),
    radial-gradient(1.5px 1.5px at 5% 25%, rgba(255,255,255,0.8), transparent),
    /* Medium stars */
    radial-gradient(1.5px 1.5px at 22% 35%, rgba(220,200,255,0.7), transparent),
    radial-gradient(1.5px 1.5px at 55% 28%, rgba(255,255,255,0.6), transparent),
    radial-gradient(1.5px 1.5px at 78% 42%, rgba(200,220,255,0.7), transparent),
    radial-gradient(1.5px 1.5px at 35% 18%, rgba(255,255,255,0.6), transparent),
    radial-gradient(1.5px 1.5px at 92% 22%, rgba(255,240,255,0.8), transparent),
    radial-gradient(1.5px 1.5px at 15% 55%, rgba(220,200,255,0.7), transparent),
    radial-gradient(1.5px 1.5px at 60% 48%, rgba(255,255,255,0.5), transparent),
    radial-gradient(1.5px 1.5px at 88% 58%, rgba(200,220,255,0.6), transparent),
    radial-gradient(1.5px 1.5px at 10% 72%, rgba(255,255,255,0.5), transparent),
    radial-gradient(1.5px 1.5px at 42% 65%, rgba(220,200,255,0.6), transparent),
    radial-gradient(1.5px 1.5px at 70% 68%, rgba(255,255,255,0.6), transparent),
    radial-gradient(1.5px 1.5px at 95% 78%, rgba(255,240,255,0.7), transparent),
    /* Small stars — field fill */
    radial-gradient(1px 1px at 8% 12%, rgba(255,255,255,0.5), transparent),
    radial-gradient(1px 1px at 18% 28%, rgba(255,255,255,0.4), transparent),
    radial-gradient(1px 1px at 28% 8%, rgba(255,255,255,0.5), transparent),
    radial-gradient(1px 1px at 38% 22%, rgba(255,255,255,0.3), transparent),
    radial-gradient(1px 1px at 48% 35%, rgba(255,255,255,0.4), transparent),
    radial-gradient(1px 1px at 52% 12%, rgba(255,255,255,0.5), transparent),
    radial-gradient(1px 1px at 62% 22%, rgba(255,255,255,0.4), transparent),
    radial-gradient(1px 1px at 72% 8%, rgba(255,255,255,0.5), transparent),
    radial-gradient(1px 1px at 82% 18%, rgba(255,255,255,0.4), transparent),
    radial-gradient(1px 1px at 92% 8%, rgba(255,255,255,0.3), transparent),
    radial-gradient(1px 1px at 5% 38%, rgba(255,255,255,0.4), transparent),
    radial-gradient(1px 1px at 15% 48%, rgba(255,255,255,0.3), transparent),
    radial-gradient(1px 1px at 25% 42%, rgba(255,255,255,0.5), transparent),
    radial-gradient(1px 1px at 48% 45%, rgba(255,255,255,0.3), transparent),
    radial-gradient(1px 1px at 58% 38%, rgba(255,255,255,0.4), transparent),
    radial-gradient(1px 1px at 68% 45%, rgba(255,255,255,0.3), transparent),
    radial-gradient(1px 1px at 78% 35%, rgba(255,255,255,0.4), transparent),
    radial-gradient(1px 1px at 88% 48%, rgba(255,255,255,0.5), transparent),
    radial-gradient(1px 1px at 30% 55%, rgba(255,255,255,0.3), transparent),
    radial-gradient(1px 1px at 40% 58%, rgba(255,255,255,0.4), transparent),
    radial-gradient(1px 1px at 65% 55%, rgba(255,255,255,0.3), transparent),
    radial-gradient(1px 1px at 75% 52%, rgba(255,255,255,0.5), transparent),
    radial-gradient(1px 1px at 85% 65%, rgba(255,255,255,0.4), transparent),
    radial-gradient(1px 1px at 12% 68%, rgba(255,255,255,0.3), transparent),
    radial-gradient(1px 1px at 22% 78%, rgba(255,255,255,0.4), transparent),
    radial-gradient(1px 1px at 32% 72%, rgba(255,255,255,0.5), transparent),
    radial-gradient(1px 1px at 52% 78%, rgba(255,255,255,0.3), transparent),
    radial-gradient(1px 1px at 62% 72%, rgba(255,255,255,0.4), transparent),
    radial-gradient(1px 1px at 82% 75%, rgba(255,255,255,0.3), transparent),
    radial-gradient(1px 1px at 45% 85%, rgba(255,255,255,0.4), transparent),
    radial-gradient(1px 1px at 55% 88%, rgba(255,255,255,0.5), transparent),
    radial-gradient(1px 1px at 72% 88%, rgba(255,255,255,0.3), transparent),
    radial-gradient(1px 1px at 90% 88%, rgba(255,255,255,0.4), transparent),
    radial-gradient(1px 1px at 18% 88%, rgba(255,255,255,0.3), transparent),
    radial-gradient(1px 1px at 38% 92%, rgba(255,255,255,0.4), transparent),
    radial-gradient(1px 1px at 58% 92%, rgba(255,255,255,0.3), transparent),
    /* Tiny dust stars */
    radial-gradient(0.5px 0.5px at 3% 5%, rgba(255,255,255,0.6), transparent),
    radial-gradient(0.5px 0.5px at 7% 15%, rgba(255,255,255,0.4), transparent),
    radial-gradient(0.5px 0.5px at 13% 3%, rgba(255,255,255,0.5), transparent),
    radial-gradient(0.5px 0.5px at 33% 2%, rgba(255,255,255,0.4), transparent),
    radial-gradient(0.5px 0.5px at 43% 8%, rgba(255,255,255,0.5), transparent),
    radial-gradient(0.5px 0.5px at 57% 3%, rgba(255,255,255,0.3), transparent),
    radial-gradient(0.5px 0.5px at 77% 2%, rgba(255,255,255,0.5), transparent),
    radial-gradient(0.5px 0.5px at 87% 6%, rgba(255,255,255,0.4), transparent),
    radial-gradient(0.5px 0.5px at 97% 12%, rgba(255,255,255,0.3), transparent),
    radial-gradient(0.5px 0.5px at 2% 18%, rgba(255,255,255,0.4), transparent),
    radial-gradient(0.5px 0.5px at 8% 22%, rgba(255,255,255,0.5), transparent),
    radial-gradient(0.5px 0.5px at 95% 33%, rgba(255,255,255,0.4), transparent),
    /* Subtle color stars */
    radial-gradient(1px 1px at 25% 12%, rgba(180,160,255,0.6), transparent),
    radial-gradient(1px 1px at 75% 25%, rgba(160,200,255,0.5), transparent),
    radial-gradient(1px 1px at 40% 38%, rgba(200,180,255,0.5), transparent),
    radial-gradient(1px 1px at 85% 40%, rgba(180,160,255,0.4), transparent),
    radial-gradient(1px 1px at 10% 45%, rgba(160,200,255,0.5), transparent),
    radial-gradient(1px 1px at 55% 55%, rgba(200,180,255,0.4), transparent),
    radial-gradient(1px 1px at 72% 62%, rgba(180,160,255,0.5), transparent),
    radial-gradient(1px 1px at 35% 75%, rgba(160,200,255,0.4), transparent),
    radial-gradient(1px 1px at 80% 82%, rgba(200,180,255,0.5), transparent);
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
  background: rgba(6, 4, 16, 0.5);
  backdrop-filter: blur(6px);
  -webkit-backdrop-filter: blur(6px);
  z-index: 10;
}

.loading-text {
  color: var(--text-muted);
  font-family: var(--font-mono);
  font-size: var(--font-size-xs);
  letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase;
}

.error-text {
  color: var(--brand-red);
  font-size: var(--font-size-base);
  font-family: var(--font-mono);
}

.retry-btn {
  padding: var(--space-2) var(--space-4);
  background: var(--brand-violet);
  color: #fff;
  border: 1px solid var(--brand-violet);
  border-radius: var(--radius-full);
  cursor: pointer;
  font-family: var(--font-mono);
  font-size: var(--font-size-xs);
  letter-spacing: var(--letter-spacing-wide);
  text-transform: uppercase;
  transition: all var(--transition-fast);
}

.retry-btn:hover {
  background: var(--brand-violet-glow);
  box-shadow: 0 0 12px rgba(139, 92, 246, 0.4);
}

.node-info {
  position: absolute;
  bottom: var(--space-4);
  left: 50%;
  transform: translateX(-50%);
  padding: var(--space-2) var(--space-4);
  background: var(--bg-glass-solid);
  backdrop-filter: var(--blur-heavy);
  -webkit-backdrop-filter: var(--blur-heavy);
  border: 1px solid rgba(139, 92, 246, 0.3);
  border-radius: var(--radius-full);
  font-size: var(--font-size-sm);
  font-family: var(--font-mono);
  color: var(--brand-violet-glow);
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
  background: var(--bg-glass);
  backdrop-filter: var(--blur-heavy);
  -webkit-backdrop-filter: var(--blur-heavy);
  border: 1px solid var(--border-glass);
  border-radius: var(--radius-full);
  font-size: 11px;
  font-family: var(--font-mono);
  color: var(--text-muted);
  pointer-events: none;
  transition: opacity 0.3s;
}

.zoom-hint.hidden {
  opacity: 0;
}
</style>
