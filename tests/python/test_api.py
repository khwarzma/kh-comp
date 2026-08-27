import math
import numpy as np
import pytest
import khcomp


def allocate_aligned_uint8(size: int, alignment: int = 64) -> np.ndarray:
    """Allocates a 1D uint8 NumPy array with 64-byte memory alignment guarantee."""
    buf = np.empty(size + alignment, dtype=np.uint8)
    offset = buf.ctypes.data % alignment
    shift = 0 if offset == 0 else (alignment - offset)
    aligned_sub = buf[shift : shift + size]
    assert aligned_sub.ctypes.data % alignment == 0
    return aligned_sub


def test_package_metadata():
    assert hasattr(khcomp, "__version__")
    assert khcomp.__version__ == "0.1.0"


def test_image_frame_compression_roundtrip():
    width = 16
    height = 16
    total_pixels = width * height

    # 1. Create spatial gradient image buffer
    input_pcm = np.zeros(total_pixels, dtype=np.uint8)
    for y in range(height):
        for x in range(width):
            input_pcm[y * width + x] = (x * 12 + y * 8) % 256

    bitstream_buf = allocate_aligned_uint8(4096, alignment=64)
    reconstructed_pcm = np.zeros(total_pixels, dtype=np.uint8)

    header = khcomp.ImageHeader()
    header.width = width
    header.height = height
    header.format = khcomp.PixelFormat.Grayscale
    header.quality_factor = 85

    pipeline = khcomp.ImageFramePipeline()

    # Encode
    bytes_written = pipeline.encode_grayscale_frame(header, input_pcm, bitstream_buf)
    assert bytes_written > 0

    # Decode
    bytes_read = pipeline.decode_grayscale_frame(
        header, bitstream_buf[:bytes_written], reconstructed_pcm
    )
    assert bytes_read == total_pixels

    # Verify quality metrics
    mse = np.mean((input_pcm.astype(float) - reconstructed_pcm.astype(float)) ** 2)
    psnr = 99.0 if mse == 0 else 10.0 * math.log10((255.0**2) / mse)
    assert psnr > 32.0


def test_video_inter_frame_compression():
    width = 32
    height = 32
    total_pixels = width * height

    frame1 = np.zeros(total_pixels, dtype=np.uint8)
    frame2 = np.zeros(total_pixels, dtype=np.uint8)

    for y in range(height):
        for x in range(width):
            frame1[y * width + x] = (x * 16 + y * 10) % 256

    for y in range(height):
        for x in range(width):
            src_x = x - 2 if x >= 2 else 0
            src_y = y - 1 if y >= 1 else 0
            frame2[y * width + x] = frame1[src_y * width + src_x]

    ring_enc = khcomp.VideoRingBuffer()
    ring_dec = khcomp.VideoRingBuffer()

    storage_enc = allocate_aligned_uint8(total_pixels, alignment=64)
    storage_dec = allocate_aligned_uint8(total_pixels, alignment=64)

    ring_enc.allocate(width, height, storage_enc)
    ring_dec.allocate(width, height, storage_dec)

    v_header = khcomp.VideoHeader()
    v_header.width = width
    v_header.height = height
    v_header.quality_factor = 85
    v_header.search_window = 4

    codec = khcomp.VideoCodecEngine()

    bitstream_iframe = allocate_aligned_uint8(4096, alignment=64)
    iframe_bytes = codec.encode_frame(
        v_header, khcomp.FrameType.IFrame, frame1, ring_enc, bitstream_iframe
    )

    bitstream_pframe = allocate_aligned_uint8(4096, alignment=64)
    pframe_bytes = codec.encode_frame(
        v_header, khcomp.FrameType.PFrame, frame2, ring_enc, bitstream_pframe
    )

    assert pframe_bytes < iframe_bytes

    reconstructed_frame2 = np.zeros(total_pixels, dtype=np.uint8)
    codec.decode_frame(
        v_header, bitstream_iframe[:iframe_bytes], ring_dec, reconstructed_frame2
    )
    dec_pframe_bytes = codec.decode_frame(
        v_header, bitstream_pframe[:pframe_bytes], ring_dec, reconstructed_frame2
    )

    assert dec_pframe_bytes == total_pixels


def test_audio_pcm16_compression_roundtrip():
    sample_rate = 44100
    channels = 2
    num_samples = 1024
    total_interleaved = num_samples * channels

    # Generate synthetic 440 Hz stereo sine wave signal (int16)
    t = np.linspace(0, num_samples / sample_rate, num_samples, endpoint=False)
    sine_wave = (16384.0 * np.sin(2.0 * np.pi * 440.0 * t)).astype(np.int16)

    pcm_input = np.empty(total_interleaved, dtype=np.int16)
    pcm_input[0::2] = sine_wave  # Left channel
    pcm_input[1::2] = sine_wave  # Right channel

    pcm_bytes = pcm_input.view(np.uint8)
    pcm_byte_size = pcm_bytes.nbytes

    bitstream_buf = allocate_aligned_uint8(pcm_byte_size * 2, alignment=64)
    reconstructed_pcm_bytes = allocate_aligned_uint8(pcm_byte_size, alignment=64)

    header = khcomp.AudioHeader()
    header.sample_rate = sample_rate
    header.channels = channels
    header.num_samples = num_samples

    audio_engine = khcomp.AudioCodecEngine()

    # 1. Encode PCM16 -> Bitstream
    compressed_bytes = audio_engine.encode_pcm16(header, pcm_bytes, bitstream_buf)
    assert compressed_bytes > 0

    # 2. Decode Bitstream -> Reconstructed PCM16
    decompressed_bytes = audio_engine.decode_pcm16(
        header, bitstream_buf[:compressed_bytes], reconstructed_pcm_bytes
    )
    assert decompressed_bytes == pcm_byte_size

    # 3. Verify Lossless Roundtrip Precision
    reconstructed_pcm16 = reconstructed_pcm_bytes.view(np.int16)
    np.testing.assert_array_equal(pcm_input, reconstructed_pcm16)