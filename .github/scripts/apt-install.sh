#!/usr/bin/env bash
# apt-get install for CI that survives a flaky package mirror: retries each
# download, and if the runner's default mirror (azure.archive.ubuntu.com on
# GitHub's images) can't be reached, switches to archive.ubuntu.com and tries
# again. Usage: apt-install.sh [apt-get install args...] PACKAGES...
set -uo pipefail
apt() { sudo apt-get -o Acquire::Retries=5 -o Acquire::http::Timeout=30 "$@"; }
try() { apt update && apt install -y "$@"; }
for attempt in 1 2; do
  try "$@" && exit 0
  echo "apt attempt $attempt failed; retrying in 15 s" >&2
  sleep 15
done
echo "Switching to archive.ubuntu.com" >&2
if [ -f /etc/apt/apt-mirrors.txt ]; then
  printf 'http://archive.ubuntu.com/ubuntu/\tpriority:1\n' | sudo tee /etc/apt/apt-mirrors.txt >/dev/null
fi
sudo find /etc/apt -name '*.list' -o -name '*.sources' | xargs -r sudo sed -i 's#http://azure\.archive\.ubuntu\.com#http://archive.ubuntu.com#g'
try "$@"
