import socket
import serial
import threading
import time

TCP_HOST = '192.168.1.32'
TCP_PORT = 8899

SERIAL_PORT = '/dev/tty.usbserial-313130'
SERIAL_BAUDRATE = 9600
SERIAL_PARITY = serial.PARITY_NONE

RETRY_DELAY = 5

# 상태 플래그
is_sending_tcp = threading.Event()
is_sending_serial = threading.Event()
mutex = threading.Lock()

def tcp_to_serial_worker(tcp_socket, serial_port, stop_event):
    logging_prefix = "[TCP->SER]"
    print(f"{logging_prefix} Worker thread started.")
    try:
        while not stop_event.is_set():
            data = tcp_socket.recv(256)
            if not data:
                print(f"{logging_prefix} TCP connection closed by peer.")
                break

            # 상대 방향 전송 중이면 대기
            while is_sending_serial.is_set() and not stop_event.is_set():
                time.sleep(0.01)

            with mutex:
                is_sending_tcp.set()
            serial_port.write(data)
            print(f"{logging_prefix} Forwarded {len(data)} bytes: {data.hex()}")
            with mutex:
                is_sending_tcp.clear()

    except Exception as e:
        print(f"{logging_prefix} Error: {e}")
    finally:
        stop_event.set()
        print(f"{logging_prefix} Worker thread stopped.")

def serial_to_tcp_worker(tcp_socket, serial_port, stop_event):
    logging_prefix = "[SER->TCP]"
    print(f"{logging_prefix} Worker thread started.")
    try:
        while not stop_event.is_set():
            data = serial_port.read(serial_port.in_waiting or 1)
            if data:
                # 상대 방향 전송 중이면 대기
                while is_sending_tcp.is_set() and not stop_event.is_set():
                    time.sleep(0.01)

                with mutex:
                    is_sending_serial.set()
                tcp_socket.sendall(data)
                print(f"{logging_prefix} Forwarded {len(data)} bytes: {data.hex()}")
                with mutex:
                    is_sending_serial.clear()

    except Exception as e:
        print(f"{logging_prefix} Error: {e}")
    finally:
        stop_event.set()
        print(f"{logging_prefix} Worker thread stopped.")

def main():
    while True:
        tcp_sock = None
        ser_port = None

        try:
            print(f"[*] Connecting to Serial Port ({SERIAL_PORT})...")
            ser_port = serial.Serial(
                port=SERIAL_PORT,
                baudrate=SERIAL_BAUDRATE,
                parity=SERIAL_PARITY,
                stopbits=serial.STOPBITS_ONE,
                bytesize=serial.EIGHTBITS,
                timeout=1
            )
            print(f"[+] Serial Port connected.")

            print(f"[*] Connecting to TCP Server ({TCP_HOST}:{TCP_PORT})...")
            tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            tcp_sock.connect((TCP_HOST, TCP_PORT))
            print(f"[+] TCP Server connected.")

            print("\n[!] Data bridge started. Press Ctrl+C to stop.\n")

            stop_event = threading.Event()

            thread_tcp_to_ser = threading.Thread(
                target=tcp_to_serial_worker,
                args=(tcp_sock, ser_port, stop_event),
                daemon=True
            )
            thread_ser_to_tcp = threading.Thread(
                target=serial_to_tcp_worker,
                args=(tcp_sock, ser_port, stop_event),
                daemon=True
            )

            thread_tcp_to_ser.start()
            thread_ser_to_tcp.start()

            stop_event.wait()

        except serial.SerialException as e:
            print(f"[!] Serial Port error: {e}")
        except socket.error as e:
            print(f"[!] TCP connection error: {e}")
        except KeyboardInterrupt:
            print("\n[!] Program terminated by user.")
            break
        except Exception as e:
            print(f"[!] Unexpected error: {e}")
        finally:
            if tcp_sock:
                tcp_sock.close()
            if ser_port and ser_port.is_open:
                ser_port.close()

            if 'KeyboardInterrupt' not in locals():
                print(f"\n[!] Reconnecting in {RETRY_DELAY} seconds...")
                time.sleep(RETRY_DELAY)

if __name__ == '__main__':
    main()