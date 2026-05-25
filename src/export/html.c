#include "export/html.h"
#include "export/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *HTML_TEMPLATE =
"<!DOCTYPE html>\n"
"<html><head><meta charset=\"utf-8\">\n"
"<title>cgraph - Code Knowledge Graph</title>\n"
"<style>\n"
"body { font-family: -apple-system, sans-serif; margin: 0; padding: 20px; background: #1a1a2e; color: #eee; }\n"
"h1 { color: #64ffda; }\n"
".stats { display: flex; gap: 20px; margin: 20px 0; }\n"
".stat { background: #16213e; padding: 15px 25px; border-radius: 8px; }\n"
".stat-value { font-size: 2em; font-weight: bold; color: #64ffda; }\n"
".stat-label { color: #888; font-size: 0.9em; }\n"
"#search { width: 300px; padding: 8px 12px; border-radius: 4px; border: 1px solid #444; background: #0f3460; color: #eee; margin: 20px 0; }\n"
"#graph { width: 100%%; height: 500px; border: 1px solid #333; border-radius: 8px; background: #0f3460; }\n"
"table { width: 100%%; border-collapse: collapse; margin: 20px 0; }\n"
"th, td { text-align: left; padding: 8px 12px; border-bottom: 1px solid #333; }\n"
"th { color: #64ffda; }\n"
"tr:hover { background: #16213e; }\n"
".node { cursor: pointer; }\n"
".link { stroke: #444; stroke-opacity: 0.6; }\n"
"</style>\n"
"</head><body>\n"
"<h1>cgraph</h1>\n"
"<div class=\"stats\" id=\"stats\"></div>\n"
"<input type=\"text\" id=\"search\" placeholder=\"Search functions...\">\n"
"<svg id=\"graph\"></svg>\n"
"<h2>Functions</h2>\n"
"<table id=\"functions\"><thead><tr><th>Name</th><th>File</th><th>Lines</th><th>Fan-in</th><th>Fan-out</th></tr></thead><tbody></tbody></table>\n"
"<script src=\"https://d3js.org/d3.v7.min.js\"></script>\n"
"<script>\n"
"fetch('data.json').then(r=>r.json()).then(data => {\n"
"  // Stats\n"
"  const stats = document.getElementById('stats');\n"
"  stats.innerHTML = `\n"
"    <div class='stat'><div class='stat-value'>${data.nodes.length}</div><div class='stat-label'>Nodes</div></div>\n"
"    <div class='stat'><div class='stat-value'>${data.edges.length}</div><div class='stat-label'>Edges</div></div>\n"
"    <div class='stat'><div class='stat-value'>${data.nodes.filter(n=>n.type==='function').length}</div><div class='stat-label'>Functions</div></div>\n"
"  `;\n"
"  // Table\n"
"  const tbody = document.querySelector('#functions tbody');\n"
"  const funcs = data.nodes.filter(n=>n.type==='function' && !n.is_external);\n"
"  funcs.forEach(f => {\n"
"    const tr = document.createElement('tr');\n"
"    tr.innerHTML = `<td>${f.name}</td><td>${f.file||''}</td><td>${f.line_end-f.line_start}</td><td>${f.fan_in}</td><td>${f.fan_out}</td>`;\n"
"    tbody.appendChild(tr);\n"
"  });\n"
"  // Search\n"
"  document.getElementById('search').addEventListener('input', e => {\n"
"    const q = e.target.value.toLowerCase();\n"
"    tbody.querySelectorAll('tr').forEach(tr => {\n"
"      tr.style.display = tr.textContent.toLowerCase().includes(q) ? '' : 'none';\n"
"    });\n"
"  });\n"
"  // Force graph\n"
"  const svg = d3.select('#graph');\n"
"  const width = svg.node().getBoundingClientRect().width;\n"
"  const height = 500;\n"
"  svg.attr('viewBox', [0, 0, width, height]);\n"
"  const callEdges = data.edges.filter(e=>e.type==='calls'||e.type==='calls_fp');\n"
"  const nodeSet = new Set();\n"
"  callEdges.forEach(e=>{nodeSet.add(e.from);nodeSet.add(e.to);});\n"
"  const graphNodes = data.nodes.filter(n=>nodeSet.has(n.id)).map(n=>({...n}));\n"
"  const nodeMap = new Map(graphNodes.map(n=>[n.id, n]));\n"
"  const links = callEdges.filter(e=>nodeMap.has(e.from)&&nodeMap.has(e.to)).map(e=>({source:e.from,target:e.to}));\n"
"  const sim = d3.forceSimulation(graphNodes)\n"
"    .force('link', d3.forceLink(links).id(d=>d.id).distance(80))\n"
"    .force('charge', d3.forceManyBody().strength(-200))\n"
"    .force('center', d3.forceCenter(width/2, height/2));\n"
"  const link = svg.append('g').selectAll('line').data(links).join('line').attr('class','link');\n"
"  const node = svg.append('g').selectAll('circle').data(graphNodes).join('circle')\n"
"    .attr('r',6).attr('fill','#64ffda').attr('class','node')\n"
"    .call(d3.drag().on('start',(e,d)=>{if(!e.active)sim.alphaTarget(0.3).restart();d.fx=d.x;d.fy=d.y;})\n"
"      .on('drag',(e,d)=>{d.fx=e.x;d.fy=e.y;}).on('end',(e,d)=>{if(!e.active)sim.alphaTarget(0);d.fx=null;d.fy=null;}));\n"
"  const label = svg.append('g').selectAll('text').data(graphNodes).join('text')\n"
"    .text(d=>d.name).attr('font-size',10).attr('fill','#aaa').attr('dx',8).attr('dy',3);\n"
"  sim.on('tick',()=>{\n"
"    link.attr('x1',d=>d.source.x).attr('y1',d=>d.source.y).attr('x2',d=>d.target.x).attr('y2',d=>d.target.y);\n"
"    node.attr('cx',d=>d.x).attr('cy',d=>d.y);\n"
"    label.attr('x',d=>d.x).attr('y',d=>d.y);\n"
"  });\n"
"});\n"
"</script></body></html>\n";

