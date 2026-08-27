import time
import numpy as np
import khcomp


def allocate_aligned_uint8(size: int, alignment: int = 64) -> np.ndarray:
    """تخصيص ذاكرة uint8 محاذية على 64 بايت لضمان متطلبات الـ SIMD والـ C++ Engine"""
    buf = np.empty(size + alignment, dtype=np.uint8)
    offset = buf.ctypes.data % alignment
    shift = 0 if offset == 0 else (alignment - offset)
    aligned_sub = buf[shift : shift + size]
    assert aligned_sub.ctypes.data % alignment == 0
    return aligned_sub


def benchmark_image():
    width, height = 1920, 1080

    # 1. تجهيز الـ Frame والـ Buffers المحاذية (64-byte aligned)
    raw_frame = np.random.randint(0, 256, size=(height, width), dtype=np.uint8)
    frame_bytes = raw_frame.tobytes()
    frame_size_bytes = len(frame_bytes)
    frame_size_mb = frame_size_bytes / (1024 * 1024)

    # تخصيص ذاكرة محاذية تضمن شرط الـ 64 بايت
    compressed_buf = allocate_aligned_uint8(frame_size_bytes, alignment=64)
    decompressed_buf = allocate_aligned_uint8(frame_size_bytes, alignment=64)

    # تجهيز كائنات الـ Engine
    pipeline = khcomp.ImageFramePipeline()
    header = khcomp.ImageHeader()
    header.width = width
    header.height = height
    header.format = khcomp.PixelFormat.Grayscale
    header.quality_factor = 80

    iterations = 20

    # 2. قياس سرعة الضغط
    start_time = time.perf_counter()
    for _ in range(iterations):
        bytes_written = pipeline.encode_grayscale_frame(
            header, frame_bytes, compressed_buf
        )
    encode_dur = time.perf_counter() - start_time
    encode_speed = (frame_size_mb * iterations) / encode_dur

    # 3. قياس سرعة فك الضغط
    start_time = time.perf_counter()
    for _ in range(iterations):
        pipeline.decode_grayscale_frame(
            header, compressed_buf[:bytes_written], decompressed_buf
        )
    decode_dur = time.perf_counter() - start_time
    decode_speed = (frame_size_mb * iterations) / decode_dur

    ratio = (bytes_written / frame_size_bytes) * 100

    print("\n--- 🖼️ Image Compression Benchmark (1080p) ---")
    print(f"Original Frame Size: {frame_size_mb:.2f} MB")
    print(f"Compressed Size:     {bytes_written / (1024*1024):.2f} MB ({ratio:.2f}%)")
    print(f"Encode Throughput:   {encode_speed:.2f} MB/s")
    print(f"Decode Throughput:   {decode_speed:.2f} MB/s")


if __name__ == "__main__":
    benchmark_image()