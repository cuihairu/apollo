#!/usr/bin/env python3
"""Generate the localized architecture SVGs used by VitePress and GitHub README.

Keep role names and relationships aligned with docs/zh/design/bigworld-apollo-mapping.md.
Run from any directory: python3 scripts/docs/render_architecture.py [--check]
"""
from pathlib import Path
from html import escape
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'docs/public/architecture'
IMG = ROOT / 'docs/public/images'
INK = '#172b45'
MUTED = '#4c5d73'
BLUE = '#245bb2'
TEAL = '#147565'
LINE = '#ced8e5'

class Diagram:
    def __init__(self, width, height, title, description, lang):
        self.width, self.height = width, height
        self.items = [f'''<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" role="img" aria-labelledby="title desc" lang="{lang}">
<title id="title">{escape(title)}</title><desc id="desc">{escape(description)}</desc>
<defs>
 <marker id="blue" markerWidth="8" markerHeight="8" refX="7" refY="4" orient="auto-start-reverse"><path d="M1 1 L7 4 L1 7" fill="none" stroke="{BLUE}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></marker>
 <marker id="teal" markerWidth="8" markerHeight="8" refX="7" refY="4" orient="auto-start-reverse"><path d="M1 1 L7 4 L1 7" fill="none" stroke="{TEAL}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></marker>
 <style>text{{font-family:ui-sans-serif,system-ui,-apple-system,"Segoe UI","Noto Sans CJK SC","Microsoft YaHei",sans-serif;fill:{INK}}}.sub{{fill:{MUTED}}}.blue{{fill:{BLUE}}}.teal{{fill:{TEAL}}}</style>
</defs><rect width="{width}" height="{height}" fill="#fff"/>''']
    def rect(self,x,y,w,h,fill='#fff',stroke=LINE,r=10,dash=False):
        self.items.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{fill}" stroke="{stroke}" stroke-width="1.3"'+(' stroke-dasharray="6 5"' if dash else '')+'/>')
    def text(self,x,y,text,size=18,weight=400,kind='',anchor='start'):
        if self.width == 480: size = max(size, 18)
        self.items.append(f'<text x="{x}" y="{y}" font-size="{size}" font-weight="{weight}" text-anchor="{anchor}"'+(f' class="{kind}"' if kind else '')+f'>{escape(text)}</text>')
    def node(self,x,y,w,h,title,lines=(),status=None,tone='normal',size=23):
        self.items.append(f'<g data-node="{escape(title, quote=True)}">')
        self.rect(x,y,w,h,fill='#f0f5fe' if tone=='blue' else '#fff',stroke='#9bb5dc' if tone=='blue' else LINE)
        self.text(x+18,y+33,title,size,600)
        for i,line in enumerate(lines):self.text(x+18,y+60+i*23,line,17,kind='sub')
        if status:self.text(x+18,y+h-14,status,14,500,'blue' if tone=='blue' else 'sub')
        self.items.append('</g>')
    def arrow(self,points,control=False,both=False):
        color = TEAL if control else BLUE
        path='M'+' L'.join(f'{x} {y}' for x,y in points)
        self.items.append(f'<path d="{path}" fill="none" stroke="{color}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" marker-end="url(#{"teal" if control else "blue"})"'+(f' marker-start="url(#{"teal" if control else "blue"})"' if both else '')+(' stroke-dasharray="7 6"' if control else '')+'/>')
    def label(self,x,y,text,size=16,color='blue'):
        # Opaque labels keep lettering separate from routed connectors.
        width = sum(size*(1 if ord(c)>255 else .55) for c in text)+16
        self.rect(x-width/2,y-size+1,width,size+8,stroke='none',r=3)
        self.text(x,y,text,size,500,color,'middle')
    def cylinder(self,x,y,w,h,title,sub):
        self.items.append(f'<path d="M{x} {y+13} A{w/2} 13 0 0 1 {x+w} {y+13} V{y+h-13} A{w/2} 13 0 0 1 {x} {y+h-13} Z" fill="#f5f7fa" stroke="{LINE}" stroke-width="1.3"/>')
        self.items.append(f'<ellipse cx="{x+w/2}" cy="{y+13}" rx="{w/2}" ry="13" fill="#eef2f7" stroke="{LINE}" stroke-width="1.3"/>')
        self.text(x+w/2,y+42,title,20,600,anchor='middle')
        self.text(x+w/2,y+66,sub,16,kind='sub',anchor='middle')
    def finish(self):return '\n'.join(self.items)+ '\n</svg>\n'

