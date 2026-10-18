#!/usr/bin/env python3
"""Generate the localized architecture SVGs used by VitePress and GitHub README.

Keep role names and relationships aligned with docs/design/bigworld-apollo-mapping.md.
Run from any directory: python3 scripts/docs/render_architecture.py [--check]
"""
from pathlib import Path
from html import escape
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'docs/public/architecture'
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
    t=lambda zh,eng:eng if en else zh
    d=Diagram(480 if mobile else 1280,1440 if mobile else 840,t('Apollo 目标进程与通信','Apollo target processes and communication'),t('实线表示业务请求与结果，虚线表示注册和管理。Base 管长期资料，cell 管模拟；worldServer 管理分配，machined 只监督本机。','Solid lines carry business requests/results; dashed lines carry registration and management. Base owns profiles, cell owns simulation, World manages placement, machined supervises locally.'),'en' if en else 'zh-CN')
    if not mobile:
        d.text(28,40,t('目标进程与通信','Target processes and communication'),27,600)
        d.arrow([(820,31),(866,31)]);d.text(878,37,t('业务请求 / 结果','Business / results'),16,kind='sub')
        d.arrow([(1060,31),(1106,31)],True);d.text(1118,37,t('注册 / 管理','Control'),16,kind='sub')
        d.text(28,75,t('已有独立原型：Base ↔ database；World/cell 拆分与整体接线待完成。','Base ↔ database has standalone prototypes; World/cell separation and full wiring remain pending.'),17,kind='sub')
        d.rect(28,106,1224,178,'#f4f9f8')
        d.text(48,134,t('认证、目录与空间管理','Authentication, directories and placement'),17,600,'teal')
        d.node(300,154,270,104,'loginServer',[t('认证 · 服务 / 在线目录','Auth · directories')])
        d.node(760,154,320,104,'worldServer',[t('Space/Cell 分配 · 拓扑 · 排空','Space/Cell placement · topology')],t('管理 / 执行待拆分','Manager / executor split pending'))
        d.arrow([(760,207),(570,207)],True);d.label(665,191,t('注册 / 位置投影','Register / project'),15,'teal')
        d.rect(28,330,1224,356,'#f8fafc')
        d.text(48,358,t('连接、业务与模拟','Connections, gameplay and simulation'),17,600)
        d.node(48,386,160,96,t('客户端','Client'),[t('游戏连接','Game client')],size=22)
        d.node(278,386,220,96,'gateway',[t('socket · 会话 · 转发','Sockets · sessions')])
        d.node(615,386,250,96,'baseServer',[t('PlayerAnchor · 长期资料','PlayerAnchor · profiles')],t('独立原型','Standalone prototype'),'blue')
        d.node(966,386,250,96,'database',[t('加载 / 保存 · journal','Load / save · journal')],t('独立原型','Standalone prototype'),'blue')
        d.node(580,558,330,112,t('cell 执行进程','cell executor'),[t('Avatar/Entity · AOI · 战斗','Avatar/Entity · AOI · combat')],t('独立执行入口待实现','Dedicated executor pending'))
        d.cylinder(1001,558,180,87,t('实际存储','Storage'),t('文件 / 数据库','Files / database'))
        d.arrow([(208,434),(278,434)],both=True);d.label(243,419,'TCP',15)
        d.arrow([(498,434),(615,434)],both=True);d.label(556,419,t('基础业务','Base gameplay'),15)
        d.arrow([(865,434),(966,434)],both=True);d.label(915,419,t('加载 / 保存','Load / save'),15)
        d.arrow([(388,482),(388,614),(580,614)],both=True);d.label(477,598,t('场景输入 / 同步','Input / state sync'),16)
        d.arrow([(745,482),(745,558)],both=True);d.label(849,526,t('快照 / 业务结果','Snapshots / outcomes'),16)
        d.arrow([(1091,482),(1091,558)],both=True);d.label(1142,527,t('存储 I/O','Storage I/O'),15)
        d.arrow([(378,386),(378,258)],True,both=True);d.label(411,313,t('目录查询','Directory'),15,'teal')
        d.arrow([(650,386),(650,303),(520,303),(520,258)],True);d.label(584,295,t('Base 注册','Base registry'),15,'teal')
        d.arrow([(48,434),(16,434),(16,207),(300,207)],True,both=True);d.label(190,192,t('认证 / 凭证','Auth / credentials'),15,'teal')
        d.arrow([(1080,207),(1232,207),(1232,712),(745,712),(745,670)],True,both=True)
        d.label(1010,704,t('cell 注册 / 分配 / 排空','cell registration / placement / drain'),16,'teal')
        d.node(48,748,1168,64,'machined',[],size=21)
        d.text(210,786,t('按机器可选部署，只拉起 / 监督本机进程；不裁决玩家归属或空间分配。','Optional per host: launch / supervise local processes; no player ownership or spatial placement decisions.'),17,kind='sub')
    else:
        d.text(24,37,t('目标进程与通信','Target process topology'),24,600)
        d.text(24,67,t('Base ↔ database：独立原型已具备','Base ↔ database: standalone prototypes'),16,kind='sub')
        d.text(24,92,t('World/cell 拆分与整体接线待完成','World/cell split and full wiring pending'),16,kind='sub')
        d.text(30,133,t('认证、目录与空间管理','Authentication and placement'),18,600,'teal')
        d.node(30,155,420,94,'loginServer',[t('认证 · 服务目录 · 玩家位置投影','Auth · services · player location projections')])
        d.node(30,291,420,102,'worldServer',[t('管理 cell · Space/Cell 分配 · 拓扑','cell registry · Space/Cell placement')],t('管理 / 执行待拆分','Manager / executor split pending'))
        d.arrow([(240,291),(240,249)],True);d.label(240,276,t('服务注册 / 位置投影','Service registry / location projection'),16,'teal')
        d.text(30,421,t('cell 向 World 注册，由 World 分配空间。','cell registers with World for placement.'),17,kind='sub')
        d.text(30,445,t('客户端向 loginServer 认证。','Client authenticates with loginServer.'),17,kind='sub')
        d.arrow([(30,504),(7,504),(7,180),(30,180)],True,both=True)
        d.node(30,464,420,80,t('客户端','Client'),[t('游戏连接','Game client')])
        d.node(30,594,420,96,'gateway',[t('持有 socket · 缓存 Base / cell 路由','Own sockets · cache Base / cell routes')])
        d.arrow([(240,544),(240,594)],both=True);d.label(285,575,'TCP',17)
        d.arrow([(30,641),(16,641),(16,202),(30,202)],True,both=True)
        d.node(30,806,190,112,'baseServer',['PlayerAnchor',t('长期资料','Persistent profile')],tone='blue',size=21)
        d.node(260,806,190,112,t('cell 进程','cell executor'),['Avatar / Entity','AOI · '+t('战斗','combat')],size=21)
        d.arrow([(240,690),(240,738),(125,738),(125,806)])
        d.arrow([(240,738),(355,738),(355,806)])
        d.label(125,780,t('基础业务','Base gameplay'),17)
        d.label(355,780,t('输入 / 同步','Input / sync'),17)
        d.arrow([(220,862),(260,862)],both=True)
        d.text(260,954,t('Base → cell：快照','Base → cell: snapshot'),16,kind='sub')
        d.text(260,985,t('cell → Base：业务结果','cell → Base: outcome'),16,kind='sub')
        d.arrow([(450,339),(464,339),(464,862),(450,862)],True,both=True)
        d.node(30,1022,190,104,'database',[t('加载 / 保存','Load / save'),'journal'],tone='blue',size=21)
        d.arrow([(125,918),(125,1022)],both=True);d.label(125,992,t('加载 / 保存','Load / save'),17)
        d.cylinder(30,1192,190,87,t('实际存储','Storage'),t('文件 / 数据库','Files / database'))
        d.arrow([(125,1126),(125,1192)],both=True);d.label(186,1167,t('存储 I/O','Storage I/O'),16)
        d.text(260,1050,t('实线：业务请求与结果','Solid: business / results'),16,kind='sub')
        d.text(260,1081,t('虚线：注册与管理','Dashed: registration'),16,kind='sub')
        d.text(260,1112,t('cell 独立入口待实现','cell executor pending'),15,kind='sub')
        d.node(30,1320,420,92,'machined',[t('按机器可选，只监督本机进程','Optional per host; local supervision only')])
    return d.finish()

