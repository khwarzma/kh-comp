#include <khcomp/bit_stream.hpp>
#include <khcomp/comp_engine.hpp>
#include <khcomp/context_model.hpp>
#include <khcomp/image_engine.hpp>
#include <khcomp/image_frame.hpp>
#include <khcomp/types.hpp>
#include <khcomp/utils.hpp>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

void test_alignment_enforcement() {
    alignas(64) uint8_t aligned_buf[128];
    uint8_t unaligned_buf[129];
    uint8_t* unaligned_ptr = unaligned_buf + 1; // Intentional misalignment

    khcomp::core::BitStreamWriter writer;
    
    // Aligned buffer set succeeds
    auto res1 = writer.set_buffer(khcomp::MutableBuffer(aligned_buf, sizeof(aligned_buf)));
    assert(res1.has_value());

    // Unaligned buffer set fails deterministically
    auto res2 = writer.set_buffer(khcomp::MutableBuffer(unaligned_ptr, 128));
    assert(!res2.has_value());
    assert(res2.error() == khcomp::CompressionError::AlignmentError);
    
    std::cout << "[PASS] Alignment Enforcement Test\n";
}

void test_bit_packing_and_reading() {
    alignas(64) uint8_t raw_buffer[256] = {0};
    khcomp::MutableBuffer mut_buf(raw_buffer, sizeof(raw_buffer));
    khcomp::ReadOnlyBuffer read_buf(raw_buffer, sizeof(raw_buffer));

    khcomp::core::BitStreamWriter writer(mut_buf);

    // Pack arbitrary bit patterns across non-byte boundaries
    assert(writer.write_bits(0b101, 3).has_value());         // 3 bits
    assert(writer.write_bits(0b11001, 5).has_value());       // 5 bits (1 byte total)
    assert(writer.write_bits(0xDEADBEEF, 32).has_value());   // 32 bits
    assert(writer.write_bits(0b1, 1).has_value());           // 1 bit
    assert(writer.flush().has_value());                      // Flush remaining tail

    khcomp::core::BitStreamReader reader(read_buf);

    auto val1 = reader.read_bits(3);
    assert(val1.has_value() && *val1 == 0b101);

    auto val2 = reader.read_bits(5);
    assert(val2.has_value() && *val2 == 0b11001);

    auto val3 = reader.read_bits(32);
    assert(val3.has_value() && *val3 == 0xDEADBEEF);

    auto val4 = reader.read_bits(1);
    assert(val4.has_value() && *val4 == 0b1);

    std::cout << "[PASS] Bit Packing and Reading Test\n";
}

void test_boundary_and_overflow_checks() {
    alignas(64) uint8_t raw_buffer[8] = {0}; // 64 bits capacity
    khcomp::MutableBuffer mut_buf(raw_buffer, sizeof(raw_buffer));
    khcomp::core::BitStreamWriter writer(mut_buf);

    assert(writer.write_bits(0xFFFFFFFFFFFFFFFFULL, 64).has_value());

    // Writing 1 additional bit must cause Overflow
    auto overflow_res = writer.write_bits(1, 1);
    assert(!overflow_res.has_value());
    assert(overflow_res.error() == khcomp::CompressionError::Overflow);

    std::cout << "[PASS] Boundary and Overflow Test\n";
}

void test_context_model_adaptation_and_normalization() {
    khcomp::core::ContextModel model(2);

    constexpr uint8_t target_symbol = 'A';

    // Prime the Order-2 history window so the context index stabilizes on [target_symbol, target_symbol]
    model.update(target_symbol);
    model.update(target_symbol);

    // Capture baseline stats for target_symbol within the stabilized context
    const auto stats_before = model.get_stats(target_symbol);
    assert(stats_before.low < stats_before.high);

    // Perform an adaptation update within the stabilized context
    model.update(target_symbol);

    // Query stats in the identical context state (since history window remains [target_symbol, target_symbol])
    const auto stats_after = model.get_stats(target_symbol);

    // Verify symbol frequency adaptation occurred
    assert(stats_after.total > stats_before.total);

    // Verify symbol decoding consistency against stabilized context bounds
    uint8_t decoded_sym = model.decode_symbol(stats_after.low);
    assert(decoded_sym == target_symbol);

    // Simulate high-frequency updates to trigger frequency normalization bounds [0, 2^16 - 1]
    for (size_t i = 0; i < 40000; ++i) {
        model.update(target_symbol);
    }

    assert(model.total_frequency() < khcomp::core::kMaxFrequency);

    std::cout << "[PASS] ContextModel Adaptation and Normalization Test\n";
}

