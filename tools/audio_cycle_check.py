"""Reproduce / verify the stop->play and station-switch audio bug over serial.

Keeps the port open for the whole run (closing it resets the board via CH340 DTR).
Usage: python tools/audio_cycle_check.py [com] [scenario]

scenarios: stop-play (default), station
"""

import sys
import time

import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM5"
SCENARIO = sys.argv[2] if len(sys.argv) > 2 else "stop-play"

ser = serial.Serial(PORT, 115200, timeout=0.3)


def pump(seconds, tag=""):
    end = time.time() + seconds
    while time.time() < end:
        try:
            line = ser.readline().decode("utf-8", "replace").rstrip()
        except Exception as exc:
            print("[serial] %r" % (exc,))
            return
        if line:
            if "state" in line or "I2S" in line or "i2s" in line or tag:
                print(line)


ser.write(b"r")  # clean boot so every run starts from a fresh I2S channel
pump(25, "boot")  # boot + connect + first audio

if SCENARIO == "station":
    print("--- switch station (n)")
    ser.write(b"n")
    pump(25, "next")
else:
    print("--- STOP")
    ser.write(b"s")
    pump(6, "stop")
    print("--- PLAY again")
    ser.write(b"s")
    pump(30, "play")

ser.write(b"s")  # leave the board stopped so the next run starts clean
pump(3, "cleanup")
print("done")
