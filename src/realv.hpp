/*****************************************************************************

YASK: Yet Another Stencil Kernel
Copyright (c) 2014-2016, Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

* The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

*****************************************************************************/

// This file defines a union to use for folded vectors of floats or doubles.

#ifndef _REAL_VEC_H
#define _REAL_VEC_H

// Control assert() by turning on with DEBUG instead of turning off with
// NDEBUG. This makes it off by default.
#ifndef DEBUG
#define NDEBUG
#endif

#include <cstdint>
#include <iostream>
#include <string>
#include <stdexcept>

#include <assert.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <immintrin.h>

#ifdef _WIN32
typedef uint16_t __mmask16;
#else
#include <unistd.h>
#endif

// safe integer divide and mod.
#include "idiv.hpp"

// macros for 1D<->nD transforms.
#include "layout_macros.hpp"

namespace yask {

    // Type to use for indexing grids and real_vec_t elements.
    // Must be signed to allow negative indices in halos.
    typedef int64_t idx_t;

    // values for 32-bit, single-precision reals.
#if REAL_BYTES == 4
    typedef float real_t;
    typedef ::uint32_t ctrl_t;
    const ctrl_t ctrl_idx_mask = 0xf;
    const ctrl_t ctrl_sel_bit = 0x10;
    typedef __mmask16 real_mask_t;
#ifdef USE_INTRIN256
    const idx_t vec_elems = 8;
    typedef float imem_t;
#define VEC_ELEMS 8
#define INAME(op) _mm256_ ## op ## _ps
#define INAMEI(op) _mm256_ ## op ## _epi32
#elif defined(USE_INTRIN512)
    const idx_t vec_elems = 16;
    typedef void imem_t;
#define VEC_ELEMS 16
#define INAME(op) _mm512_ ## op ## _ps
#define INAMEI(op) _mm512_ ## op ## _epi32
#endif

    // values for 64-bit, double-precision reals.
#elif REAL_BYTES == 8
    typedef double real_t;
    typedef ::uint64_t ctrl_t;
    const ctrl_t ctrl_idx_mask = 0x7;
    const ctrl_t ctrl_sel_bit = 0x8;
    typedef __mmask8 real_mask_t;
#ifdef USE_INTRIN256
    const idx_t vec_elems = 4;
    typedef double imem_t;
#define VEC_ELEMS 4
#define INAME(op) _mm256_ ## op ## _pd
#define INAMEI(op) _mm256_ ## op ## _epi64
#elif defined(USE_INTRIN512)
    const idx_t vec_elems = 8;
    typedef void imem_t;
#define VEC_ELEMS 8
#define INAME(op) _mm512_ ## op ## _pd
#define INAMEI(op) _mm512_ ## op ## _epi64
#endif

#else
#error "REAL_BYTES not set to 4 or 8"
#endif

    // Vector sizes.
    // This are defaults to override those generated by foldBuilder
    // in stencil_macros.hpp.
#ifndef VLEN_T
#define VLEN_T (1)
#endif
#ifndef VLEN_N
#define VLEN_N (1)
#endif
#ifndef VLEN_X
#define VLEN_X (1)
#endif
#ifndef VLEN_Y
#define VLEN_Y (1)
#endif
#ifndef VLEN_Z
#define VLEN_Z (1)
#endif
#ifndef VLEN
#define VLEN ((VLEN_T) * (VLEN_N) * (VLEN_X) * (VLEN_Y) * (VLEN_Z))
#endif

#if VLEN_T != 1
#error "Vector folding in time dimension not currently supported."
#endif

    // Emulate instrinsics for unsupported VLEN.
    // Only 256 and 512-bit vectors supported.
    // VLEN == 1 also supported as scalar.
#if VLEN == 1
#define NO_INTRINSICS
    // note: no warning here because intrinsics aren't wanted in this case.

#elif !defined(INAME)
#warning "Emulating intrinsics because HW vector length not defined; set USE_INTRIN256 or USE_INTRIN512"
#define NO_INTRINSICS

#elif VLEN != VEC_ELEMS
#warning "Emulating intrinsics because VLEN != HW vector length"
#define NO_INTRINSICS
#endif

#undef VEC_ELEMS

