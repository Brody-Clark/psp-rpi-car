import socket
import struct
import threading
import subprocess
import time

HOST = "0.0.0.0"
PORT = 12345

FRAME_W, FRAME_H, BPP = 480, 272, 4
FRAME_SIZE = FRAME_W * FRAME_H * BPP

def receive_inputs(conn):
    """Thread to receive and print PSP input data."""
    try:
        while True:
            data = conn.recv(6)  # Expect 6 bytes: 4 for buttons, 1 for lx, 1 for ly
            if not data:
                print(f'[INPUT] Error, no data.')
                break
            buttons, lx, ly = struct.unpack('<IBB', data)
            print(f'[INPUT] Buttons: {hex(buttons)}, Lx: {lx}, Ly: {ly}')
    except Exception as e:
        print(f'[ERROR] Input receive failed: {e}')
    finally:
        print('[INFO] Input thread ending')

def stream_video(conn):
    """Thread to stream MJPEG frames from ffmpeg to PSP."""
    try:
        # Start ffmpeg subprocess, capture stderr for debugging
        ffmpeg = subprocess.Popen(
            ["ffmpeg", "-i", "SPIDER.avi", "-vf", f"scale={FRAME_W}:{FRAME_H}", "-c:v","mjpeg", "-pix_fmt","yuvj420p", "-r", "15", "-f", "mjpeg", "pipe:1"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            bufsize=10**6
        )

        buffer = b""
        while True:
            chunk = ffmpeg.stdout.read(4096)
            if not chunk:
                break
            buffer += chunk
            # Extract and send complete JPEG frames
            while True:
                start = buffer.find(b'\xff\xd8')  # JPEG SOI
                end = buffer.find(b'\xff\xd9')    # JPEG EOI
                if start != -1 and end != -1 and end > start:
                    frame = buffer[start:end+2]
                    buffer = buffer[end+2:]
                    
                    # Send frame size (4 bytes, network order) + frame data
                    header = struct.pack("!I", len(frame))
                    print(f'[VIDEO] Sent frame ({len(frame)} bytes)')
                    conn.sendall(header + frame)
                else:
                    break
    except Exception as e:
        print(f'[ERROR] Video stream failed: {e}')

    finally:
        try:
            # show ffmpeg stderr for diagnostics
            if ffmpeg and ffmpeg.stderr:
                err = ffmpeg.stderr.read().decode(errors='ignore')
                if err:
                    print('[FFMPEG STDERR]\n' + err)
        except Exception:
            pass
        try:
            ffmpeg.terminate()
        except Exception:
            pass
        print('[INFO] Video thread ending')

def main():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:

        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        server.bind((HOST, PORT))
        server.listen(1)
        print(f'[INFO] Listening on {HOST}:{PORT}...')
        
        conn, addr = server.accept()
        print(f'[INFO] PSP connected from {addr}')
        
        # Start threads for input and video
        input_thread = threading.Thread(target=receive_inputs, args=(conn,))
        video_thread = threading.Thread(target=stream_video, args=(conn,))
        
        input_thread.start()
        video_thread.start()
        
        # Wait for threads to finish (or handle shutdown)
        input_thread.join()
        video_thread.join()
        
        print('[INFO] Connection closed')

if __name__ == '__main__':
    main()