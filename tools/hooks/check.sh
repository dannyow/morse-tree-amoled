#!/usr/bin/env bash
# Repo rules check. Usage: check.sh [file...]  (no args = every tracked file)
# Used by the pre-commit hook (staged files) and by CI (all files).
set -u
fail=0
files=("$@")
if [ $# -eq 0 ]; then files=(); while IFS= read -r f; do files+=("$f"); done < <(git ls-files); fi
allowed_large=$(cat .large-files-allowed 2>/dev/null)
content_ext='bin|rom|tap|tzx|sna|z80|ch8|mp4|mov|wav|mp3'
# Private words (bench host names, home network, local paths...) live OUTSIDE every repo,
# so the list itself is never published. One fixed string per line, '#' comments.
private_list="${LAB_PRIVATE_WORDS:-$HOME/.config/lab-private-words}"
repo_list="$(git rev-parse --git-dir 2>/dev/null)/info/private-words"   # per project, never tracked
private_pat=$(cat "$private_list" "$repo_list" 2>/dev/null | grep -v '^#' | grep -v '^$')
for f in ${files[@]+"${files[@]}"}; do
  [ -f "$f" ] || continue
  case "$f" in
    .env|*/.env|*secrets.h) echo "BLOCKED  $f: secrets never enter the repo"; fail=1; continue;;
  esac
  # -I: binary files are skipped; compressed bytes match short words by chance.
  if [ -n "$private_pat" ] && hit=$(grep -I -n -i -F "$private_pat" "$f" 2>/dev/null | head -3) && [ -n "$hit" ]; then
    echo "BLOCKED  $f: private word (lists: $private_list, $repo_list):"; echo "$hit" | sed 's/^/           /'; fail=1
  fi
  size=$(wc -c < "$f" | tr -d ' ')
  if [ "$size" -gt 1048576 ] && ! grep -qxF "$f" <<< "$allowed_large"; then
    echo "BLOCKED  $f: $((size/1024)) KB > 1 MB and not in .large-files-allowed"; fail=1
  fi
  if [[ "$f" =~ \.($content_ext)$ ]]; then
    d=$(dirname "$f")
    if ! ls "$d"/LICENSE* "$d"/*.LICENSE* >/dev/null 2>&1; then
      echo "BLOCKED  $f: content file without a LICENSE next to it"; fail=1
    fi
    if ! grep -qF "$f" THIRD_PARTY.md 2>/dev/null && ! grep -qF "$(basename "$f")" THIRD_PARTY.md 2>/dev/null; then
      echo "BLOCKED  $f: not listed in THIRD_PARTY.md"; fail=1
    fi
  fi
done
[ -z "$private_pat" ] && echo "note: no private word list at $private_list, that check was skipped"
[ $fail -eq 0 ] && echo "repo rules: ok (${#files[@]} files)"
exit $fail
