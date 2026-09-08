#!/usr/bin/env python3
"""
USB Bulk Transfer Benchmark - Host Side Script

This script communicates with the AM261x device running the USB bulk benchmark.
It uses USB control transfers for commands (separate from bulk data) to ensure
precise timing for accurate benchmark measurements.

Usage:
    python3 usb_bulk_benchmark_host.py --mode in --duration 10
    python3 usb_bulk_benchmark_host.py --mode out --duration 10
    python3 usb_bulk_benchmark_host.py --mode bidir --duration 10

Requirements:
    pip install pyusb
"""

import usb.core
import usb.util
import argparse
import time
import sys

# USB Vendor ID and Product ID (from TinyUSB vendor class)
USB_VID = 0x0451
USB_PID = 0x6170

# Endpoint addresses (must match device configuration)
EP_OUT = 0x01  # Host to Device
EP_IN = 0x81   # Device to Host

# Transfer parameters
BUFFER_SIZE = 16384    # 16KB buffers
TIMEOUT_MS = 5000      # 5 second timeout

# Vendor-specific control requests (separate channel from bulk data)
VENDOR_REQUEST_START_TEST = 0x01
VENDOR_REQUEST_STOP_TEST = 0x02
VENDOR_REQUEST_GET_STATUS = 0x03

# Test mode values for control requests
TEST_MODE_VALUE_IN = 0x01
TEST_MODE_VALUE_OUT = 0x02
TEST_MODE_VALUE_BIDIR = 0x03

