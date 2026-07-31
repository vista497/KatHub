<script setup lang="ts">
import { computed, ref } from 'vue'
import { marked } from 'marked'
import hljs from 'highlight.js/lib/core'
import cpp from 'highlight.js/lib/languages/cpp'
import python from 'highlight.js/lib/languages/python'
import bash from 'highlight.js/lib/languages/bash'
import typescript from 'highlight.js/lib/languages/typescript'
import json from 'highlight.js/lib/languages/json'
import type { ChatMessage } from '../../stores/chatStore'

hljs.registerLanguage('cpp', cpp)
hljs.registerLanguage('python', python)
hljs.registerLanguage('bash', bash)
hljs.registerLanguage('typescript', typescript)
hljs.registerLanguage('ts', typescript)
hljs.registerLanguage('json', json)

const props = defineProps<{
  message: ChatMessage
  showTime?: boolean
}>()

const emit = defineEmits<{
  (e: 'clarify-choice', choice: string): void
}>()

marked.setOptions({ gfm: true, breaks: true })

// ── Content block types ────────────────────────────────────────
interface ContentBlock {
  type: 'text' | 'tool_call' | 'tool_result' | 'clarify'
  content: string
  toolName?: string
  isError?: boolean
  clarifyQuestion?: string
  clarifyChoices?: string[]
}

// ── Parse message content ─────────────────────────────────────
function parseContent(raw: string, role: string, toolName?: string): ContentBlock[] {
  if (role === 'tool') {
    try {
      const obj = JSON.parse(raw)

      // ── Clarify tool result → choice buttons ──────
      if (toolName === 'clarify' || (obj.question && Array.isArray(obj.choices))) {
        return [{
          type: 'clarify',
          content: '',
          toolName: 'clarify',
          clarifyQuestion: obj.question || 'Выберите вариант:',
          clarifyChoices: obj.choices || obj.choices_offered || [],
        }]
      }

      const isErr = obj.error
        || obj.is_error
        || obj.status === 'error'
        || obj.success === false
        || obj.ok === false
      return [{ type: 'tool_result', content: JSON.stringify(obj, null, 2), toolName, isError: isErr }]
    } catch {
      return [{ type: 'tool_result', content: raw, toolName }]
    }
  }

  let content = raw.replace(/^\[Hermes Tool\]\s*/gm, '')

  // Handle <tool_calls> XML in text content
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
        blocks.push({
          type: 'tool_call',
          content: prettyParams(im[2].trim()),
          toolName: im[1],
        })
      }
      lastIdx = match.index + match[0].length
    }

    if (lastIdx < content.length) {
      const md = content.slice(lastIdx).trim()
      if (md) blocks.push({ type: 'text', content: md })
    }
    return blocks
  }

  // Handle <speak>/<detail> for voice+UI dual-channel
  const speakMatch = content.match(/<speak>([\s\S]*?)<\/speak>/)
  const speakContent = speakMatch ? speakMatch[1].trim() : ''
  if (speakMatch) {
    content = content.replace(/<speak>[\s\S]*?<\/speak>\s*/g, '').trim()
  }
  const detailMatch = content.match(/<detail>([\s\S]*?)<\/detail>/)
  if (detailMatch) {
    content = detailMatch[1].trim()
  } else if (speakContent) {
    // No <detail> — merge speak with any remaining text
    content = content ? (content + '\n\n' + speakContent) : speakContent
  }

  return [{ type: 'text', content }]
}

function prettyParams(params: string): string {
  try {
    const obj = JSON.parse(params)
    // For known tools, extract meaningful fields
    if (obj.command) return '🖥 ' + obj.command
    if (obj.path) return '📄 ' + (obj.limit ? `${obj.path}:${obj.offset}-${obj.offset + obj.limit}` : obj.path)
    if (obj.pattern) return '🔍 ' + obj.pattern
    if (obj.query) return '🌐 ' + obj.query
    return JSON.stringify(obj, null, 2)
  } catch {
    return params
  }
}

