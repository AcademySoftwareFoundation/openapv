#!/usr/bin/env python3
"""
Convert Y4M (YUV4MPEG2) files to PNG images.
Y4M is a simple uncompressed YUV format with a text header.
"""
import struct
import numpy as np
from PIL import Image
import sys
import os
import re

def parse_y4m_header(f):
    """Parse Y4M file header and return frame parameters"""
    # Read signature line (should be "YUV4MPEG2 ...")
    header_line = b''
    while True:
        char = f.read(1)
        if char == b'\n':
            break
        header_line += char
        if len(header_line) > 1024:  # Safety limit
            raise ValueError("Header too long")

    header_str = header_line.decode('ascii')

    if not header_str.startswith('YUV4MPEG2'):
        raise ValueError(f"Invalid Y4M file (header: {header_str[:20]})")

    # Parse parameters from header
    width = None
    height = None
    chroma = '420'  # default
    interlace = 'p'  # progressive

    # Parse W (width), H (height), C (chroma), I (interlace)
    for match in re.finditer(r'([WHCI])(\S+)', header_str):
        param_type = match.group(1)
        param_value = match.group(2)

        if param_type == 'W':
            width = int(param_value)
        elif param_type == 'H':
            height = int(param_value)
        elif param_type == 'C':
            chroma = param_value
        elif param_type == 'I':
            interlace = param_value

    if width is None or height is None:
        raise ValueError(f"Missing width or height in Y4M header: {header_str}")

    return width, height, chroma, interlace

def parse_frame_header(f):
    """Read and validate frame header (should be 'FRAME\n')"""
    frame_line = b''
    while True:
        char = f.read(1)
        if not char:
            return False  # EOF
        if char == b'\n':
            break
        frame_line += char
        if len(frame_line) > 100:
            raise ValueError("Frame header too long")

    frame_str = frame_line.decode('ascii')
    if not frame_str.startswith('FRAME'):
        raise ValueError(f"Invalid frame header: {frame_str}")

    return True

def read_y4m_frame(filename):
    """Read first frame from Y4M file"""
    try:
        with open(filename, 'rb') as f:
            # Parse file header
            width, height, chroma, interlace = parse_y4m_header(f)

            print(f"Y4M info: {width}x{height}, chroma={chroma}, interlace={interlace}")

            # Parse frame header
            if not parse_frame_header(f):
                raise ValueError("No frames in Y4M file")

            # Determine bit depth from chroma format
            bit_depth = 8
            if 'p10' in chroma or 'p12' in chroma or 'p14' in chroma or 'p16' in chroma:
                bit_depth = int(chroma.split('p')[1])
                chroma_base = chroma.split('p')[0]
            else:
                chroma_base = chroma

            bytes_per_sample = 2 if bit_depth > 8 else 1

            # Calculate plane sizes based on chroma subsampling
            y_pixels = width * height

            if chroma_base == '420':
                # 4:2:0 - U/V planes are 1/2 width and 1/2 height
                uv_width = width // 2
                uv_height = height // 2
            elif chroma_base == '422':
                # 4:2:2 - U/V planes are 1/2 width, full height
                uv_width = width // 2
                uv_height = height
            elif chroma_base == '444':
                # 4:4:4 - U/V planes are full resolution
                uv_width = width
                uv_height = height
            else:
                raise ValueError(f"Unsupported chroma format: {chroma_base}")

            uv_pixels = uv_width * uv_height

            # Read Y, U, V planes
            y_size = y_pixels * bytes_per_sample
            uv_size = uv_pixels * bytes_per_sample

            y_data = f.read(y_size)
            u_data = f.read(uv_size)
            v_data = f.read(uv_size)

            if len(y_data) != y_size or len(u_data) != uv_size or len(v_data) != uv_size:
                raise ValueError(f"Incomplete frame data (expected Y={y_size}, U={uv_size}, V={uv_size})")

            # Convert to numpy arrays based on bit depth
            if bit_depth > 8:
                # 10/12/14/16-bit: stored as little-endian uint16
                y_array = np.frombuffer(y_data, dtype=np.uint16).reshape((height, width))
                u_array = np.frombuffer(u_data, dtype=np.uint16).reshape((uv_height, uv_width))
                v_array = np.frombuffer(v_data, dtype=np.uint16).reshape((uv_height, uv_width))
            else:
                # 8-bit
                y_array = np.frombuffer(y_data, dtype=np.uint8).reshape((height, width))
                u_array = np.frombuffer(u_data, dtype=np.uint8).reshape((uv_height, uv_width))
                v_array = np.frombuffer(v_data, dtype=np.uint8).reshape((uv_height, uv_width))

            print(f"Y range: {y_array.min()}-{y_array.max()}")
            print(f"U range: {u_array.min()}-{u_array.max()}")
            print(f"V range: {v_array.min()}-{v_array.max()}")

            return y_array, u_array, v_array, width, height, uv_width, uv_height, chroma_base, bit_depth

    except Exception as e:
        print(f"Error reading {filename}:")
        print(f"{e}")
        return None

