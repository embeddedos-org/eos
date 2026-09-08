#!/usr/bin/env bash
# Fail unless every job handed to us succeeded.
#
# Reads the `needs` context as JSON on stdin:
#   {"build": {"result": "success"}, "test": {"result": "skipped"}}
#
# Any result other than "success" fails, `skipped` and `cancelled` included: a
# job that did not run did not verify anything, and a required check that
# passes on "did not run" is the fail-open shape this repository has spent #75,
# #82 and #84 removing from the product code.
#
# This lives in a file rather than inline in the workflow so that the rule can
# be executed by a test with real inputs, instead of a test grepping the YAML
# for the string it expects to find there.
set -euo pipefail

# Three different checks share this rule, so the message has to name the one
# that actually failed. It said "CI Gate" unconditionally, which put the wrong
# check name in the log of the other two.
gate=${GATE_NAME:-Gate}

results=$(cat)

if [ -z "$results" ] || [ "$results" = "null" ]; then
    echo "::error::${gate} received no job results; refusing to pass." >&2
    exit 1
fi

printf '%s\n' "$results"

bad=$(printf '%s' "$results" | jq -r '
    to_entries[]
    | select(.value.result != "success")
    | "  \(.key): \(.value.result)"')

if [ -n "$bad" ]; then
    echo "::error::${gate} failed. These jobs did not succeed:"
    printf '%s\n' "$bad"
    exit 1
fi

echo "All jobs succeeded."