function formatToolResult(toolName: string, result: string): string {
  try {
    const obj = JSON.parse(result)
    // ── Terminal ──────────────────────────
    if (toolName === 'terminal') {
      const out = obj.output || obj.stdout || ''
      const code = obj.exit_code !== undefined ? ` (exit ${obj.exit_code})` : ''
      return out + code
    }
    // ── read_file ─────────────────────────
    if (toolName === 'read_file') {
      const content = obj.content || ''
      const lines = obj.total_lines ? ` (${obj.total_lines} lines)` : ''
      return content + lines
    }
    // ── search_files ──────────────────────
    if (toolName === 'search_files') {
      const matches = obj.matches || []
      if (Array.isArray(matches)) {
        return matches.map((m: any) => m.path || m.file || JSON.stringify(m)).join('\n')
      }
    }
    // ── web_search ────────────────────────
    if (toolName === 'web_search') {
      const data = obj.data || obj
      const results = data?.web || data?.results || []
      if (Array.isArray(results)) {
        return results.map((r: any) => `${r.title || ''}\n  ${r.url || ''}`).join('\n\n')
      }
    }
    // ── clarify ───────────────────────────
    if (toolName === 'clarify') {
      if (obj.user_response) return '👤 ' + obj.user_response
      if (obj.choices) return (obj.choices as string[]).join(' | ')
    }
    // ── Generic ───────────────────────────
    return JSON.stringify(obj, null, 2)
  } catch {
    return result
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

// ── Build display blocks ──────────────────────────────────────
const blocks = computed(() => {
  const result = parseContent(
    props.message.content,
    props.message.role,
    props.message.tool_name
  )

  // Append tool calls from assistant messages
  if (props.message.tool_calls && props.message.tool_calls.length > 0) {
    for (const tc of props.message.tool_calls) {
      // ── Clarify: show interactive buttons BEFORE response ──
      if (tc.name === 'clarify' && tc.result === undefined) {
        try {
          const args = JSON.parse(tc.args)
          if (args.question && Array.isArray(args.choices)) {
            result.push({
              type: 'clarify' as const,
              content: '',
              toolName: 'clarify',
              clarifyQuestion: args.question,
              clarifyChoices: args.choices,
            })
            continue
          }
        } catch { /* fall through to generic tool_call */ }
      }

      if (tc.result !== undefined) {
        // Tool has completed — show as result (✓ / ✕)
        // For clarify with result: show what user chose
        if (tc.name === 'clarify') {
          try {
            const obj = JSON.parse(tc.result)
            result.push({
              type: 'tool_result' as const,
              content: obj.user_response || tc.result,
              toolName: 'clarify',
              isError: false,
            })
            continue
          } catch { /* fall through */ }
        }
        result.push({
          type: 'tool_result' as const,
          content: formatToolResult(tc.name, tc.result || ''),
          toolName: tc.name,
          isError: tc.isError || false,
        })
      } else {
        // Tool still pending — show as ⏳
        result.push({
          type: 'tool_call' as const,
          content: prettyParams(tc.args),
          toolName: tc.name,
        })
      }
    }
  }

  return result.filter(b => b.type === 'clarify' || b.content.trim())
})

const timeStr = computed(() =>
  new Date(props.message.timestamp).toLocaleTimeString([], {
    hour: '2-digit',
    minute: '2-digit',
  }),
)

// Tool block expand/collapse
const expandedTools = ref<Set<number>>(new Set())
function toggleTool(idx: number) {
  const next = new Set(expandedTools.value)
  if (next.has(idx)) next.delete(idx)
  else next.add(idx)
  expandedTools.value = next
}
</script>

<template>
  <div class="message" :class="[props.message.role]">
    <template v-for="(block, i) in blocks" :key="i">
      <!-- Tool call — ⏳ pending execution -->
      <div v-if="block.type === 'tool_call'" class="tool-block tool-pending">
        <div class="tool-header" @click="toggleTool(i)">
          <span class="tool-arrow">{{ expandedTools.has(i) ? '▼' : '▶' }}</span>
          <span class="tool-icon pending-icon">⏳</span>
          <span class="tool-name">{{ block.toolName || 'Инструмент' }}</span>
        </div>
        <pre v-if="expandedTools.has(i)" class="tool-body">{{ block.content }}</pre>
      </div>

      <!-- Clarify — question + choice buttons -->
      <div v-else-if="block.type === 'clarify'" class="clarify-block">
        <div class="clarify-question">{{ block.clarifyQuestion }}</div>
        <div class="clarify-choices">
          <button
            v-for="(choice, ci) in block.clarifyChoices"
            :key="ci"
            class="clarify-btn"
            @click="emit('clarify-choice', choice)"
          >{{ choice }}</button>
        </div>
      </div>

      <!-- Tool result — ✓ success / ✕ error -->
      <div v-else-if="block.type === 'tool_result'"
           class="tool-block"
           :class="{ 'tool-result': !block.isError, 'tool-error': block.isError }">
        <div class="tool-header" @click="toggleTool(i)">
          <span class="tool-arrow">{{ expandedTools.has(i) ? '▼' : '▶' }}</span>
          <span class="tool-icon" :class="{ 'check-icon': !block.isError, 'error-icon': block.isError }">
            {{ block.isError ? '✕' : '✓' }}
          </span>
          <span class="tool-name">{{ block.toolName || 'Результат инструмента' }}</span>
        </div>
        <pre v-if="expandedTools.has(i)" class="tool-body">{{ block.content }}</pre>
      </div>

      <!-- Text / markdown -->
      <div v-else class="bubble" v-html="renderMarkdown(block.content)"></div>
    </template>

    <div v-if="showTime" class="time">{{ timeStr }}</div>
  </div>
</template>

<style scoped>
.message {
  display: flex;
  flex-direction: column;
  max-width: 88%;
  margin-bottom: 6px;
}
.message.user     { align-self: flex-end; align-items: flex-end; }
.message.assistant { align-self: flex-start; }
.message.tool      { align-self: flex-start; }

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

/* ── Tool blocks ──────────────────────────────── */
.tool-block {
  background: #0e0e22;
  border-radius: 8px;
  overflow: hidden;
  margin-bottom: 4px;
}
.tool-pending { border: 1px solid rgba(255, 200, 50, 0.3); }
.tool-result  { border: 1px solid rgba(100, 255, 150, 0.35); }
.tool-error   { border: 1px solid rgba(255, 100, 100, 0.4); }

.tool-header {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 6px 10px;
  cursor: pointer;
  user-select: none;
  transition: background 0.12s;
}
.tool-pending .tool-header { background: rgba(255, 200, 50, 0.06); border-bottom: 1px solid rgba(255, 200, 50, 0.1); }
.tool-result .tool-header  { background: rgba(100, 255, 150, 0.08); border-bottom: 1px solid rgba(100, 255, 150, 0.12); }
.tool-error .tool-header   { background: rgba(255, 100, 100, 0.08); border-bottom: 1px solid rgba(255, 100, 100, 0.12); }
.tool-header:hover         { background: rgba(255, 255, 255, 0.04); }

.tool-arrow { font-size: 9px; color: #666; width: 10px; flex-shrink: 0; }
.tool-icon  { font-size: 12px; flex-shrink: 0; }
.pending-icon { color: #ffc832; font-size: 12px; }
.check-icon   { color: #64ff96; font-weight: bold; font-size: 13px; }
.error-icon   { color: #ff5555; font-weight: bold; font-size: 14px; }
.tool-name {
  font-size: 11px;
  font-weight: 600;
  font-family: 'Fira Code', 'JetBrains Mono', monospace;
}
.tool-pending .tool-name { color: #ffc832; }
.tool-result .tool-name  { color: #64ff96; }
.tool-error .tool-name   { color: #ff5555; }

.tool-body {
  padding: 8px 12px;
  margin: 0;
  font-size: 11px;
  font-family: 'Fira Code', 'JetBrains Mono', monospace;
  color: #8888aa;
  white-space: pre-wrap;
  word-break: break-all;
  max-height: 200px;
  overflow-y: auto;
  line-height: 1.4;
}

/* ── Markdown ─────────────────────────────────── */
.bubble :deep(h1), .bubble :deep(h2), .bubble :deep(h3) {
  font-size: 14px; font-weight: 700; color: #e8e8ff; margin: 10px 0 4px;
}
.bubble :deep(h1:first-child), .bubble :deep(h2:first-child), .bubble :deep(h3:first-child) { margin-top: 0; }
.bubble :deep(p) { margin: 0 0 6px; }
.bubble :deep(p:last-child) { margin-bottom: 0; }
.bubble :deep(strong) { color: #f0e0ff; font-weight: 700; }
.bubble :deep(em)     { color: #c8b8e8; }
.bubble :deep(ul), .bubble :deep(ol) { margin: 4px 0; padding-left: 20px; }
.bubble :deep(li)  { margin: 2px 0; }
.bubble :deep(a) { color: var(--color-accent-secondary); text-decoration: underline; }
.bubble :deep(blockquote) {
  border-left: 3px solid rgba(124, 92, 255, 0.4);
  padding: 4px 10px; margin: 6px 0;
  background: rgba(124, 92, 255, 0.05);
  border-radius: 0 4px 4px 0;
  color: #aaaacc;
}
.bubble :deep(.inline-code) {
  background: rgba(124, 92, 255, 0.15);
  color: #ccccee;
  padding: 1px 5px;
  border-radius: 3px;
  font-family: 'Fira Code', 'JetBrains Mono', monospace;
  font-size: 12px;
}
.bubble :deep(.code-block) {
  margin: 8px 0; padding: 12px;
  border-radius: 8px;
  background: #0a0a1e;
  border: 1px solid rgba(124, 92, 255, 0.12);
  overflow-x: auto;
  font-size: 11.5px; line-height: 1.5;
}
.bubble :deep(.hljs-keyword)  { color: #c792ea; }
.bubble :deep(.hljs-string)   { color: #c3e88d; }
.bubble :deep(.hljs-number)   { color: #f78c6c; }
.bubble :deep(.hljs-comment)  { color: #676e95; font-style: italic; }
.bubble :deep(.hljs-function) { color: #82aaff; }
.bubble :deep(.hljs-type)     { color: #ffcb6b; }
.bubble :deep(.hljs-built_in) { color: #89ddff; }
.bubble :deep(.hljs-title)    { color: #82aaff; }
.bubble :deep(.hljs-attr)     { color: #ffcb6b; }
.bubble :deep(hr) { border: none; border-top: 1px solid rgba(124, 92, 255, 0.15); margin: 10px 0; }
.bubble :deep(table) { border-collapse: collapse; margin: 8px 0; font-size: 12px; width: 100%; }
.bubble :deep(th), .bubble :deep(td) { border: 1px solid rgba(124, 92, 255, 0.12); padding: 6px 10px; text-align: left; }
.bubble :deep(th) { background: rgba(124, 92, 255, 0.1); color: #ccccee; font-weight: 600; }

/* ── Timestamp ────────────────────────────────── */
.time {
  font-size: 10px;
  color: var(--color-text-muted);
  margin-top: 2px;
  padding: 0 4px;
}

/* ── Clarify (choice buttons) ──────────────────── */
.clarify-block {
  background: #0d0d26;
  border: 1px solid rgba(124, 92, 255, 0.25);
  border-radius: 10px;
  padding: 12px 14px;
  margin-bottom: 4px;
}
.clarify-question {
  font-size: 13px;
  color: #ccccee;
  margin-bottom: 10px;
  line-height: 1.45;
}
.clarify-choices { display: flex; flex-wrap: wrap; gap: 8px; }
.clarify-btn {
  background: rgba(124, 92, 255, 0.12);
  border: 1px solid rgba(124, 92, 255, 0.3);
  border-radius: 8px;
  color: #d0d0ff;
  padding: 8px 16px;
  font-size: 13px;
  font-family: var(--font-sans);
  cursor: pointer;
  transition: all 0.15s;
}
.clarify-btn:hover, .clarify-btn:active {
  background: rgba(124, 92, 255, 0.25);
  border-color: var(--color-accent);
  color: #fff;
  transform: translateY(-1px);
}
</style>