    // Macro for looping through an aligned real_vec_t.
#if defined(DEBUG) || (VLEN==1) || !defined(__INTEL_COMPILER) 
#define REAL_VEC_LOOP(i)                        \
    for (int i=0; i<VLEN; i++)
#define REAL_VEC_LOOP_UNALIGNED(i)              \
    for (int i=0; i<VLEN; i++)
#else
#define REAL_VEC_LOOP(i)                                                \
    _Pragma("vector aligned") _Pragma("vector always") _Pragma("simd")  \
    for (int i=0; i<VLEN; i++)
#define REAL_VEC_LOOP_UNALIGNED(i)              \
    _Pragma("vector always") _Pragma("simd")    \
    for (int i=0; i<VLEN; i++)
#endif

    // conditional inlining
#if defined(DEBUG) || defined(_WIN32)
#define ALWAYS_INLINE inline
#else
#define ALWAYS_INLINE __attribute__((always_inline)) inline
#endif

    // The following union is used to overlay C arrays with vector types.
    // It must be an aggregate type to allow aggregate initialization,
    // so no user-provided ctors, copy operator, virtual member functions, etc.
    // These features are in real_vec_t, which contains a real_vec_t_data.
    union real_vec_t_data {
        real_t r[VLEN];
        ctrl_t ci[VLEN];

#ifndef NO_INTRINSICS
    
        // 32-bit integer vector overlay.
#if defined(USE_INTRIN256)
        __m256i mi;
#elif defined(USE_INTRIN512)
        __m512i mi;
#endif

        // real vector.
#if REAL_BYTES == 4 && defined(USE_INTRIN256)
        __m256  mr;
#elif REAL_BYTES == 4 && defined(USE_INTRIN512)
        __m512  mr;
#elif REAL_BYTES == 8 && defined(USE_INTRIN256)
        __m256d mr;
#elif REAL_BYTES == 8 && defined(USE_INTRIN512)
        __m512d mr;
#endif
#endif
    };
  

    // Type for a vector block.
    // The union 'u' is a 4-dimensional "folded" vector
    // of size VLEN_N * VLEN_X * VLEN_Y * VLEN_Z.
    struct real_vec_t {

        // union of data types.
        real_vec_t_data u;

        // default ctor does not init data!
        ALWAYS_INLINE real_vec_t() {}

        // copy vector.
        ALWAYS_INLINE real_vec_t(const real_vec_t& val) {
            operator=(val);
        }
        ALWAYS_INLINE real_vec_t(const real_vec_t_data& val) {
            operator=(val);
        }
    
        // broadcast scalar.
        ALWAYS_INLINE real_vec_t(float val) {
            operator=(val);
        }
        ALWAYS_INLINE real_vec_t(double val) {
            operator=(val);
        }
        ALWAYS_INLINE real_vec_t(int val) {
            operator=(val);
        }
        ALWAYS_INLINE real_vec_t(long val) {
            operator=(val);
        }

        // copy whole vector.
        ALWAYS_INLINE real_vec_t& operator=(const real_vec_t& rhs) {
#ifdef NO_INTRINSICS
            REAL_VEC_LOOP(i) u.r[i] = rhs[i];
#else
            u.mr = rhs.u.mr;
#endif
            return *this;
        }
        ALWAYS_INLINE real_vec_t& operator=(const real_vec_t_data& rhs) {
            u = rhs;
            return *this;
        }

        // assignment: single value broadcast.
        ALWAYS_INLINE void operator=(double val) {
#ifdef NO_INTRINSICS
            REAL_VEC_LOOP(i) u.r[i] = real_t(val);
#else
            u.mr = INAME(set1)(real_t(val));
#endif
        }
        ALWAYS_INLINE void operator=(float val) {
#ifdef NO_INTRINSICS
            REAL_VEC_LOOP(i) u.r[i] = real_t(val);
#else
            u.mr = INAME(set1)(real_t(val));
#endif
        }

        // broadcast with conversions.
        ALWAYS_INLINE void operator=(int val) {
            operator=(real_t(val));
        }
        ALWAYS_INLINE void operator=(long val) {
            operator=(real_t(val));
        }

    
        // access a real_t linearly.
        ALWAYS_INLINE real_t& operator[](idx_t l) {
            assert(l >= 0);
            assert(l < VLEN);
            return u.r[l];
        }
        ALWAYS_INLINE const real_t& operator[](idx_t l) const {
            assert(l >= 0);
            assert(l < VLEN);
            return u.r[l];
        }

