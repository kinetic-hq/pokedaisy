#!/usr/bin/env bash
# apt-get install for CI that survives a flaky package mirror. Each attempt is
# capped at 4 minutes (a stalled mirror otherwise hangs until the job's own
# timeout); after the first failure the runner switches from its default mirror
# (azure.archive.ubuntu.com on GitHub's images) to archive.ubuntu.com.
# Usage: apt-install.sh [apt-get install args...] PACKAGES...
set -uo pipefail
apt() { sudo timeout 240 apt-get -o Acquire::Retries=3 -o Acquire::http::Timeout=20 -o Acquire::https::Timeout=20 "$@"; }
try() { apt update && apt install -y "$@"; }
try "$@" && exit 0
echo "apt failed on the default mirror; switching to archive.ubuntu.com" >&2
if [ -f /etc/apt/apt-mirrors.txt ]; then
  printf 'http://archive.ubuntu.com/ubuntu/\tpriority:1\n' | sudo tee /etc/apt/apt-mirrors.txt >/dev/null
fi
sudo find /etc/apt \( -name '*.list' -o -name '*.sources' \) -exec sed -i 's#http://azure\.archive\.ubuntu\.com#http://archive.ubuntu.com#g' {} +
for attempt in 1 2; do
  try "$@" && exit 0
  echo "apt attempt $attempt on archive.ubuntu.com failed; retrying in 15 s" >&2
  sleep 15
done
exit 1
