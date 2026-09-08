#!/usr/bin/env python3
"""
USB CDC-ACM Throughput Benchmark - Host Side Script

This script communicates with the AM261x device running the USB CDC benchmark.
It uses USB vendor control transfers on EP0 for commands (separate from CDC
bulk data) to ensure precise timing and avoid serial line state glitches.

Usage:
    python3 usb_cdc_benchmark_host.py --mode tx --duration 10
    python3 usb_cdc_benchmark_host.py --mode rx --duration 10
    python3 usb_cdc_benchmark_host.py --mode all --duration 10

Requirements:
    pip install pyusb
"""

import usb.core
import usb.util
import argparse
import time
import sys

# USB Vendor ID and Product ID (from usb_descriptors.c)
USB_VID = 0x0451
USB_PID = 0x6165 

# CDC interface numbers (from usb_descriptors.c enum)
ITF_CDC_COMM = 0   # CDC communication interface (notification EP)
ITF_CDC_DATA = 1   # CDC data interface (bulk endpoints)

# Bulk endpoint addresses for CDC interface 0
EP_OUT = 0x02   # Host to Device (EPNUM_CDC_0_DATA)
EP_IN  = 0x82   # Device to Host (0x80 | EPNUM_CDC_0_DATA)

# Transfer parameters
BUFFER_SIZE = 4096    # Match CDC_CHUNK_SIZE on device
TIMEOUT_MS  = 5000    # 5 second timeout

# Vendor control request codes (must match usb_cdc_benchmark.c)
VENDOR_REQUEST_START_TEST = 0x01
VENDOR_REQUEST_STOP_TEST  = 0x02

# wValue for START_TEST
TEST_MODE_TX = 0x01   # device → host
TEST_MODE_RX = 0x02   # host  → device


class USBCdcBenchmark:
    def __init__(self, vid=USB_VID, pid=USB_PID):
        self.dev = None
        self.vid = vid
        self.pid = pid
        self.errors = 0

    def connect(self):
        """Find the CDC device and detach it from the kernel CDC-ACM driver."""
        print(f"Searching for USB device (VID:0x{self.vid:04X}, PID:0x{self.pid:04X})...")

        self.dev = usb.core.find(idVendor=self.vid, idProduct=self.pid)
        if self.dev is None:
            raise ValueError("Device not found. Make sure the device is connected and enumerated.")

        print(f"Found device:")
        print(f"  Manufacturer : {usb.util.get_string(self.dev, self.dev.iManufacturer)}")
        print(f"  Product      : {usb.util.get_string(self.dev, self.dev.iProduct)}")

        # Detach the kernel CDC-ACM driver from both CDC interfaces so pyusb
        # can access the bulk endpoints and send control transfers.
        for itf in (ITF_CDC_COMM, ITF_CDC_DATA):
            try:
                if self.dev.is_kernel_driver_active(itf):
                    self.dev.detach_kernel_driver(itf)
                    print(f"  Detached kernel driver from interface {itf}")
            except usb.core.USBError as e:
                print(f"  Warning: could not detach interface {itf}: {e}")

        self.dev.set_configuration()
        print("Device configured successfully")

    def _send_start(self, mode_value):
        """Send START_TEST vendor control transfer."""
        self.dev.ctrl_transfer(
            bmRequestType = 0x40,  # Host-to-Device, Vendor, Device
            bRequest      = VENDOR_REQUEST_START_TEST,
            wValue        = mode_value,
            wIndex        = 0,
            data_or_wLength = None,
            timeout       = TIMEOUT_MS,
        )
        time.sleep(0.05)

    def _send_stop(self):
        """Send STOP_TEST vendor control transfer."""
        try:
            self.dev.ctrl_transfer(
                bmRequestType = 0x40,
                bRequest      = VENDOR_REQUEST_STOP_TEST,
                wValue        = 0,
                wIndex        = 0,
                data_or_wLength = None,
                timeout       = TIMEOUT_MS,
            )
            time.sleep(0.05)
        except usb.core.USBError as e:
            print(f"Warning: stop command failed: {e}")
            self.errors += 1

    def test_tx(self, duration_sec=10):
        """TX mode: device sends data, host reads and discards it."""
        print(f"\n=== CDC TX (Device → Host) | {duration_sec}s ===\n")

        self._send_start(TEST_MODE_TX)

        start_time = time.time()
        bytes_received = 0
        last_print = start_time

        try:
            while (time.time() - start_time) < duration_sec:
                data = self.dev.read(EP_IN, BUFFER_SIZE, TIMEOUT_MS)
                bytes_received += len(data)

                now = time.time()
                if now - last_print >= 1.0:
                    elapsed = now - start_time
                    throughput = (bytes_received / elapsed) / (1024 * 1024)
                    print(f"  [{elapsed:.1f}s]  {throughput:.2f} MB/s  ({bytes_received/1024/1024:.2f} MB)")
                    last_print = now

        except usb.core.USBError as e:
            print(f"USB Error: {e}")
            self.errors += 1

        self._send_stop()

        elapsed = time.time() - start_time
        throughput = (bytes_received / elapsed) / (1024 * 1024) if elapsed > 0 else 0.0

        print(f"\nDuration:   {elapsed:.2f} s")
        print(f"Total:      {bytes_received/1024/1024:.2f} MB")
        print(f"Throughput: {throughput:.2f} MB/s")

        return throughput

    def test_rx(self, duration_sec=10):
        """RX mode: host sends data, device reads and discards it."""
        print(f"\n=== CDC RX (Host → Device) | {duration_sec}s ===\n")

        send_buf = bytes(i & 0xFF for i in range(BUFFER_SIZE))

        self._send_start(TEST_MODE_RX)

        start_time = time.time()
        bytes_sent = 0
        send_errors = 0
        last_print = start_time

        try:
            while (time.time() - start_time) < duration_sec:
                try:
                    written = self.dev.write(EP_OUT, send_buf, TIMEOUT_MS)
                    bytes_sent += written
                except usb.core.USBError as e:
                    print(f"  Write error: {e}")
                    send_errors += 1
                    self.errors += 1

                now = time.time()
                if now - last_print >= 1.0:
                    elapsed = now - start_time
                    throughput = (bytes_sent / elapsed) / (1024 * 1024)
                    err = f"  [{send_errors} err]" if send_errors else ""
                    print(f"  [{elapsed:.1f}s]  {throughput:.2f} MB/s  ({bytes_sent/1024/1024:.2f} MB){err}")
                    last_print = now

        except usb.core.USBError as e:
            print(f"USB Error: {e}")
            self.errors += 1

        self._send_stop()

        elapsed = time.time() - start_time
        throughput = (bytes_sent / elapsed) / (1024 * 1024) if elapsed > 0 else 0.0

        print(f"\nDuration:   {elapsed:.2f} s")
        print(f"Total:      {bytes_sent/1024/1024:.2f} MB")
        print(f"Throughput: {throughput:.2f} MB/s")
        if send_errors:
            print(f"Errors:     {send_errors}")

        return throughput

    def disconnect(self):
        """Release the USB device back to the OS."""
        if self.dev:
            usb.util.dispose_resources(self.dev)
            print("\nDevice released")