void test_arithmetic_coder_roundtrip() {
    constexpr std::string_view payload = "نص تجريبي لاختبار كفاءة الضغط الترميزي في محرك خوارزمة - Khwarzma Comp 2026";

    alignas(64) uint8_t bitstream_buffer[4096] = {0};
    khcomp::MutableBuffer write_span(bitstream_buffer, sizeof(bitstream_buffer));
    khcomp::ReadOnlyBuffer read_span(bitstream_buffer, sizeof(bitstream_buffer));

    // 1. Encode Stream
    khcomp::core::BitStreamWriter writer(write_span);
    khcomp::core::ContextModel enc_model(2);
    khcomp::core::ArithmeticEncoder encoder(&writer);

    for (char c : payload) {
        auto res = encoder.encode_symbol(static_cast<uint8_t>(c), enc_model);
        assert(res.has_value());
    }
    assert(encoder.flush().has_value());

    // 2. Decode Stream
    khcomp::core::BitStreamReader reader(read_span);
    khcomp::core::ContextModel dec_model(2);
    khcomp::core::ArithmeticDecoder decoder;
    assert(decoder.set_reader(&reader).has_value());

    std::vector<char> reconstructed;
    reconstructed.reserve(payload.size());

    for (size_t i = 0; i < payload.size(); ++i) {
        auto sym = decoder.decode_symbol(dec_model);
        assert(sym.has_value());
        reconstructed.push_back(static_cast<char>(*sym));
    }

    // 3. Verify Lossless Roundtrip Guarantee
    std::string_view reconstructed_view(reconstructed.data(), reconstructed.size());
    assert(payload == reconstructed_view);

    std::cout << "[PASS] ArithmeticCoder Lossless Roundtrip Test\n";
}

void test_image_dct_quantization_roundtrip() {
    khcomp::image::DctQuantEngine engine(90); // High quality setting

    // 8x8 input block sample (Luminance values centered around 0 [-128, 127])
    alignas(64) std::array<float, 64> original_block = {
        52.0f, 55.0f, 61.0f, 66.0f, 70.0f, 61.0f, 64.0f, 73.0f,
        63.0f, 59.0f, 55.0f, 90.0f, 109.0f, 85.0f, 69.0f, 72.0f,
        62.0f, 59.0f, 68.0f, 113.0f, 144.0f, 104.0f, 66.0f, 73.0f,
        63.0f, 58.0f, 71.0f, 122.0f, 154.0f, 106.0f, 70.0f, 69.0f,
        67.0f, 61.0f, 68.0f, 104.0f, 126.0f, 88.0f, 68.0f, 70.0f,
        79.0f, 65.0f, 60.0f, 70.0f, 77.0f, 68.0f, 58.0f, 75.0f,
        85.0f, 71.0f, 64.0f, 59.0f, 55.0f, 61.0f, 65.0f, 83.0f,
        87.0f, 79.0f, 69.0f, 68.0f, 65.0f, 76.0f, 78.0f, 94.0f
    };

    alignas(64) std::array<float, 64> dct_coeffs{};
    alignas(64) std::array<int16_t, 64> quant_coeffs{};
    alignas(64) std::array<int16_t, 64> zigzag_serialized{};
    alignas(64) std::array<int16_t, 64> zigzag_deserialized{};
    alignas(64) std::array<float, 64> dequant_coeffs{};
    alignas(64) std::array<float, 64> reconstructed_block{};

    // 1. Forward DCT with LUT
    assert(engine.forward_dct(original_block, dct_coeffs).has_value());

    // 2. Quantization
    assert(engine.quantize(dct_coeffs, quant_coeffs).has_value());

    // 3. Zig-zag Serialization -> Deserialization
    assert(engine.zigzag_serialize(quant_coeffs, zigzag_serialized).has_value());
    assert(engine.zigzag_deserialize(zigzag_serialized, zigzag_deserialized).has_value());

    for (size_t i = 0; i < 64; ++i) {
        assert(quant_coeffs[i] == zigzag_deserialized[i]);
    }

    // 4. Dequantization
    assert(engine.dequantize(zigzag_deserialized, dequant_coeffs).has_value());

    // 5. Inverse DCT with LUT
    assert(engine.inverse_dct(dequant_coeffs, reconstructed_block).has_value());

    // 6. Signal Fidelity Verification (Root Mean Square Error under Quality Factor 90)
    float max_error = 0.0f;
    float sum_squared_error = 0.0f;

    for (size_t i = 0; i < 64; ++i) {
        const float err = std::abs(original_block[i] - reconstructed_block[i]);
        sum_squared_error += err * err;
        if (err > max_error) {
            max_error = err;
        }
    }

    const float rmse = std::sqrt(sum_squared_error / 64.0f);

    // Under Q=90, RMSE must remain exceptionally low (< 3.5 intensity levels)
    assert(rmse < 3.5f);

    std::cout << "[PASS] Image DCT & Quantization Roundtrip Test (RMSE: " << rmse << ", Max Error: " << max_error << ")\n";
}

