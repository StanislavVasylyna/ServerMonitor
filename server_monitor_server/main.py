import json
import os
import shutil
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


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


def get_disk_usage(path="/"):
    usage = shutil.disk_usage(path)
    return {
        "path": path,
        "total_bytes": usage.total,
        "used_bytes": usage.used,
        "free_bytes": usage.free,
        "usage_percent": round(usage.used / usage.total * 100, 2),
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

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
