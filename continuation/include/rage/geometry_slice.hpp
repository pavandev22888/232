#ifndef RAGE_GEOMETRY_SLICE_HPP
#define RAGE_GEOMETRY_SLICE_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace rage::offline {

// Minimal observed spans, not complete original object types or a callee ABI.
inline constexpr std::size_t geometry_postcall_bytes = 0x21;
inline constexpr std::size_t geometry_output_bytes = 0x38;
using GeometryPostCallBytes = std::array<std::uint8_t, geometry_postcall_bytes>;
using GeometryOutputBytes = std::array<std::uint8_t, geometry_output_bytes>;

// observation starts at original EBP-0x4c; output starts at original EDI.
// Requires disjoint snapshots and the original SSE floating-point environment.
[[nodiscard]] std::int32_t consume_geometry_postcall(
    const GeometryPostCallBytes& observation,
    GeometryOutputBytes& output) noexcept;

} // namespace rage::offline

#endif
