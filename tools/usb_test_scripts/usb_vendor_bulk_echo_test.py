#!/usr/bin/env python3
import usb.core
import usb.util
import usb.backend.libusb1
import sys
import time
import os
import threading
import random
import argparse



# USB device identifiers for AM261x Vendor Bulk example
USB_VID = 0x0451   # Texas Instruments
USB_PID = 0x6170   # AM261x Vendor
EP_OUT  = 0x01     # Bulk OUT endpoint (host to device)
EP_IN   = 0x81     # Bulk IN  endpoint (device to host)


def find_device():
    """Locate the AM261x USB vendor device on the host."""
    print(f"Looking for USB device VID=0x{USB_VID:04X}, PID=0x{USB_PID:04X}...")
    dev = usb.core.find(idVendor=USB_VID, idProduct=USB_PID)
    if dev is None:
        raise ValueError("Device not found. Make sure the board is running the example and connected via USB.")
    print("Device found.")
    try:
        print(f"  Manufacturer : {dev.manufacturer}")
        print(f"  Product      : {dev.product}")
    except Exception:
        print("  (Could not read device strings - check driver / permissions)")
    return dev


def setup_device(dev):
    """Configure the USB device and print endpoint information."""
    # Linux/macOS only: detach a kernel driver if one has bound the interface.
    # Not implemented by the Windows backend, hence the broad except.
    try:
        if dev.is_kernel_driver_active(0):
            dev.detach_kernel_driver(0)
    except (NotImplementedError, usb.core.USBError):
        pass

    try:
        dev.set_configuration()
    except Exception as e:
        print(f"  Configuration warning: {e}")

    try:
        cfg  = dev.get_active_configuration()
        intf = cfg[(0, 0)]
        print(f"  Interface : {intf.bInterfaceNumber}")
        print("  Endpoints :")
        for ep in intf:
            print(f"    Address 0x{ep.bEndpointAddress:02X}  MaxPacketSize {ep.wMaxPacketSize}")
    except Exception as e:
        print(f"  Could not read configuration: {e}")

    return dev


def bulk_transfer(dev, data, timeout=10000, write_size=None, read_size=4096, rx_delay=0.0):
    """
    Send data to the device and receive the echo concurrently.

    write_size : bytes per dev.write() call (None = whole payload in one call)
    read_size  : max bytes per dev.read() call
    rx_delay   : seconds the receiver waits before reading; a slow reader fills
                 the device TX FIFO and exercises the flow-control path

    The receiver thread starts before the sender so it is already
    listening when data arrives. This keeps the device TX buffer
    drained throughout the transfer and avoids stalls on large payloads.
    """
    received_data = bytearray()
    send_error    = [None]
    recv_error    = [None]
    bytes_sent    = [0]

    def sender_thread():
        try:
            if write_size is None:
                bytes_sent[0] = dev.write(EP_OUT, data, timeout)
            else:
                for off in range(0, len(data), write_size):
                    bytes_sent[0] += dev.write(EP_OUT, data[off:off + write_size], timeout)
        except usb.core.USBError as e:
            send_error[0] = e

    def receiver_thread():
        """Read echoed data until all expected bytes are received or retries are exhausted."""
        expected      = len(data)
        no_data_count = 0

        if rx_delay > 0:
            time.sleep(rx_delay)

        while len(received_data) < expected and no_data_count < 10:
            try:
                remaining = expected - len(received_data)
                chunk = dev.read(EP_IN, min(remaining, read_size), timeout=2000)
                if len(chunk) > 0:
                    received_data.extend(chunk)
                    no_data_count = 0
                else:
                    no_data_count += 1
            except usb.core.USBTimeoutError:
                no_data_count += 1
            except usb.core.USBError as e:
                if 'timed out' in str(e).lower():
                    no_data_count += 1
                else:
                    recv_error[0] = e
                    break

    receiver = threading.Thread(target=receiver_thread)
    sender   = threading.Thread(target=sender_thread)

    receiver.start()
    sender.start()

    sender.join()
    receiver.join()

    print(f"  Sent     : {bytes_sent[0]} bytes")
    print(f"  Received : {len(received_data)} bytes")

    if send_error[0]:
        print(f"  Send error    : {send_error[0]}")
        return False, 0
    if recv_error[0]:
        print(f"  Receive error : {recv_error[0]}")
        return False, 0

    sent_data = data[:bytes_sent[0]]

    if bytes(received_data) == sent_data:
        return True, bytes_sent[0]
    else:
        if bytes_sent[0] != len(received_data):
            print(f"  Length mismatch : sent {bytes_sent[0]}, received {len(received_data)}")
        else:
            for i in range(len(received_data)):
                if sent_data[i] != received_data[i]:
                    print(f"  Data mismatch at byte {i} (packet {i // 512}, offset {i % 512}): "
                          f"sent 0x{sent_data[i]:02X}, received 0x{received_data[i]:02X}")
                    lo, hi = max(0, i - 4), i + 8
                    print(f"    sent     [{lo}:{hi}] : {sent_data[lo:hi].hex(' ')}")
                    print(f"    received [{lo}:{hi}] : {bytes(received_data[lo:hi]).hex(' ')}")
                    break
        return False, bytes_sent[0]


def run_test(dev, test_name, data, **kwargs):
    """Execute a single bulk echo transfer test and report the result."""
    print(f"\nTest : {test_name}")
    print(f"  Size : {len(data)} bytes")

    start  = time.time()
    passed, actual_bytes = bulk_transfer(dev, data, **kwargs)
    elapsed = time.time() - start

    if passed:
        throughput = actual_bytes / elapsed / 1024 if elapsed > 0 else 0
        print(f"  Result : PASS  ({elapsed:.3f}s, {throughput:.1f} KB/s)")
    else:
        print(f"  Result : FAIL")

    return passed


