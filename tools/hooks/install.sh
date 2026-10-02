#!/usr/bin/env bash
cd "$(git rev-parse --show-toplevel)" && git config core.hooksPath tools/hooks && chmod +x tools/hooks/* && echo "hooks installed"