        // access a real_t by n,x,y,z vector-block indices.
        ALWAYS_INLINE const real_t& operator()(idx_t n, idx_t i, idx_t j, idx_t k) const {
            assert(n >= 0);
            assert(n < VLEN_N);
            assert(i >= 0);
            assert(i < VLEN_X);
            assert(j >= 0);
            assert(j < VLEN_Y);
            assert(k >= 0);
            assert(k < VLEN_Z);

#if VLEN_FIRST_DIM_IS_UNIT_STRIDE

            // n dim is unit stride, followed by x, y, z.
            idx_t l = LAYOUT_4321(n, i, j, k, VLEN_N, VLEN_X, VLEN_Y, VLEN_Z);
#else

            // z dim is unit stride, followed by y, x, n.
            idx_t l = LAYOUT_1234(n, i, j, k, VLEN_N, VLEN_X, VLEN_Y, VLEN_Z);
#endif
        
            return u.r[l];
        }
        ALWAYS_INLINE real_t& operator()(idx_t n, idx_t i, idx_t j, idx_t k) {
            const real_vec_t* ct = const_cast<const real_vec_t*>(this);
            const real_t& cr = (*ct)(n, i, j, k);
            return const_cast<real_t&>(cr);
        }

        // unary negate.
        ALWAYS_INLINE real_vec_t operator-() const {
            real_vec_t res;
#ifdef NO_INTRINSICS
            REAL_VEC_LOOP(i) res[i] = -u.r[i];
#else
            // subtract from zero.
            res.u.mr = INAME(sub)(INAME(setzero)(), u.mr);
#endif
            return res;
        }

        // add.
        ALWAYS_INLINE real_vec_t operator+(real_vec_t rhs) const {
            real_vec_t res;
#ifdef NO_INTRINSICS
            REAL_VEC_LOOP(i) res[i] = u.r[i] + rhs[i];
#else
            res.u.mr = INAME(add)(u.mr, rhs.u.mr);
#endif
            return res;
        }
        ALWAYS_INLINE real_vec_t operator+(real_t rhs) const {
            real_vec_t rn;
            rn = rhs;               // broadcast.
            return (*this) + rn;
        }

        // sub.
        ALWAYS_INLINE real_vec_t operator-(real_vec_t rhs) const {
            real_vec_t res;
#ifdef NO_INTRINSICS
            REAL_VEC_LOOP(i) res[i] = u.r[i] - rhs[i];
#else
            res.u.mr = INAME(sub)(u.mr, rhs.u.mr);
#endif
            return res;
        }
        ALWAYS_INLINE real_vec_t operator-(real_t rhs) const {
            real_vec_t rn;
            rn = rhs;               // broadcast.
            return (*this) - rn;
        }

        // mul.
        ALWAYS_INLINE real_vec_t operator*(real_vec_t rhs) const {
            real_vec_t res;
#ifdef NO_INTRINSICS
            REAL_VEC_LOOP(i) res[i] = u.r[i] * rhs[i];
#else
            res.u.mr = INAME(mul)(u.mr, rhs.u.mr);
#endif
            return res;
        }
        ALWAYS_INLINE real_vec_t operator*(real_t rhs) const {
            real_vec_t rn;
            rn = rhs;               // broadcast.
            return (*this) * rn;
        }
    
        // div.
        ALWAYS_INLINE real_vec_t operator/(real_vec_t rhs) const {
            real_vec_t res, rcp;
#ifdef NO_INTRINSICS
            REAL_VEC_LOOP(i) res[i] = u.r[i] / rhs[i];
#elif USE_RCP14
            rcp.u.mr = INAME(rcp14)(rhs.u.mr);
            res.u.mr = INAME(mul)(u.mr, rcp.u.mr);
#elif USE_RCP28
            rcp.u.mr = INAME(rcp28)(rhs.u.mr);
            res.u.mr = INAME(mul)(u.mr, rcp.u.mr);
#else
            res.u.mr = INAME(div)(u.mr, rhs.u.mr);
#endif
            return res;
        }
        ALWAYS_INLINE real_vec_t operator/(real_t rhs) const {
            real_vec_t rn;
            rn = rhs;               // broadcast.
            return (*this) / rn;
        }

