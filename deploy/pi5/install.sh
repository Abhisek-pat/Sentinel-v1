#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  echo "Run this installer with sudo: sudo bash deploy/pi5/install.sh"
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ONNXRUNTIME_VERSION="${ONNXRUNTIME_VERSION:-1.24.4}"
ONNXRUNTIME_ARCHIVE="onnxruntime-linux-aarch64-${ONNXRUNTIME_VERSION}.tgz"
ONNXRUNTIME_URL="https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/${ONNXRUNTIME_ARCHIVE}"

if [[ "$(uname -m)" != "aarch64" ]]; then
  echo "This installer requires 64-bit Raspberry Pi OS (aarch64)."
  exit 1
fi

apt-get update
apt-get install -y build-essential ca-certificates cmake curl libopencv-dev python3-venv

if [[ ! -f /opt/onnxruntime/include/onnxruntime_cxx_api.h ]]; then
  temp_dir="$(mktemp -d)"
  trap 'rm -rf "${temp_dir}"' EXIT
  curl --fail --location "${ONNXRUNTIME_URL}" --output "${temp_dir}/${ONNXRUNTIME_ARCHIVE}"
  tar -xzf "${temp_dir}/${ONNXRUNTIME_ARCHIVE}" -C "${temp_dir}"
  rm -rf /opt/onnxruntime
  mv "${temp_dir}/onnxruntime-linux-aarch64-${ONNXRUNTIME_VERSION}" /opt/onnxruntime
fi

cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build-pi5" \
  -DCMAKE_BUILD_TYPE=Release \
  -DONNXRUNTIME_DIR=/opt/onnxruntime \
  -DCMAKE_INSTALL_PREFIX=/opt/sentinel
cmake --build "${ROOT_DIR}/build-pi5" --parallel 4
cmake --install "${ROOT_DIR}/build-pi5"

if ! id sentinel >/dev/null 2>&1; then
  useradd --system --home /var/lib/sentinel --shell /usr/sbin/nologin sentinel
fi

install -d -o sentinel -g sentinel /var/lib/sentinel/clips
install -d -o sentinel -g sentinel /var/lib/sentinel/kpi
install -d -o sentinel -g sentinel /var/lib/sentinel/dashboard
install -d -m 0750 /etc/sentinel
if [[ ! -f /etc/sentinel/sentinel.env ]]; then
  install -m 0640 -o root -g sentinel "${ROOT_DIR}/deploy/pi5/sentinel.env" /etc/sentinel/sentinel.env
fi
if [[ ! -f /etc/sentinel/llm.env ]]; then
  install -m 0640 -o root -g sentinel "${ROOT_DIR}/deploy/pi5/llm.env" /etc/sentinel/llm.env
fi
if [[ ! -f /etc/sentinel/dashboard.env ]]; then
  install -m 0640 -o root -g sentinel "${ROOT_DIR}/deploy/pi5/dashboard.env" /etc/sentinel/dashboard.env
fi
install -m 0644 "${ROOT_DIR}/deploy/pi5/sentinel.service" /etc/systemd/system/sentinel.service
install -m 0644 "${ROOT_DIR}/deploy/pi5/sentinel-dashboard.service" /etc/systemd/system/sentinel-dashboard.service
install -m 0644 "${ROOT_DIR}/deploy/pi5/sentinel-llm.service" /etc/systemd/system/sentinel-llm.service

python3 -m venv /opt/sentinel/dashboard-venv
/opt/sentinel/dashboard-venv/bin/pip install --upgrade pip
/opt/sentinel/dashboard-venv/bin/pip install \
  -r /opt/sentinel/share/sentinel/dashboard_service/requirements.txt

python3 -m venv /opt/sentinel/llm-venv
/opt/sentinel/llm-venv/bin/pip install --upgrade pip
/opt/sentinel/llm-venv/bin/pip install \
  -r /opt/sentinel/share/sentinel/llm_service/requirements.txt
/opt/sentinel/llm-venv/bin/python -m unittest discover \
  -s "${ROOT_DIR}/tests" -p test_llm_service.py -v
/opt/sentinel/dashboard-venv/bin/python -m unittest discover \
  -s "${ROOT_DIR}/tests" -p test_dashboard_service.py -v
python3 -m unittest discover -s "${ROOT_DIR}/tests" -p test_soak_test.py -v
python3 -m unittest discover -s "${ROOT_DIR}/tests" -p test_analyze_soak.py -v
python3 -m unittest discover -s "${ROOT_DIR}/tests" -p test_analyze_evaluations.py -v
python3 -m unittest discover -s "${ROOT_DIR}/tests" -p test_configure_llm_profiles.py -v
python3 -m unittest discover -s "${ROOT_DIR}/tests" -p test_final_report.py -v

systemctl daemon-reload
systemctl enable sentinel.service
systemctl enable sentinel-dashboard.service
systemctl enable sentinel-llm.service

echo
echo "Sentinel is installed but not started."
echo "Edit /etc/sentinel/sentinel.env, then run: sudo systemctl start sentinel"
echo "Start the KPI API with: sudo systemctl start sentinel-dashboard"
echo "Start the mock reasoning API with: sudo systemctl start sentinel-llm"
