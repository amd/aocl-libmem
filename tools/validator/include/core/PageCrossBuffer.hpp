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

#ifndef LIBMEM_VALIDATOR_PAGE_CROSS_BUFFER_HPP
#define LIBMEM_VALIDATOR_PAGE_CROSS_BUFFER_HPP

#include "config/Constants.hpp"
#include <sys/mman.h>
#include <cstdlib>

namespace libmem {
namespace validator {

/**
 * PageCrossBuffer - RAII wrapper for page boundary testing
 *
 * Allocates multiple page-aligned pages with a guard page at the end.
 * Useful for testing code that must cross page boundaries during iteration.
 *
 * Memory layout:
 *   [Page 0][Page 1]...[Page N-1][GUARD PAGE]
 *   └─────── usable memory ──────┘└─ PROT_NONE ─┘
 *
 * The guard page (PROT_NONE) causes SEGFAULT if code reads/writes beyond bounds.
 */
class PageCrossBuffer {
public:
    PageCrossBuffer() : base_(nullptr), total_pages_(0), valid_(false) {}

    ~PageCrossBuffer() {
        cleanup();
    }

    // Non-copyable
    PageCrossBuffer(const PageCrossBuffer&) = delete;
    PageCrossBuffer& operator=(const PageCrossBuffer&) = delete;

    // Movable
    PageCrossBuffer(PageCrossBuffer&& other) noexcept
        : base_(other.base_), total_pages_(other.total_pages_), valid_(other.valid_) {
        other.base_ = nullptr;
        other.valid_ = false;
    }

    PageCrossBuffer& operator=(PageCrossBuffer&& other) noexcept {
        if (this != &other) {
            cleanup();
            base_ = other.base_;
            total_pages_ = other.total_pages_;
            valid_ = other.valid_;
            other.base_ = nullptr;
            other.valid_ = false;
        }
        return *this;
    }

    /**
     * Allocate memory for page-cross testing
     *
     * @param size Minimum usable size needed
     * @return true if allocation and protection succeeded
     */
    bool allocate(size_t size) {
        cleanup();

        // Calculate pages: enough for size + 1 extra page for offset flexibility + 1 guard
        size_t pages_for_size = (size + PAGE_SZ - 1) / PAGE_SZ;
        total_pages_ = pages_for_size + 2;  // +1 for start offset, +1 for guard

        size_t alloc_size = total_pages_ * PAGE_SZ;

        if (posix_memalign(reinterpret_cast<void**>(&base_), PAGE_SZ, alloc_size) != 0) {
            return false;
        }

        // Protect the last page
        void* guard = base_ + (total_pages_ - 1) * PAGE_SZ;
        if (mprotect(guard, PAGE_SZ, PROT_NONE) != 0) {
            std::free(base_);
            base_ = nullptr;
            return false;
        }

        valid_ = true;
        return true;
    }

    /**
     * Get base pointer to usable memory
     */
    uint8_t* base() const { return valid_ ? base_ : nullptr; }

    /**
     * Get usable size (excludes guard page)
     */
    size_t usableSize() const { return valid_ ? (total_pages_ - 1) * PAGE_SZ : 0; }

    /**
     * Get address that starts 'offset' bytes before a page boundary
     * Useful for testing strings that CROSS page boundaries
     *
     * @param offset Bytes before the first internal page boundary
     * @return Pointer positioned offset bytes before PAGE_SZ boundary
     */
    uint8_t* getAddressNearBoundary(uint32_t offset) const {
        if (!valid_ || offset >= PAGE_SZ) return nullptr;
        return base_ + PAGE_SZ - offset;
    }

    bool isValid() const { return valid_; }

private:
    void cleanup() {
        if (base_) {
            void* guard = base_ + (total_pages_ - 1) * PAGE_SZ;
            mprotect(guard, PAGE_SZ, PROT_READ | PROT_WRITE);
            std::free(base_);
            base_ = nullptr;
        }
        valid_ = false;
    }

    uint8_t* base_;
    size_t total_pages_;
    bool valid_;
};

} // namespace validator
} // namespace libmem

#endif // LIBMEM_VALIDATOR_PAGE_CROSS_BUFFER_HPP
