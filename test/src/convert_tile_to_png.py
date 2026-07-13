#!/usr/bin/env python3
import struct
import numpy as np
from PIL import Image
import sys
import os

def read_raw_tile(filename):
    """Read raw tile buffer file with header"""
    try:
        with open(filename, 'rb') as f:
            # Read header (5 ints: width, height, bit_depth, chroma_format, version)
            header_data = f.read(5 * 4)  # 5 ints
            if len(header_data) < 20:
                raise ValueError(f"File too small or missing header: {filename}")
            
            width, height, bit_depth, chroma_format, version = struct.unpack('5i', header_data)
            print(f"Raw tile info: {width}x{height}, bit_depth={bit_depth}, format={chroma_format}")
            
            # Calculate buffer sizes based on chroma format (422 = half width for U/V)
            # Handle both old format value (422) and new format value (2)
            y_pixels = width * height
            uv_width = width // 2 if (chroma_format == 422 or chroma_format == 2) else width
            uv_pixels = uv_width * height
            
            # Read Y, U, V planes (16-bit signed)
            y_data = f.read(y_pixels * 2)  # 2 bytes per pixel
            u_data = f.read(uv_pixels * 2)
            v_data = f.read(uv_pixels * 2)
            
            # Convert to numpy arrays
            y_array = np.frombuffer(y_data, dtype=np.int16).reshape((height, width))
            u_array = np.frombuffer(u_data, dtype=np.int16).reshape((height, uv_width))
            v_array = np.frombuffer(v_data, dtype=np.int16).reshape((height, uv_width))
            
            print(f"Y range: {y_array.min()}-{y_array.max()}")
            print(f"U range: {u_array.min()}-{u_array.max()}")
            print(f"V range: {v_array.min()}-{v_array.max()}")
            
            return y_array, u_array, v_array, width, height, uv_width, bit_depth
            
    except Exception as e:
        print(f"Error reading {filename}: {e}")
        return None

def yuv_to_rgb_png(y_array, u_array, v_array, width, height, uv_width, bit_depth, output_filename):
    """Convert YUV arrays to RGB PNG"""
    # For 10-bit YUV, neutral chroma is 512 (middle of 0-1023 range)
    chroma_neutral = 1 << (bit_depth - 1)  # 512 for 10-bit
    
    # Upsample U and V to match Y dimensions (handle 4:2:2 subsampling)
    u_upsampled = np.repeat(u_array, 2, axis=1)[:, :width]  # Double width
    v_upsampled = np.repeat(v_array, 2, axis=1)[:, :width]  # Double width
    
    # Convert to floating point for calculations
    Y = y_array.astype(np.float32)
    U = u_upsampled.astype(np.float32)
    V = v_upsampled.astype(np.float32)
    
    # YUV to RGB conversion (BT.709 for HD content)
    R = Y + 1.5748 * (V - chroma_neutral)
    G = Y - 0.1873 * (U - chroma_neutral) - 0.4681 * (V - chroma_neutral)  
    B = Y + 1.8556 * (U - chroma_neutral)
    
    # Scale from 10-bit (0-1023) to 8-bit (0-255) and clamp
    max_val = (1 << bit_depth) - 1  # 1023 for 10-bit
    R = np.clip(R * 255.0 / max_val, 0, 255).astype(np.uint8)
    G = np.clip(G * 255.0 / max_val, 0, 255).astype(np.uint8)
    B = np.clip(B * 255.0 / max_val, 0, 255).astype(np.uint8)
    
    # Stack into RGB image
    rgb_image = np.stack([R, G, B], axis=2)
    
    # Save RGB image
    img = Image.fromarray(rgb_image)
    img.save(output_filename)
    print(f"Saved RGB PNG: {output_filename} ({width}x{height})")
    
    return rgb_image

def save_y_as_grayscale(y_array, bit_depth, output_filename):
    """Save Y component as grayscale PNG"""
    # Scale to 8-bit for PNG output
    max_val = (1 << bit_depth) - 1
    
    # Handle negative values by clamping to 0
    y_clamped = np.maximum(y_array, 0)
    y_clamped = np.minimum(y_clamped, max_val)
    
    # Scale to 0-255
    scaled_array = (y_clamped * 255.0 / max_val).astype(np.uint8)
    
    # Create PIL image and save
    img = Image.fromarray(scaled_array)  # PIL automatically detects grayscale format
    img.save(output_filename)
    print(f"Saved grayscale PNG: {output_filename}")

def main():
    if len(sys.argv) < 2:
        print("Usage: python convert_tile_to_png.py <raw_tile_file> [output_prefix]")
        print("Example: python convert_tile_to_png.py test/output/tile_0_0.raw")
        return
    
    input_file = sys.argv[1]
    
    # Generate output prefix from input filename
    if len(sys.argv) > 2:
        output_prefix = sys.argv[2]
    else:
        base = os.path.splitext(input_file)[0]
        output_prefix = base
    
    print(f"Converting {input_file} to PNG...")
    
    result = read_raw_tile(input_file)
    if not result:
        print("Failed to read tile file")
        return
    
    y_array, u_array, v_array, width, height, uv_width, bit_depth = result
    
    # Save RGB version
    rgb_output = output_prefix + "_rgb.png"
    yuv_to_rgb_png(y_array, u_array, v_array, width, height, uv_width, bit_depth, rgb_output)
    
    # Also save Y component as grayscale for quick preview
    y_output = output_prefix + "_y.png"
    save_y_as_grayscale(y_array, bit_depth, y_output)
    
    print(f"\nConversion complete!")
    print(f"View with: start {rgb_output}")

if __name__ == "__main__":
    main()