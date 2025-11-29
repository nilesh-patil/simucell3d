#ifndef DEF_MORTON_CODE
#define DEF_MORTON_CODE

#include <cstdint>
#include "vec3.hpp"

/*
    Morton code (Z-order curve) encoding for spatial sorting.

    Morton codes interleave the bits of x, y, z coordinates to create a single
    64-bit value. Objects with similar Morton codes are spatially close, which
    improves cache locality when iterating through objects in spatial order.

    Bit interleaving pattern (for 21 bits per dimension = 63 total bits):
    - x bits go to positions: 0, 3, 6, 9, 12, ...
    - y bits go to positions: 1, 4, 7, 10, 13, ...
    - z bits go to positions: 2, 5, 8, 11, 14, ...

    This creates a space-filling curve that preserves locality.
*/

class morton_code {
public:

    //-------------------------------------------------------------------------------
    // Encode normalized coordinates [0,1] to Morton code
    // Each coordinate is quantized to 21 bits (2^21 = 2M levels)
    //-------------------------------------------------------------------------------
    static uint64_t encode_normalized(double nx, double ny, double nz) noexcept {
        // Clamp to [0, 1] range
        if (nx < 0.0) nx = 0.0;
        if (ny < 0.0) ny = 0.0;
        if (nz < 0.0) nz = 0.0;
        if (nx > 1.0) nx = 1.0;
        if (ny > 1.0) ny = 1.0;
        if (nz > 1.0) nz = 1.0;

        // Quantize to 21-bit integers (0 to 2097151)
        constexpr uint64_t max_val = (1ULL << 21) - 1;
        const uint64_t ix = static_cast<uint64_t>(nx * max_val);
        const uint64_t iy = static_cast<uint64_t>(ny * max_val);
        const uint64_t iz = static_cast<uint64_t>(nz * max_val);

        // Interleave bits to form Morton code
        return spread_bits(ix) | (spread_bits(iy) << 1) | (spread_bits(iz) << 2);
    }

    //-------------------------------------------------------------------------------
    // Encode a 3D position within given bounds to Morton code
    // bounds array: [min_x, min_y, min_z, max_x, max_y, max_z]
    //-------------------------------------------------------------------------------
    static uint64_t encode(const vec3& pos, const double* bounds) noexcept {
        // Normalize position to [0, 1] within bounds
        const double range_x = bounds[3] - bounds[0];
        const double range_y = bounds[4] - bounds[1];
        const double range_z = bounds[5] - bounds[2];

        // Avoid division by zero
        const double nx = (range_x > 0.0) ? (pos.dx() - bounds[0]) / range_x : 0.0;
        const double ny = (range_y > 0.0) ? (pos.dy() - bounds[1]) / range_y : 0.0;
        const double nz = (range_z > 0.0) ? (pos.dz() - bounds[2]) / range_z : 0.0;

        return encode_normalized(nx, ny, nz);
    }

private:
    //-------------------------------------------------------------------------------
    // Spread 21 bits across 63 positions (every 3rd bit)
    // Uses magic numbers for efficient bit manipulation
    // https://graphics.stanford.edu/~seander/bithacks.html#InterleaveBMN
    //-------------------------------------------------------------------------------
    static uint64_t spread_bits(uint64_t v) noexcept {
        // Mask to 21 bits
        v &= 0x1FFFFF;

        // Spread bits using magic number sequence
        // Each step doubles the gaps between bits

        // Step 1: v = ---- ---- ---a bcde fghi jklm nopq rstu
        // to:      v = ---- --ab cdef ghij klmn opqr ----  ---- ---- ---- stuv
        v = (v | v << 32) & 0x001F00000000FFFF;

        // Step 2: spread 8 bits each
        v = (v | v << 16) & 0x001F0000FF0000FF;

        // Step 3: spread 4 bits each
        v = (v | v << 8)  & 0x100F00F00F00F00F;

        // Step 4: spread 2 bits each
        v = (v | v << 4)  & 0x10C30C30C30C30C3;

        // Step 5: spread 1 bit each (final interleaving)
        v = (v | v << 2)  & 0x1249249249249249;

        return v;
    }
};

#endif
