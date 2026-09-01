To download logs 

import serial
import time
import os

# ----------------------------------------
# Configuration
# ----------------------------------------
PORT = "COM7"
BAUD = 115200

os.makedirs("Logs", exist_ok=True)

# ----------------------------------------
# Connect
# ----------------------------------------
print("Connecting to STM32...")

ser = serial.Serial(PORT, BAUD, timeout=0.1)

time.sleep(2)

ser.reset_input_buffer()

print("Connected.\n")

# ----------------------------------------
# Enter Menu
# ----------------------------------------
ser.write(b'M')

print("Opening Menu...\n")

time.sleep(0.5)

while ser.in_waiting:
    print(ser.read(ser.in_waiting).decode(errors="ignore"), end="")

# ----------------------------------------
# Open Download Menu
# ----------------------------------------
ser.write(b'3')

print("\nOpening Download Menu...\n")

last_data = time.time()

while True:

    if ser.in_waiting:

        data = ser.read(ser.in_waiting).decode(errors="ignore")

        print(data, end="")

        last_data = time.time()

    if time.time() - last_data > 0.5:
        break

print("\n--------------------------------")
print("Menu Ready")
print("--------------------------------")

# ----------------------------------------
# Ask User
# ----------------------------------------

ser.reset_input_buffer()

file_no = input("\nEnter File Number : ")

ser.write((file_no + "\r").encode())

print(f"Sent : {file_no}")

print("\nDownloading...\n")

current_file = None
filepath = ""
filename = ""

# ----------------------------------------
# Receive File
# ----------------------------------------

while True:

    if not ser.in_waiting:
        continue

    line = ser.readline().decode(errors="ignore").strip()

    if line == "":
        continue

    # Ignore GPS prompt
    if line.startswith("GPS>"):
        continue

    # Ignore menu decorations
    if line.startswith("=========="):
        continue

    if "FILES" in line:
        continue

    if line.startswith("Select File"):
        continue

    if line.startswith("0. Back"):
        continue

    # ------------------------------------
    # Handle Choice : BEGIN_FILE
    # ------------------------------------
    if line.startswith("Choice"):

        if "BEGIN_FILE:" in line:

            filename = line.split("BEGIN_FILE:")[1].strip()

            filepath = os.path.join("Logs", filename)

            current_file = open(filepath, "w")

            print(f"\nReceiving : {filename}\n")

        continue

    # ------------------------------------
    # BEGIN_FILE
    # ------------------------------------
    if "BEGIN_FILE:" in line:

        filename = line.split("BEGIN_FILE:")[1].strip()

        filepath = os.path.join("Logs", filename)

        current_file = open(filepath, "w")

        print(f"\nReceiving : {filename}\n")

        continue

    # ------------------------------------
    # END_FILE
    # ------------------------------------
    if "END_FILE" in line:

        if current_file:
            current_file.close()

        print("\n===================================")
        print("Download Completed Successfully")
        print("Saved :", filepath)
        print("===================================")

        break

    # ------------------------------------
    # Save File Data
    # ------------------------------------
    if current_file:
        current_file.write(line + "\n")

    print(line)

ser.close()

print("\nDisconnected.")