def space(en=False,mobile=False):
    t=lambda zh,eng:eng if en else zh
    d=Diagram(480 if mobile else 1280,1135 if mobile else 700,t('Space、Cell、执行进程与 Instance','Space, Cell, executor and Instance'),t('默认每 Space 一个 Cell，一个执行进程承载多个空间的 Cell；未来同一 Space 可分配多个 Cell 到不同执行进程。Instance 只关联玩法空间。','Initially each Space has one Cell; one executor hosts Cells from multiple Spaces. Future Cells of one Space may run in different executors. Instance associates with a gameplay Space.'),'en' if en else 'zh-CN')
    if not mobile:
        d.text(28,40,t('运行空间、分区与玩法的边界','Runtime space, partition and gameplay boundaries'),27,600)
        d.node(467,65,346,83,'worldServer',[t('管理注册、Space/Cell 拓扑与分配','Registry · topology · placement')])
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
        d.node(24,73,432,88,'worldServer',[t('注册 · Space/Cell 拓扑 · 分配','Registry · Space/Cell topology · placement')])
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
        d.text(28,40,t('跨服只改变模拟位置，原 Base 与存储归属保持','Cross-server play changes simulation, not Base or storage ownership'),25,600)
        d.text(28,74,t('跨服设计预留 · 尚未启用默认编队；临时活动可关联 Instance。','Reserved design · not enabled in the default fleet; temporary activities may associate with Instance.'),17,kind='sub')
        d.node(460,113,360,90,'worldServer',[t('跨服 Space/Cell 分配与迁移管理','Cross-server placement / transfer')])
        for x,letter in [(28,'A'),(892,'B')]:
            d.rect(x,265,360,352,'#f8fafc')
            d.text(x+24,297,t('原服 ','Home server ')+letter,21,600)
            d.node(x+24,322,312,124,'baseServer '+letter,['PlayerAnchor '+letter,t('长期资料所有者，归属不变','Profile ownership stays here')],tone='blue')
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
        d.node(24,119,432,91,'worldServer',[t('管理跨服 Space/Cell 与迁移','Manage cross-server Space/Cell and transfer')])
        d.node(24,326,432,179,t('共享 cell 执行进程','Shared cell executor'),['Space C · Cell C1','Avatar A  /  Avatar B',t('玩法快照与运行状态','Gameplay snapshots and runtime state')])
        d.arrow([(240,210),(240,326)],True,both=True);d.label(299,274,t('注册 / 分配','Register / place'),17,'teal')
        for x,letter in [(24,'A'),(252,'B')]:
            center=x+102
            d.text(x,631,t('原服 ','Home server ')+letter,20,600)
            d.node(x,659,204,147,'baseServer '+letter,['PlayerAnchor '+letter,t('长期资料不迁走','Profiles stay here')],tone='blue',size=20)
            d.node(x,921,204,91,'database '+letter,[t('原存储归属','Original storage')],tone='blue',size=20)
            d.arrow([(center,505),(center,659)],both=True);d.label(center,575,t('快照 / 业务结果','Snapshot / outcomes'),16)
            d.arrow([(center,806),(center,921)],both=True);d.label(center,871,t('分别持久确认','Persist separately'),16)
        d.text(24,1060,t('跨服只改变模拟位置，保留原 Base。','Only simulation moves; retain original Base.'),18,600)
        d.text(24,1093,t('共享 cell 不获取各服数据库地址或凭证。','cell receives no home database credentials.'),17,kind='sub')
        d.text(24,1143,t('源冻结 → 目标准备 → 条件提交','Freeze source → prepare → conditional commit'),17,kind='sub')
        d.text(24,1175,t('目标激活 → 源清理，禁止双端模拟。','Activate target → clean source; one authority.'),17,kind='sub')
        d.text(24,1222,t('临时活动关联 Instance，不改变资料归属。','Instance activities do not move profile ownership.'),16,kind='sub')
    return d.finish()

def main():
    outputs={}
    for name,render in [('system',system),('space',space),('cross-server',cross_server)]:
        for lang,en in [('zh',False),('en',True)]:
            for mobile in [False,True]:
                outputs[OUT/f'{name}-{lang}{"-mobile" if mobile else ""}.svg']=render(en,mobile)
    if '--check' in sys.argv:
        stale=[str(p.relative_to(ROOT)) for p,body in outputs.items() if not p.exists() or p.read_text()!=body]
        if stale:raise SystemExit('Architecture SVGs are stale:\n'+'\n'.join(stale))
        print(f'{len(outputs)} localized architecture SVGs match their source')
    else:
        OUT.mkdir(parents=True,exist_ok=True)
        for p,body in outputs.items():p.write_text(body)
        print(f'Generated {len(outputs)} localized architecture SVGs')
if __name__=='__main__':main()