def make_test_data(num_packets):
    """
    Build test payload as a sequence of 512-byte packets.
    Each packet contains a 2-byte big-endian sequence number followed by 0xAA fill bytes.
    """
    data = bytearray()
    for i in range(num_packets):
        packet    = bytearray(512)
        packet[0] = (i >> 8) & 0xFF
        packet[1] = i & 0xFF
        for j in range(2, 512):
            packet[j] = 0xAA
        data.extend(packet)
    return bytes(data)


def make_pattern_data(size, seed=1):
    """
    Build a deterministic pseudo-random payload. Unlike the fixed-fill packets,
    every byte differs, so a duplicated, dropped or reordered byte is detected.
    """
    rng = random.Random(seed)
    return rng.getrandbits(8 * size).to_bytes(size, "little")


def fmt_size(n):
    """Human readable size: 512 B, 4 KB, 1 MB."""
    if n >= 1024 * 1024 and n % (1024 * 1024) == 0:
        return f"{n // (1024 * 1024)} MB"
    if n >= 1024 and n % 1024 == 0:
        return f"{n // 1024} KB"
    return f"{n} B"


def main():
    parser = argparse.ArgumentParser(description="USB vendor bulk echo test")
    parser.add_argument("--repeat", type=int, default=20,
                        help="iterations of the repeated stress test (default 20)")
    parser.add_argument("--quick", action="store_true",
                        help="skip transfers of 1 MB and above and the repeated stress test")
    args = parser.parse_args()

    KB = 1024
    MB = 1024 * KB

    print("=" * 60)
    print("USB Vendor Bulk Echo Test")
    print("=" * 60)

    try:
        dev = find_device()
        dev = setup_device(dev)

        results = []   # (group, name, passed)

        def group(title):
            print("\n" + "=" * 60)
            print(title)
            print("=" * 60)
            return title

        def test(grp, name, data, **kwargs):
            results.append((grp, name, run_test(dev, name, data, **kwargs)))

        # 1. short transfers around the 512-byte packet boundary:
        #    short packets, flush / zero-length-packet handling
        g = group("1. Packet boundary sizes")
        for size in (1, 63, 64, 65, 511, 512, 513, 1023, 1024, 1025, 4097):
            test(g, fmt_size(size), make_pattern_data(size, size))

        # 2. increasing transfer sizes, every byte differs so any duplicated,
        #    dropped or reordered byte is detected
        g = group("2. Transfer sizes")
        sizes = [4 * KB, 16 * KB, 32 * KB, 64 * KB, 128 * KB, 256 * KB, 512 * KB]
        if not args.quick:
            sizes += [1 * MB, 2 * MB, 4 * MB, 8 * MB]
        for size in sizes:
            test(g, fmt_size(size), make_pattern_data(size, size))

        # 3. fixed-fill packets, each starting with a 2-byte sequence number
        g = group("3. Sequence-numbered packets")
        for kb in (128, 256, 512):
            test(g, fmt_size(kb * KB), make_test_data(kb * KB // 512))

        # 4. different host write/read chunk sizes
        g = group("4. Host chunk sizes (256 KB)")
        for chunk in (512, 4 * KB, 16 * KB):
            test(g, f"chunk {fmt_size(chunk)}", make_pattern_data(256 * KB, 3),
                 write_size=chunk, read_size=chunk)

        # 5. slow reader: device TX FIFO fills, host is throttled, no data may be lost
        g = group("5. Flow control (slow reader)")
        test(g, "256 KB, reader starts after 1 s", make_pattern_data(256 * KB, 2), rx_delay=1.0)

        # 6. an intermittent race may pass once and fail later
        if not args.quick:
            g = group(f"6. Stress ({args.repeat} x 128 KB)")
            for n in range(args.repeat):
                test(g, f"repeat {n + 1}/{args.repeat}", make_pattern_data(128 * KB, 100 + n))

        print("\n" + "=" * 60)
        print("Test Summary")
        print("=" * 60)
        groups = list(dict.fromkeys(g for g, _, _ in results))
        for g in groups:
            in_group = [r for r in results if r[0] == g]
            ok = sum(1 for r in in_group if r[2])
            print(f"{g:<40} {ok}/{len(in_group)}")

        failed = [(g, n) for g, n, ok in results if not ok]
        passed = len(results) - len(failed)
        print("-" * 60)
        print(f"Passed : {passed}/{len(results)}")
        if not failed:
            print("All tests PASSED.")
        else:
            print("Failed tests:")
            for g, n in failed:
                print(f"  {g} / {n}")
            print("Some tests FAILED.")
            sys.exit(1)

    except ValueError as e:
        print(f"Error: {e}")
        sys.exit(1)
    except usb.core.USBError as e:
        print(f"USB Error: {e}")
        if sys.platform.startswith("win"):
            print("Install the WinUSB driver using the Zadig tool before running this script.")
        elif sys.platform.startswith("linux"):
            print("On Linux, run with sudo or add a udev rule, e.g. "
                  '/etc/udev/rules.d/99-am261x.rules:\n'
                  '  SUBSYSTEM=="usb", ATTRS{idVendor}=="0451", ATTRS{idProduct}=="6170", MODE="0666"')
        sys.exit(1)
    except Exception as e:
        print(f"Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