def yuv_to_rgb_png(y_array, u_array, v_array, width, height, uv_width, uv_height, chroma, bit_depth, output_filename):
    """Convert YUV arrays to RGB PNG"""
    # Upsample U and V to match Y dimensions
    if chroma == '420':
        # 4:2:0 - upsample both dimensions
        u_upsampled = np.repeat(np.repeat(u_array, 2, axis=0), 2, axis=1)[:height, :width]
        v_upsampled = np.repeat(np.repeat(v_array, 2, axis=0), 2, axis=1)[:height, :width]
    elif chroma == '422':
        # 4:2:2 - upsample width only
        u_upsampled = np.repeat(u_array, 2, axis=1)[:, :width]
        v_upsampled = np.repeat(v_array, 2, axis=1)[:, :width]
    else:  # 444 or other
        u_upsampled = u_array[:height, :width]
        v_upsampled = v_array[:height, :width]

    # Convert to floating point for calculations
    Y = y_array.astype(np.float32)
    U = u_upsampled.astype(np.float32)
    V = v_upsampled.astype(np.float32)

    # Determine neutral chroma value based on bit depth
    max_val = (1 << bit_depth) - 1
    chroma_neutral = 1 << (bit_depth - 1)  # 128 for 8-bit, 512 for 10-bit

    # YUV to RGB conversion (BT.709 for HD content)
    R = Y + 1.5748 * (V - chroma_neutral)
    G = Y - 0.1873 * (U - chroma_neutral) - 0.4681 * (V - chroma_neutral)
    B = Y + 1.8556 * (U - chroma_neutral)

    # Scale to 8-bit and clamp
    if bit_depth > 8:
        R = R * 255.0 / max_val
        G = G * 255.0 / max_val
        B = B * 255.0 / max_val

    R = np.clip(R, 0, 255).astype(np.uint8)
    G = np.clip(G, 0, 255).astype(np.uint8)
    B = np.clip(B, 0, 255).astype(np.uint8)

    # Stack into RGB image
    rgb_image = np.stack([R, G, B], axis=2)

    # Save RGB image
    img = Image.fromarray(rgb_image)
    img.save(output_filename)
    print(f"Saved RGB PNG: {output_filename} ({width}x{height})")

    return rgb_image

def save_y_as_grayscale(y_array, bit_depth, output_filename):
    """Save Y component as grayscale PNG"""
    # Scale to 8-bit if needed
    if bit_depth > 8:
        max_val = (1 << bit_depth) - 1
        y_scaled = (y_array.astype(np.float32) * 255.0 / max_val).astype(np.uint8)
    else:
        y_scaled = y_array.astype(np.uint8)

    img = Image.fromarray(y_scaled)
    img.save(output_filename)
    print(f"Saved grayscale PNG: {output_filename}")

def main():
    if len(sys.argv) < 2:
        print("Usage: python convert_y4m_to_png.py <y4m_file> [output_prefix]")
        print("Example: python convert_y4m_to_png.py test/output/frame.y4m")
        sys.exit(1)

    input_file = sys.argv[1]

    if not os.path.exists(input_file):
        print(f"Error: File not found: {input_file}")
        sys.exit(1)

    # Generate output prefix from input filename
    if len(sys.argv) > 2:
        output_prefix = sys.argv[2]
    else:
        base = os.path.splitext(input_file)[0]
        output_prefix = base

    print(f"Converting {input_file} to PNG...")

    result = read_y4m_frame(input_file)
    if not result:
        print("Failed to read Y4M file")
        sys.exit(1)

    y_array, u_array, v_array, width, height, uv_width, uv_height, chroma, bit_depth = result

    # Save RGB version
    rgb_output = output_prefix + "_rgb.png"
    yuv_to_rgb_png(y_array, u_array, v_array, width, height, uv_width, uv_height, chroma, bit_depth, rgb_output)

    # Also save Y component as grayscale for quick preview
    y_output = output_prefix + "_y.png"
    save_y_as_grayscale(y_array, bit_depth, y_output)

    print(f"\nConversion complete!")
    print(f"RGB image: {rgb_output}")
    print(f"Grayscale: {y_output}")

if __name__ == "__main__":
    main()
