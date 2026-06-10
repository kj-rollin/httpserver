import os
import time
import subprocess
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

SOURCE_FILE = "main.cpp"
BINARY = "./mainserver"

BUILD_CMD = [
    "clang++",
    "-std=c++17",
    SOURCE_FILE,
    "-lsqlite3",
    "-o",
    "mainserver"
]


server_process = None


def build():
    print("\n[BUILD] Compiling...")

    result = subprocess.run(BUILD_CMD)

    if result.returncode == 0:
        print("[BUILD] Success")
        return True

    print("[BUILD] Failed")
    return False


def run_server():
    global server_process

    if server_process and server_process.poll() is None:
        print("[SERVER] Already running")
        return

    if not os.path.exists(BINARY):
        print("[SERVER] Binary not found. Run build first.")
        return

    print("[SERVER] Starting...")

    server_process = subprocess.Popen(
        [BINARY],
        stdout=None,
        stderr=None
    )

    print(f"[SERVER] PID: {server_process.pid}")


def stop_server():
    global server_process

    if not server_process:
        print("[SERVER] Not running")
        return

    if server_process.poll() is None:
        print("[SERVER] Stopping...")

        server_process.terminate()

        try:
            server_process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            print("[SERVER] Force killing...")
            server_process.kill()

    server_process = None

    print("[SERVER] Stopped")


def restart_server():
    stop_server()

    if build():
        run_server()


class SourceWatcher(FileSystemEventHandler):

    def on_modified(self, event):

        if event.src_path.endswith((".cpp", ".hpp", ".h")):

            print(f"\n[WATCH] File changed: {event.src_path}")

            restart_server()


def watch():
    print("[WATCH] Watching source files...")
    restart_server()

    observer = Observer()
    observer.schedule(SourceWatcher(), ".", recursive=True)
    observer.start()

    try:
        while True:
            time.sleep(1)

    except KeyboardInterrupt:
        pass

    finally:
        observer.stop()
        observer.join()
        stop_server()


def show_help():
    print("""
Available commands:

build     -> Compile main.cpp
run       -> Start mainserver
stop      -> Stop mainserver
restart   -> Rebuild and restart
watch     -> Auto rebuild on source changes
status    -> Show server status
help      -> Show commands
quit      -> Exit
""")


def status():
    global server_process

    if server_process and server_process.poll() is None:
        print(f"[SERVER] Running (PID {server_process.pid})")
    else:
        print("[SERVER] Stopped")


def shell():
    print("HTTP Server Dev Tool")
    print("Type 'help' for commands")

    while True:

        try:
            command = input("\ndev> ")

        except (KeyboardInterrupt, EOFError):
            print()
            command = "quit"

        command = command.strip().lower()

        if command == "build":
            build()

        elif command == "run":
            run_server()

        elif command == "stop":
            stop_server()

        elif command == "restart":
            restart_server()

        elif command == "watch":
            watch()

        elif command == "status":
            status()

        elif command == "help":
            show_help()

        elif command in ("quit", "exit"):
            stop_server()
            print("Goodbye")
            break

        else:
            print(f"Invalid command: '{command}'")
            print("Type 'help' to see available commands")


if __name__ == "__main__":
    shell()
