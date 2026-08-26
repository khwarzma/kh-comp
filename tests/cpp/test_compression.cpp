#include <khcomp/bit_stream.hpp>
#include <khcomp/comp_engine.hpp>
#include <khcomp/context_model.hpp>
#include <khcomp/types.hpp>
#include <khcomp/utils.hpp>

#include <cassert>
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

int main() {
    std::cout << "Running kh-comp Phase 1 & 2 Infrastructure Unit Tests...\n";
    test_alignment_enforcement();
    test_bit_packing_and_reading();
    test_boundary_and_overflow_checks();
    test_context_model_adaptation_and_normalization();
    test_arithmetic_coder_roundtrip();
    std::cout << "All Unit Tests Passed Successfully.\n";
    return 0;
}