void test_image_frame_end_to_end_compression() {
    constexpr uint16_t width = 16;
    constexpr uint16_t height = 16;
    constexpr size_t total_pixels = width * height;

    alignas(64) std::array<uint8_t, total_pixels> original_frame{};
    alignas(64) std::array<uint8_t, total_pixels> reconstructed_frame{};
    alignas(64) std::array<uint8_t, 4096> compressed_bitstream{};

    // Generate deterministic spatial gradient test image pattern
    for (uint16_t y = 0; y < height; ++y) {
        for (uint16_t x = 0; x < width; ++x) {
            original_frame[y * width + x] = static_cast<uint8_t>((x * 12 + y * 8) % 256);
        }
    }

    khcomp::image::ImageHeader header{
        .width = width,
        .height = height,
        .format = khcomp::image::PixelFormat::Grayscale,
        .quality_factor = 85
    };

    khcomp::image::ImageFramePipeline pipeline;

    // 1. Encode Grayscale Image Frame -> Arithmetic Bitstream
    khcomp::core::BitStreamWriter writer(khcomp::MutableBuffer(compressed_bitstream.data(), compressed_bitstream.size()));
    auto enc_res = pipeline.encode_grayscale_frame(header, khcomp::ReadOnlyBuffer(original_frame.data(), original_frame.size()), writer);
    assert(enc_res.has_value());

    const size_t compressed_bytes = *enc_res;
    assert(compressed_bytes > 0);

    // 2. Decode Arithmetic Bitstream -> Reconstructed Grayscale Frame
    khcomp::core::BitStreamReader reader(khcomp::ReadOnlyBuffer(compressed_bitstream.data(), compressed_bitstream.size()));
    auto dec_res = pipeline.decode_grayscale_frame(header, reader, khcomp::MutableBuffer(reconstructed_frame.data(), reconstructed_frame.size()));
    assert(dec_res.has_value());

    // 3. Compute Frame Fidelity (PSNR & Peak Error)
    double mse = 0.0;
    double max_pixel_err = 0.0;

    for (size_t i = 0; i < total_pixels; ++i) {
        const double err = std::abs(static_cast<double>(original_frame[i]) - static_cast<double>(reconstructed_frame[i]));
        mse += err * err;
        if (err > max_pixel_err) {
            max_pixel_err = err;
        }
    }
    mse /= static_cast<double>(total_pixels);

    const double psnr = (mse > 0.0) ? (10.0 * std::log10((255.0 * 255.0) / mse)) : 99.0;

    // PSNR for Q=85 spatial gradient image frame should be > 32 dB
    assert(psnr > 32.0);

    std::cout << "[PASS] Image Frame End-to-End Test (Compressed: " << compressed_bytes 
              << " bytes, PSNR: " << psnr << " dB, Max Pixel Error: " << max_pixel_err << ")\n";
}

int main() {
    std::cout << "Running kh-comp Comprehensive Unit Tests...\n";
    test_alignment_enforcement();
    test_bit_packing_and_reading();
    test_boundary_and_overflow_checks();
    test_context_model_adaptation_and_normalization();
    test_arithmetic_coder_roundtrip();
    test_image_dct_quantization_roundtrip();
    test_image_frame_end_to_end_compression();
    std::cout << "All Unit Tests Passed Successfully.\n";
    return 0;
}