#!/usr/bin/env python3
import argparse
import os
import socket
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OOBE_DIR = ROOT / 'keshos_oobe'
SERVER = OOBE_DIR / 'server.py'
LOG_FILE = ROOT / 'logs' / 'keshos_oobe.log'


def port_in_use(port: int) -> bool:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        return sock.connect_ex(('127.0.0.1', port)) == 0


def launch_browser(url: str):
    try:
        if sys.platform.startswith('win'):
            os.startfile(url)
        elif sys.platform.startswith('linux'):
            subprocess.Popen(['xdg-open', url], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        elif sys.platform == 'darwin':
            subprocess.Popen(['open', url], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except Exception:
        pass


def main():
    parser = argparse.ArgumentParser(description='Run the KeshOS OOBE preview server.')
    parser.add_argument('--port', type=int, default=8080)
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--open-browser', action='store_true')
    args = parser.parse_args()

    LOG_FILE.parent.mkdir(parents=True, exist_ok=True)

    if port_in_use(args.port):
        print(f'Port {args.port} is already in use. Existing KeshOS OOBE server likely running.')
        if args.open_browser:
            launch_browser(f'http://{args.host}:{args.port}/')
        return 0

    print(f'Launching KeshOS OOBE preview on http://{args.host}:{args.port}/')
    print(f'Log file: {LOG_FILE}')

    with LOG_FILE.open('a', encoding='utf-8') as log_handle:
        proc = subprocess.Popen(
            [sys.executable, str(SERVER)],
            cwd=str(OOBE_DIR),
            stdout=log_handle,
            stderr=log_handle,
            text=True,
        )

        try:
            url = f'http://{args.host}:{args.port}/'
            for _ in range(30):
                time.sleep(0.4)
                if port_in_use(args.port):
                    if args.open_browser:
                        launch_browser(url)
                    print(f'Server ready: {url}')
                    return 0
            print('Server did not start in time. Check the log file for details.')
            return 1
        except KeyboardInterrupt:
            proc.terminate()
            return 0


if __name__ == '__main__':
    raise SystemExit(main())
