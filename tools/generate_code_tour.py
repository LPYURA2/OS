#!/usr/bin/env python3
"""Build standalone, dependency-free HTML viewer from real repository text files."""
import json
import html
from pathlib import Path
from datetime import date

ROOT=Path(__file__).resolve().parents[1]
EXTS={'.asm','.py','.c','.cpp','.h','.cs','.java','.md','.json','.sh','.dot','.txt','.csproj','.hex','.m8asm'}
SKIP_DIRS={'build','__pycache__','.git','_doc_render'}
LABELS={'matryoshka.py': ('M32 CPU + хост-устройства', 'Процессор / RAM / диски / ассемблер', 'Python эмулирует железо; здесь же внешний ассемблер. Смотреть VirtualMachine.step, _guard, BlockDisk, assemble.'), 'firmware.asm': ('Загрузочная ROM', 'Гостевой ассемблер', 'Прошивка исполняется CPU M32, читает SYSTEM.IMG, проверяет целостность ядра.'), 'kernel.asm': ('Ядро M32', 'Гостевой ассемблер', 'Планировщик и системные вызовы — реальный гостевой код, а не Python-макет.'), 'programs/shell.asm': ('Командная оболочка', 'Гостевое приложение', 'Разбирает команды, вызывает ядро, поддерживает run и заметки.'), 'hardware_m8/m8.py': ('M8 — отдельный CPU', 'Эксперимент Proteus', '8-битная архитектура, НЕ двоично совместима с M32.'), 'hardware_m8/gate_alu.py': ('ALU на логике', 'Эксперимент Proteus', 'Логические операции, эталонная проверка результата.'), 'engine_lab/src/mw_engine.c': ('Графическое ядро C', 'Движок хоста', 'Программный framebuffer и API; пока не выполняется в гостевой ОС.'), 'engine_lab/include/mw_engine.h': ('C ABI', 'Движок хоста', 'Контракт функций, доступных другим языкам.'), 'engine_lab/src/mw_scene.cpp': ('Сцена на C++', 'Движок хоста', 'Поверх C API реализует слой сцены.'), 'engine_lab/csharp/MatryoshkaDemo.cs': ('C# / P/Invoke', 'Движок хоста', 'Код клиента подготовлен, но сборка с .NET SDK не подтверждена.'), 'engine_lab/python/demo.py': ('Python / ctypes', 'Движок хоста', 'Подключение к общей библиотеке из Python.'), 'engine_lab/java/MatryoshkaEngine.java': ('Java / JNI', 'Движок хоста', 'Мост из Java к нативной библиотеке.'), 'programs/hello.asm': ('Hello World M32', 'Гостевое приложение', 'Программа запускается с диска через гостевой загрузчик.'), 'programs/counter.asm': ('Счётчик M32', 'Гостевое приложение', 'Фоновый процесс иллюстрирует планирование и проблему общей консоли.'), 'images/manifest.json': ('Состав образов', 'Бинарные данные', 'Контрольные сведения о бинарных образах.')}
PRIORITY={'matryoshka.py': 1, 'kernel.asm': 2, 'firmware.asm': 3, 'programs/shell.asm': 4, 'hardware_m8/m8.py': 5, 'hardware_m8/gate_alu.py': 6, 'engine_lab/src/mw_engine.c': 7, 'engine_lab/src/mw_scene.cpp': 8, 'engine_lab/include/mw_engine.h': 9, 'engine_lab/csharp/MatryoshkaDemo.cs': 10}

sources=[]
for f in ROOT.rglob('*'):
    if not f.is_file() or f.name=='CODE_TOUR.html' or any(part in SKIP_DIRS for part in f.relative_to(ROOT).parts):
        continue
    if f.suffix not in EXTS and f.name not in {'.gitignore','AGENTS.md'}:
        continue
    if f.stat().st_size > 400_000:
        continue
    path=f.relative_to(ROOT).as_posix()
    text=f.read_text(encoding='utf-8',errors='replace')
    if '\0' in text:
        continue
    short,tag,notes=LABELS.get(path,(f.name,'Дополнительные файлы','Исходник или документация — см. файл'))
    sources.append({'path':path,'title':short,'group':tag,'notes':notes,'text':text,'lines':len(text.splitlines())})
sources.sort(key=lambda x:(PRIORITY.get(x['path'],99),x['group'],x['path']))

