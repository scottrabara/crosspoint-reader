#!/usr/bin/env python3
"""Build a self-contained index.html from a render-harness output directory.

Usage: contact_sheet.py OUT_DIR [--title TEXT]

Reads OUT_DIR/report.json and writes OUT_DIR/index.html, whose thumbnails
reference OUT_DIR/pages/*.png by relative path. Standard library only.
"""

import argparse
import html
import json
import os
import statistics
import sys


def percentile(values, p):
    if not values:
        return 0.0
    ordered = sorted(values)
    rank = p * (len(ordered) - 1)
    lo = int(rank)
    hi = min(lo + 1, len(ordered) - 1)
    return ordered[lo] + (ordered[hi] - ordered[lo]) * (rank - lo)


def fmt(ms):
    return f"{ms:.2f}"


def page_total(page):
    return page["loadMs"] + page["prewarmMs"] + page["bwRenderMs"] + page["grayRenderMs"]


CSS = """
:root { --bg:#f6f6f4; --fg:#1d1d1b; --muted:#6b6b66; --card:#ffffff; --line:#dcdcd6; --accent:#2f5d8a; }
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) { --bg:#161615; --fg:#ececea; --muted:#9a9a94; --card:#20201e; --line:#383834; --accent:#8db7e0; }
}
:root[data-theme="dark"] { --bg:#161615; --fg:#ececea; --muted:#9a9a94; --card:#20201e; --line:#383834; --accent:#8db7e0; }
* { box-sizing: border-box; }
body { margin:0; padding:24px 16px 48px; background:var(--bg); color:var(--fg);
       font:14px/1.45 system-ui, -apple-system, "Segoe UI", sans-serif; }
main { max-width:1200px; margin:0 auto; }
h1 { font-size:22px; margin:0 0 4px; }
h2 { font-size:16px; margin:32px 0 10px; }
.sub { color:var(--muted); margin:0 0 16px; }
.chips { display:flex; flex-wrap:wrap; gap:6px; margin:0 0 16px; }
.chip { background:var(--card); border:1px solid var(--line); border-radius:999px; padding:3px 10px; font-size:12px; }
.chip b { font-weight:600; }
.stats { display:grid; grid-template-columns:repeat(auto-fit, minmax(150px, 1fr)); gap:10px; }
.stat { background:var(--card); border:1px solid var(--line); border-radius:8px; padding:10px 12px; }
.stat .v { font-size:20px; font-weight:600; font-variant-numeric:tabular-nums; }
.stat .k { color:var(--muted); font-size:12px; }
.tablewrap { overflow-x:auto; }
table { border-collapse:collapse; width:100%; background:var(--card); border:1px solid var(--line); border-radius:8px; }
th, td { padding:6px 10px; border-bottom:1px solid var(--line); text-align:right; white-space:nowrap;
         font-variant-numeric:tabular-nums; }
th { font-weight:600; color:var(--muted); font-size:12px; }
th:nth-child(2), td:nth-child(2) { text-align:left; white-space:normal; }
tr:last-child td { border-bottom:none; }
.chapter { margin-top:24px; }
.chapter h3 { font-size:14px; margin:0 0 8px; }
.grid { display:grid; grid-template-columns:repeat(auto-fill, minmax(140px, 1fr)); gap:12px; }
.grid.landscape { grid-template-columns:repeat(auto-fill, minmax(220px, 1fr)); }
figure { margin:0; background:var(--card); border:1px solid var(--line); border-radius:6px; padding:6px; }
figure img { display:block; width:100%; height:auto; background:#fff; border:1px solid var(--line); image-rendering:auto; }
figcaption { font-size:11px; color:var(--muted); margin-top:4px; display:flex; justify-content:space-between; gap:6px; }
a { color:var(--accent); }
.note { color:var(--muted); font-size:12px; margin-top:8px; }
"""