def system(en=False,mobile=False):
    # The chorus-style process architecture is hand-authored and kept as a
    # regeneration source at scripts/docs/templates/architecture-{lang}[,-mobile].svg
    # so edits stay reviewable and --check can detect silent drift. Template files
    # are the single source for the committed SVGs under docs/public/images/.
    tpl = ROOT / 'scripts/docs/templates' / f'architecture-{"en" if en else "zh"}{"-mobile" if mobile else ""}.svg'
    return tpl.read_text(encoding='utf-8')

def space(en=False,mobile=False):
    t=lambda zh,eng:eng if en else zh
    d=Diagram(480 if mobile else 1280,1135 if mobile else 700,t('Space、Cell、执行进程与 Instance','Space, Cell, executor and Instance'),t('默认每 Space 一个 Cell，一个执行进程承载多个空间的 Cell；未来同一 Space 可分配多个 Cell 到不同执行进程。Instance 只关联玩法空间。','Initially each Space has one Cell; one executor hosts Cells from multiple Spaces. Future Cells of one Space may run in different executors. Instance associates with a gameplay Space.'),'en' if en else 'zh-CN')
    if not mobile:
        d.text(28,40,t('运行空间、分区与玩法的边界','Runtime space, partition and gameplay boundaries'),27,600)
        d.node(467,65,346,83,'world',[t('管理注册、Space/Cell 拓扑与分配','Registry · topology · placement')])
        d.text(28,203,t('默认基础路径 · 实施目标','Initial implementation target'),22,600)
        d.text(692,203,t('未来扩展 · 尚未实现','Future extension · not implemented'),22,600)
        d.rect(28,235,560,220,'#f8fafc')
        d.text(48,268,t('cell 进程 1：承载多个 Space 的 Cell','cell process 1: Cells from several Spaces'),18,600)
        d.node(48,290,248,139,'Cell A1',['Space A',t('覆盖该空间全范围','Full-space partition')])
        d.node(320,290,248,139,'Cell B1',['Space B',t('覆盖该空间全范围','Full-space partition')])
        d.arrow([(540,148),(540,235)],True)
        d.arrow([(740,148),(740,165),(1230,165),(1230,235)],True)
        d.node(48,501,248,78,'Instance X',[t('玩法关联 Space A','Activity in Space A')],size=21)
        d.arrow([(172,429),(172,501)],True);d.label(218,480,t('关联','Associates'),15,'teal')
        d.text(322,526,t('Space B 可持续存在','Space B may persist'),18,600)
        d.text(322,554,t('无需临时 Instance','No temporary Instance required'),17,kind='sub')
        d.rect(692,235,560,220,'#f4f9f8')
        d.text(714,268,t('Space A：同一空间 ID 与坐标体系','Space A: one identity and coordinate system'),18,600)
        d.node(714,290,248,139,'Cell A1',[t('cell 进程 1 执行','Runs in cell process 1'),t('本分区权威实体','Real entities')])
        d.node(984,290,248,139,'Cell A2',[t('cell 进程 2 执行','Runs in cell process 2'),t('本分区权威实体','Real entities')])
        d.arrow([(962,359),(984,359)],both=True)
        d.text(714,518,t('跨分区交接模拟权威，保留实体 ID 与坐标。','Hand off authority; preserve entity ID and coordinates.'),17,kind='sub')
        d.text(714,550,t('Ghost、连续可见性与跨边界战斗另行实现。','Ghosts, continuous views and boundary combat are deferred.'),17,kind='sub')
        d.rect(28,614,1224,62,'#f8fafc',stroke='none')
        d.text(48,640,t('Space 对应现有 Scene 的空间语义；Cell 是分区，cell 是进程。','Space matches current Scene semantics; Cell is a partition, cell is a process.'),18,600)
        d.text(48,665,t('Instance 管玩法生命周期，不叠加为 Scene → Space 同义容器。','Instance owns gameplay lifecycle; do not add synonymous Scene → Space containers.'),17,kind='sub')
    else:
        d.text(24,38,t('空间、分区与玩法','Spaces, partitions and gameplay'),23,600)
        d.node(24,73,432,88,'world',[t('注册 · Space/Cell 拓扑 · 分配','Registry · Space/Cell topology · placement')])
        d.text(24,203,t('默认基础路径 · 实施目标','Initial implementation target'),21,600)
        d.rect(24,226,432,254,'#f8fafc')
        d.text(44,260,t('cell 进程 1','cell process 1'),20,600)
        d.node(44,285,184,160,'Cell A1',['Space A',t('空间全范围','Full space')],size=22)
        d.node(252,285,184,160,'Cell B1',['Space B',t('空间全范围','Full space')],size=22)
        d.arrow([(420,161),(420,226)],True);d.label(385,198,t('分配','Place'),16,'teal')
        d.node(24,542,432,88,'Instance X',[t('临时玩法关联 Space A','Activity in Space A')])
        d.text(24,659,t('持续空间无需临时 Instance。','Persistent Spaces need no temporary Instance.'),17,kind='sub')
        d.arrow([(136,445),(136,512),(240,512),(240,542)],True)
        d.text(24,684,t('未来扩展 · 尚未实现','Future extension · not implemented'),21,600)
        d.rect(24,707,432,249,'#f4f9f8')
        d.text(44,741,t('同一个 Space A','The same Space A'),21,600)
        d.node(44,767,184,151,'Cell A1',[t('cell 进程 1','cell process 1'),t('权威实体','Real entities')],size=22)
        d.node(252,767,184,151,'Cell A2',[t('cell 进程 2','cell process 2'),t('权威实体','Real entities')],size=22)
        d.arrow([(228,837),(252,837)],both=True)
        d.text(24,997,t('Space = 运行空间；Cell = 空间分区','Space = runtime space; Cell = partition'),18,600)
        d.text(24,1027,t('cell = 执行进程；Instance = 玩法生命周期','cell = process; Instance = gameplay lifecycle'),17,kind='sub')
        d.text(24,1060,t('Space 对应现有 Scene 语义，不叠加同义层。','Space matches existing Scene semantics.'),17,kind='sub')
        d.text(24,1091,t('Ghost、连续可见性与跨边界战斗尚未实现。','Ghosts and seamless simulation are deferred.'),16,kind='sub')
    return d.finish()