        // less-than comparator.
        bool operator<(const real_vec_t& rhs) const {
            for (int j = 0; j < VLEN; j++) {
                if (u.r[j] < rhs.u.r[j])
                    return true;
                else if (u.r[j] > rhs.u.r[j])
                    return false;
            }
            return false;
        }

        // greater-than comparator.
        bool operator>(const real_vec_t& rhs) const {
            for (int j = 0; j < VLEN; j++) {
                if (u.r[j] > rhs.u.r[j])
                    return true;
                else if (u.r[j] < rhs.u.r[j])
                    return false;
            }
            return false;
        }
    
        // equal-to comparator.
        bool operator==(const real_vec_t& rhs) const {
            for (int j = 0; j < VLEN; j++) {
                if (u.r[j] != rhs.u.r[j])
                    return false;
            }
            return true;
        }
    
        // not-equal-to comparator.
        bool operator!=(const real_vec_t& rhs) const {
            return !operator==(rhs);
        }
    
        // aligned load.
        ALWAYS_INLINE void loadFrom(const real_vec_t* __restrict from) {
#if defined(NO_INTRINSICS) || defined(NO_LOAD_INTRINSICS)
            REAL_VEC_LOOP(i) u.r[i] = (*from)[i];
#else
            u.mr = INAME(load)((imem_t const*)from);
#endif
        }

        // unaligned load.
        ALWAYS_INLINE void loadUnalignedFrom(const real_vec_t* __restrict from) {
#if defined(NO_INTRINSICS) || defined(NO_LOAD_INTRINSICS)
            REAL_VEC_LOOP_UNALIGNED(i) u.r[i] = (*from)[i];
#else
            u.mr = INAME(loadu)((imem_t const*)from);
#endif
        }

        // aligned store.
        ALWAYS_INLINE void storeTo(real_vec_t* __restrict to) const {

            // Using an explicit loop here instead of a store intrinsic may
            // allow the compiler to do more optimizations.  This is true
            // for icc 2016 r2--it may change for later versions. Try
            // defining and not defining NO_STORE_INTRINSICS and comparing
            // the sizes of the stencil computation loop and the overall
            // performance.
#if defined(NO_INTRINSICS) || defined(NO_STORE_INTRINSICS)
#if defined(__INTEL_COMPILER) && (VLEN > 1) && defined(USE_STREAMING_STORE)
            _Pragma("vector nontemporal")
#endif
                REAL_VEC_LOOP(i) (*to)[i] = u.r[i];
#elif !defined(USE_STREAMING_STORE)
            INAME(store)((imem_t*)to, u.mr);
#elif defined(ARCH_KNC)
            INAME(storenrngo)((imem_t*)to, u.mr);
#else
            INAME(stream)((imem_t*)to, u.mr);
#endif
        }

        // Output.
        void print_ctrls(std::ostream& os, bool doEnd=true) const {
            for (int j = 0; j < VLEN; j++) {
                if (j) os << ", ";
                os << "[" << j << "]=" << u.ci[j];
            }
            if (doEnd)
                os << std::endl;
        }

        void print_reals(std::ostream& os, bool doEnd=true) const {
            for (int j = 0; j < VLEN; j++) {
                if (j) os << ", ";
                os << "[" << j << "]=" << u.r[j];
            }
            if (doEnd)
                os << std::endl;
        }

    }; // real_vec_t.

    // Output using '<<'.
    inline std::ostream& operator<<(std::ostream& os, const real_vec_t& rn) {
        rn.print_reals(os, false);
        return os;
    }

    // More operator overloading.
    ALWAYS_INLINE real_vec_t operator+(real_t lhs, const real_vec_t& rhs) {
        return real_vec_t(lhs) + rhs;
    }
    ALWAYS_INLINE real_vec_t operator-(real_t lhs, const real_vec_t& rhs) {
        return real_vec_t(lhs) - rhs;
    }
    ALWAYS_INLINE real_vec_t operator*(real_t lhs, const real_vec_t& rhs) {
        return real_vec_t(lhs) * rhs;
    }
    ALWAYS_INLINE real_vec_t operator/(real_t lhs, const real_vec_t& rhs) {
        return real_vec_t(lhs) / rhs;
    }

