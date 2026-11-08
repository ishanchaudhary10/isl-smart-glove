import asyncio
import csv
import os
import time
from bleak import BleakScanner, BleakClient

# ============================================================
# VAKYA GLOVE S3 - V2 DATA COLLECTOR
# ============================================================

DEVICE_NAME = "Vakya_Glove_S3"

CHARACTERISTIC_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

# FIXED: was hardcoded to another user's Documents folder
# (C:\Users\ISHAN CHAUDHARY\...), which caused
# [WinError 5] Access is denied on any other machine/account.
# Now it saves next to this script, wherever it's run from.
OUTPUT_FILE = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "isl_training_data_ACTUAL.csv"
)

# ------------------------------------------------------------
# Sensor features - KEEPING YOUR ORIGINAL 14 FEATURES
# ------------------------------------------------------------

FEATURES = [
    "RT", "RI", "RM", "RR", "RK", "RP", "RRo",
    "LT", "LI", "LM", "LR", "LK", "LP", "LRo"
]

CSV_HEADER = ["timestamp", "label"] + FEATURES

# ------------------------------------------------------------
# Collection settings
# ------------------------------------------------------------

SAMPLES_PER_RECORDING = 100

# ESP32 data interval
SAMPLE_INTERVAL_MS = 50

# Number of recordings for each sign
RECORDINGS_PER_SIGN = 10

# Countdown before recording
COUNTDOWN_SECONDS = 2

# Rest time between recordings
REST_SECONDS = 1

# ------------------------------------------------------------
# Special labels for final system
# ------------------------------------------------------------

SPECIAL_LABELS = {
    "END",
    "RESET",
    "NO_SIGN"
}

# ------------------------------------------------------------
# Global state
# ------------------------------------------------------------

recording = False
current_label = ""
sample_count = 0

collected_data = []

current_recording_id = 0
current_session_id = 0
current_user_id = "P01"


# ============================================================
# CSV SETUP
# ============================================================

def prepare_csv():
    """
    Creates the CSV if it doesn't exist.

    IMPORTANT:
    The CSV format is kept exactly the same as your original:

    timestamp,label,14 sensor features
    """

    directory = os.path.dirname(OUTPUT_FILE)

    if directory:
        os.makedirs(directory, exist_ok=True)

    if not os.path.exists(OUTPUT_FILE):

        with open(
            OUTPUT_FILE,
            "w",
            newline="",
            encoding="utf-8"
        ) as f:

            writer = csv.writer(f)
            writer.writerow(CSV_HEADER)

        print(f"[NEW CSV] {OUTPUT_FILE}")

    else:

        print(f"[EXISTING CSV] {OUTPUT_FILE}")
        print("[INFO] New recordings will be appended.")


# ============================================================
# TIMESTAMP
# ============================================================

def get_next_timestamp():
    """
    Finds the next timestamp in the existing CSV.

    Your original behavior is preserved.
    """

    if not os.path.exists(OUTPUT_FILE):
        return 0

    last_timestamp = None

    try:

        with open(
            OUTPUT_FILE,
            "r",
            newline="",
            encoding="utf-8"
        ) as f:

            reader = csv.reader(f)

            try:
                next(reader)
            except StopIteration:
                return 0

            for row in reader:

                if len(row) >= 1:

                    try:
                        last_timestamp = int(float(row[0]))

                    except ValueError:
                        continue

    except Exception as e:

        print(f"\n[WARNING] Could not read timestamp: {e}")
        return 0

    if last_timestamp is None:
        return 0

    return last_timestamp + SAMPLE_INTERVAL_MS


# ============================================================
# PROGRESS BAR
# ============================================================

def print_progress():

    progress = int(
        (sample_count / SAMPLES_PER_RECORDING) * 20
    )

    bar = "#" * progress
    empty = "-" * (20 - progress)

    print(
        f"\r[{bar}{empty}] "
        f"{sample_count}/{SAMPLES_PER_RECORDING}",
        end="",
        flush=True
    )


# ============================================================
# BLE NOTIFICATION HANDLER
# ============================================================