def cross_server(en=False,mobile=False):
    t=lambda zh,eng:eng if en else zh
    d=Diagram(480 if mobile else 1280,1270 if mobile else 760,t('跨服玩法的 Base 与模拟归属','Base and simulation ownership in cross-server play'),t('原服 A、B 各保留 Base 长期对象和 database；共享跨服 cell 只持 Avatar 和运行状态，按每位玩家的 Base 关联提交结果。','Each home server retains its Base profiles and database. A shared cross-server cell holds Avatars/runtime state and submits results to each owning Base.'),'en' if en else 'zh-CN')
    if not mobile:
        d.text(28,40,t('跨服保留存储归属，Base 承载可独立迁移','Cross-server play retains storage; Base hosting may migrate'),25,600)
        d.text(28,74,t('跨服设计预留 · 尚未启用默认编队；临时活动可关联 Instance。','Reserved design · not enabled in the default fleet; temporary activities may associate with Instance.'),17,kind='sub')
        d.node(460,113,360,90,'world',[t('跨服 Space/Cell 分配与迁移管理','Cross-server placement / transfer')])
        for x,letter in [(28,'A'),(892,'B')]:
            d.rect(x,265,360,352,'#f8fafc')
            d.text(x+24,297,t('原服 ','Home server ')+letter,21,600)
            d.node(x+24,322,312,124,'base '+letter,['Avatar · Base '+letter,t('长期资料所有者，归属不变','Profile ownership stays here')],tone='blue')
            d.node(x+24,525,312,68,'database '+letter,[],tone='blue')
            d.arrow([(x+180,446),(x+180,525)],both=True);d.label(x+218,495,t('持久化','Persist'),16)
        d.node(460,322,360,196,t('共享 cell 执行进程','Shared cell executor'),['Space C · Cell C1','Avatar A  /  Avatar B',t('只持玩法快照与运行状态','Gameplay snapshots and runtime only')])
        d.arrow([(640,203),(640,322)],True,both=True);d.label(700,266,t('注册 / 分配','Register / place'),16,'teal')
        d.arrow([(364,360),(460,360)]);d.label(410,346,t('快照','Snapshot'),15)
        d.arrow([(460,417),(364,417)]);d.label(410,403,t('业务结果','Outcomes'),15)
        d.arrow([(916,360),(820,360)]);d.label(868,346,t('快照','Snapshot'),15)
        d.arrow([(820,417),(916,417)]);d.label(868,403,t('业务结果','Outcomes'),15)
        d.text(640,565,t('逐玩家关联原 Base','Associate each player with its Base'),18,600,anchor='middle')
        d.text(640,595,t('不获取各服数据库地址或凭证','No home database addresses or credentials'),16,kind='sub',anchor='middle')
        d.rect(28,661,1224,68,'#f4f9f8',stroke='none')
        d.text(48,688,t('交接：源冻结 → 目标准备 → 条件提交权威 / 路由 → 激活与源清理','Handoff: freeze source → prepare target → commit authority / route → activate and clean source'),18,600)
        d.text(48,714,t('同一实体只允许一个模拟写入者；结算由各原 Base 校验，经 database 分别持久确认。','One simulation writer per entity; each original Base validates settlement and persists through its database.'),17,kind='sub')
    else:
        d.text(24,38,t('跨服玩法与资料归属','Cross-server ownership'),24,600)
        d.text(24,70,t('设计预留 · 未启用默认编队','Reserved · not enabled in default fleet'),17,kind='sub')
        d.node(24,119,432,91,'world',[t('管理跨服 Space/Cell 与迁移','Manage cross-server Space/Cell and transfer')])
        d.node(24,326,432,179,t('共享 cell 执行进程','Shared cell executor'),['Space C · Cell C1','Avatar A  /  Avatar B',t('玩法快照与运行状态','Gameplay snapshots and runtime state')])
        d.arrow([(240,210),(240,326)],True,both=True);d.label(299,274,t('注册 / 分配','Register / place'),17,'teal')
        for x,letter in [(24,'A'),(252,'B')]:
            center=x+102
            d.text(x,631,t('原服 ','Home server ')+letter,20,600)
            d.node(x,659,204,147,'base '+letter,['Avatar · Base '+letter,t('长期资料不迁走','Profiles stay here')],tone='blue',size=20)
            d.node(x,921,204,91,'database '+letter,[t('原存储归属','Original storage')],tone='blue',size=20)
            d.arrow([(center,505),(center,659)],both=True);d.label(center,575,t('快照 / 业务结果','Snapshot / outcomes'),16)
            d.arrow([(center,806),(center,921)],both=True);d.label(center,871,t('分别持久确认','Persist separately'),16)
        d.text(24,1060,t('跨服保留存储归属，Base 承载可迁移。','Home storage; Base hosting can move.'),18,600)
        d.text(24,1093,t('共享 cell 不获取各服数据库地址或凭证。','cell receives no home database credentials.'),17,kind='sub')
        d.text(24,1143,t('源冻结 → 目标准备 → 条件提交','Freeze source → prepare → conditional commit'),17,kind='sub')
        d.text(24,1175,t('目标激活 → 源清理，禁止双端模拟。','Activate target → clean source; one authority.'),17,kind='sub')
        d.text(24,1222,t('临时活动关联 Instance，不改变资料归属。','Instance activities do not move profile ownership.'),16,kind='sub')
    return d.finish()

