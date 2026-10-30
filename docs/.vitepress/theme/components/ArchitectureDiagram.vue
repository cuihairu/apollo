<script setup lang="ts">
import { computed } from 'vue'
import { useData, withBase } from 'vitepress'

const props = defineProps<{ kind: 'system' | 'space' | 'cross-server' | 'callbacks' }>()
const { lang } = useData()
const english = computed(() => lang.value.startsWith('en'))
const language = computed(() => english.value ? 'en' : 'zh')
// system kind uses the chorus-style process architecture (docs/public/images/architecture-{lang}.svg);
// space / cross-server / callbacks keep their dedicated SVGs.
const systemSource = computed(() => withBase(`/images/architecture-${language.value}.svg`))
const source = computed(() => props.kind === 'system'
  ? systemSource.value
  : withBase(`/architecture/${props.kind}-${language.value}.svg`))
const narrowSource = computed(() => props.kind === 'system'
  ? systemSource.value
  : withBase(`/architecture/${props.kind}-${language.value}-mobile.svg`))
const descriptions = {
  callbacks: {
    zh: 'Apollo 外部回调与发货：interfaces 可靠受理链、按归属代次解析当前 Base 与幂等发货模块已落地；渠道验签与进程间接线规划中；apolloc init 脚手架已交付，CI 在仓库执行。',
    en: 'External callbacks and fulfillment: the interfaces intake chain, epoch-based current-Base resolution, and idempotent fulfillment modules are implemented; channel verification and inter-process wiring remain planned; the apolloc init scaffolding is delivered and CI runs in the repository.',
  },
  system: {
    zh: 'Apollo 进程架构：world 为管理端（注册/拓扑/容量/迁移），cellServer 为独立模拟执行端（Scene/Avatar/AOI/战斗/Lua），二者经 960 注册/961 裁决/962 分配/963 retire/967 自报 协调；Base 管玩家档案与基础业务并经 database 持久；gateway 直达 Base 与 cell；interfaces 接平台回调；machined 拉起并监督各进程；logger/admin 收集日志与指标。',
    en: 'Apollo process architecture: world is the manager (register/topology/capacity/migration) and cellServer the standalone simulation executor (Scene/Avatar/AOI/battle/Lua), coordinated over 960 register/961 verdict/962 assign/963 retire/967 self-report; Base owns player profiles and base gameplay persisted via database; gateway routes directly to Base and cell; interfaces receives platform callbacks; machined spawns and supervises; logger/admin collect logs and metrics.',
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
const wideHeight = computed(() => ({ system: 1915, space: 700, 'cross-server': 760, callbacks: 920 })[props.kind])
const narrowHeight = computed(() => ({ system: 1915, space: 1135, 'cross-server': 1270, callbacks: 1510 })[props.kind])
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