def notification_handler(sender, data):

    global sample_count
    global recording
    global collected_data

    # Ignore packets if we're not currently recording
    if not recording:
        return

    try:

        text = data.decode().strip()

        parts = text.split(",")

        # We expect exactly 14 sensor values
        if len(parts) != 14:
            return

        features = [float(x) for x in parts]

    except Exception:
        return

    # Store sample
    collected_data.append(features)

    sample_count += 1

    print_progress()

    # Stop once we have exactly 100 samples
    if sample_count >= SAMPLES_PER_RECORDING:

        recording = False

        print()


# ============================================================
# COUNTDOWN
# ============================================================

async def countdown():

    print()

    for i in range(COUNTDOWN_SECONDS, 0, -1):

        print(
            f"\rGet ready... {i}",
            end="",
            flush=True
        )

        await asyncio.sleep(1)

    print("\r>>> PERFORM SIGN NOW <<<")


# ============================================================
# SAVE RECORDING
# ============================================================

def save_recording():

    global collected_data

    # Safety check
    if len(collected_data) != SAMPLES_PER_RECORDING:

        print(
            f"\n[WARNING] Recording discarded."
            f" Expected {SAMPLES_PER_RECORDING} samples,"
            f" got {len(collected_data)}."
        )

        collected_data.clear()

        return False

    start_timestamp = get_next_timestamp()

    try:

        with open(
            OUTPUT_FILE,
            "a",
            newline="",
            encoding="utf-8"
        ) as f:

            writer = csv.writer(f)

            for i, features in enumerate(collected_data):

                timestamp = (
                    start_timestamp
                    + i * SAMPLE_INTERVAL_MS
                )

                writer.writerow(
                    [
                        timestamp,
                        current_label
                    ] + features
                )

    except Exception as e:

        print(f"\n[ERROR] Could not save recording: {e}")

        collected_data.clear()

        return False

    print(
        f"[SAVED] "
        f"{len(collected_data)} samples | "
        f"{current_label} | "
        f"Recording ID: {current_recording_id} | "
        f"Session: {current_session_id}"
    )

    collected_data.clear()

    return True


# ============================================================
# COLLECT ONE RECORDING
# ============================================================

async def collect_one_recording(
    recording_number,
    total_recordings
):

    global recording
    global sample_count
    global collected_data
    global current_recording_id

    # New recording ID
    current_recording_id += 1

    # Reset state
    sample_count = 0
    collected_data.clear()

    print()
    print("=" * 55)

    print(
        f"SIGN       : {current_label}"
    )

    print(
        f"USER       : {current_user_id}"
    )

    print(
        f"SESSION    : {current_session_id}"
    )

    print(
        f"RECORDING  : "
        f"{recording_number}/{total_recordings}"
    )

    print(
        f"RECORD ID  : {current_recording_id}"
    )

    print("=" * 55)

    # Countdown
    await countdown()

    # Start collection
    recording = True

    # Wait until 100 samples are collected
    while recording:

        await asyncio.sleep(0.01)

    # Safety check
    if len(collected_data) == SAMPLES_PER_RECORDING:

        save_recording()

    else:

        print(
            f"[WARNING] Incomplete recording."
            f" {len(collected_data)}/"
            f"{SAMPLES_PER_RECORDING} samples."
        )

        collected_data.clear()

    # Rest before next recording
    if recording_number < total_recordings:

        print(
            f"[REST] "
            f"Rest for {REST_SECONDS} seconds..."
        )

        await asyncio.sleep(REST_SECONDS)


# ============================================================
# COLLECT ALL RECORDINGS FOR ONE LABEL
# ============================================================

async def collect_sign():

    print()
    print("=" * 60)
    print(
        f"COLLECTING: {current_label}"
    )
    print(
        f"TOTAL RECORDINGS: {RECORDINGS_PER_SIGN}"
    )
    print("=" * 60)

    for i in range(
        1,
        RECORDINGS_PER_SIGN + 1
    ):

        await collect_one_recording(
            i,
            RECORDINGS_PER_SIGN
        )

    print()
    print(
        f"[DONE] "
        f"{current_label} "
        f"x{RECORDINGS_PER_SIGN}"
    )


