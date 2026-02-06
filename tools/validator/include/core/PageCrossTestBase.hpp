/* Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LIBMEM_VALIDATOR_PAGE_CROSS_TEST_BASE_HPP
#define LIBMEM_VALIDATOR_PAGE_CROSS_TEST_BASE_HPP

/**
 * @file PageCrossTestBase.hpp
 * @brief Base class for comprehensive page boundary testing
 *
 * Provides common infrastructure for testing functions with:
 * - Multi-page spanning buffers
 * - Full byte range (0-255) to catch signed comparison bugs
 * - Multiple alignment combinations
 * - Positions at/near page boundaries
 */

#include "core/FunctionTest.hpp"
#include "core/PageCrossBuffer.hpp"
#include <cstdlib>
#include <vector>

namespace libmem {
namespace validator {

/**
 * PageCrossTestBase - Base class for comprehensive page boundary testing
 *
 * Provides:
 *  - Standard test loop over sizes, offsets, and positions
 *  - Position generation based on size and page boundaries
 *  - Alignment offset combinations
 *  - Deterministic random seeding
 *
 * Subclasses implement:
 *  - runPageCrossTest() - Actual function-specific test logic
 *  - Optionally override minTestSize(), requiresDifferentOffsets(), usePositions()
 */
/**
 * BufferSetupResult - Result of buffer allocation and positioning
 */
struct BufferSetupResult {
    PageCrossBuffer buffer;
    uint8_t* ptr;
    bool valid;
    bool should_skip;

    BufferSetupResult() : ptr(nullptr), valid(false), should_skip(false) {}

    static BufferSetupResult success(PageCrossBuffer&& buf, uint8_t* p) {
        BufferSetupResult r;
        r.buffer = std::move(buf);
        r.ptr = p;
        r.valid = true;
        r.should_skip = false;
        return r;
    }

    static BufferSetupResult skip() {
        BufferSetupResult r;
        r.should_skip = true;
        return r;
    }

    static BufferSetupResult failure() {
        BufferSetupResult r;
        r.valid = false;
        r.should_skip = false;
        return r;
    }
};

template<typename Tag>
class PageCrossTestBase : public FunctionTest<Tag> {
public:
    /**
     * Test name - all page-cross tests share this name
     * Combined with function name: e.g., "memcpy:PageCross"
     */
    const char* name() const override { return "PageCross"; }

    TestResult execute(const TestContext& ctx) override {
        this->ctx_ = ctx;
        const size_t size = ctx.size;

        if (size < minTestSize()) {
            return TestResult::success();
        }

        // Get test positions (for tests that use position parameter)
        auto positions = usePositions() ? getTestPositions(size) : std::vector<size_t>{0};

        // Test various alignment combinations
        for (uint32_t off1 : START_OFFSETS) {
            for (uint32_t off2 : START_OFFSETS) {
                // Skip same offsets if different alignments required
                if (requiresDifferentOffsets() && off1 == off2) continue;

                for (size_t pos : positions) {
                    // Skip invalid positions
                    if (usePositions() && pos >= size - 1) continue;

                    auto result = runPageCrossTest(size, pos, off1, off2);
                    if (!result.passed) {
                        return result;
                    }
                }
            }
        }

        return TestResult::success();
    }

protected:
    // ========================================================================
    // Subclasses override these
    // ========================================================================

    /**
     * Minimum size to run this test (default: 8 bytes)
     */
    virtual size_t minTestSize() const { return 8; }

    /**
     * Whether test requires different offsets for buf1 vs buf2
     * (e.g., compare tests need different alignments to trigger misaligned path)
     */
    virtual bool requiresDifferentOffsets() const { return false; }

    /**
     * Whether test uses position parameter (e.g., mismatch position, search position)
     * If false, position loop runs once with pos=0
     */
    virtual bool usePositions() const { return true; }

    /**
     * Run the page-cross test for a specific scenario - subclasses MUST implement
     *
     * @param size Buffer/string size to test
     * @param pos Position parameter (mismatch pos, search pos, null pos, etc.)
     * @param off1 Offset from page boundary for buffer 1
     * @param off2 Offset from page boundary for buffer 2
     * @return TestResult indicating pass/fail
     */
    virtual TestResult runPageCrossTest(size_t size, size_t pos,
                                         uint32_t off1, uint32_t off2) = 0;

    // ========================================================================
    // Common utilities for subclasses
    // ========================================================================

    /**
     * Get test positions based on size
     * Includes early positions, page boundary positions, and near-end position
     */
    std::vector<size_t> getTestPositions(size_t size) const {
        std::vector<size_t> positions = {4, 16, 32};

        // Add positions around first page boundary
        if (size > PAGE_SZ) {
            positions.push_back(PAGE_SZ - 6);
            positions.push_back(PAGE_SZ - 1);
            positions.push_back(PAGE_SZ);
            positions.push_back(PAGE_SZ + 1);
            positions.push_back(PAGE_SZ + 100);
        }

        // Add positions around second page boundary
        if (size > 2 * PAGE_SZ) {
            positions.push_back(2 * PAGE_SZ - 1);
            positions.push_back(2 * PAGE_SZ);
            positions.push_back(2 * PAGE_SZ + 1);
        }

        // Add position near end
        if (size > 64) {
            positions.push_back(size - 10);
        }

        return positions;
    }

    /**
     * Seed random number generator deterministically
     * Ensures reproducible test results
     */
    void seedRandom(size_t size, size_t pos, uint32_t off1, uint32_t off2) const {
        std::srand(static_cast<unsigned>(size ^ pos ^ off1 ^ off2));
    }

    /**
     * Setup a buffer with proper allocation and positioning
     *
     * @param size Size needed for the buffer
     * @param offset Offset from page boundary
     * @return BufferSetupResult with buffer and positioned pointer
     */
    BufferSetupResult setupBuffer(size_t size, uint32_t offset) {
        PageCrossBuffer buf;

        if (!buf.allocate(size)) {
            return BufferSetupResult::failure();
        }

        uint8_t* ptr = buf.getAddressNearBoundary(offset);

        if (!ptr || ptr + size > buf.base() + buf.usableSize()) {
            return BufferSetupResult::skip();
        }

        return BufferSetupResult::success(std::move(buf), ptr);
    }

    /**
     * Check if a pointer with given size fits within buffer bounds
     *
     * @param ptr Pointer to check
     * @param size Size needed
     * @param buf Buffer to check against
     * @return true if fits, false if should skip
     */
    bool fitsInBuffer(uint8_t* ptr, size_t size, const PageCrossBuffer& buf) const {
        return ptr && (ptr + size <= buf.base() + buf.usableSize());
    }

    /**
     * Start offsets from page boundary to test
     * Covers various vector alignments (1, 16, 31, 32, 48, 63, 64)
     */
    static constexpr uint32_t START_OFFSETS[] = {1, 16, 31, 32, 48, 63, 64};
    static constexpr size_t NUM_OFFSETS = sizeof(START_OFFSETS) / sizeof(START_OFFSETS[0]);
};

// Define static member
template<typename Tag>
constexpr uint32_t PageCrossTestBase<Tag>::START_OFFSETS[];

} // namespace validator
} // namespace libmem

#endif // LIBMEM_VALIDATOR_PAGE_CROSS_TEST_BASE_HPP
