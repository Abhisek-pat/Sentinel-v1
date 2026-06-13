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
for service in sentinel sentinel-dashboard sentinel-llm; do
  echo "${service} service:"
  systemctl --no-pager --full status "${service}.service" || true
  echo
  echo "Recent ${service} logs:"
  journalctl -u "${service}.service" -n 40 --no-pager || true
  echo
done

echo "Dashboard health:"
curl --silent --show-error --max-time 5 http://127.0.0.1:8080/health || true
echo
echo "Latest KPI summary:"
curl --silent --show-error --max-time 5 \
  "http://127.0.0.1:8080/api/summary?window_minutes=10" || true
echo
echo "Latest evaluations:"
curl --silent --show-error --max-time 5 \
  "http://127.0.0.1:8080/api/evaluations?limit=10" || true
echo
echo "Reasoning health:"
curl --silent --show-error --max-time 5 http://127.0.0.1:8090/health || true
echo
echo "Reasoning providers:"
curl --silent --show-error --max-time 5 http://127.0.0.1:8090/providers || true
echo
