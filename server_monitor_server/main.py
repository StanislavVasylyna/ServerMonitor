import json
import os
import shutil
import signal
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import psutil


def read_cpu_times():
    with open("/proc/stat", encoding="utf-8") as stat_file:
        values = [int(value) for value in stat_file.readline().split()[1:]]

    idle = values[3] + (values[4] if len(values) > 4 else 0)
    return idle, sum(values)


def get_cpu_usage(sample_seconds=0.1):
    idle_before, total_before = read_cpu_times()
    time.sleep(sample_seconds)
    idle_after, total_after = read_cpu_times()

    total_delta = total_after - total_before
    idle_delta = idle_after - idle_before
    if total_delta == 0:
        return 0.0

    return round((1 - idle_delta / total_delta) * 100, 2)


def get_memory_usage():
    values = {}
    with open("/proc/meminfo", encoding="utf-8") as meminfo:
        for line in meminfo:
            key, value = line.split(":", 1)
            values[key] = int(value.strip().split()[0]) * 1024

    total = values["MemTotal"]
    available = values["MemAvailable"]
    used = total - available
    return {
        "total_bytes": total,
        "used_bytes": used,
        "available_bytes": available,
        "usage_percent": round(used / total * 100, 2),
    }


def get_disk_usage(alert_threshold=90):
    ignored_fstypes = {
        "tmpfs",
        "devtmpfs",
        "squashfs",
        "overlay",
        "aufs",
        "proc",
        "sysfs",
        "cgroup",
        "cgroup2",
        "debugfs",
        "tracefs",
        "securityfs",
        "fusectl",
        "configfs",
    }

    total_bytes = 0
    used_bytes = 0
    free_bytes = 0

    partitions = []
    alerts = []

    seen_mounts = set()

    for part in psutil.disk_partitions(all=False):
        if part.mountpoint in seen_mounts:
            continue

        seen_mounts.add(part.mountpoint)

        if not part.device.startswith("/dev/"):
            continue

        if part.fstype in ignored_fstypes:
            continue

        try:
            usage = shutil.disk_usage(part.mountpoint)
        except (PermissionError, FileNotFoundError, OSError):
            continue

        usable_bytes = usage.used + usage.free
        usage_percent = usage.used / usable_bytes * 100 if usable_bytes else 0.0

        item = {
            "device": part.device,
            "mountpoint": part.mountpoint,
            "fstype": part.fstype,
            "total_bytes": usage.total,
            "used_bytes": usage.used,
            "free_bytes": usage.free,
            "usage_percent": round(usage_percent, 2),
        }

        partitions.append(item)

        total_bytes += usage.total
        used_bytes += usage.used
        free_bytes += usage.free

        if usage_percent >= alert_threshold:
            alerts.append({
                "type": "disk_space_low",
                "level": "warning" if usage_percent < 95 else "critical",
                "device": part.device,
                "mountpoint": part.mountpoint,
                "usage_percent": round(usage_percent, 2),
                "free_bytes": usage.free,
            })

    usable_total = used_bytes + free_bytes
    total_usage_percent = used_bytes / usable_total * 100 if usable_total else 0.0

    return {
        "total_bytes": total_bytes,
        "used_bytes": used_bytes,
        "free_bytes": free_bytes,
        "usage_percent": round(total_usage_percent, 2),
        "partitions": partitions,
        "alerts": alerts,
    }


def get_server_stats():
    return {
        "cpu": {
            "usage_percent": get_cpu_usage(),
            "logical_cores": os.cpu_count(),
        },
        "memory": get_memory_usage(),
        "disk": get_disk_usage(),
        "timestamp": int(time.time()),
    }


class StatsHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.rstrip("/") not in ("", "/stats"):
            self.send_json({"error": "Not found"}, status=404)
            return

        try:
            self.send_json(get_server_stats())
        except (OSError, KeyError, ValueError) as error:
            self.send_json(
                {"error": "Could not collect server statistics", "details": str(error)},
                status=500,
            )

    def send_json(self, payload, status=200):
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, message_format, *args):
        print(f"{self.address_string()} - {message_format % args}")



def main():
    host = os.getenv("HOST", "0.0.0.0")
    port = int(os.getenv("PORT", "8000"))
    server = ThreadingHTTPServer((host, port), StatsHandler)
    print(f"Server statistics API is available at http://{host}:{port}/stats")

    def shutdown_handler(signum, frame):
        print("Stopping server...")
        server.shutdown()
        server.server_close()
        sys.exit(0)

    signal.signal(signal.SIGINT, shutdown_handler)  # Ctrl+C
    signal.signal(signal.SIGTERM, shutdown_handler)  # docker stop

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        shutdown_handler(None, None)
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