def main():
    parser = argparse.ArgumentParser(
        description='USB CDC-ACM Throughput Benchmark - Host Side',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 usb_cdc_benchmark_host.py --mode tx            # Device → Host, 10s
  python3 usb_cdc_benchmark_host.py --mode rx            # Host → Device, 10s
  python3 usb_cdc_benchmark_host.py --mode all           # Run both (default)
  python3 usb_cdc_benchmark_host.py --mode tx --duration 30

Commands are sent via USB vendor control transfers on EP0 (separate from CDC
bulk data) for precise timing and to avoid serial line state glitches.

The device prints throughput and CPU load results on its UART console.
        """
    )
    parser.add_argument('--vid', type=lambda x: int(x, 0), default=USB_VID,
                        help=f'USB Vendor ID (default: 0x{USB_VID:04X})')
    parser.add_argument('--pid', type=lambda x: int(x, 0), default=USB_PID,
                        help=f'USB Product ID (default: 0x{USB_PID:04X})')
    parser.add_argument('--mode', choices=['tx', 'rx', 'all'], default='all',
                        help='Test mode: tx (device→host), rx (host→device), all (default)')
    parser.add_argument('--duration', type=int, default=10,
                        help='Test duration in seconds (default: 10)')

    args = parser.parse_args()

    test = USBCdcBenchmark(vid=args.vid, pid=args.pid)

    try:
        test.connect()

        if args.mode == 'tx':
            throughput = test.test_tx(args.duration)
        elif args.mode == 'rx':
            throughput = test.test_rx(args.duration)
        else:  # all
            tp_tx = test.test_tx(args.duration)
            time.sleep(0.5)
            tp_rx = test.test_rx(args.duration)

        print(f"\n{'='*42}")
        if args.mode == 'all':
            print(f"  TX:  {tp_tx:.2f} MB/s  (Device → Host)")
            print(f"  RX:  {tp_rx:.2f} MB/s  (Host → Device)")
        else:
            labels = {'tx': 'Device → Host', 'rx': 'Host → Device'}
            print(f"  {throughput:.2f} MB/s  ({labels[args.mode]}, {args.duration}s)")
        if test.errors:
            print(f"  Errors: {test.errors}")
        print(f"{'='*42}")

    except KeyboardInterrupt:
        print("\nInterrupted by user (Ctrl+C)")
        try:
            test._send_stop()
        except Exception:
            pass
        return 1

    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
        return 1

    finally:
        test.disconnect()

    return 0


if __name__ == '__main__':
    sys.exit(main())