# ============================================================
# INPUT LOOP
# ============================================================

async def input_loop():

    global current_label
    global current_user_id
    global current_session_id

    print()
    print("=" * 60)
    print("       VAKYA GLOVE S3 - V2 BULK COLLECTOR")
    print("=" * 60)

    print()
    print("Commands:")
    print("  Enter sign name → start collection")
    print("  USER <id>       → change user")
    print("  SESSION         → start new session")
    print("  EXIT            → quit")
    print()

    loop = asyncio.get_event_loop()

    while True:

        user_input = await loop.run_in_executor(
            None,
            input,
            "Enter command/sign: "
        )

        user_input = user_input.strip()

        if not user_input:
            continue

        # ----------------------------------------------------
        # EXIT
        # ----------------------------------------------------

        if user_input.upper() == "EXIT":

            print("[EXIT] Stopping collector...")
            break

        # ----------------------------------------------------
        # CHANGE USER
        # ----------------------------------------------------

        if user_input.upper().startswith("USER "):

            new_user = user_input[5:].strip()

            if new_user:

                current_user_id = new_user.upper()

                print(
                    f"[USER] Current user: "
                    f"{current_user_id}"
                )

            continue

        # ----------------------------------------------------
        # NEW SESSION
        # ----------------------------------------------------

        if user_input.upper() == "SESSION":

            current_session_id += 1

            print(
                f"[SESSION] "
                f"Started session "
                f"{current_session_id}"
            )

            continue

        # ----------------------------------------------------
        # NORMAL SIGN
        # ----------------------------------------------------

        current_label = user_input.upper()

        # Warn if using a special class
        if current_label in SPECIAL_LABELS:

            print()
            print(
                f"[SPECIAL CLASS] "
                f"{current_label}"
            )

            print(
                "This class is intended for "
                "NO_SIGN / END / RESET."
            )

        # Collect sign
        await collect_sign()


# ============================================================
# BLE CONNECTION
# ============================================================

async def connect_to_glove():

    print()
    print(
        f"Searching for "
        f"{DEVICE_NAME}..."
    )

    device = None

    while device is None:

        try:

            devices = await BleakScanner.discover(
                timeout=5.0
            )

            for d in devices:

                if (
                    d.name
                    and DEVICE_NAME in d.name
                ):

                    device = d
                    break

        except Exception as e:

            print(
                f"[SCAN ERROR] {e}"
            )

        if device is None:

            print(
                "[SCAN] Device not found. "
                "Retrying..."
            )

            await asyncio.sleep(2)

    print()
    print(
        f"[FOUND] {device.name}"
    )

    print(
        f"[ADDRESS] {device.address}"
    )

    return device


# ============================================================
# MAIN
# ============================================================

async def main():

    global current_session_id

    prepare_csv()

    # Start first session
    current_session_id = 1

    print(
        f"[SESSION] Starting session "
        f"{current_session_id}"
    )

    # Find glove
    device = await connect_to_glove()

    try:

        async with BleakClient(
            device.address
        ) as client:

            print(
                "[CONNECTED] "
                f"{device.name}"
            )

            # Start BLE notifications
            await client.start_notify(
                CHARACTERISTIC_UUID,
                notification_handler
            )

            print(
                "[BLE] Notifications started."
            )

            print()
            print(
                "Ready for data collection."
            )

            # Start input system
            await input_loop()

            # Stop notifications
            try:

                await client.stop_notify(
                    CHARACTERISTIC_UUID
                )

                print(
                    "[BLE] Notifications stopped."
                )

            except Exception as e:

                print(
                    f"[BLE WARNING] "
                    f"{e}"
                )

    except Exception as e:

        print()
        print(
            f"[CONNECTION ERROR] {e}"
        )

    finally:

        print()
        print(
            "Collector stopped."
        )


# ============================================================
# PROGRAM ENTRY
# ============================================================

if __name__ == "__main__":

    try:

        asyncio.run(main())

    except KeyboardInterrupt:

        print()
        print(
            "[INTERRUPTED] "
            "Collector stopped by user."
        )

    except Exception as e:

        print()
        print(
            f"[FATAL ERROR] {e}"
        )