def callbacks(en=False, mobile=False):
    t=lambda zh,eng: eng if en else zh
    d=Diagram(480 if mobile else 1280,1510 if mobile else 920,
        t('Apollo 外部 HTTP 回调与开发工具边界','Apollo external HTTP callbacks and developer tooling'),
        t('目标设计，尚未实现。interfaces 可靠接收通知，按稳定玩家身份查询当前 Base，Base 经 database 幂等发货。脚手架与 CI 属于开发工具，不是在线业务进程。',
          'Proposed design, not implemented: interfaces durably receives callbacks and resolves current Base by player identity; Base fulfills idempotently through database. Scaffolding and CI are developer tooling, not online services.'),
        'en' if en else 'zh-CN')
    if not mobile:
        d.text(28,42,t('外部回调、动态 Base 路由与持久发货','External callbacks, dynamic Base routing and fulfillment'),26,600)
        d.text(28,76,t('目标方案：interfaces、订单事务与 init 脚手架均待实现。','Proposed: interfaces, order transactions and init scaffolding are pending.'),18,kind='sub')
        d.node(36,130,240,112,t('外部平台','Platform'),[t('支付 / 渠道通知','Payments / channels'),t('服务端回调','Server callbacks')])
        d.node(386,130,350,112,'interfaces',[t('HTTPS 验签 · 可靠受理 · 异步投递','HTTPS · verify · durable intake'),t('订单绑定稳定玩家身份','Orders bind stable player identity')])
        d.node(902,130,338,112,t('login：服务 / 归属目录','login: owner directory'),[t('玩家 → 当前 Base + 归属代次','Player → current Base + epoch'),t('供 gateway / interfaces 查询','Queried by gateway / interfaces')],size=21)
        d.arrow([(276,184),(386,184)],both=True);d.label(330,168,'HTTPS',16)
        d.arrow([(736,174),(902,174)],True,both=True);d.label(818,156,t('查当前归属','Resolve owner'),15,'teal')
        d.node(386,405,350,136,t('base：当前承载进程','base: current host'),[t('离线也可激活角色 Base 部分','Activate Base part while offline'),t('订单校验 · 幂等业务发货','Validate order · fulfill once'),t('迁移后拒绝旧写入者','Fence stale writers after migration')],size=22)
        d.node(902,405,338,136,'database',[t('通知 / 订单待办持久化','Durable notifications / pending work'),t('奖励 + 去重凭据原子提交','Atomic reward + dedup record'),t('新事务接口待实现','Transaction interfaces pending')])
        d.arrow([(560,242),(560,405)]);d.label(560,325,t('按玩家寻址 / 重试同一订单','Address by player / retry same order'),17)
        d.arrow([(736,214),(816,214),(816,441),(902,441)])
        d.label(813,305,t('先持久受理','Persist intake first'),16)
        d.arrow([(736,500),(902,500)],both=True);d.label(819,484,t('持久发货','Commit fulfillment'),15)
        d.text(36,325,t('回调确认 ≠ 发货完成','Callback ACK ≠ fulfilled'),19,600)
        d.text(36,360,t('可靠保存后及时确认通知。','ACK after durable intake.'),17,kind='sub')
        d.text(36,394,t('Base 不可达时保留待办。','Keep work if Base is unavailable.'),17,kind='sub')
        d.text(36,428,t('不永久绑定 Base ID / IP。','No permanent Base ID/IP binding.'),17,kind='sub')
        d.rect(28,593,1224,74,'#f4f9f8')
        d.text(48,623,t('world / cell 不参与支付发货；gateway 只在玩家在线时投递通知。','world / cell do not fulfill payments; gateway delivers optional online notifications.'),19,600)
        d.text(48,650,t('跨服不改变原存储归属；当前 Base 可随负载、恢复或升级迁移。','Cross-server play retains storage affiliation; current Base may move for load, recovery or updates.'),17,kind='sub')
        d.rect(28,706,1224,184,'#f8fafc')
        d.text(48,737,t('开发与交付工具 · 与在线请求链路分开','Developer and delivery tooling · separate from runtime requests'),19,600)
        d.node(48,758,500,106,t('项目初始化 CLI：init（待实现）','Project initialization CLI: init (pending)'),[t('生成配置 / Lua / 启停脚本 / CI 模板','Generate config / Lua / ops / CI template')],size=21)
        d.node(760,758,464,106,t('仓库 CI workflow','Repository CI workflow'),[t('构建 · 测试 · 契约 / 文档检查','Build · tests · contract / docs checks')],size=21)
        d.arrow([(548,810),(760,810)],True);d.label(654,794,t('生成项目可接入','Generated project can use'),15,'teal')
    else:
        d.text(24,38,t('外部 HTTP 回调','External HTTP callbacks'),25,600)
        d.text(24,72,t('interfaces / 订单事务：待实现','interfaces / transactions: pending'),18,kind='sub')
        d.node(30,109,420,96,t('外部支付 / 渠道平台','Payment / channel platform'),[t('HTTP 服务端通知','HTTP server notification')],size=21)
        d.node(30,262,420,114,'interfaces',[t('HTTPS 验签 · 可靠受理 · 异步投递','HTTPS · verify · durable intake'),t('按稳定玩家身份处理订单','Orders use stable player identity')])
        d.arrow([(240,205),(240,262)],both=True);d.label(288,242,'HTTPS',18)
        d.node(30,454,420,110,t('login：服务 / 归属目录','login: owner directory'),[t('玩家 → 当前 Base + 归属代次','Player → current Base + epoch')],size=22)
        d.arrow([(130,376),(130,454)],True,both=True);d.label(248,420,t('interfaces 查询当前归属','interfaces resolves owner'),17,'teal')
        d.node(30,680,420,133,t('base：当前承载进程','base: current hosting process'),[t('离线激活 · 校验订单 · 幂等发货','Offline activation · fulfill once'),t('迁移后旧写入者必须被隔离','Fence stale writers after transfer')],size=22)
        d.arrow([(30,320),(10,320),(10,746),(30,746)])
        d.text(42,620,t('interfaces 按玩家身份投递到当前 Base','interfaces delivers by player identity'),18,600)
        d.node(30,925,420,133,'database',[t('通知 / 待办可靠保存','Durably store intake / pending work'),t('奖励与去重凭据原子提交','Atomic reward + dedup record'),t('事务接口待实现','Transaction interfaces pending')])
        d.arrow([(330,376),(468,376),(468,960),(450,960)])
        d.arrow([(240,813),(240,925)],both=True);d.label(240,878,t('Base 提交业务结果','Base commits business result'),17)
        d.text(30,1110,t('确认收到 ≠ 发货完成；不可达则保留待办。','ACK ≠ fulfillment; retain pending work.'),18,600)
        d.text(30,1145,t('world / cell 不处理支付发货。','world / cell do not fulfill payments.'),18,kind='sub')
        d.rect(24,1200,432,280,'#f8fafc')
        d.text(42,1236,t('开发与交付工具','Developer / delivery tools'),21,600)
        d.text(42,1280,t('项目初始化 CLI：init（待实现）','Project CLI: init (pending)'),20,600)
        d.text(42,1318,t('生成配置、Lua 与启停 / CI 模板','Generate config, Lua, ops / CI'),18,kind='sub')
        d.text(42,1372,t('CI workflow 在仓库执行','CI workflow runs in repository'),20,600)
        d.text(42,1410,t('构建、测试、契约和文档检查','Build, tests, contracts and docs'),18,kind='sub')
        d.text(42,1450,t('不属于在线业务处理链路。','Outside the runtime request path.'),18,kind='sub')
    return d.finish()


def main():
    outputs={}
    # space / cross-server / callbacks render to docs/public/architecture/;
    # system is hand-authored but its template is checked into
    # scripts/docs/templates and is now part of the generation/check set.
    for name,render in [('system',system),('space',space),('cross-server',cross_server),('callbacks',callbacks)]:
        base = 'architecture' if name=='system' else name
        for lang,en in [('zh',False),('en',True)]:
            for mobile in [False,True]:
                dest = (IMG if name=='system' else OUT)
                outputs[dest/f'{base}-{lang}{"-mobile" if mobile else ""}.svg']=render(en,mobile)
    if '--check' in sys.argv:
        stale=[str(p.relative_to(ROOT)) for p,body in outputs.items() if not p.exists() or p.read_text()!=body]
        if stale:raise SystemExit('Architecture SVGs are stale:\n'+'\n'.join(stale))
        print(f'{len(outputs)} localized architecture SVGs match their source')
    else:
        OUT.mkdir(parents=True,exist_ok=True)
        for p,body in outputs.items():p.write_text(body)
        print(f'Generated {len(outputs)} localized architecture SVGs')
if __name__=='__main__':main()
