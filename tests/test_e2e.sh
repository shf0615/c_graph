#!/bin/bash
set -e
CGRAPH="$(dirname "$0")/../build/src/cgraph"
FIXTURE="$(dirname "$0")/fixtures/sample_project"
DB="/tmp/cgraph_e2e_test.db"

echo "=== Build ==="
$CGRAPH build --compile-commands "$FIXTURE/compile_commands.json" --output "$DB"

echo "=== Info ==="
$CGRAPH info --db "$DB" | python3 -m json.tool

echo "=== List functions ==="
$CGRAPH list functions --db "$DB" | python3 -m json.tool

echo "=== Show main ==="
$CGRAPH show main --db "$DB" | python3 -m json.tool

echo "=== Callers of add ==="
$CGRAPH callers add --db "$DB" | python3 -m json.tool

echo "=== Callees of main ==="
$CGRAPH callees main --db "$DB" --depth 2 | python3 -m json.tool

echo "=== Path main -> helper ==="
$CGRAPH path main helper --db "$DB" | python3 -m json.tool

echo "=== Impact of helper ==="
$CGRAPH impact helper --db "$DB" | python3 -m json.tool

echo "=== Dead code ==="
$CGRAPH dead-code --db "$DB" | python3 -m json.tool

echo "=== Threads ==="
$CGRAPH threads --db "$DB" | python3 -m json.tool

echo "=== Export HTML ==="
$CGRAPH export-html --db "$DB" --output /tmp/cgraph_e2e_html
test -f /tmp/cgraph_e2e_html/index.html
test -f /tmp/cgraph_e2e_html/data.json

echo ""
echo "=== ALL PASSED ==="
rm -f "$DB"
rm -rf /tmp/cgraph_e2e_html
