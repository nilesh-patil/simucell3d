#ifndef DEF_MORTON_CODE
#define DEF_MORTON_CODE

/**
 * @file morton_code.hpp
 * @brief Morton (Z-order) codes of 3D positions.
 * @ingroup spatial_partitioning
 */

#include <cstdint>
#include "vec3.hpp"

/**
 * @brief Morton (Z-order curve) encoding of 3D positions.
 *
 * Each coordinate is quantized to 21 bits and the bits are interleaved into a 63-bit code:
 * x bits go to positions 0, 3, 6, ..., y bits to 1, 4, 7, ..., z bits to 2, 5, 8, ....
 * Sorting objects by their code places nearby objects close together in memory. The USPG
 * contact detection strategy sorts faces this way before inserting them into its grid.
 *
 * @ingroup spatial_partitioning
 */
class morton_code {
public:

    /**
     * @brief Return the Morton code of a point given in normalized coordinates.
     * @param nx,ny,nz Coordinates in [0, 1]; values outside are clamped. Each is quantized to
     *        21 bits.
     * @return The 63-bit interleaved code.
     */
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

    /**
     * @brief Return the Morton code of a position within a box.
     * @param pos    Position, m.
     * @param bounds Box as six values {min_x, min_y, min_z, max_x, max_y, max_z}, m. An axis of
     *               zero extent maps to coordinate 0.
     * @return The 63-bit interleaved code.
     */
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