static const char *edge_type_str(EdgeType t) {
    switch (t) {
        case EDGE_CALLS: return "calls";
        case EDGE_CALLS_FP: return "calls_fp";
        case EDGE_INCLUDES: return "includes";
        case EDGE_DEFINED_IN: return "defined_in";
        case EDGE_REFERENCES_TYPE: return "references_type";
        case EDGE_ACCESSES_FIELD: return "accesses_field";
        case EDGE_READS_GLOBAL: return "reads_global";
        case EDGE_WRITES_GLOBAL: return "writes_global";
        case EDGE_ALLOCATES: return "allocates";
        case EDGE_FREES: return "frees";
        case EDGE_GUARDED_BY: return "guarded_by";
        case EDGE_ACQUIRES_LOCK: return "acquires_lock";
        case EDGE_CREATES_THREAD: return "creates_thread";
        case EDGE_IFDEF_DEPENDS: return "ifdef_depends";
        case EDGE_BELONGS_TO: return "belongs_to";
    }
    return "unknown";
}

static const char *node_type_str_html(NodeType t) {
    switch (t) {
        case NODE_FILE: return "file";
        case NODE_FUNCTION: return "function";
        case NODE_STRUCT: return "struct";
        case NODE_UNION: return "union";
        case NODE_ENUM: return "enum";
        case NODE_FIELD: return "field";
        case NODE_GLOBAL_VAR: return "global_var";
        case NODE_MACRO: return "macro";
        case NODE_TYPEDEF: return "typedef";
        case NODE_SYNC_PRIMITIVE: return "sync_primitive";
        case NODE_MODULE: return "module";
    }
    return "unknown";
}

static bool write_data_json(const Graph *g, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "{\"nodes\":[\n");
    for (uint32_t i = 0; i < g->node_count; i++) {
        const Node *n = &g->nodes[i];
        if (i > 0) fprintf(f, ",\n");
        fprintf(f, "  {\"id\":%u,\"name\":\"%s\",\"type\":\"%s\",\"file\":\"%s\","
                   "\"line_start\":%u,\"line_end\":%u,\"fan_in\":%u,\"fan_out\":%u,"
                   "\"is_external\":%s}",
                n->id, n->name, node_type_str_html(n->type), n->file ? n->file : "",
                n->line_start, n->line_end, n->fan_in, n->fan_out,
                n->is_external ? "true" : "false");
    }
    fprintf(f, "\n],\"edges\":[\n");
    for (uint32_t i = 0; i < g->edge_count; i++) {
        const Edge *e = &g->edges[i];
        if (i > 0) fprintf(f, ",\n");
        fprintf(f, "  {\"from\":%u,\"to\":%u,\"type\":\"%s\"}", e->from, e->to, edge_type_str(e->type));
    }
    fprintf(f, "\n]}\n");
    fclose(f);
    return true;
}

bool html_export(const Graph *g, const char *output_dir) {
    mkdir(output_dir, 0755);

    char path[2048];
    snprintf(path, sizeof(path), "%s/index.html", output_dir);
    FILE *f = fopen(path, "w");
    if (!f) return false;
    fputs(HTML_TEMPLATE, f);
    fclose(f);

    snprintf(path, sizeof(path), "%s/data.json", output_dir);
    return write_data_json(g, path);
}
