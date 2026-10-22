"""Portal smoke test: enter AP setup over serial, exercise the web API, reboot the board.

Run with the PlatformIO venv python (it has pyserial):

    C:/Users/Mrk/.platformio/penv/Scripts/python.exe tools/portal_check.py [ip] [com]

Default ip = 192.168.1.147 (the board's LAN address - the portal also listens there because
it brings the saved network up in AP_STA mode), default com = COM5.
Keep the serial port open for the whole run: closing it drops DTR and resets the board.
"""

import sys
import time
import urllib.error
import urllib.request

import serial

BOARD = "http://" + (sys.argv[1] if len(sys.argv) > 1 else "192.168.1.147")
PORT = sys.argv[2] if len(sys.argv) > 2 else "COM5"


class Link:
    def __init__(self, port):
        self.port = port
        self.ser = serial.Serial(port, 115200, timeout=0.3)

    def pump(self, seconds):
        end = time.time() + seconds
        while time.time() < end:
            try:
                data = self.ser.readline()
                if data:
                    print(data.decode("utf-8", "replace").rstrip())
            except Exception as exc:  # a board reset pulls the port out from under us
                print("[serial] %r" % (exc,))
                try:
                    self.ser.close()
                except Exception:
                    pass
                time.sleep(1.5)
                try:
                    self.ser = serial.Serial(self.port, 115200, timeout=0.3)
                    print("[serial] reopened")
                except Exception as exc2:
                    print("[serial] reopen failed %r" % (exc2,))
                    time.sleep(1)


def call(path, body=None):
    url = BOARD + path
    data = body.encode() if body else None
    try:
        with urllib.request.urlopen(urllib.request.Request(url, data=data), timeout=15) as r:
            return r.status, r.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as exc:
        return exc.code, exc.read().decode("utf-8", "replace")
    except Exception as exc:
        return "ERR", str(exc)


def main():
    link = Link(PORT)
    link.pump(15)
    link.ser.write(b"w")
    link.pump(12)

    code, page = call("/")
    print("\nGET /             -> %s %d bytes, has title: %s" % (code, len(page), "Retro Radio" in page))
    print("GET /api/status   ->", *call("/api/status"))
    print("GET /api/stations ->", *call("/api/stations"))
    t0 = time.time()
    print("GET /api/scan     -> %.1fs" % (time.time() - t0), *call("/api/scan"))
    print("POST add          ->", *call("/api/station/add",
                                      "name=Smoke+Test&url=http%3A%2F%2Fexample.com%2Fs.mp3"))
    code, stations = call("/api/stations")
    print("stations now      ->", code, stations)
    print("POST del          ->", *call("/api/station/del", "idx=1"))
    print("GET /nope         ->", call("/nope")[0])

    link.ser.write(b"r")
    link.pump(12)


if __name__ == "__main__":
    main()
