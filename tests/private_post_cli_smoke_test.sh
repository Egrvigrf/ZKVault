#!/usr/bin/env bash
set -euo pipefail

binary_path="$1"
workspace="$(mktemp -d)"
cleanup() {
    rm -rf "$workspace"
}
trap cleanup EXIT

document_path="$workspace/document.json"
bundle_path="$workspace/bundle.json"
preview_path="$workspace/preview.json"

cat >"$document_path" <<'JSON'
{
  "metadata": {
    "slug": "notes/private-post",
    "title": "Private Post",
    "excerpt": "Short preview",
    "published_at": "2026-04-23T00:00:00Z"
  },
  "markdown": "# Private\nSecret content"
}
JSON

"$binary_path" validate-document "$document_path"
printf 'open-sesame\nopen-sesame\n' | "$binary_path" encrypt-post "$document_path" "$bundle_path"
"$binary_path" validate-bundle "$bundle_path"
printf 'open-sesame\n' | "$binary_path" decrypt-post-preview "$bundle_path" >"$preview_path"

grep -q '"title": "Private Post"' "$preview_path"
grep -q '"markdown": "# Private\\nSecret content"' "$preview_path"