    // wrappers around some intrinsics w/non-intrinsic equivalents.
    // TODO: make these methods in the real_vec_t union.

    // Get consecutive elements from two vectors.
    // Concat a and b, shift right by count elements, keep rightmost elements.
    // Thus, shift of 0 returns b; shift of VLEN returns a.
    // Must be a template because count has to be known at compile-time.
    template<int count>
    ALWAYS_INLINE void real_vec_align(real_vec_t& res, const real_vec_t& a, const real_vec_t& b) {
#ifdef TRACE_INTRINSICS
        std::cout << "real_vec_align w/count=" << count << ":" << std::endl;
        std::cout << " a: ";
        a.print_reals(cout);
        std::cout << " b: ";
        b.print_reals(cout);
#endif

#if defined(NO_INTRINSICS)
        // must make temp copies in case &res == &a or &b.
        real_vec_t tmpa = a, tmpb = b;
        for (int i = 0; i < VLEN-count; i++)
            res.u.r[i] = tmpb.u.r[i + count];
        for (int i = VLEN-count; i < VLEN; i++)
            res.u.r[i] = tmpa.u.r[i + count - VLEN];
    
#elif defined(USE_INTRIN256)
        // Not really an intrinsic, but not element-wise, either.
        // Put the 2 parts in a local array, then extract the desired part
        // using an unaligned load.
        real_t r2[VLEN * 2];
        *((real_vec_t*)(&r2[0])) = b;
        *((real_vec_t*)(&r2[VLEN])) = a;
        res = *((real_vec_t*)(&r2[count]));
    
#elif REAL_BYTES == 8 && defined(ARCH_KNC) && defined(USE_INTRIN512)
        // For KNC, for 64-bit align, use the 32-bit op w/2x count.
        res.u.mi = _mm512_alignr_epi32(a.u.mi, b.u.mi, count*2);

#else
        res.u.mi = INAMEI(alignr)(a.u.mi, b.u.mi, count);
#endif

#ifdef TRACE_INTRINSICS
        std::cout << " res: ";
        res.print_reals(cout);
#endif
    }

    // Get consecutive elements from two vectors w/masking.
    // Concat a and b, shift right by count elements, keep rightmost elements.
    // Elements in res corresponding to 0 bits in k1 are unchanged.
    template<int count>
    ALWAYS_INLINE void real_vec_align_masked(real_vec_t& res, const real_vec_t& a, const real_vec_t& b,
                                             unsigned int k1) {
#ifdef TRACE_INTRINSICS
        std::cout << "real_vec_align w/count=" << count << " w/mask:" << std::endl;
        std::cout << " a: ";
        a.print_reals(cout);
        std::cout << " b: ";
        b.print_reals(cout);
        std::cout << " res(before): ";
        res.print_reals(cout);
        std::cout << " mask: 0x" << hex << k1 << std::endl;
#endif

#if defined(NO_INTRINSICS) || !defined(USE_INTRIN512)
        // must make temp copies in case &res == &a or &b.
        real_vec_t tmpa = a, tmpb = b;
        for (int i = 0; i < VLEN-count; i++)
            if ((k1 >> i) & 1)
                res.u.r[i] = tmpb.u.r[i + count];
        for (int i = VLEN-count; i < VLEN; i++)
            if ((k1 >> i) & 1)
                res.u.r[i] = tmpa.u.r[i + count - VLEN];
#else
        res.u.mi = INAMEI(mask_alignr)(res.u.mi, real_mask_t(k1), a.u.mi, b.u.mi, count);
#endif

#ifdef TRACE_INTRINSICS
        std::cout << " res(after): ";
        res.print_reals(cout);
#endif
    }

    // Rearrange elements in a vector.
    ALWAYS_INLINE void real_vec_permute(real_vec_t& res, const real_vec_t& ctrl, const real_vec_t& a) {

#ifdef TRACE_INTRINSICS
        std::cout << "real_vec_permute:" << std::endl;
        std::cout << " ctrl: ";
        ctrl.print_ctrls(cout);
        std::cout << " a: ";
        a.print_reals(cout);
#endif

#if defined(NO_INTRINSICS) || !defined(USE_INTRIN512)
        // must make a temp copy in case &res == &a.
        real_vec_t tmp = a;
        for (int i = 0; i < VLEN; i++)
            res.u.r[i] = tmp.u.r[ctrl.u.ci[i]];
#else
        res.u.mi = INAMEI(permutexvar)(ctrl.u.mi, a.u.mi);
#endif

#ifdef TRACE_INTRINSICS
        std::cout << " res: ";
        res.print_reals(cout);
#endif
    }

