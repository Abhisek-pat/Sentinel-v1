#!/usr/bin/env bash
set -euo pipefail

echo "Architecture: $(uname -m)"
echo "OS: $(. /etc/os-release && echo "${PRETTY_NAME}")"
echo "Memory:"
free -h
echo
echo "Thermal/throttling state:"
vcgencmd measure_temp 2>/dev/null || true
vcgencmd get_throttled 2>/dev/null || true
echo
echo "Sentinel service:"
systemctl --no-pager --full status sentinel.service || true
echo
echo "Recent Sentinel logs:"
journalctl -u sentinel.service -n 40 --no-pager || true
