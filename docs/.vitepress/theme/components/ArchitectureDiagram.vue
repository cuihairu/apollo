<script setup lang="ts">
import { computed } from 'vue'
import { useData, withBase } from 'vitepress'

const props = defineProps<{ kind: 'system' | 'space' | 'cross-server' }>()
const { lang } = useData()
const english = computed(() => lang.value.startsWith('en'))
const language = computed(() => english.value ? 'en' : 'zh')
const source = computed(() => withBase(`/architecture/${props.kind}-${language.value}.svg`))
const narrowSource = computed(() => withBase(`/architecture/${props.kind}-${language.value}-mobile.svg`))
const descriptions = {
  system: {
    zh: 'Apollo 目标架构：gateway 分别路由到 Base 与 cell，worldServer 管理空间和执行进程，Base 经 database 保存长期资料。',
    en: 'Apollo target architecture: gateway routes to Base and cell; worldServer manages spaces and executors; Base saves profiles through database.',
  },
  space: {
    zh: 'Space、Cell、cell 进程与 Instance 的边界：默认每 Space 一个 Cell，未来可在同一 Space 内扩展多个 Cell。',
    en: 'Space, Cell, executor and Instance boundaries: initially one Cell per Space, with multiple Cells in one Space reserved for later.',
  },
  'cross-server': {
    zh: '跨服设计：共享 cell 承担模拟，各玩家原 Base 和 database 保持资料归属，业务结果分别回到原 Base 校验保存。',
    en: 'Cross-server design: a shared cell simulates players while original Bases and databases retain profiles and validate and persist results.',
  },
}
const description = computed(() => descriptions[props.kind][language.value])
const wideHeight = computed(() => ({ system: 840, space: 700, 'cross-server': 760 })[props.kind])
const narrowHeight = computed(() => ({ system: 1440, space: 1135, 'cross-server': 1270 })[props.kind])
</script>

<template>
  <figure class="architecture-figure" :aria-label="description">
    <div class="architecture-canvas">
      <img class="architecture-wide" :src="source" width="1280" :height="wideHeight" :alt="description" decoding="async" />
      <img class="architecture-narrow" :src="narrowSource" width="480" :height="narrowHeight" :alt="description" decoding="async" />
    </div>
    <figcaption class="architecture-caption">
      <span>{{ english ? 'Target design. Implementation status is documented separately.' : '目标设计图；实际接线与实现状态以应用说明为准。' }}</span>
      <a :href="source" target="_blank" rel="noopener noreferrer">
        {{ english ? 'Open full-size diagram' : '查看矢量原图' }}
        <svg aria-hidden="true" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M7 17 17 7M7 7h10v10" /></svg>
      </a>
    </figcaption>
  </figure>
</template>

<style scoped>
.architecture-figure {
  container: architecture / inline-size;
  margin: 28px 0 36px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 12px;
  overflow: hidden;
}
.architecture-canvas {
  background: #fff;
  padding: 12px;
}
.architecture-canvas img {
  display: block;
  width: 100%;
  height: auto;
  margin: 0;
}
.architecture-canvas .architecture-narrow {
  display: none;
}
.architecture-caption {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  justify-content: space-between;
  gap: 8px 20px;
  padding: 13px 18px;
  border-top: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg-soft);
  color: var(--vp-c-text-2);
  font-size: 13px;
  line-height: 1.6;
}
.architecture-caption a {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  min-height: 28px;
  color: var(--vp-c-brand-1);
  font-weight: 500;
  text-decoration: underline;
  text-underline-offset: 3px;
}
.architecture-caption a:focus-visible {
  outline: 2px solid var(--vp-c-brand-1);
  outline-offset: 4px;
  border-radius: 2px;
}
@container architecture (max-width: 720px) {
  .architecture-canvas { padding: 4px; }
  .architecture-canvas .architecture-wide { display: none; }
  .architecture-canvas .architecture-narrow { display: block; max-width: 520px; margin: 0 auto; }
  .architecture-caption { padding: 12px 16px; }
  .architecture-caption a { min-height: 44px; }
}
@supports not (container-type: inline-size) {
  @media (max-width: 760px) {
    .architecture-canvas .architecture-wide { display: none; }
    .architecture-canvas .architecture-narrow { display: block; max-width: 520px; margin: 0 auto; }
  }
}
</style>
