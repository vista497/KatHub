<script setup lang="ts">
import { ref, onMounted, onUnmounted, watch, nextTick } from 'vue'
import * as d3 from 'd3'
import { useGraphStore } from '../../stores/graphStore'

const container = ref<HTMLDivElement>()
const svgRef = ref<SVGSVGElement>()
const graph = useGraphStore()

let simulation: d3.Simulation<any, any> | null = null

function initGraph() {
  if (!svgRef.value || !container.value || !graph.nodes.length) return

  const width = container.value.clientWidth
  const height = container.value.clientHeight

  // Clear
  d3.select(svgRef.value).selectAll('*').remove()

  const svg = d3.select(svgRef.value)
    .attr('width', width)
    .attr('height', height)

  // Background stars
  svg.append('rect')
    .attr('width', width)
    .attr('height', height)
    .attr('fill', 'transparent')

  // Zoom group
  const g = svg.append('g')

  const zoom = d3.zoom<SVGSVGElement, unknown>()
    .scaleExtent([0.1, 5])
    .on('zoom', (event) => {
      g.attr('transform', event.transform)
    })

  svg.call(zoom)

  // Links
  const link = g.append('g')
    .selectAll('line')
    .data(graph.links)
    .join('line')
    .attr('stroke', 'var(--color-link, rgba(124,92,255,0.15))')
    .attr('stroke-width', 0.5)
    .attr('stroke-opacity', 0.4)

  // Nodes group
  const node = g.append('g')
    .selectAll('g')
    .data(graph.nodes)
    .join('g')
    .attr('cursor', 'pointer')

  // Drag behavior
  const drag = d3.drag<any, any>()
    .on('start', (_event, d) => {
      if (!_event.active) simulation!.alphaTarget(0.3).restart()
      d.fx = d.x
      d.fy = d.y
    })
    .on('drag', (event, d) => {
      d.fx = event.x
      d.fy = event.y
    })
    .on('end', (_event, d) => {
      if (!_event.active) simulation!.alphaTarget(0)
      d.fx = null
      d.fy = null
    })

  node.call(drag)

  // Glow circle (behind)
  node.append('circle')
    .attr('r', (d: any) => d.type === 'folder' ? 12 : 8)
    .attr('fill', (d: any) => {
      switch (d.type) {
        case 'folder': return 'var(--color-node-folder, #ff6b9d)'
        case 'tag': return 'var(--color-node-tag, #5ce0ff)'
        default: return 'var(--color-node-default, #7c5cff)'
      }
    })
    .attr('opacity', 0.08)

  // Main circle
  node.append('circle')
    .attr('r', (d: any) => d.type === 'folder' ? 6 : d.type === 'tag' ? 4 : 3)
    .attr('fill', (d: any) => {
      switch (d.type) {
        case 'folder': return 'var(--color-node-folder, #ff6b9d)'
        case 'tag': return 'var(--color-node-tag, #5ce0ff)'
        default: return 'var(--color-node-default, #7c5cff)'
      }
    })
    .attr('opacity', 0.85)
    .attr('stroke', (d: any) => d.id === graph.selectedNode ? '#ffffff' : 'transparent')
    .attr('stroke-width', 2)

  // Labels (only folders)
  node.append('text')
    .text((d: any) => d.label)
    .attr('font-size', '8px')
    .attr('fill', 'var(--color-text-secondary, #8888aa)')
    .attr('dx', 8)
    .attr('dy', 3)
    .attr('opacity', (d: any) => d.type === 'folder' ? 0.7 : 0)

  // Click
  node.on('click', (_event: any, d: any) => {
    graph.selectNode(d.id === graph.selectedNode ? null : d.id)
  })

  // Click on background to deselect
  svg.on('click', () => {
    graph.clearSelection()
  })

  // Simulation
  simulation = d3.forceSimulation(graph.nodes)
    .force('link', d3.forceLink(graph.links).id((d: any) => d.id).distance(60))
    .force('charge', d3.forceManyBody().strength(-120))
    .force('center', d3.forceCenter(width / 2, height / 2))
    .force('collision', d3.forceCollide().radius(12))
    .on('tick', () => {
      link
        .attr('x1', (d: any) => d.source.x)
        .attr('y1', (d: any) => d.source.y)
        .attr('x2', (d: any) => d.target.x)
        .attr('y2', (d: any) => d.target.y)
      node.attr('transform', (d: any) => `translate(${d.x},${d.y})`)
    })
}

function onResize() {
  initGraph()
}

onMounted(() => {
  window.addEventListener('resize', onResize)
  graph.fetchGraph()
})

onUnmounted(() => {
  window.removeEventListener('resize', onResize)
  simulation?.stop()
})

watch(() => graph.nodes.length, async () => {
  if (graph.nodes.length) {
    await nextTick()
    initGraph()
  }
})

// Redraw on selection change
watch(() => graph.selectedNode, () => {
  if (graph.nodes.length) initGraph()
})
</script>

<template>
  <div ref="container" class="graph-container">
    <!-- Starfield background -->
    <div class="starfield"></div>

    <!-- Loading -->
    <div v-if="graph.loading" class="overlay">
      <div class="loading-text">Loading galaxy...</div>
    </div>

    <!-- Error -->
    <div v-else-if="graph.error" class="overlay">
      <div class="error-text">⚠️ {{ graph.error }}</div>
      <button class="retry-btn" @click="graph.fetchGraph">Retry</button>
    </div>

    <!-- SVG -->
    <svg ref="svgRef" class="graph-svg"></svg>

    <!-- Selected node info -->
    <div v-if="graph.selectedNode" class="node-info">
      {{ graph.selectedNode }}
    </div>
  </div>
</template>

<style scoped>
.graph-container {
  position: relative;
  width: 100%;
  height: 100%;
  overflow: hidden;
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
</style>