class USBBulkBenchmark:
    def __init__(self, vid=USB_VID, pid=USB_PID):
        """Initialize USB device connection"""
        self.dev = None
        self.vid = vid
        self.pid = pid
        self.total_bytes_sent = 0
        self.total_bytes_received = 0
        self.errors = 0
        
    def connect(self):
        """Find and connect to USB device"""
        print(f"Searching for USB device (VID:0x{self.vid:04X}, PID:0x{self.pid:04X})...")
        
        # Find device
        self.dev = usb.core.find(idVendor=self.vid, idProduct=self.pid)
        
        if self.dev is None:
            raise ValueError("Device not found. Make sure the device is connected and enumerated.")
        
        print(f"Found device:")
        print(f"  Manufacturer: {self.dev.manufacturer}")
        print(f"  Product: {self.dev.product}")
        print(f"  Serial: {self.dev.serial_number}")
        
        # Set configuration
        try:
            self.dev.set_configuration()
        except usb.core.USBError as e:
            print(f"Warning: Could not set configuration: {e}")
            print("Trying to detach kernel driver...")
            try:
                if self.dev.is_kernel_driver_active(0):
                    self.dev.detach_kernel_driver(0)
                self.dev.set_configuration()
            except Exception as e2:
                print(f"Error: {e2}")
                raise
        
        print("Device configured successfully")
    
    def send_start_command(self, mode_value):
        """Send start test command via control transfer (precise timing)"""
        try:
            # Vendor-specific control request: START_TEST
            # bmRequestType: 0x40 = Host-to-Device, Vendor, Device
            # bRequest: VENDOR_REQUEST_START_TEST
            # wValue: test mode (IN/OUT/BIDIR)
            # wIndex: 0
            # data: None (zero-length)
            self.dev.ctrl_transfer(
                bmRequestType=0x40,  # Host-to-Device, Vendor, Device
                bRequest=VENDOR_REQUEST_START_TEST,
                wValue=mode_value,
                wIndex=0,
                data_or_wLength=None,
                timeout=TIMEOUT_MS
            )
            time.sleep(0.05)  # Small delay for device to start
        except usb.core.USBError as e:
            print(f"Error sending start command: {e}")
            raise
    
    def send_stop_command(self):
        """Send stop test command via control transfer (precise timing)"""
        try:
            # Vendor-specific control request: STOP_TEST
            self.dev.ctrl_transfer(
                bmRequestType=0x40,  # Host-to-Device, Vendor, Device
                bRequest=VENDOR_REQUEST_STOP_TEST,
                wValue=0,
                wIndex=0,
                data_or_wLength=None,
                timeout=TIMEOUT_MS
            )
            time.sleep(0.05)  # Small delay for device to stop
        except usb.core.USBError as e:
            print(f"Error sending stop command: {e}")
            raise
    
    def get_device_status(self):
        """Get device test status via control transfer"""
        try:
            # Vendor-specific control request: GET_STATUS
            result = self.dev.ctrl_transfer(
                bmRequestType=0xC0,  # Device-to-Host, Vendor, Device
                bRequest=VENDOR_REQUEST_GET_STATUS,
                wValue=0,
                wIndex=0,
                data_or_wLength=1,
                timeout=TIMEOUT_MS
            )
            return result[0] if result else 0
        except usb.core.USBError as e:
            print(f"Error getting status: {e}")
            return 0
        
    def test_bulk_out(self, duration_sec=10):
        """Test Host to Device (OUT) transfers"""
        print(f"\n=== Bulk OUT (Host → Device) | {duration_sec}s ===\n")

        # Send start command via control transfer
        self.send_start_command(TEST_MODE_VALUE_OUT)

        # Generate test pattern
        test_data = bytearray(range(256)) * (BUFFER_SIZE // 256)

        start_time = time.time()
        bytes_sent = 0
        transfer_count = 0
        send_errors = 0
        last_print = start_time

        try:
            while (time.time() - start_time) < duration_sec:
                # Send data
                written = self.dev.write(EP_OUT, test_data, TIMEOUT_MS)
                bytes_sent += written
                transfer_count += 1

                # Print progress every second
                now = time.time()
                if now - last_print >= 1.0:
                    elapsed = now - start_time
                    throughput_mbps = (bytes_sent / elapsed) / (1024 * 1024)
                    err = f"  [{send_errors} err]" if send_errors else ""
                    print(f"  [{elapsed:.1f}s]  {throughput_mbps:.2f} MB/s  ({bytes_sent/1024/1024:.2f} MB){err}")
                    last_print = now

        except usb.core.USBError as e:
            print(f"USB Error: {e}")
            send_errors += 1
            self.errors += 1
        
        # Send stop command via control transfer for precise timing
        try:
            self.send_stop_command()
        except usb.core.USBError as e:
            print(f"Warning: Stop command failed (device may have disconnected): {e}")
            self.errors += 1
        
        # Calculate final statistics
        elapsed = time.time() - start_time
        throughput_mbps = (bytes_sent / elapsed) / (1024 * 1024) if elapsed > 0 else 0

        print(f"\nDuration:   {elapsed:.2f} s")
        print(f"Total:      {bytes_sent/1024/1024:.2f} MB")
        print(f"Throughput: {throughput_mbps:.2f} MB/s")
        if send_errors:
            print(f"Errors:     {send_errors}")

        self.total_bytes_sent = bytes_sent
        return throughput_mbps
    
    def test_bulk_in(self, duration_sec=10):
        """Test Device to Host (IN) transfers"""
        print(f"\n=== Bulk IN (Device → Host) | {duration_sec}s ===\n")

        # Send start command via control transfer
        self.send_start_command(TEST_MODE_VALUE_IN)

        # Generate expected test pattern
        expected_pattern = bytearray(range(256)) * (BUFFER_SIZE // 256)

        start_time = time.time()
        bytes_received = 0
        transfer_count = 0
        validation_errors = 0
        last_print = start_time

        try:
            while (time.time() - start_time) < duration_sec:
                # Read data
                data = self.dev.read(EP_IN, BUFFER_SIZE, TIMEOUT_MS)
                bytes_received += len(data)

                # Validate received data matches expected pattern
                if len(data) >= BUFFER_SIZE:
                    if bytes(data[:BUFFER_SIZE]) != expected_pattern:
                        validation_errors += 1
                        if validation_errors == 1:
                            print(f"  WARNING: Received data does not match expected pattern!")

                transfer_count += 1

                # Print progress every second
                now = time.time()
                if now - last_print >= 1.0:
                    elapsed = now - start_time
                    throughput_mbps = (bytes_received / elapsed) / (1024 * 1024)
                    err = f"  [{validation_errors} val err]" if validation_errors else ""
                    print(f"  [{elapsed:.1f}s]  {throughput_mbps:.2f} MB/s  ({bytes_received/1024/1024:.2f} MB){err}")
                    last_print = now

        except usb.core.USBError as e:
            print(f"USB Error: {e}")
            self.errors += 1
        
        # Send stop command via control transfer for precise timing
        try:
            self.send_stop_command()
        except usb.core.USBError as e:
            print(f"Warning: Stop command failed (device may have disconnected): {e}")
            self.errors += 1
        
        # Calculate final statistics
        elapsed = time.time() - start_time
        throughput_mbps = (bytes_received / elapsed) / (1024 * 1024) if elapsed > 0 else 0

        print(f"\nDuration:   {elapsed:.2f} s")
        print(f"Total:      {bytes_received/1024/1024:.2f} MB")
        print(f"Throughput: {throughput_mbps:.2f} MB/s")
        if validation_errors:
            print(f"Validation errors: {validation_errors}")

        self.total_bytes_received = bytes_received
        return throughput_mbps
    
    def test_bidirectional(self, duration_sec=10):
        """Test sequential bidirectional: RX, TX, RX, TX... (no concurrent contention)"""
        print(f"\n=== Bidirectional | {duration_sec}s ===\n")

        # Send start command via control transfer
        self.send_start_command(TEST_MODE_VALUE_BIDIR)

        # Generate test pattern
        test_data = bytearray(range(256)) * (BUFFER_SIZE // 256)
        expected_pattern = bytearray(range(256)) * (BUFFER_SIZE // 256)

        start_time = time.time()
        bytes_sent = 0
        bytes_received = 0
        tx_transfers = 0
        rx_transfers = 0
        validation_errors = 0
        last_print = start_time

        try:
            while (time.time() - start_time) < duration_sec:
                # Sequential: RX then TX
                try:
                    # Read data (RX)
                    data = self.dev.read(EP_IN, BUFFER_SIZE, TIMEOUT_MS)
                    bytes_received += len(data)
                    rx_transfers += 1

                    # Validate received data
                    if len(data) >= BUFFER_SIZE:
                        if bytes(data[:BUFFER_SIZE]) != expected_pattern:
                            validation_errors += 1

                    # Write data (TX)
                    written = self.dev.write(EP_OUT, test_data, TIMEOUT_MS)
                    bytes_sent += written
                    tx_transfers += 1

                except usb.core.USBError as e:
                    print(f"  USB Error: {e}")
                    self.errors += 1
                    break

                # Print progress every second
                now = time.time()
                if now - last_print >= 1.0:
                    elapsed = now - start_time
                    tx_throughput = (bytes_sent / elapsed) / (1024 * 1024) if elapsed > 0 else 0
                    rx_throughput = (bytes_received / elapsed) / (1024 * 1024) if elapsed > 0 else 0
                    total_throughput = tx_throughput + rx_throughput
                    err = f"  [{validation_errors} err]" if validation_errors else ""
                    print(f"  [{elapsed:.1f}s]  TX {tx_throughput:.2f}  RX {rx_throughput:.2f}  |  {total_throughput:.2f} MB/s{err}")
                    last_print = now

        except usb.core.USBError as e:
            print(f"USB Error: {e}")
            self.errors += 1

        # Send stop command via control transfer for precise timing
        try:
            self.send_stop_command()
        except usb.core.USBError as e:
            print(f"Warning: Stop command failed (device may have disconnected): {e}")
            self.errors += 1

        # Calculate final statistics
        elapsed = time.time() - start_time
        tx_throughput = (bytes_sent / elapsed) / (1024 * 1024) if elapsed > 0 else 0
        rx_throughput = (bytes_received / elapsed) / (1024 * 1024) if elapsed > 0 else 0
        total_throughput = tx_throughput + rx_throughput

        print(f"\nDuration:  {elapsed:.2f} s")
        print(f"TX:        {bytes_sent/1024/1024:.2f} MB  ({tx_throughput:.2f} MB/s)")
        print(f"RX:        {bytes_received/1024/1024:.2f} MB  ({rx_throughput:.2f} MB/s)")
        print(f"Combined:  {total_throughput:.2f} MB/s")
        if validation_errors:
            print(f"Validation errors: {validation_errors}")

        self.total_bytes_sent = bytes_sent
        self.total_bytes_received = bytes_received

        return total_throughput
    
    def disconnect(self):
        """Release USB device"""
        if self.dev:
            usb.util.dispose_resources(self.dev)
            print("\nDevice disconnected")

def main():
    parser = argparse.ArgumentParser(
        description='USB Bulk Transfer Benchmark - Host Side',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 usb_bulk_benchmark_host.py --mode in --duration 10    # Test device→host
  python3 usb_bulk_benchmark_host.py --mode out --duration 10   # Test host→device
  python3 usb_bulk_benchmark_host.py --mode bidir --duration 10 # Test both directions
  python3 usb_bulk_benchmark_host.py --mode all --duration 10   # Run all three (default)

Commands are sent via USB control transfers (separate from bulk data)
for precise timing and accurate benchmark measurements.

The device will print detailed results on its UART console.
        """
    )
    parser.add_argument('--vid', type=lambda x: int(x, 0), default=USB_VID,
                        help='USB Vendor ID (default: 0x0451)')
    parser.add_argument('--pid', type=lambda x: int(x, 0), default=USB_PID,
                        help='USB Product ID (default: 0x6170)')
    parser.add_argument('--mode', choices=['in', 'out', 'bidir', 'all'], default='all',
                        help='Test mode: in (device→host), out (host→device), bidir (both), all (default)')
    parser.add_argument('--duration', type=int, default=10,
                        help='Test duration in seconds (default: 10)')
    
    args = parser.parse_args()
    
    # Create test instance
    test = USBBulkBenchmark(vid=args.vid, pid=args.pid)
    
    try:
        # Connect to device
        test.connect()
        
        # Run test based on mode
        if args.mode == 'out':
            throughput = test.test_bulk_out(args.duration)
        elif args.mode == 'in':
            throughput = test.test_bulk_in(args.duration)
        elif args.mode == 'bidir':
            throughput = test.test_bidirectional(args.duration)
        else:  # all
            tp_out   = test.test_bulk_out(args.duration)
            tp_in    = test.test_bulk_in(args.duration)
            tp_bidir = test.test_bidirectional(args.duration)

        # Print summary
        mode_labels = {'in': 'Device → Host', 'out': 'Host → Device', 'bidir': 'Bidirectional'}
        print(f"\n{'='*42}")
        if args.mode == 'all':
            print(f"  OUT:   {tp_out:.2f} MB/s  (Host → Device)")
            print(f"  IN:    {tp_in:.2f} MB/s  (Device → Host)")
            print(f"  BIDIR: {tp_bidir:.2f} MB/s  (Bidirectional)")
        else:
            print(f"  {throughput:.2f} MB/s  ({mode_labels[args.mode]}, {args.duration}s)")
        if test.errors:
            print(f"  Errors: {test.errors}")
        print(f"{'='*42}")

    except KeyboardInterrupt:
        print("\nInterrupted by user (Ctrl+C)")
        try:
            if test.dev is not None:
                test.send_stop_command()
        except Exception as e:
            print(f"Warning: Could not send stop command: {e}")
        return 1

    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
        return 1
    
    finally:
        test.disconnect()

if __name__ == '__main__':
    sys.exit(main())