from enum import Enum
from typing import Buffer, Union

ReadableBuffer = Union[Buffer, bytes, bytearray, memoryview]

class CompressionError(Enum):
    OK: int
    InvalidInput: int
    BufferTooSmall: int
    AlignmentError: int
    Overflow: int
    CorruptedData: int

class PixelFormat(Enum):
    Grayscale: int
    RGB24: int
    YUV420p: int

class ImageHeader:
    width: int
    height: int
    format: PixelFormat
    quality_factor: int
    def __init__(self) -> None: ...

class ImageFramePipeline:
    def __init__(self) -> None: ...
    def encode_grayscale_frame(
        self,
        header: ImageHeader,
        input_buf: ReadableBuffer,
        output_bitstream_buf: Buffer
    ) -> int: ...
    def decode_grayscale_frame(
        self,
        header: ImageHeader,
        input_bitstream_buf: ReadableBuffer,
        output_buf: Buffer
    ) -> int: ...

class FrameType(Enum):
    IFrame: int
    PFrame: int

class VideoHeader:
    width: int
    height: int
    quality_factor: int
    search_window: int
    def __init__(self) -> None: ...

class VideoRingBuffer:
    def __init__(self) -> None: ...
    def allocate(
        self,
        width: int,
        height: int,
        storage_buf: Buffer
    ) -> None: ...

class VideoCodecEngine:
    def __init__(self) -> None: ...
    def encode_frame(
        self,
        header: VideoHeader,
        type: FrameType,
        input_buf: ReadableBuffer,
        ref_ring: VideoRingBuffer,
        output_bitstream_buf: Buffer
    ) -> int: ...
    def decode_frame(
        self,
        header: VideoHeader,
        input_bitstream_buf: ReadableBuffer,
        ref_ring: VideoRingBuffer,
        output_buf: Buffer
    ) -> int: ...

class AudioHeader:
    sample_rate: int
    channels: int
    num_samples: int
    def __init__(self) -> None: ...

class AudioCodecEngine:
    def __init__(self) -> None: ...
    def encode_pcm16(
        self,
        header: AudioHeader,
        pcm_in: ReadableBuffer,
        output_bitstream_buf: Buffer
    ) -> int: ...
    def decode_pcm16(
        self,
        header: AudioHeader,
        input_bitstream_buf: ReadableBuffer,
        pcm_out: Buffer
    ) -> int: ...