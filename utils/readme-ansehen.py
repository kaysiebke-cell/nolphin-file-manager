#!/usr/bin/env python3
"""Zeigt eine Markdown-Datei im Browser, ähnlich wie GitHub sie darstellt.

Aufruf:  utils/readme-ansehen.py [datei.md]     (Standard: README.md)
Alles läuft lokal, es wird nichts hochgeladen. Die HTML-Datei landet in
~/.cache/nolphin-readme/ und gehört nicht ins Repository.
"""
import html, os, re, subprocess, sys
import markdown

hier = os.path.dirname(os.path.abspath(__file__))
quelle = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(hier, "..", "README.md"))
text = open(quelle, encoding="utf-8").read()

body = markdown.markdown(text, extensions=["tables", "fenced_code", "sane_lists"])
# GitHub-Aufgabenlisten: [ ] und [x] als Kästchen
body = re.sub(r"<li>\[ \]", '<li class="task"><input type="checkbox" disabled>', body)
body = re.sub(r"<li>\[[xX]\]", '<li class="task"><input type="checkbox" checked disabled>', body)

css = """
:root{color-scheme:light dark;--bg:#fff;--fg:#1f2328;--mut:#59636e;--bd:#d1d9e0;--code:#f6f8fa;--lnk:#0969da;--quote:#d1d9e0}
@media(prefers-color-scheme:dark){:root{--bg:#0d1117;--fg:#f0f6fc;--mut:#9198a1;--bd:#3d444d;--code:#151b23;--lnk:#4493f8;--quote:#3d444d}}
body{background:var(--bg);color:var(--fg);font:16px/1.5 -apple-system,"Segoe UI",Helvetica,Arial,sans-serif;margin:0}
main{max-width:980px;margin:0 auto;padding:32px 45px}
@media(max-width:700px){main{padding:16px}}
h1,h2{border-bottom:1px solid var(--bd);padding-bottom:.3em}
h1{font-size:2em}h2{font-size:1.5em}h3{font-size:1.25em}
h1,h2,h3,h4{margin:24px 0 16px;font-weight:600;line-height:1.25}
a{color:var(--lnk);text-decoration:none}a:hover{text-decoration:underline}
code{background:var(--code);padding:.2em .4em;border-radius:6px;font:85% ui-monospace,SFMono-Regular,Menlo,monospace}
pre{background:var(--code);padding:16px;border-radius:6px;overflow:auto;line-height:1.45}
pre code{padding:0;background:none;font-size:85%}
blockquote{margin:0 0 16px;padding:0 1em;color:var(--mut);border-left:.25em solid var(--quote)}
table{border-collapse:collapse;display:block;overflow:auto;margin-bottom:16px}
th,td{border:1px solid var(--bd);padding:6px 13px}
tr:nth-child(2n){background:var(--code)}th{font-weight:600}
hr{border:0;border-top:1px solid var(--bd);margin:24px 0}
li.task{list-style:none;margin-left:-1.4em}li.task input{margin-right:.5em}
img{max-width:100%}
"""
seite = f"""<!doctype html><html lang="de"><head><meta charset="utf-8"><base href="{html.escape("file://" + os.path.dirname(quelle) + "/")}">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>{html.escape(os.path.basename(quelle))}</title><style>{css}</style></head>
<body><main>{body}</main></body></html>"""

ziel_dir = os.path.expanduser("~/.cache/nolphin-readme")
os.makedirs(ziel_dir, exist_ok=True)
ziel = os.path.join(ziel_dir, os.path.splitext(os.path.basename(quelle))[0] + ".html")
open(ziel, "w", encoding="utf-8").write(seite)
print(ziel)
subprocess.Popen(["xdg-open", ziel], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
