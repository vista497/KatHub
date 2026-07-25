<script setup lang="ts">
import { computed } from 'vue'
import { marked } from 'marked'
import hljs from 'highlight.js/lib/core'
import cpp from 'highlight.js/lib/languages/cpp'
import python from 'highlight.js/lib/languages/python'
import bash from 'highlight.js/lib/languages/bash'
import typescript from 'highlight.js/lib/languages/typescript'
import json from 'highlight.js/lib/languages/json'

hljs.registerLanguage('cpp', cpp)
hljs.registerLanguage('python', python)
hljs.registerLanguage('bash', bash)
hljs.registerLanguage('typescript', typescript)
hljs.registerLanguage('ts', typescript)
hljs.registerLanguage('json', json)

const props = defineProps<{
  message: { id: string; role: string; content: string; timestamp: number }
}>()

marked.setOptions({ gfm: true, breaks: true })

// ── Content parser ──────────────────────────────────────────

interface ContentBlock {
  type: 'text' | 'tool_call' | 'tool_result'
  content: string
  toolName?: string
}

function parseContent(raw: string, role: string): ContentBlock[] {
  // Tool result — try to pretty-print JSON
  if (role === 'tool') {
    try {
      const obj = JSON.parse(raw)
      return [{ type: 'tool_result', content: JSON.stringify(obj, null, 2) }]
    } catch {
      return [{ type: 'tool_result', content: raw }]
    }
  }

  let content = raw.replace(/^\[Hermes Tool\]\s*/gm, '')

  // Detect <tool_calls> blocks (Hermes XML format)
  if (/<tool_calls>/.test(content)) {
    const blocks: ContentBlock[] = []
    const tcRegex = /<tool_calls>([\s\S]*?)<\/tool_calls>/g
    let lastIdx = 0
    let match: RegExpExecArray | null

    while ((match = tcRegex.exec(content)) !== null) {
      if (match.index > lastIdx) {
        const md = content.slice(lastIdx, match.index).trim()
        if (md) blocks.push({ type: 'text', content: md })
      }

      const tcContent = match[1]
      const invokeRegex = /<invoke name="([^"]+)">([\s\S]*?)<\/invoke>/g
      let im: RegExpExecArray | null
      while ((im = invokeRegex.exec(tcContent)) !== null) {
        const toolName = im[1]
        const params = prettyParams(im[2].trim())
        blocks.push({ type: 'tool_call', content: params, toolName })
      }

      lastIdx = match.index + match[0].length
    }

    if (lastIdx < content.length) {
      const md = content.slice(lastIdx).trim()
      if (md) blocks.push({ type: 'text', content: md })
    }

    return blocks
  }

  // Extract <speak> tag — use for compact spoken prefix
  const speakMatch = content.match(/<speak>([\s\S]*?)<\/speak>/)
  const speakContent = speakMatch ? speakMatch[1].trim() : ''
  if (speakMatch) {
    // Remove speak tag from content
    content = content.replace(/<speak>[\s\S]*?<\/speak>\s*/g, '').trim()
  }

  // Extract <detail> if present — this is the authoritative UI content
  const detailMatch = content.match(/<detail>([\s\S]*?)<\/detail>/)
  if (detailMatch) {
    content = detailMatch[1].trim()
  } else if (!content && speakContent) {
    // No detail AND nothing outside speak — use speak content
    content = speakContent
  }

  return [{ type: 'text', content }]
}

function prettyParams(params: string): string {
  try {
    const obj = JSON.parse(params)
    return JSON.stringify(obj, null, 2)
  } catch {
    return params
  }
}

function escapeHtml(s: string): string {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
}

function renderMarkdown(text: string): string {
  const renderer = new marked.Renderer()

  renderer.code = function({ text, lang }: { text: string; lang?: string }) {
    const language = lang && hljs.getLanguage(lang) ? lang : 'plaintext'
    try {
      const highlighted = hljs.highlight(text, { language }).value
      return `<pre class="code-block"><code class="hljs language-${language}">${highlighted}</code></pre>`
    } catch {
      return `<pre class="code-block"><code>${escapeHtml(text)}</code></pre>`
    }
  }

  renderer.codespan = function({ text }: { text: string }) {
    return `<code class="inline-code">${escapeHtml(text)}</code>`
  }

  return marked.parse(text, { renderer }) as string
}

const blocks = computed(() =>
  parseContent(props.message.content, props.message.role)
    .filter(b => b.content.trim())
)

const timeStr = computed(() =>
  new Date(props.message.timestamp).toLocaleTimeString([], {
    hour: '2-digit',
    minute: '2-digit',
  }),
)

const isToolRelated = computed(() =>
  props.message.role === 'tool' || blocks.value.some(b => b.type !== 'text'),
)
</script>

<template>
  <div class="message" :class="[message.role, { 'tool-msg': isToolRelated }]">
    <template v-for="(block, i) in blocks" :key="i">
      <!-- Tool call (invoke) -->
      <div v-if="block.type === 'tool_call'" class="tool-call">
        <div class="tool-header">
          <span class="tool-icon">🔧</span>
          <span class="tool-name">{{ block.toolName }}</span>
        </div>
        <pre class="tool-params">{{ block.content }}</pre>
      </div>

      <!-- Tool result -->
      <div v-else-if="block.type === 'tool_result'" class="tool-result">
        <div class="tool-header">
          <span class="tool-icon">📋</span>
          <span class="tool-name">Tool Result</span>
        </div>
        <pre class="tool-params">{{ block.content }}</pre>
      </div>

      <!-- Regular text / markdown -->
      <div v-else class="bubble" v-html="renderMarkdown(block.content)"></div>
    </template>

    <div class="time">{{ timeStr }}</div>
  </div>