json_blob=json.dumps(sources,ensure_ascii=False,separators=(',',':')).replace('<','\\u003c').replace('>','\\u003e').replace('&','\\u0026')
TEMPLATE=r"""<!DOCTYPE html>
<html lang="ru"><head><meta charset="UTF-8"/><meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>MATRYOSHKA | Исходный код компьютера и ОС</title>
<style>
:root{color-scheme:dark;--b:#0b101b;--p:#131b2b;--line:#2a3547;--txt:#e9edf6;--mut:#91a2bc;--a:#69e7bf;--vio:#a9b4ff}
*{box-sizing:border-box}body{margin:0;background:var(--b);color:var(--txt);font-family:Inter,system-ui,-apple-system,"Segoe UI",sans-serif}header{padding:24px clamp(16px,4vw,52px);border-bottom:1px solid var(--line);background:linear-gradient(105deg,#111c2d,#132a29 75%,#10182a)}
h1{margin:4px 0 8px;font-weight:800;font-size:clamp(25px,4vw,42px);letter-spacing:-1.5px}h1 span{color:var(--a)}p{line-height:1.5}header p{color:var(--mut);margin:5px 0 14px}.eyebrow{font-size:12px;color:var(--a);font-weight:800;letter-spacing:2px}.metrics{display:flex;flex-wrap:wrap;gap:7px}.metric{border:1px solid #426052;background:#112b2b;border-radius:8px;padding:8px 12px;color:#b5ffdf;font-size:12px}.warning{border-left:3px solid #e5b85d;background:#28251e;padding:10px 14px;margin:15px 0;color:#f1dbae;border-radius:4px;font-size:13px}
.main{display:grid;grid-template-columns:290px minmax(0,1fr);min-height:calc(100vh - 190px)}aside{border-right:1px solid var(--line);background:#101723;padding:16px;overflow:auto;max-height:calc(100vh - 0px)}.search{background:#192335;color:white;border:1px solid #34435a;border-radius:8px;width:100%;padding:11px;font-size:14px;outline:none}.search:focus{border-color:var(--a)}.group{color:#94a6c3;font-size:11px;margin:18px 8px 7px;letter-spacing:1.4px;font-weight:800;text-transform:uppercase}.file{display:flex;align-items:center;gap:6px;cursor:pointer;border:1px solid transparent;border-radius:8px;padding:9px;color:#dbe7f7;background:none;width:100%;text-align:left;font-family:inherit;font-size:12px;overflow:hidden}.file:hover{background:#1d2b40}.file.active{border-color:#336558;background:#1a3738;color:#adffe4}.file small{display:block;color:#92a1b8;font-size:10px;line-height:1.4}.file .fp{word-break:break-all;font-family:ui-monospace,Consolas,monospace}.file .dot{width:7px;height:7px;background:var(--a);border-radius:100%;flex-shrink:0}.panel{padding:clamp(14px,3vw,30px);min-width:0}.arch{display:flex;gap:8px;align-items:center;overflow:auto;padding:14px 0 22px}.arch span{background:#1a283a;padding:10px 13px;border:1px solid #3a4b62;border-radius:8px;white-space:nowrap;color:#c8d5e8;font-size:12px}.arch b{color:var(--a);font-size:14px}.bar{display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:10px;margin-bottom:12px}.bar strong{font-size:19px}.bar .subtitle{display:block;font-size:12px;color:var(--mut);font-weight:400;margin-top:6px}.pill{display:inline-block;font-size:11px;border:1px solid #3b5568;border-radius:100px;padding:5px 9px;color:#bbdfed}.tools{display:flex;align-items:center;gap:8px;flex-wrap:wrap}.tools input{width:110px;background:#192335;color:#e6f6ff;border:1px solid #384864;padding:8px 9px;border-radius:8px}.tools button{color:#dcf9ef;background:#254438;border:1px solid #3a7366;border-radius:8px;padding:8px 10px;cursor:pointer}.tools button:hover{background:#336458}.desc{color:#acc0d9;font-size:13px;margin:12px 0 18px}.codewrap{border-radius:11px;border:1px solid #324057;background:#090e16;overflow:auto;max-height:67vh}.code{min-width:max-content;padding:13px 0;font:12px/1.7 ui-monospace,"SFMono-Regular",Consolas,"Liberation Mono",monospace;tab-size:4}.ln{display:grid;grid-template-columns:65px auto;gap:0}.ln:hover{background:#1a2b36}.ln.match{background:#294d42}.ln .num{text-align:right;color:#71839c;padding:0 16px 0 7px;user-select:none;border-right:1px solid #263347}.ln .txt{white-space:pre;padding:0 18px}.footer{margin-top:12px;color:#7d90a9;font-size:12px}.hide{display:none!important}
@media(max-width:790px){.main{display:block}aside{max-height:270px;border-right:0;border-bottom:1px solid var(--line)}.panel{padding:15px}.arch{padding:5px 0 13px}.codewrap{max-height:61vh}.file{font-size:12px}header{padding:20px 15px}h1{letter-spacing:-1px}}
</style></head><body>
<header><div class="eyebrow">OPEN SOURCE TOUR / ACTUAL PROJECT FILES</div><h1>МАТРЁШКА<span> // ОС</span></h1><p>Процессор, память, диски, прошивка, ассемблер, ядро, приложения и лаборатория C / C++ / C# / Python / Java — в настоящем исходном коде.</p>
<div class="metrics"><span class="metric">M32: 33 инструкции / 256 КиБ RAM</span><span class="metric">M8: независимый 8-битный CPU</span><span class="metric">Офлайн • исходники встроены</span><span class="metric" id="stat">Загрузка...</span></div>
<div class="warning"><b>Что здесь реально:</b> M32 исполняет гостевые ASM ядро и приложения. M8 пока отдельный несовместимый эксперимент. Многоязычный движок работает на хосте, <b>не внутри гостевой ОС</b>. C# — неподтверждённый в сборке клиент.</div></header>
<div class="main"><aside><input class="search" id="search" placeholder="Поиск файлов и содержимого..." aria-label="Поиск файлов и исходного кода"/><div id="nav"></div></aside>
<main class="panel"><div class="arch" aria-label="Схема выполнения"><span>Termux / Python</span><b>›</b><span>CPU M32 / RAM / I/O</span><b>›</b><span>ROM firmware</span><b>›</b><span>Гостевое ядро</span><b>›</b><span>Shell + приложения</span></div>
<div class="bar"><div><strong id="file-title">Загрузка исходников</strong><span class="subtitle" id="file-path"></span></div><div class="tools"><span class="pill" id="line-count">0 строк</span><input id="goto" type="number" min="1" placeholder="№ строки" aria-label="Номер строки"/><button id="go">Перейти</button></div></div>
<div class="desc" id="file-desc"></div><div class="codewrap" id="scroll"><div class="code" id="code"></div></div><div class="footer">Код подлинный: скопирован из файлов пакета в момент генерации страницы. После изменения исходников — запусти python tools/generate_code_tour.py. Для проверки запуска см. SOURCE_MAP_RU.md.</div></main></div>
<script type="application/json" id="source-data">__SOURCE_DATA__</script>
<script>
(function(){'use strict';
const files=JSON.parse(document.getElementById('source-data').textContent);
const nav=document.getElementById('nav'), box=document.getElementById('code');let current=0,query='';
const total=files.reduce((s,f)=>s+f.lines,0);
document.getElementById('stat').textContent=files.length+' файлов / '+total.toLocaleString('ru-RU')+' строк';
function node(tag,cls,text){const n=document.createElement(tag);if(cls)n.className=cls;if(text!==undefined)n.textContent=text;return n;}
function renderNav(){nav.replaceChildren();let last='';let shown=0;
 files.forEach((f,i)=>{if(query && !(f.title+' '+f.path+' '+f.group+' '+f.notes+' '+f.text).toLowerCase().includes(query))return;
 if(f.group!==last){last=f.group;nav.append(node('div','group',last));}
 const b=node('button','file'+(i===current?' active':''));const dot=node('span','dot');const content=node('span');content.append(node('span','fp',f.path));content.append(node('small','',f.title));b.append(dot,content);b.onclick=()=>show(i);nav.append(b);shown++;
 });if(!shown)nav.append(node('p','desc','Нет совпадений.'));
}
function show(i){current=i;const f=files[i];document.getElementById('file-title').textContent=f.title;document.getElementById('file-path').textContent=f.path+' • '+f.group;
document.getElementById('file-desc').textContent=f.notes;document.getElementById('line-count').textContent=f.lines+' строк';document.getElementById('goto').value='';
box.replaceChildren();const fragment=document.createDocumentFragment();const lines=f.text.split('\n');if(lines[lines.length-1]==='')lines.pop();
let first=-1;lines.forEach((s,idx)=>{const l=node('div','ln'+(query && s.toLowerCase().includes(query)?' match':''));l.id='line-'+(idx+1);const n=node('span','num',String(idx+1));const t=node('span','txt',s||' ');l.append(n,t);fragment.append(l);if(first<0 && query && s.toLowerCase().includes(query))first=idx+1;});box.append(fragment);
document.getElementById('scroll').scrollTop=0;renderNav();if(first>0 && query)jump(first);
}
function jump(n){const l=document.getElementById('line-'+n);if(l){const w=document.getElementById('scroll');w.scrollTop=l.offsetTop-box.offsetTop-70;}}
document.getElementById('go').onclick=()=>jump(Number(document.getElementById('goto').value));document.getElementById('goto').addEventListener('keydown',e=>{if(e.key==='Enter')jump(Number(e.target.value));});
document.getElementById('search').addEventListener('input',e=>{query=e.target.value.trim().toLowerCase();renderNav();});
show(0);
})();
</script></body></html>"""
page=TEMPLATE.replace('__SOURCE_DATA__',json_blob)
(ROOT/'CODE_TOUR.html').write_text(page,encoding='utf-8')
print('Generated',ROOT/'CODE_TOUR.html','files',len(sources),'lines',sum(x['lines'] for x in sources),'bytes',len(page.encode()))