def build(out_dir, title_override):
    report_path = os.path.join(out_dir, "report.json")
    with open(report_path, encoding="utf-8") as f:
        report = json.load(f)

    settings = report["settings"]
    layout = report["layout"]
    totals = report["totals"]
    pages = report["pages"]
    chapters = report["chapters"]
    title = title_override or report.get("title") or os.path.basename(report["epub"])
    esc = html.escape

    page_totals = [page_total(p) for p in pages]
    missing = [p["file"] for p in pages if not os.path.exists(os.path.join(out_dir, p["file"]))]

    parts = [
        "<!doctype html><html lang='en'><head><meta charset='utf-8'>",
        "<meta name='viewport' content='width=device-width, initial-scale=1'>",
        f"<title>Render Harness Results</title><style>{CSS}</style></head><body><main>",
        f"<h1>{esc(title)}</h1>",
        f"<p class='sub'>{esc(os.path.basename(report['epub']))} &middot; harness {esc(report['harnessGitSha'])}"
        f" &middot; {layout['screenWidth']}&times;{layout['screenHeight']} logical, viewport "
        f"{layout['viewportWidth']}&times;{layout['viewportHeight']} at ({layout['marginLeft']}, {layout['marginTop']})</p>",
        "<div class='chips'>",
    ]
    for key, value in settings.items():
        parts.append(f"<span class='chip'>{esc(key)} <b>{esc(str(value))}</b></span>")
    parts.append("</div>")

    stats = [
        ("chapters", str(totals["chapters"])),
        ("pages laid out", str(totals["pages"])),
        ("pages rendered", str(totals["pagesRendered"])),
        ("total build ms", fmt(totals["buildMs"])),
        ("page median ms", fmt(percentile(page_totals, 0.5))),
        ("page p95 ms", fmt(percentile(page_totals, 0.95))),
        ("epub load ms", fmt(report["epubLoadMs"]) + (" (cached)" if report["epubCacheHit"] else "")),
    ]
    parts.append("<div class='stats'>")
    for k, v in stats:
        parts.append(f"<div class='stat'><div class='v'>{esc(v)}</div><div class='k'>{esc(k)}</div></div>")
    parts.append("</div>")
    parts.append(
        "<p class='note'>Host timings are relative only: they show where time goes and how changes compare, "
        "not how long the ESP32-C3 takes. Page total = load + prewarm + BW render + gray render.</p>"
    )

    parts.append("<h2>Timing by chapter</h2><div class='tablewrap'><table><thead><tr>")
    for head in ("spine", "title", "build ms", "cached", "pages", "rendered", "load med", "prewarm med",
                 "BW med", "gray med", "total med", "total p95"):
        parts.append(f"<th>{head}</th>")
    parts.append("</tr></thead><tbody>")
    for c in chapters:
        cp = [p for p in pages if p["chapter"] == c["spineIndex"]]

        def med(key):
            return fmt(statistics.median([p[key] for p in cp])) if cp else "-"

        totals_c = [page_total(p) for p in cp]
        parts.append(
            "<tr>"
            f"<td>{c['spineIndex']}</td><td>{esc(c['title'] or '')}{'' if c['ok'] else ' (FAILED)'}</td>"
            f"<td>{fmt(c['buildMs'])}</td><td>{'yes' if c['cacheHit'] else 'no'}</td>"
            f"<td>{c['pageCount']}</td><td>{c['pagesRendered']}</td>"
            f"<td>{med('loadMs')}</td><td>{med('prewarmMs')}</td><td>{med('bwRenderMs')}</td>"
            f"<td>{med('grayRenderMs')}</td>"
            f"<td>{fmt(percentile(totals_c, 0.5)) if cp else '-'}</td>"
            f"<td>{fmt(percentile(totals_c, 0.95)) if cp else '-'}</td>"
            "</tr>"
        )
    parts.append("</tbody></table></div>")

    landscape = layout["screenWidth"] > layout["screenHeight"]
    parts.append("<h2>Pages</h2>")
    for c in chapters:
        cp = [p for p in pages if p["chapter"] == c["spineIndex"]]
        if not cp:
            continue
        parts.append(f"<section class='chapter'><h3>Spine {c['spineIndex']}: {esc(c['title'] or '')}</h3>")
        parts.append(f"<div class='grid{' landscape' if landscape else ''}'>")
        for p in cp:
            src = esc(p["file"])
            parts.append(
                f"<figure><a href='{src}'><img loading='lazy' src='{src}' "
                f"width='{layout['screenWidth']}' height='{layout['screenHeight']}' "
                f"alt='spine {p['chapter']} page {p['page']}'></a>"
                f"<figcaption><span>p{p['page']}</span><span>{fmt(page_total(p))} ms</span></figcaption></figure>"
            )
        parts.append("</div></section>")

    parts.append("</main></body></html>\n")
    index_path = os.path.join(out_dir, "index.html")
    with open(index_path, "w", encoding="utf-8") as f:
        f.write("".join(parts))
    return index_path, missing


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("out_dir")
    parser.add_argument("--title")
    args = parser.parse_args()
    index_path, missing = build(args.out_dir, args.title)
    if missing:
        print(f"contact_sheet: {len(missing)} referenced page(s) missing, e.g. {missing[0]}", file=sys.stderr)
        return 1
    print(f"wrote {index_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