</template>

<style scoped>
.message {
  display: flex;
  flex-direction: column;
  max-width: 88%;
  margin-bottom: 6px;
}

.message.user {
  align-self: flex-end;
  align-items: flex-end;
}

.message.assistant {
  align-self: flex-start;
}

.message.tool {
  align-self: flex-start;
}

/* ── Bubble ───────────────────────────────────── */
.bubble {
  padding: 10px 14px;
  border-radius: 12px;
  font-size: 13px;
  line-height: 1.55;
  word-break: break-word;
}

.user .bubble {
  background: var(--color-accent);
  color: #fff;
  border-bottom-right-radius: 4px;
}

.assistant .bubble {
  background: #14142e;
  border: 1px solid rgba(124, 92, 255, 0.1);
  border-bottom-left-radius: 4px;
  color: #d0d0e8;
}

/* ── Markdown inside bubble ───────────────────── */
.bubble :deep(h1),
.bubble :deep(h2),
.bubble :deep(h3) {
  font-size: 14px;
  font-weight: 700;
  color: #e8e8ff;
  margin: 10px 0 4px;
}
.bubble :deep(h1:first-child),
.bubble :deep(h2:first-child),
.bubble :deep(h3:first-child) { margin-top: 0; }

.bubble :deep(p) { margin: 0 0 6px; }
.bubble :deep(p:last-child) { margin-bottom: 0; }

.bubble :deep(strong) { color: #f0e0ff; font-weight: 700; }
.bubble :deep(em)     { color: #c8b8e8; }

.bubble :deep(ul),
.bubble :deep(ol) { margin: 4px 0; padding-left: 20px; }
.bubble :deep(li)  { margin: 2px 0; }

.bubble :deep(a) {
  color: var(--color-accent-secondary);
  text-decoration: underline;
}

.bubble :deep(blockquote) {
  border-left: 3px solid rgba(124, 92, 255, 0.4);
  padding: 4px 10px;
  margin: 6px 0;
  background: rgba(124, 92, 255, 0.05);
  border-radius: 0 4px 4px 0;
  color: #aaaacc;
}

/* ── Inline code ──────────────────────────────── */
.bubble :deep(.inline-code) {
  background: rgba(124, 92, 255, 0.15);
  color: #ccccee;
  padding: 1px 5px;
  border-radius: 3px;
  font-family: 'Fira Code', 'JetBrains Mono', monospace;
  font-size: 12px;
}

/* ── Code blocks ──────────────────────────────── */
.bubble :deep(.code-block) {
  margin: 8px 0;
  padding: 12px;
  border-radius: 8px;
  background: #0a0a1e;
  border: 1px solid rgba(124, 92, 255, 0.12);
  overflow-x: auto;
  font-size: 11.5px;
  line-height: 1.5;
}

/* highlight.js overrides (in case theme doesn't apply) */
.bubble :deep(.hljs-keyword)  { color: #c792ea; }
.bubble :deep(.hljs-string)   { color: #c3e88d; }
.bubble :deep(.hljs-number)   { color: #f78c6c; }
.bubble :deep(.hljs-comment)  { color: #676e95; font-style: italic; }
.bubble :deep(.hljs-function) { color: #82aaff; }
.bubble :deep(.hljs-type)     { color: #ffcb6b; }
.bubble :deep(.hljs-built_in) { color: #89ddff; }
.bubble :deep(.hljs-title)    { color: #82aaff; }
.bubble :deep(.hljs-attr)     { color: #ffcb6b; }

/* ── Tables / hr ──────────────────────────────── */
.bubble :deep(hr) {
  border: none;
  border-top: 1px solid rgba(124, 92, 255, 0.15);
  margin: 10px 0;
}
.bubble :deep(table) { border-collapse: collapse; margin: 8px 0; font-size: 12px; width: 100%; }
.bubble :deep(th),
.bubble :deep(td) { border: 1px solid rgba(124, 92, 255, 0.12); padding: 6px 10px; text-align: left; }
.bubble :deep(th) { background: rgba(124, 92, 255, 0.1); color: #ccccee; font-weight: 600; }

/* ── Tool call blocks ─────────────────────────── */
.tool-call {
  background: #0e0e22;
  border: 1px solid rgba(92, 224, 255, 0.2);
  border-radius: 8px;
  overflow: hidden;
  margin-bottom: 4px;
}

.tool-result {
  background: #0e0e22;
  border: 1px solid rgba(255, 140, 100, 0.2);
  border-radius: 8px;
  overflow: hidden;
  margin-bottom: 4px;
}

.tool-header {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 6px 10px;
  background: rgba(92, 224, 255, 0.06);
  border-bottom: 1px solid rgba(92, 224, 255, 0.1);
}

.tool-result .tool-header {
  background: rgba(255, 140, 100, 0.06);
  border-bottom: 1px solid rgba(255, 140, 100, 0.1);
}

.tool-icon {
  font-size: 12px;
}

.tool-name {
  font-size: 11px;
  font-weight: 600;
  color: var(--color-accent-secondary);
  font-family: 'Fira Code', 'JetBrains Mono', monospace;
}

.tool-result .tool-name {
  color: #ff9575;
}

.tool-params {
  padding: 8px 12px;
  margin: 0;
  font-size: 11px;
  font-family: 'Fira Code', 'JetBrains Mono', monospace;
  color: #8888aa;
  white-space: pre-wrap;
  word-break: break-all;
  max-height: 160px;
  overflow-y: auto;
  line-height: 1.4;
}

/* ── Timestamp ────────────────────────────────── */
.time {
  font-size: 10px;
  color: var(--color-text-muted);
  margin-top: 2px;
  padding: 0 4px;
}
</style>