    // Rearrange elements in a vector w/masking.
    // Elements in res corresponding to 0 bits in k1 are unchanged.
    ALWAYS_INLINE void real_vec_permute_masked(real_vec_t& res, const real_vec_t& ctrl, const real_vec_t& a,
                                               unsigned int k1) {
#ifdef TRACE_INTRINSICS
        std::cout << "real_vec_permute w/mask:" << std::endl;
        std::cout << " ctrl: ";
        ctrl.print_ctrls(cout);
        std::cout << " a: ";
        a.print_reals(cout);
        std::cout << " mask: 0x" << hex << k1 << std::endl;
        std::cout << " res(before): ";
        res.print_reals(cout);
#endif

#if defined(NO_INTRINSICS) || !defined(USE_INTRIN512)
        // must make a temp copy in case &res == &a.
        real_vec_t tmp = a;
        for (int i = 0; i < VLEN; i++) {
            if ((k1 >> i) & 1)
                res.u.r[i] = tmp.u.r[ctrl.u.ci[i]];
        }
#else
        res.u.mi = INAMEI(mask_permutexvar)(res.u.mi, real_mask_t(k1), ctrl.u.mi, a.u.mi);
#endif

#ifdef TRACE_INTRINSICS
        std::cout << " res(after): ";
        res.print_reals(cout);
#endif
    }

    // Rearrange elements in 2 vectors.
    // (The masking versions of these instrs do not preserve the source,
    // so we don't have a masking version of this function.)
    ALWAYS_INLINE void real_vec_permute2(real_vec_t& res, const real_vec_t& ctrl,
                                         const real_vec_t& a, const real_vec_t& b) {
#ifdef TRACE_INTRINSICS
        std::cout << "real_vec_permute2:" << std::endl;
        std::cout << " ctrl: ";
        ctrl.print_ctrls(cout);
        std::cout << " a: ";
        a.print_reals(cout);
        std::cout << " b: ";
        b.print_reals(cout);
#endif

#if defined(NO_INTRINSICS) || !defined(USE_INTRIN512)
        // must make temp copies in case &res == &a or &b.
        real_vec_t tmpa = a, tmpb = b;
        for (int i = 0; i < VLEN; i++) {
            int sel = ctrl.u.ci[i] & ctrl_sel_bit; // 0 => a, 1 => b.
            int idx = ctrl.u.ci[i] & ctrl_idx_mask; // index.
            res.u.r[i] = sel ? tmpb.u.r[idx] : tmpa.u.r[idx];
        }

#elif defined(ARCH_KNC)
        std::cerr << "error: 2-input permute not supported on KNC" << std::endl;
        exit(1);
#else
        res.u.mi = INAMEI(permutex2var)(a.u.mi, ctrl.u.mi, b.u.mi);
#endif

#ifdef TRACE_INTRINSICS
        std::cout << " res: ";
        res.print_reals(cout);
#endif
    }

    // default max abs difference in validation.
#ifndef EPSILON
#define EPSILON (1e-3)
#endif

    // check whether two reals are close enough.
    template<typename T>
    inline bool within_tolerance(T val, T ref, T epsilon) {
        bool ok;
        double adiff = fabs(val - ref);
        if (fabs(ref) > 1.0)
            epsilon = fabs(ref * epsilon);
        ok = adiff < epsilon;
#ifdef DEBUG_TOLERANCE
        if (!ok)
            std::cerr << "outside tolerance of " << epsilon << ": " << val << " != " << ref <<
                " because " << adiff << " >= " << epsilon << std::endl;
#endif
        return ok;
    }

    // Compare two real_vec_t's.
    inline bool within_tolerance(const real_vec_t& val, const real_vec_t& ref,
                                 const real_vec_t& epsilon) {
        for (int j = 0; j < VLEN; j++) {
            if (!within_tolerance(val.u.r[j], ref.u.r[j], epsilon.u.r[j]))
                return false;
        }
        return true;
    }

}
#endif
