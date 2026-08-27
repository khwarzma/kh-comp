#include <khcomp/audio_engine.hpp>
#include <khcomp/bit_stream.hpp>
#include <khcomp/comp_engine.hpp>
#include <khcomp/image_engine.hpp>
#include <khcomp/image_frame.hpp>
#include <khcomp/types.hpp>
#include <khcomp/video_ring_buffer.hpp>

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstdint>
#include <stdexcept>

namespace py = pybind11;

PYBIND11_MODULE(_native, m) {
    m.doc() = "Khwarzma High-Throughput Media & Data Compression Engine (C++23)";

    // ==========================================
    // Compression Error Enum
    // ==========================================
    py::enum_<khcomp::CompressionError>(m, "CompressionError")
        .value("InvalidInput", khcomp::CompressionError::InvalidInput)
        .value("BufferTooSmall", khcomp::CompressionError::BufferTooSmall)
        .value("AlignmentError", khcomp::CompressionError::AlignmentError)
        .value("Overflow", khcomp::CompressionError::Overflow)
        .value("CorruptedData", khcomp::CompressionError::CorruptedData)
        .export_values();

    // ==========================================
    // Image Engine Bindings
    // ==========================================
    py::enum_<khcomp::image::PixelFormat>(m, "PixelFormat")
        .value("Grayscale", khcomp::image::PixelFormat::Grayscale)
        .export_values();

    py::class_<khcomp::image::ImageHeader>(m, "ImageHeader")
        .def(py::init<>())
        .def_readwrite("width", &khcomp::image::ImageHeader::width)
        .def_readwrite("height", &khcomp::image::ImageHeader::height)
        .def_readwrite("format", &khcomp::image::ImageHeader::format)
        .def_readwrite("quality_factor", &khcomp::image::ImageHeader::quality_factor);

    py::class_<khcomp::image::ImageFramePipeline>(m, "ImageFramePipeline")
        .def(py::init<>())
        .def("encode_grayscale_frame", [](khcomp::image::ImageFramePipeline& self,
                                          const khcomp::image::ImageHeader& header,
                                          py::buffer input_buf,
                                          py::buffer output_bitstream_buf) -> size_t {
            py::buffer_info in_info = input_buf.request();
            py::buffer_info out_info = output_bitstream_buf.request();

            if (reinterpret_cast<uintptr_t>(out_info.ptr) % 64 != 0) {
                throw std::runtime_error("Output bitstream buffer must be 64-byte aligned.");
            }

            khcomp::ReadOnlyBuffer read_span(static_cast<const uint8_t*>(in_info.ptr), in_info.size * in_info.itemsize);
            khcomp::MutableBuffer write_span(static_cast<uint8_t*>(out_info.ptr), out_info.size * out_info.itemsize);

            khcomp::core::BitStreamWriter writer(write_span);
            auto res = self.encode_grayscale_frame(header, read_span, writer);

            if (!res.has_value()) {
                throw std::runtime_error("Image encoding failed with error code.");
            }
            return *res;
        })
        .def("decode_grayscale_frame", [](khcomp::image::ImageFramePipeline& self,
                                          const khcomp::image::ImageHeader& header,
                                          py::buffer input_bitstream_buf,
                                          py::buffer output_buf) -> size_t {
            py::buffer_info in_info = input_bitstream_buf.request();
            py::buffer_info out_info = output_buf.request();

            if (reinterpret_cast<uintptr_t>(in_info.ptr) % 64 != 0) {
                throw std::runtime_error("Input bitstream buffer must be 64-byte aligned.");
            }

            khcomp::ReadOnlyBuffer read_span(static_cast<const uint8_t*>(in_info.ptr), in_info.size * in_info.itemsize);
            khcomp::MutableBuffer write_span(static_cast<uint8_t*>(out_info.ptr), out_info.size * out_info.itemsize);

            khcomp::core::BitStreamReader reader(read_span);
            auto res = self.decode_grayscale_frame(header, reader, write_span);

            if (!res.has_value()) {
                throw std::runtime_error("Image decoding failed with error code.");
            }
            return *res;
        });

    // ==========================================
    // Video Engine Bindings
    // ==========================================
    py::enum_<khcomp::video::FrameType>(m, "FrameType")
        .value("IFrame", khcomp::video::FrameType::IFrame)
        .value("PFrame", khcomp::video::FrameType::PFrame)
        .export_values();

    py::class_<khcomp::video::VideoHeader>(m, "VideoHeader")
        .def(py::init<>())
        .def_readwrite("width", &khcomp::video::VideoHeader::width)
        .def_readwrite("height", &khcomp::video::VideoHeader::height)
        .def_readwrite("quality_factor", &khcomp::video::VideoHeader::quality_factor)
        .def_readwrite("search_window", &khcomp::video::VideoHeader::search_window);

    py::class_<khcomp::video::VideoRingBuffer>(m, "VideoRingBuffer")
        .def(py::init<>())
        .def("allocate", [](khcomp::video::VideoRingBuffer& self, uint16_t width, uint16_t height, py::buffer storage_buf) {
            py::buffer_info info = storage_buf.request();
            khcomp::MutableBuffer storage(static_cast<uint8_t*>(info.ptr), info.size * info.itemsize);
            auto res = self.allocate(width, height, storage);
            if (!res.has_value()) {
                throw std::runtime_error("VideoRingBuffer allocation failed.");
            }
        });

    py::class_<khcomp::video::VideoCodecEngine>(m, "VideoCodecEngine")
        .def(py::init<>())
        .def("encode_frame", [](khcomp::video::VideoCodecEngine& self,
                                const khcomp::video::VideoHeader& header,
                                khcomp::video::FrameType type,
                                py::buffer input_buf,
                                khcomp::video::VideoRingBuffer& ref_ring,
                                py::buffer output_bitstream_buf) -> size_t {
            py::buffer_info in_info = input_buf.request();
            py::buffer_info out_info = output_bitstream_buf.request();

            if (reinterpret_cast<uintptr_t>(out_info.ptr) % 64 != 0) {
                throw std::runtime_error("Output bitstream buffer must be 64-byte aligned.");
            }

            khcomp::ReadOnlyBuffer read_span(static_cast<const uint8_t*>(in_info.ptr), in_info.size * in_info.itemsize);
            khcomp::MutableBuffer write_span(static_cast<uint8_t*>(out_info.ptr), out_info.size * out_info.itemsize);

            khcomp::core::BitStreamWriter writer(write_span);
            auto res = self.encode_frame(header, type, read_span, ref_ring, writer);

            if (!res.has_value()) {
                throw std::runtime_error("Video frame encoding failed with error code.");
            }
            return *res;
        })
        .def("decode_frame", [](khcomp::video::VideoCodecEngine& self,
                                const khcomp::video::VideoHeader& header,
                                py::buffer input_bitstream_buf,
                                khcomp::video::VideoRingBuffer& ref_ring,
                                py::buffer output_buf) -> size_t {
            py::buffer_info in_info = input_bitstream_buf.request();
            py::buffer_info out_info = output_buf.request();

            if (reinterpret_cast<uintptr_t>(in_info.ptr) % 64 != 0) {
                throw std::runtime_error("Input bitstream buffer must be 64-byte aligned.");
            }

            khcomp::ReadOnlyBuffer read_span(static_cast<const uint8_t*>(in_info.ptr), in_info.size * in_info.itemsize);
            khcomp::MutableBuffer write_span(static_cast<uint8_t*>(out_info.ptr), out_info.size * out_info.itemsize);

            khcomp::core::BitStreamReader reader(read_span);
            auto res = self.decode_frame(header, reader, ref_ring, write_span);

            if (!res.has_value()) {
                throw std::runtime_error("Video frame decoding failed with error code.");
            }
            return *res;
        });

    // ==========================================
    // Audio Engine Bindings
    // ==========================================
    py::class_<khcomp::audio::AudioHeader>(m, "AudioHeader")
        .def(py::init<>())
        .def_readwrite("sample_rate", &khcomp::audio::AudioHeader::sample_rate)
        .def_readwrite("channels", &khcomp::audio::AudioHeader::channels)
        .def_readwrite("num_samples", &khcomp::audio::AudioHeader::num_samples);

    py::class_<khcomp::audio::AudioCodecEngine>(m, "AudioCodecEngine")
        .def(py::init<>())
        .def("encode_pcm16", [](khcomp::audio::AudioCodecEngine& self,
                                const khcomp::audio::AudioHeader& header,
                                py::buffer pcm_in,
                                py::buffer output_bitstream_buf) -> size_t {
            py::buffer_info in_info = pcm_in.request();
            py::buffer_info out_info = output_bitstream_buf.request();

            if (reinterpret_cast<uintptr_t>(out_info.ptr) % 64 != 0) {
                throw std::runtime_error("Output bitstream buffer must be 64-byte aligned.");
            }

            khcomp::ReadOnlyBuffer read_span(static_cast<const uint8_t*>(in_info.ptr), in_info.size * in_info.itemsize);
            khcomp::MutableBuffer write_span(static_cast<uint8_t*>(out_info.ptr), out_info.size * out_info.itemsize);

            khcomp::core::BitStreamWriter writer(write_span);
            auto res = self.encode_pcm16(header, read_span, writer);

            if (!res.has_value()) {
                throw std::runtime_error("Audio PCM16 encoding failed with error code.");
            }
            return *res;
        })
        .def("decode_pcm16", [](khcomp::audio::AudioCodecEngine& self,
                                const khcomp::audio::AudioHeader& header,
                                py::buffer input_bitstream_buf,
                                py::buffer pcm_out) -> size_t {
            py::buffer_info in_info = input_bitstream_buf.request();
            py::buffer_info out_info = pcm_out.request();

            if (reinterpret_cast<uintptr_t>(in_info.ptr) % 64 != 0) {
                throw std::runtime_error("Input bitstream buffer must be 64-byte aligned.");
            }

            khcomp::ReadOnlyBuffer read_span(static_cast<const uint8_t*>(in_info.ptr), in_info.size * in_info.itemsize);
            khcomp::MutableBuffer write_span(static_cast<uint8_t*>(out_info.ptr), out_info.size * out_info.itemsize);

            khcomp::core::BitStreamReader reader(read_span);
            auto res = self.decode_pcm16(header, reader, write_span);

            if (!res.has_value()) {
                throw std::runtime_error("Audio PCM16 decoding failed with error code.");
            }
            return *res;
        });
}