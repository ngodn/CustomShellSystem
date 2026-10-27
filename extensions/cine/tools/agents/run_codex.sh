#!/usr/bin/env bash
# Run a Codex lane (astra = gpt-6-astra, sol = gpt-6.1-sol) non-interactively on a brief.
# Usage: run_codex.sh <astra|sol> <brief.md> [extra prompt text]
#        run_codex.sh <astra|sol> --resume <session-id> "<follow-up message>"
# Logs: work/agents/<lane>-<stamp>/{events.jsonl,last-message.md,command.txt,exit.txt}
set -u
CODEX_BIN="${CODEX_BIN:-/home/eins0fx/.local/share/mise/installs/codex/0.162.1/bin/codex}"
PKG="$(cd "$(dirname "$0")/../.." && pwd)"
LANE="${1:?lane}"; shift
case "$LANE" in
  astra) MODEL="gpt-6-astra" ;;
  sol)   MODEL="gpt-6.1-sol" ;;
  *) echo "lane must be astra or sol" >&2; exit 2 ;;
esac
STAMP="$(date +%Y%m%d-%H%M%S)"
RUN="$PKG/work/agents/$LANE-$STAMP"
mkdir -p "$RUN"
COMMON=(--dangerously-bypass-approvals-and-sandbox --skip-git-repo-check -C "$PKG" -m "$MODEL" --json -o "$RUN/last-message.md"
        -c model_reasoning_effort=high -c approval_policy=never -c sandbox_mode=danger-full-access
        --add-dir /home/eins0fx/development/mods/msII)
if [ "${1:-}" = "--resume" ]; then
  SID="${2:?session id}"; shift 2
  MSG="${*:-continue}"
  echo "codex exec resume $SID (lane $LANE, model $MODEL)" > "$RUN/command.txt"
  printf '%s\n' "$MSG" > "$RUN/prompt.md"
  "$CODEX_BIN" exec "${COMMON[@]}" resume "$SID" "$MSG" </dev/null > "$RUN/events.jsonl" 2> "$RUN/stderr.log" &
  echo $! > "$RUN/pid.txt"; wait $!
else
  BRIEF="${1:?brief path}"; shift
  EXTRA="${*:-}"
  { echo "You are the $LANE lane for the CINE extension (CustomShellSystem/extensions/cine). Your brief is the file $BRIEF. Read it in full first and follow it. The CINE root is $PKG. Write your final report to the REPORT.md path the brief names and finish with a short summary of what you did, what you measured, what is open, and what you deleted."; [ -n "$EXTRA" ] && printf '\n%s\n' "$EXTRA"; } > "$RUN/prompt.md"
  echo "codex exec (lane $LANE, model $MODEL) brief=$BRIEF" > "$RUN/command.txt"
  "$CODEX_BIN" exec "${COMMON[@]}" - < "$RUN/prompt.md" > "$RUN/events.jsonl" 2> "$RUN/stderr.log" &
  echo $! > "$RUN/pid.txt"; wait $!
fi
echo $? > "$RUN/exit.txt"
# To stop a lane: kill $(cat <run>/pid.txt); never pkill/pgrep by name, other Codex sessions share the binary.
echo "$RUN"
