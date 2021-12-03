/*
* Botan 2.18.2 Amalgamation
* (C) 1999-2020 The Botan Authors
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include "botan_all.h"

#include <chrono>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <vector>


namespace Botan {

/**
* If top bit of arg is set, return ~0. Otherwise return 0.
*/
template<typename T>
inline T expand_top_bit(T a)
   {
   return static_cast<T>(0) - (a >> (sizeof(T)*8-1));
   }

/**
* If arg is zero, return ~0. Otherwise return 0
*/
template<typename T>
inline T ct_is_zero(T x)
   {
   return expand_top_bit<T>(~x & (x - 1));
   }

/**
* Power of 2 test. T should be an unsigned integer type
* @param arg an integer value
* @return true iff arg is 2^n for some n > 0
*/
template<typename T>
inline constexpr bool is_power_of_2(T arg)
   {
   return (arg != 0) && (arg != 1) && ((arg & static_cast<T>(arg-1)) == 0);
   }

/**
* Return the index of the highest set bit
* T is an unsigned integer type
* @param n an integer value
* @return index of the highest set bit in n
*/
template<typename T>
inline size_t high_bit(T n)
   {
   size_t hb = 0;

   for(size_t s = 8*sizeof(T) / 2; s > 0; s /= 2)
      {
      const size_t z = s * ((~ct_is_zero(n >> s)) & 1);
      hb += z;
      n >>= z;
      }

   hb += n;

   return hb;
   }

/**
* Return the number of significant bytes in n
* @param n an integer value
* @return number of significant bytes in n
*/
template<typename T>
inline size_t significant_bytes(T n)
   {
   size_t b = 0;

   for(size_t s = 8*sizeof(n) / 2; s >= 8; s /= 2)
      {
      const size_t z = s * (~ct_is_zero(n >> s) & 1);
      b += z/8;
      n >>= z;
      }

   b += (n != 0);

   return b;
   }

/**
* Count the trailing zero bits in n
* @param n an integer value
* @return maximum x st 2^x divides n
*/
template<typename T>
inline size_t ctz(T n)
   {
   /*
   * If n == 0 then this function will compute 8*sizeof(T)-1, so
   * initialize lb to 1 if n == 0 to produce the expected result.
   */
   size_t lb = ct_is_zero(n) & 1;

   for(size_t s = 8*sizeof(T) / 2; s > 0; s /= 2)
      {
      const T mask = (static_cast<T>(1) << s) - 1;
      const size_t z = s * (ct_is_zero(n & mask) & 1);
      lb += z;
      n >>= z;
      }

   return lb;
   }

template<typename T>
uint8_t ceil_log2(T x)
   {
   static_assert(sizeof(T) < 32, "Abnormally large scalar");

   if(x >> (sizeof(T)*8-1))
      return sizeof(T)*8;

   uint8_t result = 0;
   T compare = 1;

   while(compare < x)
      {
      compare <<= 1;
      result++;
      }

   return result;
   }

// Potentially variable time ctz used for OCB
inline size_t var_ctz32(uint32_t n)
   {
#if defined(BOTAN_BUILD_COMPILER_IS_GCC) || defined(BOTAN_BUILD_COMPILER_IS_CLANG)
   if(n == 0)
      return 32;
   return __builtin_ctz(n);
#else
   return ctz<uint32_t>(n);
#endif
   }

template<typename T>
inline T bit_permute_step(T x, T mask, size_t shift)
   {
   /*
   See https://reflectionsonsecurity.wordpress.com/2014/05/11/efficient-bit-permutation-using-delta-swaps/
   and http://programming.sirrida.de/bit_perm.html
   */
   const T swap = ((x >> shift) ^ x) & mask;
   return (x ^ swap) ^ (swap << shift);
   }

template<typename T>
inline void swap_bits(T& x, T& y, T mask, size_t shift)
   {
   const T swap = ((x >> shift) ^ y) & mask;
   x ^= swap << shift;
   y ^= swap;
   }

}

namespace Botan {

/**
* Perform encoding using the base provided
* @param base object giving access to the encodings specifications
* @param output an array of at least base.encode_max_output bytes
* @param input is some binary data
* @param input_length length of input in bytes
* @param input_consumed is an output parameter which says how many
*        bytes of input were actually consumed. If less than
*        input_length, then the range input[consumed:length]
*        should be passed in later along with more input.
* @param final_inputs true iff this is the last input, in which case
         padding chars will be applied if needed
* @return number of bytes written to output
*/
template <class Base>
size_t base_encode(Base&& base,
                   char output[],
                   const uint8_t input[],
                   size_t input_length,
                   size_t& input_consumed,
                   bool final_inputs)
   {
   input_consumed = 0;

   const size_t encoding_bytes_in = base.encoding_bytes_in();
   const size_t encoding_bytes_out = base.encoding_bytes_out();

   size_t input_remaining = input_length;
   size_t output_produced = 0;

   while(input_remaining >= encoding_bytes_in)
      {
      base.encode(output + output_produced, input + input_consumed);

      input_consumed += encoding_bytes_in;
      output_produced += encoding_bytes_out;
      input_remaining -= encoding_bytes_in;
      }

   if(final_inputs && input_remaining)
      {
      std::vector<uint8_t> remainder(encoding_bytes_in, 0);
      for(size_t i = 0; i != input_remaining; ++i)
         { remainder[i] = input[input_consumed + i]; }

      base.encode(output + output_produced, remainder.data());

      const size_t bits_consumed = base.bits_consumed();
      const size_t remaining_bits_before_padding = base.remaining_bits_before_padding();

      size_t empty_bits = 8 * (encoding_bytes_in - input_remaining);
      size_t index = output_produced + encoding_bytes_out - 1;
      while(empty_bits >= remaining_bits_before_padding)
         {
         output[index--] = '=';
         empty_bits -= bits_consumed;
         }

      input_consumed += input_remaining;
      output_produced += encoding_bytes_out;
      }

   return output_produced;
   }


template <typename Base>
std::string base_encode_to_string(Base&& base, const uint8_t input[], size_t input_length)
   {
   const size_t output_length = base.encode_max_output(input_length);
   std::string output(output_length, 0);

   size_t consumed = 0;
   size_t produced = 0;

   if(output_length > 0)
      {
      produced = base_encode(base, &output.front(),
                                   input, input_length,
                                   consumed, true);
      }

   BOTAN_ASSERT_EQUAL(consumed, input_length, "Consumed the entire input");
   BOTAN_ASSERT_EQUAL(produced, output.size(), "Produced expected size");

   return output;
   }

/**
* Perform decoding using the base provided
* @param base object giving access to the encodings specifications
* @param output an array of at least Base::decode_max_output bytes
* @param input some base input
* @param input_length length of input in bytes
* @param input_consumed is an output parameter which says how many
*        bytes of input were actually consumed. If less than
*        input_length, then the range input[consumed:length]
*        should be passed in later along with more input.
* @param final_inputs true iff this is the last input, in which case
         padding is allowed
* @param ignore_ws ignore whitespace on input; if false, throw an
                   exception if whitespace is encountered
* @return number of bytes written to output
*/
template <typename Base>
size_t base_decode(Base&& base,
                   uint8_t output[],
                   const char input[],
                   size_t input_length,
                   size_t& input_consumed,
                   bool final_inputs,
                   bool ignore_ws = true)
   {
   const size_t decoding_bytes_in = base.decoding_bytes_in();
   const size_t decoding_bytes_out = base.decoding_bytes_out();

   uint8_t* out_ptr = output;
   std::vector<uint8_t> decode_buf(decoding_bytes_in, 0);
   size_t decode_buf_pos = 0;
   size_t final_truncate = 0;

   clear_mem(output, base.decode_max_output(input_length));

   for(size_t i = 0; i != input_length; ++i)
      {
      const uint8_t bin = base.lookup_binary_value(input[i]);

      if(base.check_bad_char(bin, input[i], ignore_ws)) // May throw Invalid_Argument
         {
         decode_buf[decode_buf_pos] = bin;
         ++decode_buf_pos;
         }

      /*
      * If we're at the end of the input, pad with 0s and truncate
      */
      if(final_inputs && (i == input_length - 1))
         {
         if(decode_buf_pos)
            {
            for(size_t j = decode_buf_pos; j < decoding_bytes_in; ++j)
               { decode_buf[j] = 0; }

            final_truncate = decoding_bytes_in - decode_buf_pos;
            decode_buf_pos = decoding_bytes_in;
            }
         }

      if(decode_buf_pos == decoding_bytes_in)
         {
         base.decode(out_ptr, decode_buf.data());

         out_ptr += decoding_bytes_out;
         decode_buf_pos = 0;
         input_consumed = i+1;
         }
      }

   while(input_consumed < input_length &&
         base.lookup_binary_value(input[input_consumed]) == 0x80)
      {
      ++input_consumed;
      }

   size_t written = (out_ptr - output) - base.bytes_to_remove(final_truncate);

   return written;
   }

template<typename Base>
size_t base_decode_full(Base&& base, uint8_t output[], const char input[], size_t input_length, bool ignore_ws)
   {
   size_t consumed = 0;
   const size_t written = base_decode(base, output, input, input_length, consumed, true, ignore_ws);

   if(consumed != input_length)
      {
      throw Invalid_Argument(base.name() + " decoding failed, input did not have full bytes");
      }

   return written;
   }

template<typename Vector, typename Base>
Vector base_decode_to_vec(Base&& base,
                          const char input[],
                          size_t input_length,
                          bool ignore_ws)
   {
   const size_t output_length = base.decode_max_output(input_length);
   Vector bin(output_length);

   const size_t written =
      base_decode_full(base, bin.data(), input, input_length, ignore_ws);

   bin.resize(written);
   return bin;
   }

}

#if defined(BOTAN_HAS_VALGRIND)
  #include <valgrind/memcheck.h>
#endif

namespace Botan {

namespace CT {

/**
* Use valgrind to mark the contents of memory as being undefined.
* Valgrind will accept operations which manipulate undefined values,
* but will warn if an undefined value is used to decided a conditional
* jump or a load/store address. So if we poison all of our inputs we
* can confirm that the operations in question are truly const time
* when compiled by whatever compiler is in use.
*
* Even better, the VALGRIND_MAKE_MEM_* macros work even when the
* program is not run under valgrind (though with a few cycles of
* overhead, which is unfortunate in final binaries as these
* annotations tend to be used in fairly important loops).
*
* This approach was first used in ctgrind (https://github.com/agl/ctgrind)
* but calling the valgrind mecheck API directly works just as well and
* doesn't require a custom patched valgrind.
*/
template<typename T>
inline void poison(const T* p, size_t n)
   {
#if defined(BOTAN_HAS_VALGRIND)
   VALGRIND_MAKE_MEM_UNDEFINED(p, n * sizeof(T));
#else
   BOTAN_UNUSED(p);
   BOTAN_UNUSED(n);
#endif
   }

template<typename T>
inline void unpoison(const T* p, size_t n)
   {
#if defined(BOTAN_HAS_VALGRIND)
   VALGRIND_MAKE_MEM_DEFINED(p, n * sizeof(T));
#else
   BOTAN_UNUSED(p);
   BOTAN_UNUSED(n);
#endif
   }

template<typename T>
inline void unpoison(T& p)
   {
#if defined(BOTAN_HAS_VALGRIND)
   VALGRIND_MAKE_MEM_DEFINED(&p, sizeof(T));
#else
   BOTAN_UNUSED(p);
#endif
   }

/**
* A Mask type used for constant-time operations. A Mask<T> always has value
* either 0 (all bits cleared) or ~0 (all bits set). All operations in a Mask<T>
* are intended to compile to code which does not contain conditional jumps.
* This must be verified with tooling (eg binary disassembly or using valgrind)
* since you never know what a compiler might do.
*/
template<typename T>
class Mask
   {
   public:
      static_assert(std::is_unsigned<T>::value, "CT::Mask only defined for unsigned integer types");

      Mask(const Mask<T>& other) = default;
      Mask<T>& operator=(const Mask<T>& other) = default;

      /**
      * Derive a Mask from a Mask of a larger type
      */
      template<typename U>
      Mask(Mask<U> o) : m_mask(static_cast<T>(o.value()))
         {
         static_assert(sizeof(U) > sizeof(T), "sizes ok");
         }

      /**
      * Return a Mask<T> with all bits set
      */
      static Mask<T> set()
         {
         return Mask<T>(static_cast<T>(~0));
         }

      /**
      * Return a Mask<T> with all bits cleared
      */
      static Mask<T> cleared()
         {
         return Mask<T>(0);
         }

      /**
      * Return a Mask<T> which is set if v is != 0
      */
      static Mask<T> expand(T v)
         {
         return ~Mask<T>::is_zero(v);
         }

      /**
      * Return a Mask<T> which is set if m is set
      */
      template<typename U>
      static Mask<T> expand(Mask<U> m)
         {
         static_assert(sizeof(U) < sizeof(T), "sizes ok");
         return ~Mask<T>::is_zero(m.value());
         }

      /**
      * Return a Mask<T> which is set if v is == 0 or cleared otherwise
      */
      static Mask<T> is_zero(T x)
         {
         return Mask<T>(ct_is_zero<T>(x));
         }

      /**
      * Return a Mask<T> which is set if x == y
      */
      static Mask<T> is_equal(T x, T y)
         {
         return Mask<T>::is_zero(static_cast<T>(x ^ y));
         }

      /**
      * Return a Mask<T> which is set if x < y
      */
      static Mask<T> is_lt(T x, T y)
         {
         return Mask<T>(expand_top_bit<T>(x^((x^y) | ((x-y)^x))));
         }

      /**
      * Return a Mask<T> which is set if x > y
      */
      static Mask<T> is_gt(T x, T y)
         {
         return Mask<T>::is_lt(y, x);
         }

      /**
      * Return a Mask<T> which is set if x <= y
      */
      static Mask<T> is_lte(T x, T y)
         {
         return ~Mask<T>::is_gt(x, y);
         }

      /**
      * Return a Mask<T> which is set if x >= y
      */
      static Mask<T> is_gte(T x, T y)
         {
         return ~Mask<T>::is_lt(x, y);
         }

      static Mask<T> is_within_range(T v, T l, T u)
         {
         //return Mask<T>::is_gte(v, l) & Mask<T>::is_lte(v, u);

         const T v_lt_l = v^((v^l) | ((v-l)^v));
         const T v_gt_u = u^((u^v) | ((u-v)^u));
         const T either = v_lt_l | v_gt_u;
         return ~Mask<T>(expand_top_bit(either));
         }

      static Mask<T> is_any_of(T v, std::initializer_list<T> accepted)
         {
         T accept = 0;

         for(auto a: accepted)
            {
            const T diff = a ^ v;
            const T eq_zero = ~diff & (diff - 1);
            accept |= eq_zero;
            }

         return Mask<T>(expand_top_bit(accept));
         }

      /**
      * AND-combine two masks
      */
      Mask<T>& operator&=(Mask<T> o)
         {
         m_mask &= o.value();
         return (*this);
         }

      /**
      * XOR-combine two masks
      */
      Mask<T>& operator^=(Mask<T> o)
         {
         m_mask ^= o.value();
         return (*this);
         }

      /**
      * OR-combine two masks
      */
      Mask<T>& operator|=(Mask<T> o)
         {
         m_mask |= o.value();
         return (*this);
         }

      /**
      * AND-combine two masks
      */
      friend Mask<T> operator&(Mask<T> x, Mask<T> y)
         {
         return Mask<T>(x.value() & y.value());
         }

      /**
      * XOR-combine two masks
      */
      friend Mask<T> operator^(Mask<T> x, Mask<T> y)
         {
         return Mask<T>(x.value() ^ y.value());
         }

      /**
      * OR-combine two masks
      */
      friend Mask<T> operator|(Mask<T> x, Mask<T> y)
         {
         return Mask<T>(x.value() | y.value());
         }

      /**
      * Negate this mask
      */
      Mask<T> operator~() const
         {
         return Mask<T>(~value());
         }

      /**
      * Return x if the mask is set, or otherwise zero
      */
      T if_set_return(T x) const
         {
         return m_mask & x;
         }

      /**
      * Return x if the mask is cleared, or otherwise zero
      */
      T if_not_set_return(T x) const
         {
         return ~m_mask & x;
         }

      /**
      * If this mask is set, return x, otherwise return y
      */
      T select(T x, T y) const
         {
         // (x & value()) | (y & ~value())
         return static_cast<T>(y ^ (value() & (x ^ y)));
         }

      T select_and_unpoison(T x, T y) const
         {
         T r = this->select(x, y);
         CT::unpoison(r);
         return r;
         }

      /**
      * If this mask is set, return x, otherwise return y
      */
      Mask<T> select_mask(Mask<T> x, Mask<T> y) const
         {
         return Mask<T>(select(x.value(), y.value()));
         }

      /**
      * Conditionally set output to x or y, depending on if mask is set or
      * cleared (resp)
      */
      void select_n(T output[], const T x[], const T y[], size_t len) const
         {
         for(size_t i = 0; i != len; ++i)
            output[i] = this->select(x[i], y[i]);
         }

      /**
      * If this mask is set, zero out buf, otherwise do nothing
      */
      void if_set_zero_out(T buf[], size_t elems)
         {
         for(size_t i = 0; i != elems; ++i)
            {
            buf[i] = this->if_not_set_return(buf[i]);
            }
         }

      /**
      * Return the value of the mask, unpoisoned
      */
      T unpoisoned_value() const
         {
         T r = value();
         CT::unpoison(r);
         return r;
         }

      /**
      * Return true iff this mask is set
      */
      bool is_set() const
         {
         return unpoisoned_value() != 0;
         }

      /**
      * Return the underlying value of the mask
      */
      T value() const
         {
         return m_mask;
         }

   private:
      Mask(T m) : m_mask(m) {}

      T m_mask;
   };

template<typename T>
inline Mask<T> conditional_copy_mem(T cnd,
                                    T* to,
                                    const T* from0,
                                    const T* from1,
                                    size_t elems)
   {
   const auto mask = CT::Mask<T>::expand(cnd);
   mask.select_n(to, from0, from1, elems);
   return mask;
   }

template<typename T>
inline void conditional_swap(bool cnd, T& x, T& y)
   {
   const auto swap = CT::Mask<T>::expand(cnd);

   T t0 = swap.select(y, x);
   T t1 = swap.select(x, y);
   x = t0;
   y = t1;
   }

template<typename T>
inline void conditional_swap_ptr(bool cnd, T& x, T& y)
   {
   uintptr_t xp = reinterpret_cast<uintptr_t>(x);
   uintptr_t yp = reinterpret_cast<uintptr_t>(y);

   conditional_swap<uintptr_t>(cnd, xp, yp);

   x = reinterpret_cast<T>(xp);
   y = reinterpret_cast<T>(yp);
   }

/**
* If bad_mask is unset, return in[delim_idx:input_length] copied to
* new buffer. If bad_mask is set, return an all zero vector of
* unspecified length.
*/
secure_vector<uint8_t> copy_output(CT::Mask<uint8_t> bad_input,
                                   const uint8_t input[],
                                   size_t input_length,
                                   size_t delim_idx);

secure_vector<uint8_t> strip_leading_zeros(const uint8_t in[], size_t length);

inline secure_vector<uint8_t> strip_leading_zeros(const secure_vector<uint8_t>& in)
   {
   return strip_leading_zeros(in.data(), in.size());
   }

}

}

namespace Botan {

class donna128 final
   {
   public:
      donna128(uint64_t ll = 0, uint64_t hh = 0) { l = ll; h = hh; }

      donna128(const donna128&) = default;
      donna128& operator=(const donna128&) = default;

      friend donna128 operator>>(const donna128& x, size_t shift)
         {
         donna128 z = x;
         if(shift > 0)
            {
            const uint64_t carry = z.h << (64 - shift);
            z.h = (z.h >> shift);
            z.l = (z.l >> shift) | carry;
            }
         return z;
         }

      friend donna128 operator<<(const donna128& x, size_t shift)
         {
         donna128 z = x;
         if(shift > 0)
            {
            const uint64_t carry = z.l >> (64 - shift);
            z.l = (z.l << shift);
            z.h = (z.h << shift) | carry;
            }
         return z;
         }

      friend uint64_t operator&(const donna128& x, uint64_t mask)
         {
         return x.l & mask;
         }

      uint64_t operator&=(uint64_t mask)
         {
         h = 0;
         l &= mask;
         return l;
         }

      donna128& operator+=(const donna128& x)
         {
         l += x.l;
         h += x.h;

         const uint64_t carry = (l < x.l);
         h += carry;
         return *this;
         }

      donna128& operator+=(uint64_t x)
         {
         l += x;
         const uint64_t carry = (l < x);
         h += carry;
         return *this;
         }

      uint64_t lo() const { return l; }
      uint64_t hi() const { return h; }
   private:
      uint64_t h = 0, l = 0;
   };

inline donna128 operator*(const donna128& x, uint64_t y)
   {
   BOTAN_ARG_CHECK(x.hi() == 0, "High 64 bits of donna128 set to zero during multiply");

   uint64_t lo = 0, hi = 0;
   mul64x64_128(x.lo(), y, &lo, &hi);
   return donna128(lo, hi);
   }

inline donna128 operator*(uint64_t y, const donna128& x)
   {
   return x * y;
   }

inline donna128 operator+(const donna128& x, const donna128& y)
   {
   donna128 z = x;
   z += y;
   return z;
   }

inline donna128 operator+(const donna128& x, uint64_t y)
   {
   donna128 z = x;
   z += y;
   return z;
   }

inline donna128 operator|(const donna128& x, const donna128& y)
   {
   return donna128(x.lo() | y.lo(), x.hi() | y.hi());
   }

inline uint64_t carry_shift(const donna128& a, size_t shift)
   {
   return (a >> shift).lo();
   }

inline uint64_t combine_lower(const donna128& a, size_t s1,
                              const donna128& b, size_t s2)
   {
   donna128 z = (a >> s1) | (b << s2);
   return z.lo();
   }

#if defined(BOTAN_TARGET_HAS_NATIVE_UINT128)
inline uint64_t carry_shift(const uint128_t a, size_t shift)
   {
   return static_cast<uint64_t>(a >> shift);
   }

inline uint64_t combine_lower(const uint128_t a, size_t s1,
                              const uint128_t b, size_t s2)
   {
   return static_cast<uint64_t>((a >> s1) | (b << s2));
   }
#endif

}

namespace Botan {

/**
* No_Filesystem_Access Exception
*/
class BOTAN_PUBLIC_API(2,0) No_Filesystem_Access final : public Exception
   {
   public:
      No_Filesystem_Access() : Exception("No filesystem access enabled.")
         {}
   };

BOTAN_TEST_API bool has_filesystem_impl();

BOTAN_TEST_API std::vector<std::string> get_files_recursive(const std::string& dir);

}

namespace Botan {

namespace OS {

/*
* This header is internal (not installed) and these functions are not
* intended to be called by applications. However they are given public
* visibility (using BOTAN_TEST_API macro) for the tests. This also probably
* allows them to be overridden by the application on ELF systems, but
* this hasn't been tested.
*/

/**
* @return process ID assigned by the operating system.
* On Unix and Windows systems, this always returns a result
* On IncludeOS it returns 0 since there is no process ID to speak of
* in a unikernel.
*/
uint32_t BOTAN_TEST_API get_process_id();

/**
* Test if we are currently running with elevated permissions
* eg setuid, setgid, or with POSIX caps set.
*/
bool running_in_privileged_state();

/**
* @return CPU processor clock, if available
*
* On Windows, calls QueryPerformanceCounter.
*
* Under GCC or Clang on supported platforms the hardware cycle counter is queried.
* Currently supported processors are x86, PPC, Alpha, SPARC, IA-64, S/390x, and HP-PA.
* If no CPU cycle counter is available on this system, returns zero.
*/
uint64_t BOTAN_TEST_API get_cpu_cycle_counter();

size_t BOTAN_TEST_API get_cpu_total();
size_t BOTAN_TEST_API get_cpu_available();

/**
* Return the ELF auxiliary vector cooresponding to the given ID.
* This only makes sense on Unix-like systems and is currently
* only supported on Linux, Android, and FreeBSD.
*
* Returns zero if not supported on the current system or if
* the id provided is not known.
*/
unsigned long get_auxval(unsigned long id);

/*
* @return best resolution timestamp available
*
* The epoch and update rate of this clock is arbitrary and depending
* on the hardware it may not tick at a constant rate.
*
* Uses hardware cycle counter, if available.
* On POSIX platforms clock_gettime is used with a monotonic timer
* As a final fallback std::chrono::high_resolution_clock is used.
*/
uint64_t BOTAN_TEST_API get_high_resolution_clock();

/**
* @return system clock (reflecting wall clock) with best resolution
* available, normalized to nanoseconds resolution.
*/
uint64_t BOTAN_TEST_API get_system_timestamp_ns();

/**
* @return maximum amount of memory (in bytes) Botan could/should
* hyptothetically allocate for the memory poool. Reads environment
* variable "BOTAN_MLOCK_POOL_SIZE", set to "0" to disable pool.
*/
size_t get_memory_locking_limit();

/**
* Return the size of a memory page, if that can be derived on the
* current system. Otherwise returns some default value (eg 4096)
*/
size_t system_page_size();

/**
* Read the value of an environment variable, setting it to value_out if it
* exists.  Returns false and sets value_out to empty string if no such variable
* is set. If the process seems to be running in a privileged state (such as
* setuid) then always returns false and does not examine the environment.
*/
bool read_env_variable(std::string& value_out, const std::string& var_name);

/**
* Read the value of an environment variable and convert it to an
* integer. If not set or conversion fails, returns the default value.
*
* If the process seems to be running in a privileged state (such as setuid)
* then always returns nullptr, similiar to glibc's secure_getenv.
*/
size_t read_env_variable_sz(const std::string& var_name, size_t def_value = 0);

/**
* Request count pages of RAM which are locked into memory using mlock,
* VirtualLock, or some similar OS specific API. Free it with free_locked_pages.
*
* Returns an empty list on failure. This function is allowed to return fewer
* than count pages.
*
* The contents of the allocated pages are undefined.
*
* Each page is preceded by and followed by a page which is marked
* as noaccess, such that accessing it will cause a crash. This turns
* out of bound reads/writes into crash events.
*
* @param count requested number of locked pages
*/
std::vector<void*> allocate_locked_pages(size_t count);

/**
* Free memory allocated by allocate_locked_pages
* @param pages a list of pages returned by allocate_locked_pages
*/
void free_locked_pages(const std::vector<void*>& pages);

/**
* Set the MMU to prohibit access to this page
*/
void page_prohibit_access(void* page);

/**
* Set the MMU to allow R/W access to this page
*/
void page_allow_access(void* page);


/**
* Run a probe instruction to test for support for a CPU instruction.
* Runs in system-specific env that catches illegal instructions; this
* function always fails if the OS doesn't provide this.
* Returns value of probe_fn, if it could run.
* If error occurs, returns negative number.
* This allows probe_fn to indicate errors of its own, if it wants.
* For example the instruction might not only be only available on some
* CPUs, but also buggy on some subset of these - the probe function
* can test to make sure the instruction works properly before
* indicating that the instruction is available.
*
* @warning on Unix systems uses signal handling in a way that is not
* thread safe. It should only be called in a single-threaded context
* (ie, at static init time).
*
* If probe_fn throws an exception the result is undefined.
*
* Return codes:
* -1 illegal instruction detected
*/
int BOTAN_TEST_API run_cpu_instruction_probe(std::function<int ()> probe_fn);

/**
* Represents a terminal state
*/
class BOTAN_UNSTABLE_API Echo_Suppression
   {
   public:
      /**
      * Reenable echo on this terminal. Can be safely called
      * multiple times. May throw if an error occurs.
      */
      virtual void reenable_echo() = 0;

      /**
      * Implicitly calls reenable_echo, but swallows/ignored all
      * errors which would leave the terminal in an invalid state.
      */
      virtual ~Echo_Suppression() = default;
   };

/**
* Suppress echo on the terminal
* Returns null if this operation is not supported on the current system.
*/
std::unique_ptr<Echo_Suppression> BOTAN_UNSTABLE_API suppress_echo_on_terminal();

}

}

namespace Botan {

template<typename T>
inline void prefetch_readonly(const T* addr, size_t length)
   {
#if defined(__GNUG__)
   const size_t Ts_per_cache_line = CPUID::cache_line_size() / sizeof(T);

   for(size_t i = 0; i <= length; i += Ts_per_cache_line)
      __builtin_prefetch(addr + i, 0);
#endif
   }

template<typename T>
inline void prefetch_readwrite(const T* addr, size_t length)
   {
#if defined(__GNUG__)
   const size_t Ts_per_cache_line = CPUID::cache_line_size() / sizeof(T);

   for(size_t i = 0; i <= length; i += Ts_per_cache_line)
      __builtin_prefetch(addr + i, 1);
#endif
   }

}

namespace Botan {

/**
* Round up
* @param n a non-negative integer
* @param align_to the alignment boundary
* @return n rounded up to a multiple of align_to
*/
inline size_t round_up(size_t n, size_t align_to)
   {
   BOTAN_ARG_CHECK(align_to != 0, "align_to must not be 0");

   if(n % align_to)
      n += align_to - (n % align_to);
   return n;
   }

/**
* Round down
* @param n an integer
* @param align_to the alignment boundary
* @return n rounded down to a multiple of align_to
*/
template<typename T>
inline constexpr T round_down(T n, T align_to)
   {
   return (align_to == 0) ? n : (n - (n % align_to));
   }

/**
* Clamp
*/
inline size_t clamp(size_t n, size_t lower_bound, size_t upper_bound)
   {
   if(n < lower_bound)
      return lower_bound;
   if(n > upper_bound)
      return upper_bound;
   return n;
   }

}

namespace Botan {

class BOTAN_PUBLIC_API(2,0) Integer_Overflow_Detected final : public Exception
   {
   public:
      Integer_Overflow_Detected(const std::string& file, int line) :
         Exception("Integer overflow detected at " + file + ":" + std::to_string(line))
         {}

      ErrorType error_type() const noexcept override { return ErrorType::InternalError; }
   };

inline size_t checked_add(size_t x, size_t y, const char* file, int line)
   {
   // TODO: use __builtin_x_overflow on GCC and Clang
   size_t z = x + y;
   if(z < x)
      {
      throw Integer_Overflow_Detected(file, line);
      }
   return z;
   }

#define BOTAN_CHECKED_ADD(x,y) checked_add(x,y,__FILE__,__LINE__)

}

namespace Botan {

inline std::vector<uint8_t> to_byte_vector(const std::string& s)
   {
   return std::vector<uint8_t>(s.cbegin(), s.cend());
   }

inline std::string to_string(const secure_vector<uint8_t> &bytes)
   {
   return std::string(bytes.cbegin(), bytes.cend());
   }

/**
* Return the keys of a map as a std::set
*/
template<typename K, typename V>
std::set<K> map_keys_as_set(const std::map<K, V>& kv)
   {
   std::set<K> s;
   for(auto&& i : kv)
      {
      s.insert(i.first);
      }
   return s;
   }

/*
* Searching through a std::map
* @param mapping the map to search
* @param key is what to look for
* @param null_result is the value to return if key is not in mapping
* @return mapping[key] or null_result
*/
template<typename K, typename V>
inline V search_map(const std::map<K, V>& mapping,
                    const K& key,
                    const V& null_result = V())
   {
   auto i = mapping.find(key);
   if(i == mapping.end())
      return null_result;
   return i->second;
   }

template<typename K, typename V, typename R>
inline R search_map(const std::map<K, V>& mapping, const K& key,
                    const R& null_result, const R& found_result)
   {
   auto i = mapping.find(key);
   if(i == mapping.end())
      return null_result;
   return found_result;
   }

/*
* Insert a key/value pair into a multimap
*/
template<typename K, typename V>
void multimap_insert(std::multimap<K, V>& multimap,
                     const K& key, const V& value)
   {
   multimap.insert(std::make_pair(key, value));
   }

/**
* Existence check for values
*/
template<typename T>
bool value_exists(const std::vector<T>& vec,
                  const T& val)
   {
   for(size_t i = 0; i != vec.size(); ++i)
      if(vec[i] == val)
         return true;
   return false;
   }

template<typename T, typename Pred>
void map_remove_if(Pred pred, T& assoc)
   {
   auto i = assoc.begin();
   while(i != assoc.end())
      {
      if(pred(i->first))
         assoc.erase(i++);
      else
         i++;
      }
   }

}

namespace Botan {

class BOTAN_TEST_API Timer final
   {
   public:
      Timer(const std::string& name,
            const std::string& provider,
            const std::string& doing,
            uint64_t event_mult,
            size_t buf_size,
            double clock_cycle_ratio,
            uint64_t clock_speed)
         : m_name(name + ((provider.empty() || provider == "base") ? "" : " [" + provider + "]"))
         , m_doing(doing)
         , m_buf_size(buf_size)
         , m_event_mult(event_mult)
         , m_clock_cycle_ratio(clock_cycle_ratio)
         , m_clock_speed(clock_speed)
         {}

      Timer(const std::string& name) :
         Timer(name, "", "", 1, 0, 0.0, 0)
         {}

      Timer(const std::string& name, size_t buf_size) :
         Timer(name, "", "", buf_size, buf_size, 0.0, 0)
         {}

      Timer(const Timer& other) = default;
      Timer& operator=(const Timer& other) = default;

      void start();

      void stop();

      bool under(std::chrono::milliseconds msec)
         {
         return (milliseconds() < msec.count());
         }

      class Timer_Scope final
         {
         public:
            explicit Timer_Scope(Timer& timer)
               : m_timer(timer)
               {
               m_timer.start();
               }
            ~Timer_Scope()
               {
               try
                  {
                  m_timer.stop();
                  }
               catch(...) {}
               }
         private:
            Timer& m_timer;
         };

      template<typename F>
      auto run(F f) -> decltype(f())
         {
         Timer_Scope timer(*this);
         return f();
         }

      template<typename F>
      void run_until_elapsed(std::chrono::milliseconds msec, F f)
         {
         while(this->under(msec))
            {
            run(f);
            }
         }

      uint64_t value() const
         {
         return m_time_used;
         }

      double seconds() const
         {
         return milliseconds() / 1000.0;
         }

      double milliseconds() const
         {
         return value() / 1000000.0;
         }

      double ms_per_event() const
         {
         return milliseconds() / events();
         }

      uint64_t cycles_consumed() const
         {
         if(m_clock_speed != 0)
            {
            return static_cast<uint64_t>((m_clock_speed * value()) / 1000.0);
            }
         return m_cpu_cycles_used;
         }

      uint64_t events() const
         {
         return m_event_count * m_event_mult;
         }

      const std::string& get_name() const
         {
         return m_name;
         }

      const std::string& doing() const
         {
         return m_doing;
         }

      size_t buf_size() const
         {
         return m_buf_size;
         }

      double bytes_per_second() const
         {
         return seconds() > 0.0 ? events() / seconds() : 0.0;
         }

      double events_per_second() const
         {
         return seconds() > 0.0 ? events() / seconds() : 0.0;
         }

      double seconds_per_event() const
         {
         return events() > 0 ? seconds() / events() : 0.0;
         }

      void set_custom_msg(const std::string& s)
         {
         m_custom_msg = s;
         }

      bool operator<(const Timer& other) const;

      std::string to_string() const;

   private:
      std::string result_string_bps() const;
      std::string result_string_ops() const;

      // const data
      std::string m_name, m_doing;
      size_t m_buf_size;
      uint64_t m_event_mult;
      double m_clock_cycle_ratio;
      uint64_t m_clock_speed;

      // set at runtime
      std::string m_custom_msg;
      uint64_t m_time_used = 0, m_timer_start = 0;
      uint64_t m_event_count = 0;

      uint64_t m_max_time = 0, m_min_time = 0;
      uint64_t m_cpu_cycles_start = 0, m_cpu_cycles_used = 0;
   };

}
/*
* (C) 2013,2015 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <sstream>

#if defined(BOTAN_HAS_BLOCK_CIPHER)
#endif

#if defined(BOTAN_HAS_AEAD_CCM)
#endif

#if defined(BOTAN_HAS_AEAD_CHACHA20_POLY1305)
#endif

#if defined(BOTAN_HAS_AEAD_EAX)
#endif

#if defined(BOTAN_HAS_AEAD_GCM)
#endif

#if defined(BOTAN_HAS_AEAD_OCB)
#endif

#if defined(BOTAN_HAS_AEAD_SIV)
#endif

namespace Botan {

void AEAD_Mode::set_associated_data_n(size_t i, const uint8_t ad[], size_t ad_len)
   {
   if(i == 0)
      this->set_associated_data(ad, ad_len);
   else
      throw Invalid_Argument("AEAD '" + name() + "' does not support multiple associated data");
   }

std::unique_ptr<AEAD_Mode> AEAD_Mode::create_or_throw(const std::string& algo,
                                                      Cipher_Dir dir,
                                                      const std::string& provider)
   {
   if(auto aead = AEAD_Mode::create(algo, dir, provider))
      return aead;

   throw Lookup_Error("AEAD", algo, provider);
   }

std::unique_ptr<AEAD_Mode> AEAD_Mode::create(const std::string& algo,
                                             Cipher_Dir dir,
                                             const std::string& provider)
   {
   BOTAN_UNUSED(provider);
#if defined(BOTAN_HAS_AEAD_CHACHA20_POLY1305)
   if(algo == "ChaCha20Poly1305")
      {
      if(dir == ENCRYPTION)
         return std::unique_ptr<AEAD_Mode>(new ChaCha20Poly1305_Encryption);
      else
         return std::unique_ptr<AEAD_Mode>(new ChaCha20Poly1305_Decryption);

      }
#endif

   if(algo.find('/') != std::string::npos)
      {
      const std::vector<std::string> algo_parts = split_on(algo, '/');
      const std::string cipher_name = algo_parts[0];
      const std::vector<std::string> mode_info = parse_algorithm_name(algo_parts[1]);

      if(mode_info.empty())
         return std::unique_ptr<AEAD_Mode>();

      std::ostringstream alg_args;

      alg_args << '(' << cipher_name;
      for(size_t i = 1; i < mode_info.size(); ++i)
         alg_args << ',' << mode_info[i];
      for(size_t i = 2; i < algo_parts.size(); ++i)
         alg_args << ',' << algo_parts[i];
      alg_args << ')';

      const std::string mode_name = mode_info[0] + alg_args.str();
      return AEAD_Mode::create(mode_name, dir);
      }

#if defined(BOTAN_HAS_BLOCK_CIPHER)

   SCAN_Name req(algo);

   if(req.arg_count() == 0)
      {
      return std::unique_ptr<AEAD_Mode>();
      }

   std::unique_ptr<BlockCipher> bc(BlockCipher::create(req.arg(0), provider));

   if(!bc)
      {
      return std::unique_ptr<AEAD_Mode>();
      }

#if defined(BOTAN_HAS_AEAD_CCM)
   if(req.algo_name() == "CCM")
      {
      size_t tag_len = req.arg_as_integer(1, 16);
      size_t L_len = req.arg_as_integer(2, 3);
      if(dir == ENCRYPTION)
         return std::unique_ptr<AEAD_Mode>(new CCM_Encryption(bc.release(), tag_len, L_len));
      else
         return std::unique_ptr<AEAD_Mode>(new CCM_Decryption(bc.release(), tag_len, L_len));
      }
#endif

#if defined(BOTAN_HAS_AEAD_GCM)
   if(req.algo_name() == "GCM")
      {
      size_t tag_len = req.arg_as_integer(1, 16);
      if(dir == ENCRYPTION)
         return std::unique_ptr<AEAD_Mode>(new GCM_Encryption(bc.release(), tag_len));
      else
         return std::unique_ptr<AEAD_Mode>(new GCM_Decryption(bc.release(), tag_len));
      }
#endif

#if defined(BOTAN_HAS_AEAD_OCB)
   if(req.algo_name() == "OCB")
      {
      size_t tag_len = req.arg_as_integer(1, 16);
      if(dir == ENCRYPTION)
         return std::unique_ptr<AEAD_Mode>(new OCB_Encryption(bc.release(), tag_len));
      else
         return std::unique_ptr<AEAD_Mode>(new OCB_Decryption(bc.release(), tag_len));
      }
#endif

#if defined(BOTAN_HAS_AEAD_EAX)
   if(req.algo_name() == "EAX")
      {
      size_t tag_len = req.arg_as_integer(1, bc->block_size());
      if(dir == ENCRYPTION)
         return std::unique_ptr<AEAD_Mode>(new EAX_Encryption(bc.release(), tag_len));
      else
         return std::unique_ptr<AEAD_Mode>(new EAX_Decryption(bc.release(), tag_len));
      }
#endif

#if defined(BOTAN_HAS_AEAD_SIV)
   if(req.algo_name() == "SIV")
      {
      if(dir == ENCRYPTION)
         return std::unique_ptr<AEAD_Mode>(new SIV_Encryption(bc.release()));
      else
         return std::unique_ptr<AEAD_Mode>(new SIV_Decryption(bc.release()));
      }
#endif

#endif

   return std::unique_ptr<AEAD_Mode>();
   }



}
/*
* (C) 1999-2010,2015,2017,2018,2020 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

#if defined(BOTAN_HAS_AES_POWER8) || defined(BOTAN_HAS_AES_ARMV8) || defined(BOTAN_HAS_AES_NI)
   #define BOTAN_HAS_HW_AES_SUPPORT
#endif

/*
* One of three AES implementation strategies are used to get a constant time
* implementation which is immune to common cache/timing based side channels:
*
* - If AES hardware support is available (AES-NI, POWER8, Aarch64) use that
*
* - If 128-bit SIMD with byte shuffles are available (SSSE3, NEON, or Altivec),
*   use the vperm technique published by Mike Hamburg at CHES 2009.
*
* - If no hardware or SIMD support, fall back to a constant time bitsliced
*   implementation. This uses 32-bit words resulting in 2 blocks being processed
*   in parallel. Moving to 4 blocks (with 64-bit words) would approximately
*   double performance on 64-bit CPUs. Likewise moving to 128 bit SIMD would
*   again approximately double performance vs 64-bit. However the assumption is
*   that most 64-bit CPUs either have hardware AES or SIMD shuffle support and
*   that the majority of users falling back to this code will be 32-bit cores.
*   If this assumption proves to be unsound, the bitsliced code can easily be
*   extended to operate on either 32 or 64 bit words depending on the native
*   wordsize of the target processor.
*
* Useful references
*
* - "Accelerating AES with Vector Permute Instructions" Mike Hamburg
*   https://www.shiftleft.org/papers/vector_aes/vector_aes.pdf
*
* - "Faster and Timing-Attack Resistant AES-GCM" Käsper and Schwabe
*   https://eprint.iacr.org/2009/129.pdf
*
* - "A new combinational logic minimization technique with applications to cryptology."
*   Boyar and Peralta https://eprint.iacr.org/2009/191.pdf
*
* - "A depth-16 circuit for the AES S-box" Boyar and Peralta
*    https://eprint.iacr.org/2011/332.pdf
*
* - "A Very Compact S-box for AES" Canright
*   https://www.iacr.org/archive/ches2005/032.pdf
*   https://core.ac.uk/download/pdf/36694529.pdf (extended)
*/

namespace {

/*
This is an AES sbox circuit which can execute in bitsliced mode up to 32x in
parallel.

The circuit is from the "Circuit Minimization Team" group
http://www.cs.yale.edu/homes/peralta/CircuitStuff/CMT.html
http://www.cs.yale.edu/homes/peralta/CircuitStuff/SLP_AES_113.txt

This circuit has size 113 and depth 27. In software it is much faster than
circuits which are considered faster for hardware purposes (where circuit depth
is the critical constraint), because unlike in hardware, on common CPUs we can
only execute - at best - 3 or 4 logic operations per cycle. So a smaller circuit
is superior. On an x86-64 machine this circuit is about 15% faster than the
circuit of size 128 and depth 16 given in "A depth-16 circuit for the AES S-box".

Another circuit for AES Sbox of size 102 and depth 24 is describted in "New
Circuit Minimization Techniques for Smaller and Faster AES SBoxes"
[https://eprint.iacr.org/2019/802] however it relies on "non-standard" gates
like MUX, NOR, NAND, etc and so in practice in bitsliced software, its size is
actually a bit larger than this circuit, as few CPUs have such instructions and
otherwise they must be emulated using a sequence of available bit operations.
*/
void AES_SBOX(uint32_t V[8])
   {
   const uint32_t U0 = V[0];
   const uint32_t U1 = V[1];
   const uint32_t U2 = V[2];
   const uint32_t U3 = V[3];
   const uint32_t U4 = V[4];
   const uint32_t U5 = V[5];
   const uint32_t U6 = V[6];
   const uint32_t U7 = V[7];

   const uint32_t y14 = U3 ^ U5;
   const uint32_t y13 = U0 ^ U6;
   const uint32_t y9 = U0 ^ U3;
   const uint32_t y8 = U0 ^ U5;
   const uint32_t t0 = U1 ^ U2;
   const uint32_t y1 = t0 ^ U7;
   const uint32_t y4 = y1 ^ U3;
   const uint32_t y12 = y13 ^ y14;
   const uint32_t y2 = y1 ^ U0;
   const uint32_t y5 = y1 ^ U6;
   const uint32_t y3 = y5 ^ y8;
   const uint32_t t1 = U4 ^ y12;
   const uint32_t y15 = t1 ^ U5;
   const uint32_t y20 = t1 ^ U1;
   const uint32_t y6 = y15 ^ U7;
   const uint32_t y10 = y15 ^ t0;
   const uint32_t y11 = y20 ^ y9;
   const uint32_t y7 = U7 ^ y11;
   const uint32_t y17 = y10 ^ y11;
   const uint32_t y19 = y10 ^ y8;
   const uint32_t y16 = t0 ^ y11;
   const uint32_t y21 = y13 ^ y16;
   const uint32_t y18 = U0 ^ y16;
   const uint32_t t2 = y12 & y15;
   const uint32_t t3 = y3 & y6;
   const uint32_t t4 = t3 ^ t2;
   const uint32_t t5 = y4 & U7;
   const uint32_t t6 = t5 ^ t2;
   const uint32_t t7 = y13 & y16;
   const uint32_t t8 = y5 & y1;
   const uint32_t t9 = t8 ^ t7;
   const uint32_t t10 = y2 & y7;
   const uint32_t t11 = t10 ^ t7;
   const uint32_t t12 = y9 & y11;
   const uint32_t t13 = y14 & y17;
   const uint32_t t14 = t13 ^ t12;
   const uint32_t t15 = y8 & y10;
   const uint32_t t16 = t15 ^ t12;
   const uint32_t t17 = t4 ^ y20;
   const uint32_t t18 = t6 ^ t16;
   const uint32_t t19 = t9 ^ t14;
   const uint32_t t20 = t11 ^ t16;
   const uint32_t t21 = t17 ^ t14;
   const uint32_t t22 = t18 ^ y19;
   const uint32_t t23 = t19 ^ y21;
   const uint32_t t24 = t20 ^ y18;
   const uint32_t t25 = t21 ^ t22;
   const uint32_t t26 = t21 & t23;
   const uint32_t t27 = t24 ^ t26;
   const uint32_t t28 = t25 & t27;
   const uint32_t t29 = t28 ^ t22;
   const uint32_t t30 = t23 ^ t24;
   const uint32_t t31 = t22 ^ t26;
   const uint32_t t32 = t31 & t30;
   const uint32_t t33 = t32 ^ t24;
   const uint32_t t34 = t23 ^ t33;
   const uint32_t t35 = t27 ^ t33;
   const uint32_t t36 = t24 & t35;
   const uint32_t t37 = t36 ^ t34;
   const uint32_t t38 = t27 ^ t36;
   const uint32_t t39 = t29 & t38;
   const uint32_t t40 = t25 ^ t39;
   const uint32_t t41 = t40 ^ t37;
   const uint32_t t42 = t29 ^ t33;
   const uint32_t t43 = t29 ^ t40;
   const uint32_t t44 = t33 ^ t37;
   const uint32_t t45 = t42 ^ t41;
   const uint32_t z0 = t44 & y15;
   const uint32_t z1 = t37 & y6;
   const uint32_t z2 = t33 & U7;
   const uint32_t z3 = t43 & y16;
   const uint32_t z4 = t40 & y1;
   const uint32_t z5 = t29 & y7;
   const uint32_t z6 = t42 & y11;
   const uint32_t z7 = t45 & y17;
   const uint32_t z8 = t41 & y10;
   const uint32_t z9 = t44 & y12;
   const uint32_t z10 = t37 & y3;
   const uint32_t z11 = t33 & y4;
   const uint32_t z12 = t43 & y13;
   const uint32_t z13 = t40 & y5;
   const uint32_t z14 = t29 & y2;
   const uint32_t z15 = t42 & y9;
   const uint32_t z16 = t45 & y14;
   const uint32_t z17 = t41 & y8;
   const uint32_t tc1 = z15 ^ z16;
   const uint32_t tc2 = z10 ^ tc1;
   const uint32_t tc3 = z9 ^ tc2;
   const uint32_t tc4 = z0 ^ z2;
   const uint32_t tc5 = z1 ^ z0;
   const uint32_t tc6 = z3 ^ z4;
   const uint32_t tc7 = z12 ^ tc4;
   const uint32_t tc8 = z7 ^ tc6;
   const uint32_t tc9 = z8 ^ tc7;
   const uint32_t tc10 = tc8 ^ tc9;
   const uint32_t tc11 = tc6 ^ tc5;
   const uint32_t tc12 = z3 ^ z5;
   const uint32_t tc13 = z13 ^ tc1;
   const uint32_t tc14 = tc4 ^ tc12;
   const uint32_t S3 = tc3 ^ tc11;
   const uint32_t tc16 = z6 ^ tc8;
   const uint32_t tc17 = z14 ^ tc10;
   const uint32_t tc18 = ~tc13 ^ tc14;
   const uint32_t S7 = z12 ^ tc18;
   const uint32_t tc20 = z15 ^ tc16;
   const uint32_t tc21 = tc2 ^ z11;
   const uint32_t S0 = tc3 ^ tc16;
   const uint32_t S6 = tc10 ^ tc18;
   const uint32_t S4 = tc14 ^ S3;
   const uint32_t S1 = ~(S3 ^ tc16);
   const uint32_t tc26 = tc17 ^ tc20;
   const uint32_t S2 = ~(tc26 ^ z17);
   const uint32_t S5 = tc21 ^ tc17;

   V[0] = S0;
   V[1] = S1;
   V[2] = S2;
   V[3] = S3;
   V[4] = S4;
   V[5] = S5;
   V[6] = S6;
   V[7] = S7;
   }

/*
A circuit for inverse AES Sbox of size 121 and depth 21 from
http://www.cs.yale.edu/homes/peralta/CircuitStuff/CMT.html
http://www.cs.yale.edu/homes/peralta/CircuitStuff/Sinv.txt
*/
void AES_INV_SBOX(uint32_t V[8])
   {
   const uint32_t U0 = V[0];
   const uint32_t U1 = V[1];
   const uint32_t U2 = V[2];
   const uint32_t U3 = V[3];
   const uint32_t U4 = V[4];
   const uint32_t U5 = V[5];
   const uint32_t U6 = V[6];
   const uint32_t U7 = V[7];

   const uint32_t Y0 = U0 ^ U3;
   const uint32_t Y2 = ~(U1 ^ U3);
   const uint32_t Y4 = U0 ^ Y2;
   const uint32_t RTL0 = U6 ^ U7;
   const uint32_t Y1 = Y2 ^ RTL0;
   const uint32_t Y7 = ~(U2 ^ Y1);
   const uint32_t RTL1 = U3 ^ U4;
   const uint32_t Y6 = ~(U7 ^ RTL1);
   const uint32_t Y3 = Y1 ^ RTL1;
   const uint32_t RTL2 = ~(U0 ^ U2);
   const uint32_t Y5 = U5 ^ RTL2;
   const uint32_t sa1 = Y0 ^ Y2;
   const uint32_t sa0 = Y1 ^ Y3;
   const uint32_t sb1 = Y4 ^ Y6;
   const uint32_t sb0 = Y5 ^ Y7;
   const uint32_t ah = Y0 ^ Y1;
   const uint32_t al = Y2 ^ Y3;
   const uint32_t aa = sa0 ^ sa1;
   const uint32_t bh = Y4 ^ Y5;
   const uint32_t bl = Y6 ^ Y7;
   const uint32_t bb = sb0 ^ sb1;
   const uint32_t ab20 = sa0 ^ sb0;
   const uint32_t ab22 = al ^ bl;
   const uint32_t ab23 = Y3 ^ Y7;
   const uint32_t ab21 = sa1 ^ sb1;
   const uint32_t abcd1 = ah & bh;
   const uint32_t rr1 = Y0 & Y4;
   const uint32_t ph11 = ab20 ^ abcd1;
   const uint32_t t01 = Y1 & Y5;
   const uint32_t ph01 = t01 ^ abcd1;
   const uint32_t abcd2 = al & bl;
   const uint32_t r1 = Y2 & Y6;
   const uint32_t pl11 = ab22 ^ abcd2;
   const uint32_t r2 = Y3 & Y7;
   const uint32_t pl01 = r2 ^ abcd2;
   const uint32_t r3 = sa0 & sb0;
   const uint32_t vr1 = aa & bb;
   const uint32_t pr1 = vr1 ^ r3;
   const uint32_t wr1 = sa1 & sb1;
   const uint32_t qr1 = wr1 ^ r3;
   const uint32_t ab0 = ph11 ^ rr1;
   const uint32_t ab1 = ph01 ^ ab21;
   const uint32_t ab2 = pl11 ^ r1;
   const uint32_t ab3 = pl01 ^ qr1;
   const uint32_t cp1 = ab0 ^ pr1;
   const uint32_t cp2 = ab1 ^ qr1;
   const uint32_t cp3 = ab2 ^ pr1;
   const uint32_t cp4 = ab3 ^ ab23;
   const uint32_t tinv1 = cp3 ^ cp4;
   const uint32_t tinv2 = cp3 & cp1;
   const uint32_t tinv3 = cp2 ^ tinv2;
   const uint32_t tinv4 = cp1 ^ cp2;
   const uint32_t tinv5 = cp4 ^ tinv2;
   const uint32_t tinv6 = tinv5 & tinv4;
   const uint32_t tinv7 = tinv3 & tinv1;
   const uint32_t d2 = cp4 ^ tinv7;
   const uint32_t d0 = cp2 ^ tinv6;
   const uint32_t tinv8 = cp1 & cp4;
   const uint32_t tinv9 = tinv4 & tinv8;
   const uint32_t tinv10 = tinv4 ^ tinv2;
   const uint32_t d1 = tinv9 ^ tinv10;
   const uint32_t tinv11 = cp2 & cp3;
   const uint32_t tinv12 = tinv1 & tinv11;
   const uint32_t tinv13 = tinv1 ^ tinv2;
   const uint32_t d3 = tinv12 ^ tinv13;
   const uint32_t sd1 = d1 ^ d3;
   const uint32_t sd0 = d0 ^ d2;
   const uint32_t dl = d0 ^ d1;
   const uint32_t dh = d2 ^ d3;
   const uint32_t dd = sd0 ^ sd1;
   const uint32_t abcd3 = dh & bh;
   const uint32_t rr2 = d3 & Y4;
   const uint32_t t02 = d2 & Y5;
   const uint32_t abcd4 = dl & bl;
   const uint32_t r4 = d1 & Y6;
   const uint32_t r5 = d0 & Y7;
   const uint32_t r6 = sd0 & sb0;
   const uint32_t vr2 = dd & bb;
   const uint32_t wr2 = sd1 & sb1;
   const uint32_t abcd5 = dh & ah;
   const uint32_t r7 = d3 & Y0;
   const uint32_t r8 = d2 & Y1;
   const uint32_t abcd6 = dl & al;
   const uint32_t r9 = d1 & Y2;
   const uint32_t r10 = d0 & Y3;
   const uint32_t r11 = sd0 & sa0;
   const uint32_t vr3 = dd & aa;
   const uint32_t wr3 = sd1 & sa1;
   const uint32_t ph12 = rr2 ^ abcd3;
   const uint32_t ph02 = t02 ^ abcd3;
   const uint32_t pl12 = r4 ^ abcd4;
   const uint32_t pl02 = r5 ^ abcd4;
   const uint32_t pr2 = vr2 ^ r6;
   const uint32_t qr2 = wr2 ^ r6;
   const uint32_t p0 = ph12 ^ pr2;
   const uint32_t p1 = ph02 ^ qr2;
   const uint32_t p2 = pl12 ^ pr2;
   const uint32_t p3 = pl02 ^ qr2;
   const uint32_t ph13 = r7 ^ abcd5;
   const uint32_t ph03 = r8 ^ abcd5;
   const uint32_t pl13 = r9 ^ abcd6;
   const uint32_t pl03 = r10 ^ abcd6;
   const uint32_t pr3 = vr3 ^ r11;
   const uint32_t qr3 = wr3 ^ r11;
   const uint32_t p4 = ph13 ^ pr3;
   const uint32_t S7 = ph03 ^ qr3;
   const uint32_t p6 = pl13 ^ pr3;
   const uint32_t p7 = pl03 ^ qr3;
   const uint32_t S3 = p1 ^ p6;
   const uint32_t S6 = p2 ^ p6;
   const uint32_t S0 = p3 ^ p6;
   const uint32_t X11 = p0 ^ p2;
   const uint32_t S5 = S0 ^ X11;
   const uint32_t X13 = p4 ^ p7;
   const uint32_t X14 = X11 ^ X13;
   const uint32_t S1 = S3 ^ X14;
   const uint32_t X16 = p1 ^ S7;
   const uint32_t S2 = X14 ^ X16;
   const uint32_t X18 = p0 ^ p4;
   const uint32_t X19 = S5 ^ X16;
   const uint32_t S4 = X18 ^ X19;

   V[0] = S0;
   V[1] = S1;
   V[2] = S2;
   V[3] = S3;
   V[4] = S4;
   V[5] = S5;
   V[6] = S6;
   V[7] = S7;
   }

inline void bit_transpose(uint32_t B[8])
   {
   swap_bits<uint32_t>(B[1], B[0], 0x55555555, 1);
   swap_bits<uint32_t>(B[3], B[2], 0x55555555, 1);
   swap_bits<uint32_t>(B[5], B[4], 0x55555555, 1);
   swap_bits<uint32_t>(B[7], B[6], 0x55555555, 1);

   swap_bits<uint32_t>(B[2], B[0], 0x33333333, 2);
   swap_bits<uint32_t>(B[3], B[1], 0x33333333, 2);
   swap_bits<uint32_t>(B[6], B[4], 0x33333333, 2);
   swap_bits<uint32_t>(B[7], B[5], 0x33333333, 2);

   swap_bits<uint32_t>(B[4], B[0], 0x0F0F0F0F, 4);
   swap_bits<uint32_t>(B[5], B[1], 0x0F0F0F0F, 4);
   swap_bits<uint32_t>(B[6], B[2], 0x0F0F0F0F, 4);
   swap_bits<uint32_t>(B[7], B[3], 0x0F0F0F0F, 4);
   }

inline void ks_expand(uint32_t B[8], const uint32_t K[], size_t r)
   {
   /*
   This is bit_transpose of K[r..r+4] || K[r..r+4], we can save some computation
   due to knowing the first and second halves are the same data.
   */
   for(size_t i = 0; i != 4; ++i)
      B[i] = K[r + i];

   swap_bits<uint32_t>(B[1], B[0], 0x55555555, 1);
   swap_bits<uint32_t>(B[3], B[2], 0x55555555, 1);

   swap_bits<uint32_t>(B[2], B[0], 0x33333333, 2);
   swap_bits<uint32_t>(B[3], B[1], 0x33333333, 2);

   B[4] = B[0];
   B[5] = B[1];
   B[6] = B[2];
   B[7] = B[3];

   swap_bits<uint32_t>(B[4], B[0], 0x0F0F0F0F, 4);
   swap_bits<uint32_t>(B[5], B[1], 0x0F0F0F0F, 4);
   swap_bits<uint32_t>(B[6], B[2], 0x0F0F0F0F, 4);
   swap_bits<uint32_t>(B[7], B[3], 0x0F0F0F0F, 4);
   }

inline void shift_rows(uint32_t B[8])
   {
   // 3 0 1 2 7 4 5 6 10 11 8 9 14 15 12 13 17 18 19 16 21 22 23 20 24 25 26 27 28 29 30 31
#if defined(BOTAN_TARGET_CPU_HAS_NATIVE_64BIT)
   for(size_t i = 0; i != 8; i += 2)
      {
      uint64_t x = (static_cast<uint64_t>(B[i]) << 32) | B[i+1];
      x = bit_permute_step<uint64_t>(x, 0x0022331100223311, 2);
      x = bit_permute_step<uint64_t>(x, 0x0055005500550055, 1);
      B[i] = static_cast<uint32_t>(x >> 32);
      B[i+1] = static_cast<uint32_t>(x);
      }
#else
   for(size_t i = 0; i != 8; ++i)
      {
      uint32_t x = B[i];
      x = bit_permute_step<uint32_t>(x, 0x00223311, 2);
      x = bit_permute_step<uint32_t>(x, 0x00550055, 1);
      B[i] = x;
      }
#endif
   }

inline void inv_shift_rows(uint32_t B[8])
   {
   // Inverse of shift_rows, just inverting the steps

#if defined(BOTAN_TARGET_CPU_HAS_NATIVE_64BIT)
   for(size_t i = 0; i != 8; i += 2)
      {
      uint64_t x = (static_cast<uint64_t>(B[i]) << 32) | B[i+1];
      x = bit_permute_step<uint64_t>(x, 0x0055005500550055, 1);
      x = bit_permute_step<uint64_t>(x, 0x0022331100223311, 2);
      B[i] = static_cast<uint32_t>(x >> 32);
      B[i+1] = static_cast<uint32_t>(x);
      }
#else
   for(size_t i = 0; i != 8; ++i)
      {
      uint32_t x = B[i];
      x = bit_permute_step<uint32_t>(x, 0x00550055, 1);
      x = bit_permute_step<uint32_t>(x, 0x00223311, 2);
      B[i] = x;
      }
#endif
   }

inline void mix_columns(uint32_t B[8])
   {
   // carry high bits in B[0] to positions in 0x1b == 0b11011
   const uint32_t X2[8] = {
      B[1],
      B[2],
      B[3],
      B[4] ^ B[0],
      B[5] ^ B[0],
      B[6],
      B[7] ^ B[0],
      B[0],
   };

   for(size_t i = 0; i != 8; i++)
      {
      const uint32_t X3 = B[i] ^ X2[i];
      B[i] = X2[i] ^ rotr<8>(B[i]) ^ rotr<16>(B[i]) ^ rotr<24>(X3);
      }
   }

void inv_mix_columns(uint32_t B[8])
   {
   /*
   OpenSSL's bsaes implementation credits Jussi Kivilinna with the lovely
   matrix decomposition

   | 0e 0b 0d 09 |   | 02 03 01 01 |   | 05 00 04 00 |
   | 09 0e 0b 0d | = | 01 02 03 01 | x | 00 05 00 04 |
   | 0d 09 0e 0b |   | 01 01 02 03 |   | 04 00 05 00 |
   | 0b 0d 09 0e |   | 03 01 01 02 |   | 00 04 00 05 |

   Notice the first component is simply the MixColumns matrix. So we can
   multiply first by (05,00,04,00) then perform MixColumns to get the equivalent
   of InvMixColumn.
   */
   const uint32_t X4[8] = {
      B[2],
      B[3],
      B[4] ^ B[0],
      B[5] ^ B[0] ^ B[1],
      B[6] ^ B[1],
      B[7] ^ B[0],
      B[0] ^ B[1],
      B[1],
   };

   for(size_t i = 0; i != 8; i++)
      {
      const uint32_t X5 = X4[i] ^ B[i];
      B[i] = X5 ^ rotr<16>(X4[i]);
      }

   mix_columns(B);
   }

/*
* AES Encryption
*/
void aes_encrypt_n(const uint8_t in[], uint8_t out[],
                   size_t blocks,
                   const secure_vector<uint32_t>& EK)
   {
   BOTAN_ASSERT(EK.size() == 44 || EK.size() == 52 || EK.size() == 60, "Key was set");

   const size_t rounds = (EK.size() - 4) / 4;

   uint32_t KS[13*8] = { 0 }; // actual maximum is (rounds - 1) * 8
   for(size_t i = 0; i < rounds - 1; i += 1)
      {
      ks_expand(&KS[8*i], EK.data(), 4*i + 4);
      }

   const size_t BLOCK_SIZE = 16;
   const size_t BITSLICED_BLOCKS = 8*sizeof(uint32_t) / BLOCK_SIZE;

   while(blocks > 0)
      {
      const size_t this_loop = std::min(blocks, BITSLICED_BLOCKS);

      uint32_t B[8] = { 0 };

      load_be(B, in, this_loop*4);

      for(size_t i = 0; i != 8; ++i)
         B[i] ^= EK[i % 4];

      bit_transpose(B);

      for(size_t r = 0; r != rounds - 1; ++r)
         {
         AES_SBOX(B);
         shift_rows(B);
         mix_columns(B);

         for(size_t i = 0; i != 8; ++i)
            B[i] ^= KS[8*r + i];
         }

      // Final round:
      AES_SBOX(B);
      shift_rows(B);
      bit_transpose(B);

      for(size_t i = 0; i != 8; ++i)
         B[i] ^= EK[4*rounds + i % 4];

      copy_out_be(out, this_loop*4*sizeof(uint32_t), B);

      in += this_loop * BLOCK_SIZE;
      out += this_loop * BLOCK_SIZE;
      blocks -= this_loop;
      }
   }

/*
* AES Decryption
*/
void aes_decrypt_n(const uint8_t in[], uint8_t out[], size_t blocks,
                   const secure_vector<uint32_t>& DK)
   {
   BOTAN_ASSERT(DK.size() == 44 || DK.size() == 52 || DK.size() == 60, "Key was set");

   const size_t rounds = (DK.size() - 4) / 4;

   uint32_t KS[13*8] = { 0 }; // actual maximum is (rounds - 1) * 8
   for(size_t i = 0; i < rounds - 1; i += 1)
      {
      ks_expand(&KS[8*i], DK.data(), 4*i + 4);
      }

   const size_t BLOCK_SIZE = 16;
   const size_t BITSLICED_BLOCKS = 8*sizeof(uint32_t) / BLOCK_SIZE;

   while(blocks > 0)
      {
      const size_t this_loop = std::min(blocks, BITSLICED_BLOCKS);

      uint32_t B[8] = { 0 };

      load_be(B, in, this_loop*4);

      for(size_t i = 0; i != 8; ++i)
         B[i] ^= DK[i % 4];

      bit_transpose(B);

      for(size_t r = 0; r != rounds - 1; ++r)
         {
         AES_INV_SBOX(B);
         inv_shift_rows(B);
         inv_mix_columns(B);

         for(size_t i = 0; i != 8; ++i)
            B[i] ^= KS[8*r + i];
         }

      // Final round:
      AES_INV_SBOX(B);
      inv_shift_rows(B);
      bit_transpose(B);

      for(size_t i = 0; i != 8; ++i)
         B[i] ^= DK[4*rounds + i % 4];

      copy_out_be(out, this_loop*4*sizeof(uint32_t), B);

      in += this_loop * BLOCK_SIZE;
      out += this_loop * BLOCK_SIZE;
      blocks -= this_loop;
      }
   }

inline uint32_t xtime32(uint32_t s)
   {
   const uint32_t lo_bit = 0x01010101;
   const uint32_t mask = 0x7F7F7F7F;
   const uint32_t poly = 0x1B;

   return ((s & mask) << 1) ^ (((s >> 7) & lo_bit) * poly);
   }

inline uint32_t InvMixColumn(uint32_t s1)
   {
   const uint32_t s2 = xtime32(s1);
   const uint32_t s4 = xtime32(s2);
   const uint32_t s8 = xtime32(s4);
   const uint32_t s9 = s8 ^ s1;
   const uint32_t s11 = s9 ^ s2;
   const uint32_t s13 = s9 ^ s4;
   const uint32_t s14 = s8 ^ s4 ^ s2;

   return s14 ^ rotr<8>(s9) ^ rotr<16>(s13) ^ rotr<24>(s11);
   }

void InvMixColumn_x4(uint32_t x[4])
   {
   x[0] = InvMixColumn(x[0]);
   x[1] = InvMixColumn(x[1]);
   x[2] = InvMixColumn(x[2]);
   x[3] = InvMixColumn(x[3]);
   }

uint32_t SE_word(uint32_t x)
   {
   uint32_t I[8] = { 0 };

   for(size_t i = 0; i != 8; ++i)
      I[i] = (x >> (7-i)) & 0x01010101;

   AES_SBOX(I);

   x = 0;

   for(size_t i = 0; i != 8; ++i)
      x |= ((I[i] & 0x01010101) << (7-i));

   return x;
   }

void aes_key_schedule(const uint8_t key[], size_t length,
                      secure_vector<uint32_t>& EK,
                      secure_vector<uint32_t>& DK,
                      bool bswap_keys = false)
   {
   static const uint32_t RC[10] = {
      0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000,
      0x20000000, 0x40000000, 0x80000000, 0x1B000000, 0x36000000 };

   const size_t X = length / 4;

   // Can't happen, but make static analyzers happy
   BOTAN_ASSERT_NOMSG(X == 4 || X == 6 || X == 8);

   const size_t rounds = (length / 4) + 6;

   // Help the optimizer
   BOTAN_ASSERT_NOMSG(rounds == 10 || rounds == 12 || rounds == 14);

   CT::poison(key, length);

   EK.resize(length + 28);
   DK.resize(length + 28);

   for(size_t i = 0; i != X; ++i)
      EK[i] = load_be<uint32_t>(key, i);

   for(size_t i = X; i < 4*(rounds+1); i += X)
      {
      EK[i] = EK[i-X] ^ RC[(i-X)/X] ^ rotl<8>(SE_word(EK[i-1]));

      for(size_t j = 1; j != X && (i+j) < EK.size(); ++j)
         {
         EK[i+j] = EK[i+j-X];

         if(X == 8 && j == 4)
            EK[i+j] ^= SE_word(EK[i+j-1]);
         else
            EK[i+j] ^= EK[i+j-1];
         }
      }

   for(size_t i = 0; i != 4*(rounds+1); i += 4)
      {
      DK[i  ] = EK[4*rounds - i  ];
      DK[i+1] = EK[4*rounds - i+1];
      DK[i+2] = EK[4*rounds - i+2];
      DK[i+3] = EK[4*rounds - i+3];
      }

   for(size_t i = 4; i != 4*rounds; i += 4)
      {
      InvMixColumn_x4(&DK[i]);
      }

   if(bswap_keys)
      {
      // HW AES on little endian needs the subkeys to be byte reversed
      for(size_t i = 0; i != EK.size(); ++i)
         EK[i] = reverse_bytes(EK[i]);
      for(size_t i = 0; i != DK.size(); ++i)
         DK[i] = reverse_bytes(DK[i]);
      }

   CT::unpoison(EK.data(), EK.size());
   CT::unpoison(DK.data(), DK.size());
   CT::unpoison(key, length);
   }

size_t aes_parallelism()
   {
#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return 4; // pipelined
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return 2; // pipelined
      }
#endif

   // bitsliced:
   return 2;
   }

const char* aes_provider()
   {
#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return "cpu";
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return "vperm";
      }
#endif

   return "base";
   }

}

std::string AES_128::provider() const { return aes_provider(); }
std::string AES_192::provider() const { return aes_provider(); }
std::string AES_256::provider() const { return aes_provider(); }

size_t AES_128::parallelism() const { return aes_parallelism(); }
size_t AES_192::parallelism() const { return aes_parallelism(); }
size_t AES_256::parallelism() const { return aes_parallelism(); }

void AES_128::encrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_EK.empty() == false);

#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return hw_aes_encrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_encrypt_n(in, out, blocks);
      }
#endif

   aes_encrypt_n(in, out, blocks, m_EK);
   }

void AES_128::decrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_DK.empty() == false);

#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return hw_aes_decrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_decrypt_n(in, out, blocks);
      }
#endif

   aes_decrypt_n(in, out, blocks, m_DK);
   }

void AES_128::key_schedule(const uint8_t key[], size_t length)
   {
#if defined(BOTAN_HAS_AES_NI)
   if(CPUID::has_aes_ni())
      {
      return aesni_key_schedule(key, length);
      }
#endif

#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return aes_key_schedule(key, length, m_EK, m_DK, CPUID::is_little_endian());
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_key_schedule(key, length);
      }
#endif

   aes_key_schedule(key, length, m_EK, m_DK);
   }

void AES_128::clear()
   {
   zap(m_EK);
   zap(m_DK);
   }

void AES_192::encrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_EK.empty() == false);

#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return hw_aes_encrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_encrypt_n(in, out, blocks);
      }
#endif

   aes_encrypt_n(in, out, blocks, m_EK);
   }

void AES_192::decrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_DK.empty() == false);

#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return hw_aes_decrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_decrypt_n(in, out, blocks);
      }
#endif

   aes_decrypt_n(in, out, blocks, m_DK);
   }

void AES_192::key_schedule(const uint8_t key[], size_t length)
   {
#if defined(BOTAN_HAS_AES_NI)
   if(CPUID::has_aes_ni())
      {
      return aesni_key_schedule(key, length);
      }
#endif

#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return aes_key_schedule(key, length, m_EK, m_DK, CPUID::is_little_endian());
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_key_schedule(key, length);
      }
#endif

   aes_key_schedule(key, length, m_EK, m_DK);
   }

void AES_192::clear()
   {
   zap(m_EK);
   zap(m_DK);
   }

void AES_256::encrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_EK.empty() == false);

#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return hw_aes_encrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_encrypt_n(in, out, blocks);
      }
#endif

   aes_encrypt_n(in, out, blocks, m_EK);
   }

void AES_256::decrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_DK.empty() == false);

#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return hw_aes_decrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_decrypt_n(in, out, blocks);
      }
#endif

   aes_decrypt_n(in, out, blocks, m_DK);
   }

void AES_256::key_schedule(const uint8_t key[], size_t length)
   {
#if defined(BOTAN_HAS_AES_NI)
   if(CPUID::has_aes_ni())
      {
      return aesni_key_schedule(key, length);
      }
#endif

#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return aes_key_schedule(key, length, m_EK, m_DK, CPUID::is_little_endian());
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_key_schedule(key, length);
      }
#endif

   aes_key_schedule(key, length, m_EK, m_DK);
   }

void AES_256::clear()
   {
   zap(m_EK);
   zap(m_DK);
   }

}
/*
* (C) 2019 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

void Buffered_Computation::update_be(uint16_t val)
   {
   uint8_t inb[sizeof(val)];
   store_be(val, inb);
   add_data(inb, sizeof(inb));
   }

void Buffered_Computation::update_be(uint32_t val)
   {
   uint8_t inb[sizeof(val)];
   store_be(val, inb);
   add_data(inb, sizeof(inb));
   }

void Buffered_Computation::update_be(uint64_t val)
   {
   uint8_t inb[sizeof(val)];
   store_be(val, inb);
   add_data(inb, sizeof(inb));
   }

void Buffered_Computation::update_le(uint16_t val)
   {
   uint8_t inb[sizeof(val)];
   store_le(val, inb);
   add_data(inb, sizeof(inb));
   }

void Buffered_Computation::update_le(uint32_t val)
   {
   uint8_t inb[sizeof(val)];
   store_le(val, inb);
   add_data(inb, sizeof(inb));
   }

void Buffered_Computation::update_le(uint64_t val)
   {
   uint8_t inb[sizeof(val)];
   store_le(val, inb);
   add_data(inb, sizeof(inb));
   }

}
/*
* SCAN Name Abstraction
* (C) 2008-2009,2015 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

std::string make_arg(const std::vector<std::pair<size_t, std::string>>& name, size_t start)
   {
   std::string output = name[start].second;
   size_t level = name[start].first;

   size_t paren_depth = 0;

   for(size_t i = start + 1; i != name.size(); ++i)
      {
      if(name[i].first <= name[start].first)
         break;

      if(name[i].first > level)
         {
         output += "(" + name[i].second;
         ++paren_depth;
         }
      else if(name[i].first < level)
         {
         for (size_t j = name[i].first; j < level; j++) {
            output += ")";
            --paren_depth;
         }
         output += "," + name[i].second;
         }
      else
         {
         if(output[output.size() - 1] != '(')
            output += ",";
         output += name[i].second;
         }

      level = name[i].first;
      }

   for(size_t i = 0; i != paren_depth; ++i)
      output += ")";

   return output;
   }

}

SCAN_Name::SCAN_Name(const char* algo_spec) : SCAN_Name(std::string(algo_spec))
   {
   }

SCAN_Name::SCAN_Name(std::string algo_spec) : m_orig_algo_spec(algo_spec), m_alg_name(), m_args(), m_mode_info()
   { 
   if(algo_spec.size() == 0)
      throw Invalid_Argument("Expected algorithm name, got empty string");

   std::vector<std::pair<size_t, std::string>> name;
   size_t level = 0;
   std::pair<size_t, std::string> accum = std::make_pair(level, "");

   const std::string decoding_error = "Bad SCAN name '" + algo_spec + "': ";

   for(size_t i = 0; i != algo_spec.size(); ++i)
      {
      char c = algo_spec[i];

      if(c == '/' || c == ',' || c == '(' || c == ')')
         {
         if(c == '(')
            ++level;
         else if(c == ')')
            {
            if(level == 0)
               throw Decoding_Error(decoding_error + "Mismatched parens");
            --level;
            }

         if(c == '/' && level > 0)
            accum.second.push_back(c);
         else
            {
            if(accum.second != "")
               name.push_back(accum);
            accum = std::make_pair(level, "");
            }
         }
      else
         accum.second.push_back(c);
      }

   if(accum.second != "")
      name.push_back(accum);

   if(level != 0)
      throw Decoding_Error(decoding_error + "Missing close paren");

   if(name.size() == 0)
      throw Decoding_Error(decoding_error + "Empty name");

   m_alg_name = name[0].second;

   bool in_modes = false;

   for(size_t i = 1; i != name.size(); ++i)
      {
      if(name[i].first == 0)
         {
         m_mode_info.push_back(make_arg(name, i));
         in_modes = true;
         }
      else if(name[i].first == 1 && !in_modes)
         m_args.push_back(make_arg(name, i));
      }
   }

std::string SCAN_Name::arg(size_t i) const
   {
   if(i >= arg_count())
      throw Invalid_Argument("SCAN_Name::arg " + std::to_string(i) +
                             " out of range for '" + to_string() + "'");
   return m_args[i];
   }

std::string SCAN_Name::arg(size_t i, const std::string& def_value) const
   {
   if(i >= arg_count())
      return def_value;
   return m_args[i];
   }

size_t SCAN_Name::arg_as_integer(size_t i, size_t def_value) const
   {
   if(i >= arg_count())
      return def_value;
   return to_u32bit(m_args[i]);
   }

}
/*
* (C) 2018 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

void SymmetricAlgorithm::throw_key_not_set_error() const
   {
   throw Key_Not_Set(name());
   }

void SymmetricAlgorithm::set_key(const uint8_t key[], size_t length)
   {
   if(!valid_keylength(length))
      throw Invalid_Key_Length(name(), length);
   key_schedule(key, length);
   }

}
/*
* OctetString
* (C) 1999-2007 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <algorithm>

namespace Botan {

/*
* Create an OctetString from RNG output
*/
OctetString::OctetString(RandomNumberGenerator& rng,
                         size_t len)
   {
   rng.random_vec(m_data, len);
   }

/*
* Create an OctetString from a hex string
*/
OctetString::OctetString(const std::string& hex_string)
   {
   if(!hex_string.empty())
      {
      m_data.resize(1 + hex_string.length() / 2);
      m_data.resize(hex_decode(m_data.data(), hex_string));
      }
   }

/*
* Create an OctetString from a byte string
*/
OctetString::OctetString(const uint8_t in[], size_t n)
   {
   m_data.assign(in, in + n);
   }

/*
* Set the parity of each key byte to odd
*/
void OctetString::set_odd_parity()
   {
   const uint8_t ODD_PARITY[256] = {
      0x01, 0x01, 0x02, 0x02, 0x04, 0x04, 0x07, 0x07, 0x08, 0x08, 0x0B, 0x0B,
      0x0D, 0x0D, 0x0E, 0x0E, 0x10, 0x10, 0x13, 0x13, 0x15, 0x15, 0x16, 0x16,
      0x19, 0x19, 0x1A, 0x1A, 0x1C, 0x1C, 0x1F, 0x1F, 0x20, 0x20, 0x23, 0x23,
      0x25, 0x25, 0x26, 0x26, 0x29, 0x29, 0x2A, 0x2A, 0x2C, 0x2C, 0x2F, 0x2F,
      0x31, 0x31, 0x32, 0x32, 0x34, 0x34, 0x37, 0x37, 0x38, 0x38, 0x3B, 0x3B,
      0x3D, 0x3D, 0x3E, 0x3E, 0x40, 0x40, 0x43, 0x43, 0x45, 0x45, 0x46, 0x46,
      0x49, 0x49, 0x4A, 0x4A, 0x4C, 0x4C, 0x4F, 0x4F, 0x51, 0x51, 0x52, 0x52,
      0x54, 0x54, 0x57, 0x57, 0x58, 0x58, 0x5B, 0x5B, 0x5D, 0x5D, 0x5E, 0x5E,
      0x61, 0x61, 0x62, 0x62, 0x64, 0x64, 0x67, 0x67, 0x68, 0x68, 0x6B, 0x6B,
      0x6D, 0x6D, 0x6E, 0x6E, 0x70, 0x70, 0x73, 0x73, 0x75, 0x75, 0x76, 0x76,
      0x79, 0x79, 0x7A, 0x7A, 0x7C, 0x7C, 0x7F, 0x7F, 0x80, 0x80, 0x83, 0x83,
      0x85, 0x85, 0x86, 0x86, 0x89, 0x89, 0x8A, 0x8A, 0x8C, 0x8C, 0x8F, 0x8F,
      0x91, 0x91, 0x92, 0x92, 0x94, 0x94, 0x97, 0x97, 0x98, 0x98, 0x9B, 0x9B,
      0x9D, 0x9D, 0x9E, 0x9E, 0xA1, 0xA1, 0xA2, 0xA2, 0xA4, 0xA4, 0xA7, 0xA7,
      0xA8, 0xA8, 0xAB, 0xAB, 0xAD, 0xAD, 0xAE, 0xAE, 0xB0, 0xB0, 0xB3, 0xB3,
      0xB5, 0xB5, 0xB6, 0xB6, 0xB9, 0xB9, 0xBA, 0xBA, 0xBC, 0xBC, 0xBF, 0xBF,
      0xC1, 0xC1, 0xC2, 0xC2, 0xC4, 0xC4, 0xC7, 0xC7, 0xC8, 0xC8, 0xCB, 0xCB,
      0xCD, 0xCD, 0xCE, 0xCE, 0xD0, 0xD0, 0xD3, 0xD3, 0xD5, 0xD5, 0xD6, 0xD6,
      0xD9, 0xD9, 0xDA, 0xDA, 0xDC, 0xDC, 0xDF, 0xDF, 0xE0, 0xE0, 0xE3, 0xE3,
      0xE5, 0xE5, 0xE6, 0xE6, 0xE9, 0xE9, 0xEA, 0xEA, 0xEC, 0xEC, 0xEF, 0xEF,
      0xF1, 0xF1, 0xF2, 0xF2, 0xF4, 0xF4, 0xF7, 0xF7, 0xF8, 0xF8, 0xFB, 0xFB,
      0xFD, 0xFD, 0xFE, 0xFE };

   for(size_t j = 0; j != m_data.size(); ++j)
      m_data[j] = ODD_PARITY[m_data[j]];
   }

/*
* Hex encode an OctetString
*/
std::string OctetString::to_string() const
   {
   return hex_encode(m_data.data(), m_data.size());
   }

/*
* XOR Operation for OctetStrings
*/
OctetString& OctetString::operator^=(const OctetString& k)
   {
   if(&k == this) { zeroise(m_data); return (*this); }
   xor_buf(m_data.data(), k.begin(), std::min(length(), k.length()));
   return (*this);
   }

/*
* Equality Operation for OctetStrings
*/
bool operator==(const OctetString& s1, const OctetString& s2)
   {
   return (s1.bits_of() == s2.bits_of());
   }

/*
* Unequality Operation for OctetStrings
*/
bool operator!=(const OctetString& s1, const OctetString& s2)
   {
   return !(s1 == s2);
   }

/*
* Append Operation for OctetStrings
*/
OctetString operator+(const OctetString& k1, const OctetString& k2)
   {
   secure_vector<uint8_t> out;
   out += k1.bits_of();
   out += k2.bits_of();
   return OctetString(out);
   }

/*
* XOR Operation for OctetStrings
*/
OctetString operator^(const OctetString& k1, const OctetString& k2)
   {
   secure_vector<uint8_t> out(std::max(k1.length(), k2.length()));

   copy_mem(out.data(), k1.begin(), k1.length());
   xor_buf(out.data(), k2.begin(), k2.length());
   return OctetString(out);
   }

}
/*
* Block Ciphers
* (C) 2015 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_AES)
#endif

#if defined(BOTAN_HAS_ARIA)
#endif

#if defined(BOTAN_HAS_BLOWFISH)
#endif

#if defined(BOTAN_HAS_CAMELLIA)
#endif

#if defined(BOTAN_HAS_CAST_128)
#endif

#if defined(BOTAN_HAS_CAST_256)
#endif

#if defined(BOTAN_HAS_CASCADE)
#endif

#if defined(BOTAN_HAS_DES)
#endif

#if defined(BOTAN_HAS_GOST_28147_89)
#endif

#if defined(BOTAN_HAS_IDEA)
#endif

#if defined(BOTAN_HAS_KASUMI)
#endif

#if defined(BOTAN_HAS_LION)
#endif

#if defined(BOTAN_HAS_MISTY1)
#endif

#if defined(BOTAN_HAS_NOEKEON)
#endif

#if defined(BOTAN_HAS_SEED)
#endif

#if defined(BOTAN_HAS_SERPENT)
#endif

#if defined(BOTAN_HAS_SHACAL2)
#endif

#if defined(BOTAN_HAS_SM4)
#endif

#if defined(BOTAN_HAS_TWOFISH)
#endif

#if defined(BOTAN_HAS_THREEFISH_512)
#endif

#if defined(BOTAN_HAS_XTEA)
#endif

#if defined(BOTAN_HAS_OPENSSL)
#endif

#if defined(BOTAN_HAS_COMMONCRYPTO)
#endif

namespace Botan {

std::unique_ptr<BlockCipher>
BlockCipher::create(const std::string& algo,
                    const std::string& provider)
   {
#if defined(BOTAN_HAS_COMMONCRYPTO)
   if(provider.empty() || provider == "commoncrypto")
      {
      if(auto bc = make_commoncrypto_block_cipher(algo))
         return bc;

      if(!provider.empty())
         return nullptr;
      }
#endif

#if defined(BOTAN_HAS_OPENSSL)
   if(provider.empty() || provider == "openssl")
      {
      if(auto bc = make_openssl_block_cipher(algo))
         return bc;

      if(!provider.empty())
         return nullptr;
      }
#endif

   // TODO: CryptoAPI
   // TODO: /dev/crypto

   // Only base providers from here on out
   if(provider.empty() == false && provider != "base")
      return nullptr;

#if defined(BOTAN_HAS_AES)
   if(algo == "AES-128")
      {
      return std::unique_ptr<BlockCipher>(new AES_128);
      }

   if(algo == "AES-192")
      {
      return std::unique_ptr<BlockCipher>(new AES_192);
      }

   if(algo == "AES-256")
      {
      return std::unique_ptr<BlockCipher>(new AES_256);
      }
#endif

#if defined(BOTAN_HAS_ARIA)
   if(algo == "ARIA-128")
      {
      return std::unique_ptr<BlockCipher>(new ARIA_128);
      }

   if(algo == "ARIA-192")
      {
      return std::unique_ptr<BlockCipher>(new ARIA_192);
      }

   if(algo == "ARIA-256")
      {
      return std::unique_ptr<BlockCipher>(new ARIA_256);
      }
#endif

#if defined(BOTAN_HAS_SERPENT)
   if(algo == "Serpent")
      {
      return std::unique_ptr<BlockCipher>(new Serpent);
      }
#endif

#if defined(BOTAN_HAS_SHACAL2)
   if(algo == "SHACAL2")
      {
      return std::unique_ptr<BlockCipher>(new SHACAL2);
      }
#endif

#if defined(BOTAN_HAS_TWOFISH)
   if(algo == "Twofish")
      {
      return std::unique_ptr<BlockCipher>(new Twofish);
      }
#endif

#if defined(BOTAN_HAS_THREEFISH_512)
   if(algo == "Threefish-512")
      {
      return std::unique_ptr<BlockCipher>(new Threefish_512);
      }
#endif

#if defined(BOTAN_HAS_BLOWFISH)
   if(algo == "Blowfish")
      {
      return std::unique_ptr<BlockCipher>(new Blowfish);
      }
#endif

#if defined(BOTAN_HAS_CAMELLIA)
   if(algo == "Camellia-128")
      {
      return std::unique_ptr<BlockCipher>(new Camellia_128);
      }

   if(algo == "Camellia-192")
      {
      return std::unique_ptr<BlockCipher>(new Camellia_192);
      }

   if(algo == "Camellia-256")
      {
      return std::unique_ptr<BlockCipher>(new Camellia_256);
      }
#endif

#if defined(BOTAN_HAS_DES)
   if(algo == "DES")
      {
      return std::unique_ptr<BlockCipher>(new DES);
      }

   if(algo == "DESX")
      {
      return std::unique_ptr<BlockCipher>(new DESX);
      }

   if(algo == "TripleDES" || algo == "3DES" || algo == "DES-EDE")
      {
      return std::unique_ptr<BlockCipher>(new TripleDES);
      }
#endif

#if defined(BOTAN_HAS_NOEKEON)
   if(algo == "Noekeon")
      {
      return std::unique_ptr<BlockCipher>(new Noekeon);
      }
#endif

#if defined(BOTAN_HAS_CAST_128)
   if(algo == "CAST-128" || algo == "CAST5")
      {
      return std::unique_ptr<BlockCipher>(new CAST_128);
      }
#endif

#if defined(BOTAN_HAS_CAST_256)
   if(algo == "CAST-256")
      {
      return std::unique_ptr<BlockCipher>(new CAST_256);
      }
#endif

#if defined(BOTAN_HAS_IDEA)
   if(algo == "IDEA")
      {
      return std::unique_ptr<BlockCipher>(new IDEA);
      }
#endif

#if defined(BOTAN_HAS_KASUMI)
   if(algo == "KASUMI")
      {
      return std::unique_ptr<BlockCipher>(new KASUMI);
      }
#endif

#if defined(BOTAN_HAS_MISTY1)
   if(algo == "MISTY1")
      {
      return std::unique_ptr<BlockCipher>(new MISTY1);
      }
#endif

#if defined(BOTAN_HAS_SEED)
   if(algo == "SEED")
      {
      return std::unique_ptr<BlockCipher>(new SEED);
      }
#endif

#if defined(BOTAN_HAS_SM4)
   if(algo == "SM4")
      {
      return std::unique_ptr<BlockCipher>(new SM4);
      }
#endif

#if defined(BOTAN_HAS_XTEA)
   if(algo == "XTEA")
      {
      return std::unique_ptr<BlockCipher>(new XTEA);
      }
#endif

   const SCAN_Name req(algo);

#if defined(BOTAN_HAS_GOST_28147_89)
   if(req.algo_name() == "GOST-28147-89")
      {
      return std::unique_ptr<BlockCipher>(new GOST_28147_89(req.arg(0, "R3411_94_TestParam")));
      }
#endif

#if defined(BOTAN_HAS_CASCADE)
   if(req.algo_name() == "Cascade" && req.arg_count() == 2)
      {
      std::unique_ptr<BlockCipher> c1(BlockCipher::create(req.arg(0)));
      std::unique_ptr<BlockCipher> c2(BlockCipher::create(req.arg(1)));

      if(c1 && c2)
         return std::unique_ptr<BlockCipher>(new Cascade_Cipher(c1.release(), c2.release()));
      }
#endif

#if defined(BOTAN_HAS_LION)
   if(req.algo_name() == "Lion" && req.arg_count_between(2, 3))
      {
      std::unique_ptr<HashFunction> hash(HashFunction::create(req.arg(0)));
      std::unique_ptr<StreamCipher> stream(StreamCipher::create(req.arg(1)));

      if(hash && stream)
         {
         const size_t block_size = req.arg_as_integer(2, 1024);
         return std::unique_ptr<BlockCipher>(new Lion(hash.release(), stream.release(), block_size));
         }
      }
#endif

   BOTAN_UNUSED(req);
   BOTAN_UNUSED(provider);

   return nullptr;
   }

//static
std::unique_ptr<BlockCipher>
BlockCipher::create_or_throw(const std::string& algo,
                             const std::string& provider)
   {
   if(auto bc = BlockCipher::create(algo, provider))
      {
      return bc;
      }
   throw Lookup_Error("Block cipher", algo, provider);
   }

std::vector<std::string> BlockCipher::providers(const std::string& algo)
   {
   return probe_providers_of<BlockCipher>(algo, { "base", "openssl", "commoncrypto" });
   }

}
/*
* Runtime CPU detection
* (C) 2009,2010,2013,2017 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <ostream>

namespace Botan {

bool CPUID::has_simd_32()
   {
#if defined(BOTAN_TARGET_SUPPORTS_SSE2)
   return CPUID::has_sse2();
#elif defined(BOTAN_TARGET_SUPPORTS_ALTIVEC)
   return CPUID::has_altivec();
#elif defined(BOTAN_TARGET_SUPPORTS_NEON)
   return CPUID::has_neon();
#else
   return true;
#endif
   }

//static
std::string CPUID::to_string()
   {
   std::vector<std::string> flags;

#define CPUID_PRINT(flag) do { if(has_##flag()) { flags.push_back(#flag); } } while(0)

#if defined(BOTAN_TARGET_CPU_IS_X86_FAMILY)
   CPUID_PRINT(sse2);
   CPUID_PRINT(ssse3);
   CPUID_PRINT(sse41);
   CPUID_PRINT(sse42);
   CPUID_PRINT(avx2);
   CPUID_PRINT(avx512f);
   CPUID_PRINT(avx512dq);
   CPUID_PRINT(avx512bw);
   CPUID_PRINT(avx512_icelake);

   CPUID_PRINT(rdtsc);
   CPUID_PRINT(bmi1);
   CPUID_PRINT(bmi2);
   CPUID_PRINT(adx);

   CPUID_PRINT(aes_ni);
   CPUID_PRINT(clmul);
   CPUID_PRINT(rdrand);
   CPUID_PRINT(rdseed);
   CPUID_PRINT(intel_sha);
   CPUID_PRINT(avx512_aes);
   CPUID_PRINT(avx512_clmul);
#endif

#if defined(BOTAN_TARGET_CPU_IS_PPC_FAMILY)
   CPUID_PRINT(altivec);
   CPUID_PRINT(power_crypto);
   CPUID_PRINT(darn_rng);
#endif

#if defined(BOTAN_TARGET_CPU_IS_ARM_FAMILY)
   CPUID_PRINT(neon);
   CPUID_PRINT(arm_sve);

   CPUID_PRINT(arm_sha1);
   CPUID_PRINT(arm_sha2);
   CPUID_PRINT(arm_aes);
   CPUID_PRINT(arm_pmull);
   CPUID_PRINT(arm_sha2_512);
   CPUID_PRINT(arm_sha3);
   CPUID_PRINT(arm_sm3);
   CPUID_PRINT(arm_sm4);
#endif

#undef CPUID_PRINT

   return string_join(flags, ' ');
   }

//static
void CPUID::print(std::ostream& o)
   {
   o << "CPUID flags: " << CPUID::to_string() << "\n";
   }

//static
void CPUID::initialize()
   {
   state() = CPUID_Data();
   }

CPUID::CPUID_Data::CPUID_Data()
   {
   m_cache_line_size = 0;
   m_processor_features = 0;

#if defined(BOTAN_TARGET_CPU_IS_PPC_FAMILY) || \
    defined(BOTAN_TARGET_CPU_IS_ARM_FAMILY) || \
    defined(BOTAN_TARGET_CPU_IS_X86_FAMILY)

   m_processor_features = detect_cpu_features(&m_cache_line_size);

#endif

   m_processor_features |= CPUID::CPUID_INITIALIZED_BIT;

   if(m_cache_line_size == 0)
      m_cache_line_size = BOTAN_TARGET_CPU_DEFAULT_CACHE_LINE_SIZE;

   m_endian_status = runtime_check_endian();
   }

//static
CPUID::Endian_Status CPUID::CPUID_Data::runtime_check_endian()
   {
   // Check runtime endian
   const uint32_t endian32 = 0x01234567;
   const uint8_t* e8 = reinterpret_cast<const uint8_t*>(&endian32);

   CPUID::Endian_Status endian = CPUID::Endian_Status::Unknown;

   if(e8[0] == 0x01 && e8[1] == 0x23 && e8[2] == 0x45 && e8[3] == 0x67)
      {
      endian = CPUID::Endian_Status::Big;
      }
   else if(e8[0] == 0x67 && e8[1] == 0x45 && e8[2] == 0x23 && e8[3] == 0x01)
      {
      endian = CPUID::Endian_Status::Little;
      }
   else
      {
      throw Internal_Error("Unexpected endian at runtime, neither big nor little");
      }

   // If we were compiled with a known endian, verify it matches at runtime
#if defined(BOTAN_TARGET_CPU_IS_LITTLE_ENDIAN)
   BOTAN_ASSERT(endian == CPUID::Endian_Status::Little, "Build and runtime endian match");
#elif defined(BOTAN_TARGET_CPU_IS_BIG_ENDIAN)
   BOTAN_ASSERT(endian == CPUID::Endian_Status::Big, "Build and runtime endian match");
#endif

   return endian;
   }

std::vector<Botan::CPUID::CPUID_bits>
CPUID::bit_from_string(const std::string& tok)
   {
#if defined(BOTAN_TARGET_CPU_IS_X86_FAMILY)
   if(tok == "sse2" || tok == "simd")
      return {Botan::CPUID::CPUID_SSE2_BIT};
   if(tok == "ssse3")
      return {Botan::CPUID::CPUID_SSSE3_BIT};
   if(tok == "sse41")
      return {Botan::CPUID::CPUID_SSE41_BIT};
   if(tok == "sse42")
      return {Botan::CPUID::CPUID_SSE42_BIT};
   // aes_ni is the string printed on the console when running "botan cpuid"
   if(tok == "aesni" || tok == "aes_ni")
      return {Botan::CPUID::CPUID_AESNI_BIT};
   if(tok == "clmul")
      return {Botan::CPUID::CPUID_CLMUL_BIT};
   if(tok == "avx2")
      return {Botan::CPUID::CPUID_AVX2_BIT};
   if(tok == "avx512f")
      return {Botan::CPUID::CPUID_AVX512F_BIT};
   if(tok == "avx512_icelake")
      return {Botan::CPUID::CPUID_AVX512_ICL_BIT};
   // there were two if statements testing "sha" and "intel_sha" separately; combined
   if(tok == "sha" || tok=="intel_sha")
      return {Botan::CPUID::CPUID_SHA_BIT};
   if(tok == "rdtsc")
      return {Botan::CPUID::CPUID_RDTSC_BIT};
   if(tok == "bmi1")
      return {Botan::CPUID::CPUID_BMI1_BIT};
   if(tok == "bmi2")
      return {Botan::CPUID::CPUID_BMI2_BIT};
   if(tok == "adx")
      return {Botan::CPUID::CPUID_ADX_BIT};
   if(tok == "rdrand")
      return {Botan::CPUID::CPUID_RDRAND_BIT};
   if(tok == "rdseed")
      return {Botan::CPUID::CPUID_RDSEED_BIT};
   if(tok == "avx512_aes")
      return {Botan::CPUID::CPUID_AVX512_AES_BIT};
   if(tok == "avx512_clmul")
      return {Botan::CPUID::CPUID_AVX512_CLMUL_BIT};

#elif defined(BOTAN_TARGET_CPU_IS_PPC_FAMILY)
   if(tok == "altivec" || tok == "simd")
      return {Botan::CPUID::CPUID_ALTIVEC_BIT};
   if(tok == "power_crypto")
      return {Botan::CPUID::CPUID_POWER_CRYPTO_BIT};
   if(tok == "darn_rng")
      return {Botan::CPUID::CPUID_DARN_BIT};

#elif defined(BOTAN_TARGET_CPU_IS_ARM_FAMILY)
   if(tok == "neon" || tok == "simd")
      return {Botan::CPUID::CPUID_ARM_NEON_BIT};
   if(tok == "arm_sve")
      return {Botan::CPUID::CPUID_ARM_SVE_BIT};
   if(tok == "armv8sha1" || tok == "arm_sha1")
      return {Botan::CPUID::CPUID_ARM_SHA1_BIT};
   if(tok == "armv8sha2" || tok == "arm_sha2")
      return {Botan::CPUID::CPUID_ARM_SHA2_BIT};
   if(tok == "armv8aes" || tok == "arm_aes")
      return {Botan::CPUID::CPUID_ARM_AES_BIT};
   if(tok == "armv8pmull" || tok == "arm_pmull")
      return {Botan::CPUID::CPUID_ARM_PMULL_BIT};
   if(tok == "armv8sha3" || tok == "arm_sha3")
      return {Botan::CPUID::CPUID_ARM_SHA3_BIT};
   if(tok == "armv8sha2_512" || tok == "arm_sha2_512")
      return {Botan::CPUID::CPUID_ARM_SHA2_512_BIT};
   if(tok == "armv8sm3" || tok == "arm_sm3")
      return {Botan::CPUID::CPUID_ARM_SM3_BIT};
   if(tok == "armv8sm4" || tok == "arm_sm4")
      return {Botan::CPUID::CPUID_ARM_SM4_BIT};

#else
   BOTAN_UNUSED(tok);
#endif

   return {};
   }

}
/*
* Runtime CPU detection for ARM
* (C) 2009,2010,2013,2017 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_TARGET_CPU_IS_ARM_FAMILY)

#if defined(BOTAN_TARGET_OS_IS_IOS)
  #include <sys/types.h>
  #include <sys/sysctl.h>

#else

#if defined(BOTAN_TARGET_OS_HAS_GETAUXVAL) || defined(BOTAN_TARGET_OS_HAS_ELF_AUX_INFO)
  #include <sys/auxv.h>
#endif

#endif

#endif

namespace Botan {

#if defined(BOTAN_TARGET_CPU_IS_ARM_FAMILY)

#if defined(BOTAN_TARGET_OS_IS_IOS)

namespace {

uint64_t flags_by_ios_machine_type(const std::string& machine)
   {
   /*
   * This relies on a map of known machine names to features. This
   * will quickly grow out of date as new products are introduced, but
   * is apparently the best we can do for iOS.
   */

   struct version_info {
      std::string name;
      size_t min_version_neon;
      size_t min_version_armv8;
      };

   static const version_info min_versions[] = {
      { "iPhone", 2, 6 },
      { "iPad", 1, 4 },
      { "iPod", 4, 7 },
      { "AppleTV", 2, 5 },
   };

   if(machine.size() < 3)
      return 0;

   auto comma = machine.find(',');

   // Simulator, or something we don't know about
   if(comma == std::string::npos)
      return 0;

   std::string product = machine.substr(0, comma);

   size_t version = 0;
   size_t place = 1;
   while(product.size() > 1 && ::isdigit(product.back()))
      {
      const size_t digit = product.back() - '0';
      version += digit * place;
      place *= 10;
      product.pop_back();
      }

   if(version == 0)
      return 0;

   for(const version_info& info : min_versions)
      {
      if(info.name != product)
         continue;

      if(version >= info.min_version_armv8)
         {
         return CPUID::CPUID_ARM_AES_BIT |
                CPUID::CPUID_ARM_PMULL_BIT |
                CPUID::CPUID_ARM_SHA1_BIT |
                CPUID::CPUID_ARM_SHA2_BIT |
                CPUID::CPUID_ARM_NEON_BIT;
         }

      if(version >= info.min_version_neon)
         return CPUID::CPUID_ARM_NEON_BIT;
      }

   // Some other product we don't know about
   return 0;
   }

}

#endif

uint64_t CPUID::CPUID_Data::detect_cpu_features(size_t* cache_line_size)
   {
   BOTAN_UNUSED(cache_line_size);

   uint64_t detected_features = 0;

#if defined(BOTAN_TARGET_OS_HAS_GETAUXVAL) || defined(BOTAN_TARGET_OS_HAS_ELF_AUX_INFO)
   /*
   * On systems with getauxval these bits should normally be defined
   * in bits/auxv.h but some buggy? glibc installs seem to miss them.
   * These following values are all fixed, for the Linux ELF format,
   * so we just hardcode them in ARM_hwcap_bit enum.
   */

   enum ARM_hwcap_bit {
#if defined(BOTAN_TARGET_ARCH_IS_ARM32)
      NEON_bit  = (1 << 12),
      AES_bit   = (1 << 0),
      PMULL_bit = (1 << 1),
      SHA1_bit  = (1 << 2),
      SHA2_bit  = (1 << 3),

      ARCH_hwcap_neon   = 16, // AT_HWCAP
      ARCH_hwcap_crypto = 26, // AT_HWCAP2
#elif defined(BOTAN_TARGET_ARCH_IS_ARM64)
      NEON_bit  = (1 << 1),
      AES_bit   = (1 << 3),
      PMULL_bit = (1 << 4),
      SHA1_bit  = (1 << 5),
      SHA2_bit  = (1 << 6),
      SHA3_bit  = (1 << 17),
      SM3_bit  = (1 << 18),
      SM4_bit  = (1 << 19),
      SHA2_512_bit  = (1 << 21),
      SVE_bit = (1 << 22),

      ARCH_hwcap_neon   = 16, // AT_HWCAP
      ARCH_hwcap_crypto = 16, // AT_HWCAP
#endif
   };

#if defined(AT_DCACHEBSIZE)
   // Exists only on Linux
   const unsigned long dcache_line = OS::get_auxval(AT_DCACHEBSIZE);

   // plausibility check
   if(dcache_line == 32 || dcache_line == 64 || dcache_line == 128)
      *cache_line_size = static_cast<size_t>(dcache_line);
#endif

   const unsigned long hwcap_neon = OS::get_auxval(ARM_hwcap_bit::ARCH_hwcap_neon);
   if(hwcap_neon & ARM_hwcap_bit::NEON_bit)
      detected_features |= CPUID::CPUID_ARM_NEON_BIT;

   /*
   On aarch64 this ends up calling getauxval twice with AT_HWCAP
   It doesn't seem worth optimizing this out, since getauxval is
   just reading a field in the ELF header.
   */
   const unsigned long hwcap_crypto = OS::get_auxval(ARM_hwcap_bit::ARCH_hwcap_crypto);
   if(hwcap_crypto & ARM_hwcap_bit::AES_bit)
      detected_features |= CPUID::CPUID_ARM_AES_BIT;
   if(hwcap_crypto & ARM_hwcap_bit::PMULL_bit)
      detected_features |= CPUID::CPUID_ARM_PMULL_BIT;
   if(hwcap_crypto & ARM_hwcap_bit::SHA1_bit)
      detected_features |= CPUID::CPUID_ARM_SHA1_BIT;
   if(hwcap_crypto & ARM_hwcap_bit::SHA2_bit)
      detected_features |= CPUID::CPUID_ARM_SHA2_BIT;

#if defined(BOTAN_TARGET_ARCH_IS_ARM64)
   if(hwcap_crypto & ARM_hwcap_bit::SHA3_bit)
      detected_features |= CPUID::CPUID_ARM_SHA3_BIT;
   if(hwcap_crypto & ARM_hwcap_bit::SM3_bit)
      detected_features |= CPUID::CPUID_ARM_SM3_BIT;
   if(hwcap_crypto & ARM_hwcap_bit::SM4_bit)
      detected_features |= CPUID::CPUID_ARM_SM4_BIT;
   if(hwcap_crypto & ARM_hwcap_bit::SHA2_512_bit)
      detected_features |= CPUID::CPUID_ARM_SHA2_512_BIT;
   if(hwcap_crypto & ARM_hwcap_bit::SVE_bit)
      detected_features |= CPUID::CPUID_ARM_SVE_BIT;
#endif

#elif defined(BOTAN_TARGET_OS_IS_IOS)

   char machine[64] = { 0 };
   size_t size = sizeof(machine) - 1;
   ::sysctlbyname("hw.machine", machine, &size, nullptr, 0);

   detected_features = flags_by_ios_machine_type(machine);
   // No way to detect cache line size on iOS?

#elif defined(BOTAN_USE_GCC_INLINE_ASM) && defined(BOTAN_TARGET_ARCH_IS_ARM64)

   /*
   No getauxval API available, fall back on probe functions. We only
   bother with Aarch64 here to simplify the code and because going to
   extreme contortions to support detect NEON on devices that probably
   don't support it doesn't seem worthwhile.

   NEON registers v0-v7 are caller saved in Aarch64
   */

   auto neon_probe  = []() noexcept -> int { asm("and v0.16b, v0.16b, v0.16b"); return 1; };
   auto aes_probe   = []() noexcept -> int { asm(".word 0x4e284800"); return 1; };
   auto pmull_probe = []() noexcept -> int { asm(".word 0x0ee0e000"); return 1; };
   auto sha1_probe  = []() noexcept -> int { asm(".word 0x5e280800"); return 1; };
   auto sha2_probe  = []() noexcept -> int { asm(".word 0x5e282800"); return 1; };

   // Only bother running the crypto detection if we found NEON

   if(OS::run_cpu_instruction_probe(neon_probe) == 1)
      {
      detected_features |= CPUID::CPUID_ARM_NEON_BIT;

      if(OS::run_cpu_instruction_probe(aes_probe) == 1)
         detected_features |= CPUID::CPUID_ARM_AES_BIT;
      if(OS::run_cpu_instruction_probe(pmull_probe) == 1)
         detected_features |= CPUID::CPUID_ARM_PMULL_BIT;
      if(OS::run_cpu_instruction_probe(sha1_probe) == 1)
         detected_features |= CPUID::CPUID_ARM_SHA1_BIT;
      if(OS::run_cpu_instruction_probe(sha2_probe) == 1)
         detected_features |= CPUID::CPUID_ARM_SHA2_BIT;
      }

#endif

   return detected_features;
   }

#endif

}
/*
* Runtime CPU detection for POWER/PowerPC
* (C) 2009,2010,2013,2017 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_TARGET_CPU_IS_PPC_FAMILY)

/*
* On macOS and OpenBSD ppc, use sysctl to detect AltiVec
*/
#if defined(BOTAN_TARGET_OS_IS_MACOS)
  #include <sys/sysctl.h>
#elif defined(BOTAN_TARGET_OS_IS_OPENBSD)
  #include <sys/param.h>
  #include <sys/sysctl.h>
  #include <machine/cpu.h>
#endif

#endif

namespace Botan {

#if defined(BOTAN_TARGET_CPU_IS_PPC_FAMILY)

/*
* PowerPC specific block: check for AltiVec using either
* sysctl or by reading processor version number register.
*/
uint64_t CPUID::CPUID_Data::detect_cpu_features(size_t* cache_line_size)
   {
   BOTAN_UNUSED(cache_line_size);

#if defined(BOTAN_TARGET_OS_IS_MACOS) || defined(BOTAN_TARGET_OS_IS_OPENBSD)
   // On macOS and OpenBSD, use sysctl

   int sels[2] = {
#if defined(BOTAN_TARGET_OS_IS_OPENBSD)
      CTL_MACHDEP, CPU_ALTIVEC
#else
      CTL_HW, HW_VECTORUNIT
#endif
   };

   int vector_type = 0;
   size_t length = sizeof(vector_type);
   int error = ::sysctl(sels, 2, &vector_type, &length, NULL, 0);

   if(error == 0 && vector_type > 0)
      return CPUID::CPUID_ALTIVEC_BIT;

#elif (defined(BOTAN_TARGET_OS_HAS_GETAUXVAL) || defined(BOTAN_TARGET_HAS_ELF_AUX_INFO)) && defined(BOTAN_TARGET_ARCH_IS_PPC64)

   enum PPC_hwcap_bit {
      ALTIVEC_bit  = (1 << 28),
      CRYPTO_bit   = (1 << 25),
      DARN_bit     = (1 << 21),

      ARCH_hwcap_altivec = 16, // AT_HWCAP
      ARCH_hwcap_crypto  = 26, // AT_HWCAP2
   };

   uint64_t detected_features = 0;

   const unsigned long hwcap_altivec = OS::get_auxval(PPC_hwcap_bit::ARCH_hwcap_altivec);
   if(hwcap_altivec & PPC_hwcap_bit::ALTIVEC_bit)
      detected_features |= CPUID::CPUID_ALTIVEC_BIT;

   const unsigned long hwcap_crypto = OS::get_auxval(PPC_hwcap_bit::ARCH_hwcap_crypto);
   if(hwcap_crypto & PPC_hwcap_bit::CRYPTO_bit)
     detected_features |= CPUID::CPUID_POWER_CRYPTO_BIT;
   if(hwcap_crypto & PPC_hwcap_bit::DARN_bit)
     detected_features |= CPUID::CPUID_DARN_BIT;

   return detected_features;

#else

   /*
   On PowerPC, MSR 287 is PVR, the Processor Version Number
   Normally it is only accessible to ring 0, but Linux and NetBSD
   (others, too, maybe?) will trap and emulate it for us.
   */

   int pvr = OS::run_cpu_instruction_probe([]() noexcept -> int {
      uint32_t pvr = 0;
      asm volatile("mfspr %0, 287" : "=r" (pvr));
      // Top 16 bits suffice to identify the model
      return static_cast<int>(pvr >> 16);
      });

   if(pvr > 0)
      {
      const uint16_t ALTIVEC_PVR[] = {
         0x003E, // IBM POWER6
         0x003F, // IBM POWER7
         0x004A, // IBM POWER7p
         0x004B, // IBM POWER8E
         0x004C, // IBM POWER8 NVL
         0x004D, // IBM POWER8
         0x004E, // IBM POWER9
         0x000C, // G4-7400
         0x0039, // G5 970
         0x003C, // G5 970FX
         0x0044, // G5 970MP
         0x0070, // Cell PPU
         0, // end
      };

      for(size_t i = 0; ALTIVEC_PVR[i]; ++i)
         {
         if(pvr == ALTIVEC_PVR[i])
            return CPUID::CPUID_ALTIVEC_BIT;
         }

      return 0;
      }

   // TODO try direct instruction probing

#endif

   return 0;
   }

#endif

}
/*
* Runtime CPU detection for x86
* (C) 2009,2010,2013,2017 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_TARGET_CPU_IS_X86_FAMILY)

#if defined(BOTAN_BUILD_COMPILER_IS_MSVC)
  #include <intrin.h>
#elif defined(BOTAN_BUILD_COMPILER_IS_INTEL)
  #include <ia32intrin.h>
#elif defined(BOTAN_BUILD_COMPILER_IS_GCC) || defined(BOTAN_BUILD_COMPILER_IS_CLANG)
  #include <cpuid.h>
#endif

#endif

namespace Botan {

#if defined(BOTAN_TARGET_CPU_IS_X86_FAMILY)

uint64_t CPUID::CPUID_Data::detect_cpu_features(size_t* cache_line_size)
   {
#if defined(BOTAN_BUILD_COMPILER_IS_MSVC)
  #define X86_CPUID(type, out) do { __cpuid((int*)out, type); } while(0)
  #define X86_CPUID_SUBLEVEL(type, level, out) do { __cpuidex((int*)out, type, level); } while(0)

#elif defined(BOTAN_BUILD_COMPILER_IS_INTEL)
  #define X86_CPUID(type, out) do { __cpuid(out, type); } while(0)
  #define X86_CPUID_SUBLEVEL(type, level, out) do { __cpuidex((int*)out, type, level); } while(0)

#elif defined(BOTAN_TARGET_ARCH_IS_X86_64) && defined(BOTAN_USE_GCC_INLINE_ASM)
  #define X86_CPUID(type, out)                                                    \
     asm("cpuid\n\t" : "=a" (out[0]), "=b" (out[1]), "=c" (out[2]), "=d" (out[3]) \
         : "0" (type))

  #define X86_CPUID_SUBLEVEL(type, level, out)                                    \
     asm("cpuid\n\t" : "=a" (out[0]), "=b" (out[1]), "=c" (out[2]), "=d" (out[3]) \
         : "0" (type), "2" (level))

#elif defined(BOTAN_BUILD_COMPILER_IS_GCC) || defined(BOTAN_BUILD_COMPILER_IS_CLANG)
  #define X86_CPUID(type, out) do { __get_cpuid(type, out, out+1, out+2, out+3); } while(0)

  #define X86_CPUID_SUBLEVEL(type, level, out) \
     do { __cpuid_count(type, level, out[0], out[1], out[2], out[3]); } while(0)
#else
  #warning "No way of calling x86 cpuid instruction for this compiler"
  #define X86_CPUID(type, out) do { clear_mem(out, 4); } while(0)
  #define X86_CPUID_SUBLEVEL(type, level, out) do { clear_mem(out, 4); } while(0)
#endif

   uint64_t features_detected = 0;
   uint32_t cpuid[4] = { 0 };

   // CPUID 0: vendor identification, max sublevel
   X86_CPUID(0, cpuid);

   const uint32_t max_supported_sublevel = cpuid[0];

   const uint32_t INTEL_CPUID[3] = { 0x756E6547, 0x6C65746E, 0x49656E69 };
   const uint32_t AMD_CPUID[3] = { 0x68747541, 0x444D4163, 0x69746E65 };
   const bool is_intel = same_mem(cpuid + 1, INTEL_CPUID, 3);
   const bool is_amd = same_mem(cpuid + 1, AMD_CPUID, 3);

   if(max_supported_sublevel >= 1)
      {
      // CPUID 1: feature bits
      X86_CPUID(1, cpuid);
      const uint64_t flags0 = (static_cast<uint64_t>(cpuid[2]) << 32) | cpuid[3];

      enum x86_CPUID_1_bits : uint64_t {
         RDTSC = (1ULL << 4),
         SSE2 = (1ULL << 26),
         CLMUL = (1ULL << 33),
         SSSE3 = (1ULL << 41),
         SSE41 = (1ULL << 51),
         SSE42 = (1ULL << 52),
         AESNI = (1ULL << 57),
         RDRAND = (1ULL << 62)
      };

      if(flags0 & x86_CPUID_1_bits::RDTSC)
         features_detected |= CPUID::CPUID_RDTSC_BIT;
      if(flags0 & x86_CPUID_1_bits::SSE2)
         features_detected |= CPUID::CPUID_SSE2_BIT;
      if(flags0 & x86_CPUID_1_bits::CLMUL)
         features_detected |= CPUID::CPUID_CLMUL_BIT;
      if(flags0 & x86_CPUID_1_bits::SSSE3)
         features_detected |= CPUID::CPUID_SSSE3_BIT;
      if(flags0 & x86_CPUID_1_bits::SSE41)
         features_detected |= CPUID::CPUID_SSE41_BIT;
      if(flags0 & x86_CPUID_1_bits::SSE42)
         features_detected |= CPUID::CPUID_SSE42_BIT;
      if(flags0 & x86_CPUID_1_bits::AESNI)
         features_detected |= CPUID::CPUID_AESNI_BIT;
      if(flags0 & x86_CPUID_1_bits::RDRAND)
         features_detected |= CPUID::CPUID_RDRAND_BIT;
      }

   if(is_intel)
      {
      // Intel cache line size is in cpuid(1) output
      *cache_line_size = 8 * get_byte(2, cpuid[1]);
      }
   else if(is_amd)
      {
      // AMD puts it in vendor zone
      X86_CPUID(0x80000005, cpuid);
      *cache_line_size = get_byte(3, cpuid[2]);
      }

   if(max_supported_sublevel >= 7)
      {
      clear_mem(cpuid, 4);
      X86_CPUID_SUBLEVEL(7, 0, cpuid);

      enum x86_CPUID_7_bits : uint64_t {
         BMI1 = (1ULL << 3),
         AVX2 = (1ULL << 5),
         BMI2 = (1ULL << 8),
         AVX512_F = (1ULL << 16),
         AVX512_DQ = (1ULL << 17),
         RDSEED = (1ULL << 18),
         ADX = (1ULL << 19),
         AVX512_IFMA = (1ULL << 21),
         SHA = (1ULL << 29),
         AVX512_BW = (1ULL << 30),
         AVX512_VL = (1ULL << 31),
         AVX512_VBMI = (1ULL << 33),
         AVX512_VBMI2 = (1ULL << 38),
         AVX512_VAES = (1ULL << 41),
         AVX512_VCLMUL = (1ULL << 42),
         AVX512_VBITALG = (1ULL << 44),
      };

      const uint64_t flags7 = (static_cast<uint64_t>(cpuid[2]) << 32) | cpuid[1];

      if(flags7 & x86_CPUID_7_bits::AVX2)
         features_detected |= CPUID::CPUID_AVX2_BIT;
      if(flags7 & x86_CPUID_7_bits::BMI1)
         {
         features_detected |= CPUID::CPUID_BMI1_BIT;
         /*
         We only set the BMI2 bit if BMI1 is also supported, so BMI2
         code can safely use both extensions. No known processor
         implements BMI2 but not BMI1.
         */
         if(flags7 & x86_CPUID_7_bits::BMI2)
            features_detected |= CPUID::CPUID_BMI2_BIT;
         }

      if(flags7 & x86_CPUID_7_bits::AVX512_F)
         {
         features_detected |= CPUID::CPUID_AVX512F_BIT;

         if(flags7 & x86_CPUID_7_bits::AVX512_DQ)
            features_detected |= CPUID::CPUID_AVX512DQ_BIT;
         if(flags7 & x86_CPUID_7_bits::AVX512_BW)
            features_detected |= CPUID::CPUID_AVX512BW_BIT;

         const uint64_t ICELAKE_FLAGS =
            x86_CPUID_7_bits::AVX512_F |
            x86_CPUID_7_bits::AVX512_DQ |
            x86_CPUID_7_bits::AVX512_IFMA |
            x86_CPUID_7_bits::AVX512_BW |
            x86_CPUID_7_bits::AVX512_VL |
            x86_CPUID_7_bits::AVX512_VBMI |
            x86_CPUID_7_bits::AVX512_VBMI2 |
            x86_CPUID_7_bits::AVX512_VBITALG;

         if((flags7 & ICELAKE_FLAGS) == ICELAKE_FLAGS)
            features_detected |= CPUID::CPUID_AVX512_ICL_BIT;

         if(flags7 & x86_CPUID_7_bits::AVX512_VAES)
            features_detected |= CPUID::CPUID_AVX512_AES_BIT;
         if(flags7 & x86_CPUID_7_bits::AVX512_VCLMUL)
            features_detected |= CPUID::CPUID_AVX512_CLMUL_BIT;
         }

      if(flags7 & x86_CPUID_7_bits::RDSEED)
         features_detected |= CPUID::CPUID_RDSEED_BIT;
      if(flags7 & x86_CPUID_7_bits::ADX)
         features_detected |= CPUID::CPUID_ADX_BIT;
      if(flags7 & x86_CPUID_7_bits::SHA)
         features_detected |= CPUID::CPUID_SHA_BIT;
      }

#undef X86_CPUID
#undef X86_CPUID_SUBLEVEL

   /*
   * If we don't have access to CPUID, we can still safely assume that
   * any x86-64 processor has SSE2 and RDTSC
   */
#if defined(BOTAN_TARGET_ARCH_IS_X86_64)
   if(features_detected == 0)
      {
      features_detected |= CPUID::CPUID_SSE2_BIT;
      features_detected |= CPUID::CPUID_RDTSC_BIT;
      }
#endif

   return features_detected;
   }

#endif

}
/*
* Counter mode
* (C) 1999-2011,2014 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

CTR_BE::CTR_BE(BlockCipher* ciph) :
   m_cipher(ciph),
   m_block_size(m_cipher->block_size()),
   m_ctr_size(m_block_size),
   m_ctr_blocks(m_cipher->parallel_bytes() / m_block_size),
   m_counter(m_cipher->parallel_bytes()),
   m_pad(m_counter.size()),
   m_pad_pos(0)
   {
   }

CTR_BE::CTR_BE(BlockCipher* cipher, size_t ctr_size) :
   m_cipher(cipher),
   m_block_size(m_cipher->block_size()),
   m_ctr_size(ctr_size),
   m_ctr_blocks(m_cipher->parallel_bytes() / m_block_size),
   m_counter(m_cipher->parallel_bytes()),
   m_pad(m_counter.size()),
   m_pad_pos(0)
   {
   BOTAN_ARG_CHECK(m_ctr_size >= 4 && m_ctr_size <= m_block_size,
                   "Invalid CTR-BE counter size");
   }

void CTR_BE::clear()
   {
   m_cipher->clear();
   zeroise(m_pad);
   zeroise(m_counter);
   zap(m_iv);
   m_pad_pos = 0;
   }

size_t CTR_BE::default_iv_length() const
   {
   return m_block_size;
   }

bool CTR_BE::valid_iv_length(size_t iv_len) const
   {
   return (iv_len <= m_block_size);
   }

Key_Length_Specification CTR_BE::key_spec() const
   {
   return m_cipher->key_spec();
   }

CTR_BE* CTR_BE::clone() const
   {
   return new CTR_BE(m_cipher->clone(), m_ctr_size);
   }

void CTR_BE::key_schedule(const uint8_t key[], size_t key_len)
   {
   m_cipher->set_key(key, key_len);

   // Set a default all-zeros IV
   set_iv(nullptr, 0);
   }

std::string CTR_BE::name() const
   {
   if(m_ctr_size == m_block_size)
      return ("CTR-BE(" + m_cipher->name() + ")");
   else
      return ("CTR-BE(" + m_cipher->name() + "," + std::to_string(m_ctr_size) + ")");

   }

void CTR_BE::cipher(const uint8_t in[], uint8_t out[], size_t length)
   {
   verify_key_set(m_iv.empty() == false);

   const uint8_t* pad_bits = &m_pad[0];
   const size_t pad_size = m_pad.size();

   if(m_pad_pos > 0)
      {
      const size_t avail = pad_size - m_pad_pos;
      const size_t take = std::min(length, avail);
      xor_buf(out, in, pad_bits + m_pad_pos, take);
      length -= take;
      in += take;
      out += take;
      m_pad_pos += take;

      if(take == avail)
         {
         add_counter(m_ctr_blocks);
         m_cipher->encrypt_n(m_counter.data(), m_pad.data(), m_ctr_blocks);
         m_pad_pos = 0;
         }
      }

   while(length >= pad_size)
      {
      xor_buf(out, in, pad_bits, pad_size);
      length -= pad_size;
      in += pad_size;
      out += pad_size;

      add_counter(m_ctr_blocks);
      m_cipher->encrypt_n(m_counter.data(), m_pad.data(), m_ctr_blocks);
      }

   xor_buf(out, in, pad_bits, length);
   m_pad_pos += length;
   }

void CTR_BE::set_iv(const uint8_t iv[], size_t iv_len)
   {
   if(!valid_iv_length(iv_len))
      throw Invalid_IV_Length(name(), iv_len);

   m_iv.resize(m_block_size);
   zeroise(m_iv);
   buffer_insert(m_iv, 0, iv, iv_len);

   seek(0);
   }

void CTR_BE::add_counter(const uint64_t counter)
   {
   const size_t ctr_size = m_ctr_size;
   const size_t ctr_blocks = m_ctr_blocks;
   const size_t BS = m_block_size;

   if(ctr_size == 4)
      {
      const size_t off = (BS - 4);
      const uint32_t low32 = static_cast<uint32_t>(counter + load_be<uint32_t>(&m_counter[off], 0));

      for(size_t i = 0; i != ctr_blocks; ++i)
         {
         store_be(uint32_t(low32 + i), &m_counter[i*BS+off]);
         }
      }
   else if(ctr_size == 8)
      {
      const size_t off = (BS - 8);
      const uint64_t low64 = counter + load_be<uint64_t>(&m_counter[off], 0);

      for(size_t i = 0; i != ctr_blocks; ++i)
         {
         store_be(uint64_t(low64 + i), &m_counter[i*BS+off]);
         }
      }
   else if(ctr_size == 16)
      {
      const size_t off = (BS - 16);
      uint64_t b0 = load_be<uint64_t>(&m_counter[off], 0);
      uint64_t b1 = load_be<uint64_t>(&m_counter[off], 1);
      b1 += counter;
      b0 += (b1 < counter) ? 1 : 0; // carry

      for(size_t i = 0; i != ctr_blocks; ++i)
         {
         store_be(b0, &m_counter[i*BS+off]);
         store_be(b1, &m_counter[i*BS+off+8]);
         b1 += 1;
         b0 += (b1 == 0); // carry
         }
      }
   else
      {
      for(size_t i = 0; i != ctr_blocks; ++i)
         {
         uint64_t local_counter = counter;
         uint16_t carry = static_cast<uint8_t>(local_counter);
         for(size_t j = 0; (carry || local_counter) && j != ctr_size; ++j)
            {
            const size_t off = i*BS + (BS-1-j);
            const uint16_t cnt = static_cast<uint16_t>(m_counter[off]) + carry;
            m_counter[off] = static_cast<uint8_t>(cnt);
            local_counter = (local_counter >> 8);
            carry = (cnt >> 8) + static_cast<uint8_t>(local_counter);
            }
         }
      }
   }

void CTR_BE::seek(uint64_t offset)
   {
   verify_key_set(m_iv.empty() == false);

   const uint64_t base_counter = m_ctr_blocks * (offset / m_counter.size());

   zeroise(m_counter);
   buffer_insert(m_counter, 0, m_iv);

   const size_t BS = m_block_size;

   // Set m_counter blocks to IV, IV + 1, ... IV + n

   if(m_ctr_size == 4 && BS >= 8)
      {
      const uint32_t low32 = load_be<uint32_t>(&m_counter[BS-4], 0);

      if(m_ctr_blocks >= 4 && is_power_of_2(m_ctr_blocks))
         {
         size_t written = 1;
         while(written < m_ctr_blocks)
            {
            copy_mem(&m_counter[written*BS], &m_counter[0], BS*written);
            written *= 2;
            }
         }
      else
         {
         for(size_t i = 1; i != m_ctr_blocks; ++i)
            {
            copy_mem(&m_counter[i*BS], &m_counter[0], BS - 4);
            }
         }

      for(size_t i = 1; i != m_ctr_blocks; ++i)
         {
         const uint32_t c = static_cast<uint32_t>(low32 + i);
         store_be(c, &m_counter[(BS-4)+i*BS]);
         }
      }
   else
      {
      // do everything sequentially:
      for(size_t i = 1; i != m_ctr_blocks; ++i)
         {
         buffer_insert(m_counter, i*BS, &m_counter[(i-1)*BS], BS);

         for(size_t j = 0; j != m_ctr_size; ++j)
            if(++m_counter[i*BS + (BS - 1 - j)])
               break;
         }
      }

   if(base_counter > 0)
      add_counter(base_counter);

   m_cipher->encrypt_n(m_counter.data(), m_pad.data(), m_ctr_blocks);
   m_pad_pos = offset % m_counter.size();
   }
}
/*
* Entropy Source Polling
* (C) 2008-2010,2015 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_SYSTEM_RNG)
#endif

#if defined(BOTAN_HAS_PROCESSOR_RNG)
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_RDRAND)
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_RDSEED)
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_DARN)
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_DEV_RANDOM)
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_WIN32)
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_PROC_WALKER)
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_GETENTROPY)
#endif

namespace Botan {

namespace {

#if defined(BOTAN_HAS_SYSTEM_RNG)

class System_RNG_EntropySource final : public Entropy_Source
   {
   public:
      size_t poll(RandomNumberGenerator& rng) override
         {
         const size_t poll_bits = BOTAN_RNG_RESEED_POLL_BITS;
         rng.reseed_from_rng(system_rng(), poll_bits);
         return poll_bits;
         }

      std::string name() const override { return "system_rng"; }
   };

#endif

#if defined(BOTAN_HAS_PROCESSOR_RNG)

class Processor_RNG_EntropySource final : public Entropy_Source
   {
   public:
      size_t poll(RandomNumberGenerator& rng) override
         {
         /*
         * Intel's documentation for RDRAND at
         * https://software.intel.com/en-us/articles/intel-digital-random-number-generator-drng-software-implementation-guide
         * claims that software can guarantee a reseed event by polling enough data:
         * "There is an upper bound of 511 samples per seed in the implementation
         * where samples are 128 bits in size and can provide two 64-bit random
         * numbers each."
         *
         * By requesting 65536 bits we are asking for 512 samples and thus are assured
         * that at some point in producing the output, at least one reseed of the
         * internal state will occur.
         *
         * The reseeding conditions of the POWER and ARM processor RNGs are not known
         * but probably work in a somewhat similar manner. The exact amount requested
         * may be tweaked if and when such conditions become publically known.
         */
         const size_t poll_bits = 65536;
         rng.reseed_from_rng(m_hwrng, poll_bits);
         // Avoid trusting a black box, don't count this as contributing entropy:
         return 0;
         }

      std::string name() const override { return m_hwrng.name(); }
   private:
      Processor_RNG m_hwrng;
   };

#endif

}

std::unique_ptr<Entropy_Source> Entropy_Source::create(const std::string& name)
   {
#if defined(BOTAN_HAS_SYSTEM_RNG)
   if(name == "system_rng" || name == "win32_cryptoapi")
      {
      return std::unique_ptr<Entropy_Source>(new System_RNG_EntropySource);
      }
#endif

#if defined(BOTAN_HAS_PROCESSOR_RNG)
   if(name == "hwrng" || name == "rdrand" || name == "p9_darn")
      {
      if(Processor_RNG::available())
         {
         return std::unique_ptr<Entropy_Source>(new Processor_RNG_EntropySource);
         }
      }
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_RDSEED)
   if(name == "rdseed")
      {
      return std::unique_ptr<Entropy_Source>(new Intel_Rdseed);
      }
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_GETENTROPY)
   if(name == "getentropy")
      {
      return std::unique_ptr<Entropy_Source>(new Getentropy);
      }
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_DEV_RANDOM)
   if(name == "dev_random")
      {
      return std::unique_ptr<Entropy_Source>(new Device_EntropySource(BOTAN_SYSTEM_RNG_POLL_DEVICES));
      }
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_PROC_WALKER)
   if(name == "proc_walk" && OS::running_in_privileged_state() == false)
      {
      const std::string root_dir = BOTAN_ENTROPY_PROC_FS_PATH;
      if(!root_dir.empty())
         return std::unique_ptr<Entropy_Source>(new ProcWalking_EntropySource(root_dir));
      }
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_WIN32)
   if(name == "system_stats")
      {
      return std::unique_ptr<Entropy_Source>(new Win32_EntropySource);
      }
#endif

   BOTAN_UNUSED(name);
   return std::unique_ptr<Entropy_Source>();
   }

void Entropy_Sources::add_source(std::unique_ptr<Entropy_Source> src)
   {
   if(src.get())
      {
      m_srcs.push_back(std::move(src));
      }
   }

std::vector<std::string> Entropy_Sources::enabled_sources() const
   {
   std::vector<std::string> sources;
   for(size_t i = 0; i != m_srcs.size(); ++i)
      {
      sources.push_back(m_srcs[i]->name());
      }
   return sources;
   }

size_t Entropy_Sources::poll(RandomNumberGenerator& rng,
                             size_t poll_bits,
                             std::chrono::milliseconds timeout)
   {
   typedef std::chrono::system_clock clock;

   auto deadline = clock::now() + timeout;

   size_t bits_collected = 0;

   for(size_t i = 0; i != m_srcs.size(); ++i)
      {
      bits_collected += m_srcs[i]->poll(rng);

      if (bits_collected >= poll_bits || clock::now() > deadline)
         break;
      }

   return bits_collected;
   }

size_t Entropy_Sources::poll_just(RandomNumberGenerator& rng, const std::string& the_src)
   {
   for(size_t i = 0; i != m_srcs.size(); ++i)
      {
      if(m_srcs[i]->name() == the_src)
         {
         return m_srcs[i]->poll(rng);
         }
      }

   return 0;
   }

Entropy_Sources::Entropy_Sources(const std::vector<std::string>& sources)
   {
   for(auto&& src_name : sources)
      {
      add_source(Entropy_Source::create(src_name));
      }
   }

Entropy_Sources& Entropy_Sources::global_sources()
   {
   static Entropy_Sources global_entropy_sources(BOTAN_ENTROPY_DEFAULT_SOURCES);

   return global_entropy_sources;
   }

}

/*
* GCM Mode Encryption
* (C) 2013,2015 Jack Lloyd
* (C) 2016 Daniel Neus, Rohde & Schwarz Cybersecurity
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

/*
* GCM_Mode Constructor
*/
GCM_Mode::GCM_Mode(BlockCipher* cipher, size_t tag_size) :
   m_tag_size(tag_size),
   m_cipher_name(cipher->name()),
   m_ctr(new CTR_BE(cipher, 4)),
   m_ghash(new GHASH)
   {
   if(cipher->block_size() != GCM_BS)
      throw Invalid_Argument("Invalid block cipher for GCM");

   /* We allow any of the values 128, 120, 112, 104, or 96 bits as a tag size */
   /* 64 bit tag is still supported but deprecated and will be removed in the future */
   if(m_tag_size != 8 && (m_tag_size < 12 || m_tag_size > 16))
      throw Invalid_Argument(name() + ": Bad tag size " + std::to_string(m_tag_size));
   }

GCM_Mode::~GCM_Mode() { /* for unique_ptr */ }

void GCM_Mode::clear()
   {
   m_ctr->clear();
   m_ghash->clear();
   reset();
   }

void GCM_Mode::reset()
   {
   m_ghash->reset();
   }

std::string GCM_Mode::name() const
   {
   return (m_cipher_name + "/GCM(" + std::to_string(tag_size()) + ")");
   }

std::string GCM_Mode::provider() const
   {
   return m_ghash->provider();
   }

size_t GCM_Mode::update_granularity() const
   {
   return GCM_BS * std::max<size_t>(2, BOTAN_BLOCK_CIPHER_PAR_MULT);
   }

bool GCM_Mode::valid_nonce_length(size_t len) const
   {
   // GCM does not support empty nonces
   return (len > 0);
   }

Key_Length_Specification GCM_Mode::key_spec() const
   {
   return m_ctr->key_spec();
   }

void GCM_Mode::key_schedule(const uint8_t key[], size_t keylen)
   {
   m_ctr->set_key(key, keylen);

   const std::vector<uint8_t> zeros(GCM_BS);
   m_ctr->set_iv(zeros.data(), zeros.size());

   secure_vector<uint8_t> H(GCM_BS);
   m_ctr->encipher(H);
   m_ghash->set_key(H);
   }

void GCM_Mode::set_associated_data(const uint8_t ad[], size_t ad_len)
   {
   m_ghash->set_associated_data(ad, ad_len);
   }

void GCM_Mode::start_msg(const uint8_t nonce[], size_t nonce_len)
   {
   if(!valid_nonce_length(nonce_len))
      throw Invalid_IV_Length(name(), nonce_len);

   if(m_y0.size() != GCM_BS)
      m_y0.resize(GCM_BS);

   clear_mem(m_y0.data(), m_y0.size());

   if(nonce_len == 12)
      {
      copy_mem(m_y0.data(), nonce, nonce_len);
      m_y0[15] = 1;
      }
   else
      {
      m_ghash->nonce_hash(m_y0, nonce, nonce_len);
      }

   m_ctr->set_iv(m_y0.data(), m_y0.size());

   clear_mem(m_y0.data(), m_y0.size());
   m_ctr->encipher(m_y0);

   m_ghash->start(m_y0.data(), m_y0.size());
   clear_mem(m_y0.data(), m_y0.size());
   }

size_t GCM_Encryption::process(uint8_t buf[], size_t sz)
   {
   BOTAN_ARG_CHECK(sz % update_granularity() == 0, "Invalid buffer size");
   m_ctr->cipher(buf, buf, sz);
   m_ghash->update(buf, sz);
   return sz;
   }

void GCM_Encryption::finish(secure_vector<uint8_t>& buffer, size_t offset)
   {
   BOTAN_ARG_CHECK(offset <= buffer.size(), "Invalid offset");
   const size_t sz = buffer.size() - offset;
   uint8_t* buf = buffer.data() + offset;

   m_ctr->cipher(buf, buf, sz);
   m_ghash->update(buf, sz);

   uint8_t mac[16] = { 0 };
   m_ghash->final(mac, tag_size());
   buffer += std::make_pair(mac, tag_size());
   }

size_t GCM_Decryption::process(uint8_t buf[], size_t sz)
   {
   BOTAN_ARG_CHECK(sz % update_granularity() == 0, "Invalid buffer size");
   m_ghash->update(buf, sz);
   m_ctr->cipher(buf, buf, sz);
   return sz;
   }

void GCM_Decryption::finish(secure_vector<uint8_t>& buffer, size_t offset)
   {
   BOTAN_ARG_CHECK(offset <= buffer.size(), "Invalid offset");
   const size_t sz = buffer.size() - offset;
   uint8_t* buf = buffer.data() + offset;

   if(sz < tag_size())
      throw Decoding_Error("Insufficient input for GCM decryption, tag missing");

   const size_t remaining = sz - tag_size();

   // handle any final input before the tag
   if(remaining)
      {
      m_ghash->update(buf, remaining);
      m_ctr->cipher(buf, buf, remaining);
      }

   uint8_t mac[16] = { 0 };
   m_ghash->final(mac, tag_size());

   const uint8_t* included_tag = &buffer[remaining+offset];

   if(!constant_time_compare(mac, included_tag, tag_size()))
      throw Invalid_Authentication_Tag("GCM tag check failed");

   buffer.resize(offset + remaining);
   }

}
/*
* GCM GHASH
* (C) 2013,2015,2017 Jack Lloyd
* (C) 2016 Daniel Neus, Rohde & Schwarz Cybersecurity
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

std::string GHASH::provider() const
   {
#if defined(BOTAN_HAS_GHASH_CLMUL_CPU)
   if(CPUID::has_carryless_multiply())
      return "clmul";
#endif

#if defined(BOTAN_HAS_GHASH_CLMUL_VPERM)
   if(CPUID::has_vperm())
      return "vperm";
#endif

   return "base";
   }

void GHASH::ghash_multiply(secure_vector<uint8_t>& x,
                         const uint8_t input[],
                         size_t blocks)
   {
#if defined(BOTAN_HAS_GHASH_CLMUL_CPU)
   if(CPUID::has_carryless_multiply())
      {
      return ghash_multiply_cpu(x.data(), m_H_pow.data(), input, blocks);
      }
#endif

#if defined(BOTAN_HAS_GHASH_CLMUL_VPERM)
   if(CPUID::has_vperm())
      {
      return ghash_multiply_vperm(x.data(), m_HM.data(), input, blocks);
      }
#endif

   CT::poison(x.data(), x.size());

   const uint64_t ALL_BITS = 0xFFFFFFFFFFFFFFFF;

   uint64_t X[2] = {
      load_be<uint64_t>(x.data(), 0),
      load_be<uint64_t>(x.data(), 1)
   };

   for(size_t b = 0; b != blocks; ++b)
      {
      X[0] ^= load_be<uint64_t>(input, 2*b);
      X[1] ^= load_be<uint64_t>(input, 2*b+1);

      uint64_t Z[2] = { 0, 0 };

      for(size_t i = 0; i != 64; ++i)
         {
         const uint64_t X0MASK = (ALL_BITS + (X[0] >> 63)) ^ ALL_BITS;
         const uint64_t X1MASK = (ALL_BITS + (X[1] >> 63)) ^ ALL_BITS;

         X[0] <<= 1;
         X[1] <<= 1;

         Z[0] ^= m_HM[4*i  ] & X0MASK;
         Z[1] ^= m_HM[4*i+1] & X0MASK;
         Z[0] ^= m_HM[4*i+2] & X1MASK;
         Z[1] ^= m_HM[4*i+3] & X1MASK;
         }

      X[0] = Z[0];
      X[1] = Z[1];
      }

   store_be<uint64_t>(x.data(), X[0], X[1]);
   CT::unpoison(x.data(), x.size());
   }

void GHASH::ghash_update(secure_vector<uint8_t>& ghash,
                         const uint8_t input[], size_t length)
   {
   verify_key_set(!m_HM.empty());

   /*
   This assumes if less than block size input then we're just on the
   final block and should pad with zeros
   */

   const size_t full_blocks = length / GCM_BS;
   const size_t final_bytes = length - (full_blocks * GCM_BS);

   if(full_blocks > 0)
      {
      ghash_multiply(ghash, input, full_blocks);
      }

   if(final_bytes)
      {
      uint8_t last_block[GCM_BS] = { 0 };
      copy_mem(last_block, input + full_blocks * GCM_BS, final_bytes);
      ghash_multiply(ghash, last_block, 1);
      secure_scrub_memory(last_block, final_bytes);
      }
   }

void GHASH::key_schedule(const uint8_t key[], size_t length)
   {
   m_H.assign(key, key+length);
   m_H_ad.resize(GCM_BS);
   m_ad_len = 0;
   m_text_len = 0;

   uint64_t H0 = load_be<uint64_t>(m_H.data(), 0);
   uint64_t H1 = load_be<uint64_t>(m_H.data(), 1);

   const uint64_t R = 0xE100000000000000;

   m_HM.resize(256);

   // precompute the multiples of H
   for(size_t i = 0; i != 2; ++i)
      {
      for(size_t j = 0; j != 64; ++j)
         {
         /*
         we interleave H^1, H^65, H^2, H^66, H3, H67, H4, H68
         to make indexing nicer in the multiplication code
         */
         m_HM[4*j+2*i] = H0;
         m_HM[4*j+2*i+1] = H1;

         // GCM's bit ops are reversed so we carry out of the bottom
         const uint64_t carry = R * (H1 & 1);
         H1 = (H1 >> 1) | (H0 << 63);
         H0 = (H0 >> 1) ^ carry;
         }
      }

#if defined(BOTAN_HAS_GHASH_CLMUL_CPU)
   if(CPUID::has_carryless_multiply())
      {
      m_H_pow.resize(8);
      ghash_precompute_cpu(m_H.data(), m_H_pow.data());
      }
#endif
   }

void GHASH::start(const uint8_t nonce[], size_t len)
   {
   BOTAN_ARG_CHECK(len == 16, "GHASH requires a 128-bit nonce");
   m_nonce.assign(nonce, nonce + len);
   m_ghash = m_H_ad;
   }

void GHASH::set_associated_data(const uint8_t input[], size_t length)
   {
   if(m_ghash.empty() == false)
      throw Invalid_State("Too late to set AD in GHASH");

   zeroise(m_H_ad);

   ghash_update(m_H_ad, input, length);
   m_ad_len = length;
   }

void GHASH::update_associated_data(const uint8_t ad[], size_t length)
   {
   verify_key_set(m_ghash.size() == GCM_BS);
   m_ad_len += length;
   ghash_update(m_ghash, ad, length);
   }

void GHASH::update(const uint8_t input[], size_t length)
   {
   verify_key_set(m_ghash.size() == GCM_BS);
   m_text_len += length;
   ghash_update(m_ghash, input, length);
   }

void GHASH::add_final_block(secure_vector<uint8_t>& hash,
                            size_t ad_len, size_t text_len)
   {
   /*
   * stack buffer is fine here since the text len is public
   * and the length of the AD is probably not sensitive either.
   */
   uint8_t final_block[GCM_BS];
   store_be<uint64_t>(final_block, 8*ad_len, 8*text_len);
   ghash_update(hash, final_block, GCM_BS);
   }

void GHASH::final(uint8_t mac[], size_t mac_len)
   {
   BOTAN_ARG_CHECK(mac_len > 0 && mac_len <= 16, "GHASH output length");
   add_final_block(m_ghash, m_ad_len, m_text_len);

   for(size_t i = 0; i != mac_len; ++i)
      mac[i] = m_ghash[i] ^ m_nonce[i];

   m_ghash.clear();
   m_text_len = 0;
   }

void GHASH::nonce_hash(secure_vector<uint8_t>& y0, const uint8_t nonce[], size_t nonce_len)
   {
   BOTAN_ASSERT(m_ghash.size() == 0, "nonce_hash called during wrong time");

   ghash_update(y0, nonce, nonce_len);
   add_final_block(y0, 0, nonce_len);
   }

void GHASH::clear()
   {
   zap(m_H);
   zap(m_HM);
   reset();
   }

void GHASH::reset()
   {
   zeroise(m_H_ad);
   m_ghash.clear();
   m_nonce.clear();
   m_text_len = m_ad_len = 0;
   }

}
/*
* Hash Functions
* (C) 2015 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_ADLER32)
#endif

#if defined(BOTAN_HAS_CRC24)
#endif

#if defined(BOTAN_HAS_CRC32)
#endif

#if defined(BOTAN_HAS_GOST_34_11)
#endif

#if defined(BOTAN_HAS_KECCAK)
#endif

#if defined(BOTAN_HAS_MD4)
#endif

#if defined(BOTAN_HAS_MD5)
#endif

#if defined(BOTAN_HAS_RIPEMD_160)
#endif

#if defined(BOTAN_HAS_SHA1)
#endif

#if defined(BOTAN_HAS_SHA2_32)
#endif

#if defined(BOTAN_HAS_SHA2_64)
#endif

#if defined(BOTAN_HAS_SHA3)
#endif

#if defined(BOTAN_HAS_SHAKE)
#endif

#if defined(BOTAN_HAS_SKEIN_512)
#endif

#if defined(BOTAN_HAS_STREEBOG)
#endif

#if defined(BOTAN_HAS_SM3)
#endif

#if defined(BOTAN_HAS_TIGER)
#endif

#if defined(BOTAN_HAS_WHIRLPOOL)
#endif

#if defined(BOTAN_HAS_PARALLEL_HASH)
#endif

#if defined(BOTAN_HAS_COMB4P)
#endif

#if defined(BOTAN_HAS_BLAKE2B)
#endif

#if defined(BOTAN_HAS_OPENSSL)
#endif

#if defined(BOTAN_HAS_COMMONCRYPTO)
#endif

namespace Botan {

std::unique_ptr<HashFunction> HashFunction::create(const std::string& algo_spec,
                                                   const std::string& provider)
   {

#if defined(BOTAN_HAS_COMMONCRYPTO)
   if(provider.empty() || provider == "commoncrypto")
      {
      if(auto hash = make_commoncrypto_hash(algo_spec))
         return hash;

      if(!provider.empty())
         return nullptr;
      }
#endif

#if defined(BOTAN_HAS_OPENSSL)
   if(provider.empty() || provider == "openssl")
      {
      if(auto hash = make_openssl_hash(algo_spec))
         return hash;

      if(!provider.empty())
         return nullptr;
      }
#endif

   if(provider.empty() == false && provider != "base")
      return nullptr; // unknown provider

#if defined(BOTAN_HAS_SHA1)
   if(algo_spec == "SHA-160" ||
      algo_spec == "SHA-1" ||
      algo_spec == "SHA1")
      {
      return std::unique_ptr<HashFunction>(new SHA_160);
      }
#endif

#if defined(BOTAN_HAS_SHA2_32)
   if(algo_spec == "SHA-224")
      {
      return std::unique_ptr<HashFunction>(new SHA_224);
      }

   if(algo_spec == "SHA-256")
      {
      return std::unique_ptr<HashFunction>(new SHA_256);
      }
#endif

#if defined(BOTAN_HAS_SHA2_64)
   if(algo_spec == "SHA-384")
      {
      return std::unique_ptr<HashFunction>(new SHA_384);
      }

   if(algo_spec == "SHA-512")
      {
      return std::unique_ptr<HashFunction>(new SHA_512);
      }

   if(algo_spec == "SHA-512-256")
      {
      return std::unique_ptr<HashFunction>(new SHA_512_256);
      }
#endif

#if defined(BOTAN_HAS_RIPEMD_160)
   if(algo_spec == "RIPEMD-160")
      {
      return std::unique_ptr<HashFunction>(new RIPEMD_160);
      }
#endif

#if defined(BOTAN_HAS_WHIRLPOOL)
   if(algo_spec == "Whirlpool")
      {
      return std::unique_ptr<HashFunction>(new Whirlpool);
      }
#endif

#if defined(BOTAN_HAS_MD5)
   if(algo_spec == "MD5")
      {
      return std::unique_ptr<HashFunction>(new MD5);
      }
#endif

#if defined(BOTAN_HAS_MD4)
   if(algo_spec == "MD4")
      {
      return std::unique_ptr<HashFunction>(new MD4);
      }
#endif

#if defined(BOTAN_HAS_GOST_34_11)
   if(algo_spec == "GOST-R-34.11-94" || algo_spec == "GOST-34.11")
      {
      return std::unique_ptr<HashFunction>(new GOST_34_11);
      }
#endif

#if defined(BOTAN_HAS_ADLER32)
   if(algo_spec == "Adler32")
      {
      return std::unique_ptr<HashFunction>(new Adler32);
      }
#endif

#if defined(BOTAN_HAS_CRC24)
   if(algo_spec == "CRC24")
      {
      return std::unique_ptr<HashFunction>(new CRC24);
      }
#endif

#if defined(BOTAN_HAS_CRC32)
   if(algo_spec == "CRC32")
      {
      return std::unique_ptr<HashFunction>(new CRC32);
      }
#endif

   const SCAN_Name req(algo_spec);

#if defined(BOTAN_HAS_TIGER)
   if(req.algo_name() == "Tiger")
      {
      return std::unique_ptr<HashFunction>(
         new Tiger(req.arg_as_integer(0, 24),
                   req.arg_as_integer(1, 3)));
      }
#endif

#if defined(BOTAN_HAS_SKEIN_512)
   if(req.algo_name() == "Skein-512")
      {
      return std::unique_ptr<HashFunction>(
         new Skein_512(req.arg_as_integer(0, 512), req.arg(1, "")));
      }
#endif

#if defined(BOTAN_HAS_BLAKE2B)
   if(req.algo_name() == "Blake2b" || req.algo_name() == "BLAKE2b")
      {
      return std::unique_ptr<HashFunction>(
         new Blake2b(req.arg_as_integer(0, 512)));
   }
#endif

#if defined(BOTAN_HAS_KECCAK)
   if(req.algo_name() == "Keccak-1600")
      {
      return std::unique_ptr<HashFunction>(
         new Keccak_1600(req.arg_as_integer(0, 512)));
      }
#endif

#if defined(BOTAN_HAS_SHA3)
   if(req.algo_name() == "SHA-3")
      {
      return std::unique_ptr<HashFunction>(
         new SHA_3(req.arg_as_integer(0, 512)));
      }
#endif

#if defined(BOTAN_HAS_SHAKE)
   if(req.algo_name() == "SHAKE-128")
      {
      return std::unique_ptr<HashFunction>(new SHAKE_128(req.arg_as_integer(0, 128)));
      }
   if(req.algo_name() == "SHAKE-256")
      {
      return std::unique_ptr<HashFunction>(new SHAKE_256(req.arg_as_integer(0, 256)));
      }
#endif

#if defined(BOTAN_HAS_STREEBOG)
   if(algo_spec == "Streebog-256")
      {
      return std::unique_ptr<HashFunction>(new Streebog_256);
      }
   if(algo_spec == "Streebog-512")
      {
      return std::unique_ptr<HashFunction>(new Streebog_512);
      }
#endif

#if defined(BOTAN_HAS_SM3)
   if(algo_spec == "SM3")
      {
      return std::unique_ptr<HashFunction>(new SM3);
      }
#endif

#if defined(BOTAN_HAS_WHIRLPOOL)
   if(req.algo_name() == "Whirlpool")
      {
      return std::unique_ptr<HashFunction>(new Whirlpool);
      }
#endif

#if defined(BOTAN_HAS_PARALLEL_HASH)
   if(req.algo_name() == "Parallel")
      {
      std::vector<std::unique_ptr<HashFunction>> hashes;

      for(size_t i = 0; i != req.arg_count(); ++i)
         {
         auto h = HashFunction::create(req.arg(i));
         if(!h)
            {
            return nullptr;
            }
         hashes.push_back(std::move(h));
         }

      return std::unique_ptr<HashFunction>(new Parallel(hashes));
      }
#endif

#if defined(BOTAN_HAS_COMB4P)
   if(req.algo_name() == "Comb4P" && req.arg_count() == 2)
      {
      std::unique_ptr<HashFunction> h1(HashFunction::create(req.arg(0)));
      std::unique_ptr<HashFunction> h2(HashFunction::create(req.arg(1)));

      if(h1 && h2)
         return std::unique_ptr<HashFunction>(new Comb4P(h1.release(), h2.release()));
      }
#endif


   return nullptr;
   }

//static
std::unique_ptr<HashFunction>
HashFunction::create_or_throw(const std::string& algo,
                              const std::string& provider)
   {
   if(auto hash = HashFunction::create(algo, provider))
      {
      return hash;
      }
   throw Lookup_Error("Hash", algo, provider);
   }

std::vector<std::string> HashFunction::providers(const std::string& algo_spec)
   {
   return probe_providers_of<HashFunction>(algo_spec, {"base", "openssl", "commoncrypto"});
   }

}

/*
* Hex Encoding and Decoding
* (C) 2010,2020 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

char hex_encode_nibble(uint8_t n, bool uppercase)
   {
   BOTAN_DEBUG_ASSERT(n <= 15);

   const auto in_09 = CT::Mask<uint8_t>::is_lt(n, 10);

   const char c_09 = n + '0';
   const char c_af = n + (uppercase ? 'A' : 'a') - 10;

   return in_09.select(c_09, c_af);
   }

}

void hex_encode(char output[],
                const uint8_t input[],
                size_t input_length,
                bool uppercase)
   {
   for(size_t i = 0; i != input_length; ++i)
      {
      const uint8_t n0 = (input[i] >> 4) & 0xF;
      const uint8_t n1 = (input[i]     ) & 0xF;

      output[2*i  ] = hex_encode_nibble(n0, uppercase);
      output[2*i+1] = hex_encode_nibble(n1, uppercase);
      }
   }

std::string hex_encode(const uint8_t input[],
                       size_t input_length,
                       bool uppercase)
   {
   std::string output(2 * input_length, 0);

   if(input_length)
      hex_encode(&output.front(), input, input_length, uppercase);

   return output;
   }

namespace {

uint8_t hex_char_to_bin(char input)
   {
   const uint8_t c = static_cast<uint8_t>(input);

   const auto is_alpha_upper = CT::Mask<uint8_t>::is_within_range(c, uint8_t('A'), uint8_t('F'));
   const auto is_alpha_lower = CT::Mask<uint8_t>::is_within_range(c, uint8_t('a'), uint8_t('f'));
   const auto is_decimal     = CT::Mask<uint8_t>::is_within_range(c, uint8_t('0'), uint8_t('9'));

   const auto is_whitespace  = CT::Mask<uint8_t>::is_any_of(c, {
         uint8_t(' '), uint8_t('\t'), uint8_t('\n'), uint8_t('\r')
      });

   const uint8_t c_upper = c - uint8_t('A') + 10;
   const uint8_t c_lower = c - uint8_t('a') + 10;
   const uint8_t c_decim = c - uint8_t('0');

   uint8_t ret = 0xFF; // default value

   ret = is_alpha_upper.select(c_upper, ret);
   ret = is_alpha_lower.select(c_lower, ret);
   ret = is_decimal.select(c_decim, ret);
   ret = is_whitespace.select(0x80, ret);

   return ret;
   }

}


size_t hex_decode(uint8_t output[],
                  const char input[],
                  size_t input_length,
                  size_t& input_consumed,
                  bool ignore_ws)
   {
   uint8_t* out_ptr = output;
   bool top_nibble = true;

   clear_mem(output, input_length / 2);

   for(size_t i = 0; i != input_length; ++i)
      {
      const uint8_t bin = hex_char_to_bin(input[i]);

      if(bin >= 0x10)
         {
         if(bin == 0x80 && ignore_ws)
            continue;

         std::string bad_char(1, input[i]);
         if(bad_char == "\t")
           bad_char = "\\t";
         else if(bad_char == "\n")
           bad_char = "\\n";

         throw Invalid_Argument(
           std::string("hex_decode: invalid hex character '") +
           bad_char + "'");
         }

      if(top_nibble)
         *out_ptr |= bin << 4;
      else
         *out_ptr |= bin;

      top_nibble = !top_nibble;
      if(top_nibble)
         ++out_ptr;
      }

   input_consumed = input_length;
   size_t written = (out_ptr - output);

   /*
   * We only got half of a uint8_t at the end; zap the half-written
   * output and mark it as unread
   */
   if(!top_nibble)
      {
      *out_ptr = 0;
      input_consumed -= 1;
      }

   return written;
   }

size_t hex_decode(uint8_t output[],
                  const char input[],
                  size_t input_length,
                  bool ignore_ws)
   {
   size_t consumed = 0;
   size_t written = hex_decode(output, input, input_length,
                               consumed, ignore_ws);

   if(consumed != input_length)
      throw Invalid_Argument("hex_decode: input did not have full bytes");

   return written;
   }

size_t hex_decode(uint8_t output[],
                  const std::string& input,
                  bool ignore_ws)
   {
   return hex_decode(output, input.data(), input.length(), ignore_ws);
   }

secure_vector<uint8_t> hex_decode_locked(const char input[],
                                      size_t input_length,
                                      bool ignore_ws)
   {
   secure_vector<uint8_t> bin(1 + input_length / 2);

   size_t written = hex_decode(bin.data(),
                               input,
                               input_length,
                               ignore_ws);

   bin.resize(written);
   return bin;
   }

secure_vector<uint8_t> hex_decode_locked(const std::string& input,
                                      bool ignore_ws)
   {
   return hex_decode_locked(input.data(), input.size(), ignore_ws);
   }

std::vector<uint8_t> hex_decode(const char input[],
                             size_t input_length,
                             bool ignore_ws)
   {
   std::vector<uint8_t> bin(1 + input_length / 2);

   size_t written = hex_decode(bin.data(),
                               input,
                               input_length,
                               ignore_ws);

   bin.resize(written);
   return bin;
   }

std::vector<uint8_t> hex_decode(const std::string& input,
                             bool ignore_ws)
   {
   return hex_decode(input.data(), input.size(), ignore_ws);
   }

}
/*
* HMAC
* (C) 1999-2007,2014,2020 Jack Lloyd
*     2007 Yves Jerschow
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

/*
* Update a HMAC Calculation
*/
void HMAC::add_data(const uint8_t input[], size_t length)
   {
   verify_key_set(m_ikey.empty() == false);
   m_hash->update(input, length);
   }

/*
* Finalize a HMAC Calculation
*/
void HMAC::final_result(uint8_t mac[])
   {
   verify_key_set(m_okey.empty() == false);
   m_hash->final(mac);
   m_hash->update(m_okey);
   m_hash->update(mac, m_hash_output_length);
   m_hash->final(mac);
   m_hash->update(m_ikey);
   }

Key_Length_Specification HMAC::key_spec() const
   {
   // Support very long lengths for things like PBKDF2 and the TLS PRF
   return Key_Length_Specification(0, 4096);
   }

size_t HMAC::output_length() const
   {
   return m_hash_output_length;
   }

/*
* HMAC Key Schedule
*/
void HMAC::key_schedule(const uint8_t key[], size_t length)
   {
   const uint8_t ipad = 0x36;
   const uint8_t opad = 0x5C;

   m_hash->clear();

   m_ikey.resize(m_hash_block_size);
   m_okey.resize(m_hash_block_size);

   clear_mem(m_ikey.data(), m_ikey.size());
   clear_mem(m_okey.data(), m_okey.size());

   /*
   * Sometimes the HMAC key length itself is sensitive, as with PBKDF2 where it
   * reveals the length of the passphrase. Make some attempt to hide this to
   * side channels. Clearly if the secret is longer than the block size then the
   * branch to hash first reveals that. In addition, counting the number of
   * compression functions executed reveals the size at the granularity of the
   * hash function's block size.
   *
   * The greater concern is for smaller keys; being able to detect when a
   * passphrase is say 4 bytes may assist choosing weaker targets. Even though
   * the loop bounds are constant, we can only actually read key[0..length] so
   * it doesn't seem possible to make this computation truly constant time.
   *
   * We don't mind leaking if the length is exactly zero since that's
   * trivial to simply check.
   */

   if(length > m_hash_block_size)
      {
      m_hash->update(key, length);
      m_hash->final(m_ikey.data());
      }
   else if(length > 0)
      {
      for(size_t i = 0, i_mod_length = 0; i != m_hash_block_size; ++i)
         {
         /*
         access key[i % length] but avoiding division due to variable
         time computation on some processors.
         */
         auto needs_reduction = CT::Mask<size_t>::is_lte(length, i_mod_length);
         i_mod_length = needs_reduction.select(0, i_mod_length);
         const uint8_t kb = key[i_mod_length];

         auto in_range = CT::Mask<size_t>::is_lt(i, length);
         m_ikey[i] = static_cast<uint8_t>(in_range.if_set_return(kb));
         i_mod_length += 1;
         }
      }

   for(size_t i = 0; i != m_hash_block_size; ++i)
      {
      m_ikey[i] ^= ipad;
      m_okey[i] = m_ikey[i] ^ ipad ^ opad;
      }

   m_hash->update(m_ikey);
   }

/*
* Clear memory of sensitive data
*/
void HMAC::clear()
   {
   m_hash->clear();
   zap(m_ikey);
   zap(m_okey);
   }

/*
* Return the name of this type
*/
std::string HMAC::name() const
   {
   return "HMAC(" + m_hash->name() + ")";
   }

/*
* Return a clone of this object
*/
MessageAuthenticationCode* HMAC::clone() const
   {
   return new HMAC(m_hash->clone());
   }

/*
* HMAC Constructor
*/
HMAC::HMAC(HashFunction* hash) :
   m_hash(hash),
   m_hash_output_length(m_hash->output_length()),
   m_hash_block_size(m_hash->hash_block_size())
   {
   BOTAN_ARG_CHECK(m_hash_block_size >= m_hash_output_length,
                   "HMAC is not compatible with this hash function");
   }

}
/*
* Message Authentication Code base class
* (C) 1999-2008 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_CBC_MAC)
#endif

#if defined(BOTAN_HAS_CMAC)
#endif

#if defined(BOTAN_HAS_GMAC)
#endif

#if defined(BOTAN_HAS_HMAC)
#endif

#if defined(BOTAN_HAS_POLY1305)
#endif

#if defined(BOTAN_HAS_SIPHASH)
#endif

#if defined(BOTAN_HAS_ANSI_X919_MAC)
#endif

namespace Botan {

std::unique_ptr<MessageAuthenticationCode>
MessageAuthenticationCode::create(const std::string& algo_spec,
                                  const std::string& provider)
   {
   const SCAN_Name req(algo_spec);

#if defined(BOTAN_HAS_GMAC)
   if(req.algo_name() == "GMAC" && req.arg_count() == 1)
      {
      if(provider.empty() || provider == "base")
         {
         if(auto bc = BlockCipher::create(req.arg(0)))
            return std::unique_ptr<MessageAuthenticationCode>(new GMAC(bc.release()));
         }
      }
#endif

#if defined(BOTAN_HAS_HMAC)
   if(req.algo_name() == "HMAC" && req.arg_count() == 1)
      {
      // TODO OpenSSL
      if(provider.empty() || provider == "base")
         {
         if(auto h = HashFunction::create(req.arg(0)))
            return std::unique_ptr<MessageAuthenticationCode>(new HMAC(h.release()));
         }
      }
#endif

#if defined(BOTAN_HAS_POLY1305)
   if(req.algo_name() == "Poly1305" && req.arg_count() == 0)
      {
      if(provider.empty() || provider == "base")
         return std::unique_ptr<MessageAuthenticationCode>(new Poly1305);
      }
#endif

#if defined(BOTAN_HAS_SIPHASH)
   if(req.algo_name() == "SipHash")
      {
      if(provider.empty() || provider == "base")
         {
         return std::unique_ptr<MessageAuthenticationCode>(
            new SipHash(req.arg_as_integer(0, 2), req.arg_as_integer(1, 4)));
         }
      }
#endif

#if defined(BOTAN_HAS_CMAC)
   if((req.algo_name() == "CMAC" || req.algo_name() == "OMAC") && req.arg_count() == 1)
      {
      // TODO: OpenSSL CMAC
      if(provider.empty() || provider == "base")
         {
         if(auto bc = BlockCipher::create(req.arg(0)))
            return std::unique_ptr<MessageAuthenticationCode>(new CMAC(bc.release()));
         }
      }
#endif


#if defined(BOTAN_HAS_CBC_MAC)
   if(req.algo_name() == "CBC-MAC" && req.arg_count() == 1)
      {
      if(provider.empty() || provider == "base")
         {
         if(auto bc = BlockCipher::create(req.arg(0)))
            return std::unique_ptr<MessageAuthenticationCode>(new CBC_MAC(bc.release()));
         }
      }
#endif

#if defined(BOTAN_HAS_ANSI_X919_MAC)
   if(req.algo_name() == "X9.19-MAC")
      {
      if(provider.empty() || provider == "base")
         {
         return std::unique_ptr<MessageAuthenticationCode>(new ANSI_X919_MAC);
         }
      }
#endif

   BOTAN_UNUSED(req);
   BOTAN_UNUSED(provider);

   return nullptr;
   }

std::vector<std::string>
MessageAuthenticationCode::providers(const std::string& algo_spec)
   {
   return probe_providers_of<MessageAuthenticationCode>(algo_spec, {"base", "openssl"});
   }

//static
std::unique_ptr<MessageAuthenticationCode>
MessageAuthenticationCode::create_or_throw(const std::string& algo,
                                           const std::string& provider)
   {
   if(auto mac = MessageAuthenticationCode::create(algo, provider))
      {
      return mac;
      }
   throw Lookup_Error("MAC", algo, provider);
   }

void MessageAuthenticationCode::start_msg(const uint8_t nonce[], size_t nonce_len)
   {
   BOTAN_UNUSED(nonce);
   if(nonce_len > 0)
      throw Invalid_IV_Length(name(), nonce_len);
   }

/*
* Default (deterministic) MAC verification operation
*/
bool MessageAuthenticationCode::verify_mac(const uint8_t mac[], size_t length)
   {
   secure_vector<uint8_t> our_mac = final();

   if(our_mac.size() != length)
      return false;

   return constant_time_compare(our_mac.data(), mac, length);
   }

}
/*
* Merkle-Damgard Hash Function
* (C) 1999-2008,2018 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

/*
* MDx_HashFunction Constructor
*/
MDx_HashFunction::MDx_HashFunction(size_t block_len,
                                   bool byte_big_endian,
                                   bool bit_big_endian,
                                   uint8_t cnt_size) :
   m_pad_char(bit_big_endian == true ? 0x80 : 0x01),
   m_counter_size(cnt_size),
   m_block_bits(ceil_log2(block_len)),
   m_count_big_endian(byte_big_endian),
   m_count(0),
   m_buffer(block_len),
   m_position(0)
   {
   if(!is_power_of_2(block_len))
      throw Invalid_Argument("MDx_HashFunction block length must be a power of 2");
   if(m_block_bits < 3 || m_block_bits > 16)
      throw Invalid_Argument("MDx_HashFunction block size too large or too small");
   if(m_counter_size < 8 || m_counter_size > block_len)
      throw Invalid_State("MDx_HashFunction invalid counter length");
   }

/*
* Clear memory of sensitive data
*/
void MDx_HashFunction::clear()
   {
   zeroise(m_buffer);
   m_count = m_position = 0;
   }

/*
* Update the hash
*/
void MDx_HashFunction::add_data(const uint8_t input[], size_t length)
   {
   const size_t block_len = static_cast<size_t>(1) << m_block_bits;

   m_count += length;

   if(m_position)
      {
      buffer_insert(m_buffer, m_position, input, length);

      if(m_position + length >= block_len)
         {
         compress_n(m_buffer.data(), 1);
         input += (block_len - m_position);
         length -= (block_len - m_position);
         m_position = 0;
         }
      }

   // Just in case the compiler can't figure out block_len is a power of 2
   const size_t full_blocks = length >> m_block_bits;
   const size_t remaining   = length & (block_len - 1);

   if(full_blocks > 0)
      {
      compress_n(input, full_blocks);
      }

   buffer_insert(m_buffer, m_position, input + full_blocks * block_len, remaining);
   m_position += remaining;
   }

/*
* Finalize a hash
*/
void MDx_HashFunction::final_result(uint8_t output[])
   {
   const size_t block_len = static_cast<size_t>(1) << m_block_bits;

   clear_mem(&m_buffer[m_position], block_len - m_position);
   m_buffer[m_position] = m_pad_char;

   if(m_position >= block_len - m_counter_size)
      {
      compress_n(m_buffer.data(), 1);
      zeroise(m_buffer);
      }

   write_count(&m_buffer[block_len - m_counter_size]);

   compress_n(m_buffer.data(), 1);
   copy_out(output);
   clear();
   }

/*
* Write the count bits to the buffer
*/
void MDx_HashFunction::write_count(uint8_t out[])
   {
   BOTAN_ASSERT_NOMSG(m_counter_size <= output_length());
   BOTAN_ASSERT_NOMSG(m_counter_size >= 8);

   const uint64_t bit_count = m_count * 8;

   if(m_count_big_endian)
      store_be(bit_count, out + m_counter_size - 8);
   else
      store_le(bit_count, out + m_counter_size - 8);
   }

}
/*
* Cipher Modes
* (C) 2015 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_BLOCK_CIPHER)
#endif

#if defined(BOTAN_HAS_AEAD_MODES)
#endif

#if defined(BOTAN_HAS_MODE_CBC)
#endif

#if defined(BOTAN_HAS_MODE_CFB)
#endif

#if defined(BOTAN_HAS_MODE_XTS)
#endif

#if defined(BOTAN_HAS_OPENSSL)
#endif

#if defined(BOTAN_HAS_COMMONCRYPTO)
#endif

namespace Botan {

std::unique_ptr<Cipher_Mode> Cipher_Mode::create_or_throw(const std::string& algo,
                                                          Cipher_Dir direction,
                                                          const std::string& provider)
   {
   if(auto mode = Cipher_Mode::create(algo, direction, provider))
      return mode;

   throw Lookup_Error("Cipher mode", algo, provider);
   }

std::unique_ptr<Cipher_Mode> Cipher_Mode::create(const std::string& algo,
                                                 Cipher_Dir direction,
                                                 const std::string& provider)
   {
#if defined(BOTAN_HAS_COMMONCRYPTO)
   if(provider.empty() || provider == "commoncrypto")
      {
      std::unique_ptr<Cipher_Mode> commoncrypto_cipher(make_commoncrypto_cipher_mode(algo, direction));

      if(commoncrypto_cipher)
         return commoncrypto_cipher;

      if(!provider.empty())
         return std::unique_ptr<Cipher_Mode>();
      }
#endif

#if defined(BOTAN_HAS_OPENSSL)
   if(provider.empty() || provider == "openssl")
      {
      std::unique_ptr<Cipher_Mode> openssl_cipher(make_openssl_cipher_mode(algo, direction));

      if(openssl_cipher)
         return openssl_cipher;

      if(!provider.empty())
         return std::unique_ptr<Cipher_Mode>();
      }
#endif

#if defined(BOTAN_HAS_STREAM_CIPHER)
   if(auto sc = StreamCipher::create(algo))
      {
      return std::unique_ptr<Cipher_Mode>(new Stream_Cipher_Mode(sc.release()));
      }
#endif

#if defined(BOTAN_HAS_AEAD_MODES)
   if(auto aead = AEAD_Mode::create(algo, direction))
      {
      return std::unique_ptr<Cipher_Mode>(aead.release());
      }
#endif

   if(algo.find('/') != std::string::npos)
      {
      const std::vector<std::string> algo_parts = split_on(algo, '/');
      const std::string cipher_name = algo_parts[0];
      const std::vector<std::string> mode_info = parse_algorithm_name(algo_parts[1]);

      if(mode_info.empty())
         return std::unique_ptr<Cipher_Mode>();

      std::ostringstream alg_args;

      alg_args << '(' << cipher_name;
      for(size_t i = 1; i < mode_info.size(); ++i)
         alg_args << ',' << mode_info[i];
      for(size_t i = 2; i < algo_parts.size(); ++i)
         alg_args << ',' << algo_parts[i];
      alg_args << ')';

      const std::string mode_name = mode_info[0] + alg_args.str();
      return Cipher_Mode::create(mode_name, direction, provider);
      }

#if defined(BOTAN_HAS_BLOCK_CIPHER)

   SCAN_Name spec(algo);

   if(spec.arg_count() == 0)
      {
      return std::unique_ptr<Cipher_Mode>();
      }

   std::unique_ptr<BlockCipher> bc(BlockCipher::create(spec.arg(0), provider));

   if(!bc)
      {
      return std::unique_ptr<Cipher_Mode>();
      }

#if defined(BOTAN_HAS_MODE_CBC)
   if(spec.algo_name() == "CBC")
      {
      const std::string padding = spec.arg(1, "PKCS7");

      if(padding == "CTS")
         {
         if(direction == ENCRYPTION)
            return std::unique_ptr<Cipher_Mode>(new CTS_Encryption(bc.release()));
         else
            return std::unique_ptr<Cipher_Mode>(new CTS_Decryption(bc.release()));
         }
      else
         {
         std::unique_ptr<BlockCipherModePaddingMethod> pad(get_bc_pad(padding));

         if(pad)
            {
            if(direction == ENCRYPTION)
               return std::unique_ptr<Cipher_Mode>(new CBC_Encryption(bc.release(), pad.release()));
            else
               return std::unique_ptr<Cipher_Mode>(new CBC_Decryption(bc.release(), pad.release()));
            }
         }
      }
#endif

#if defined(BOTAN_HAS_MODE_XTS)
   if(spec.algo_name() == "XTS")
      {
      if(direction == ENCRYPTION)
         return std::unique_ptr<Cipher_Mode>(new XTS_Encryption(bc.release()));
      else
         return std::unique_ptr<Cipher_Mode>(new XTS_Decryption(bc.release()));
      }
#endif

#if defined(BOTAN_HAS_MODE_CFB)
   if(spec.algo_name() == "CFB")
      {
      const size_t feedback_bits = spec.arg_as_integer(1, 8*bc->block_size());
      if(direction == ENCRYPTION)
         return std::unique_ptr<Cipher_Mode>(new CFB_Encryption(bc.release(), feedback_bits));
      else
         return std::unique_ptr<Cipher_Mode>(new CFB_Decryption(bc.release(), feedback_bits));
      }
#endif

#endif

   return std::unique_ptr<Cipher_Mode>();
   }

//static
std::vector<std::string> Cipher_Mode::providers(const std::string& algo_spec)
   {
   const std::vector<std::string>& possible = { "base", "openssl", "commoncrypto" };
   std::vector<std::string> providers;
   for(auto&& prov : possible)
      {
      std::unique_ptr<Cipher_Mode> mode = Cipher_Mode::create(algo_spec, ENCRYPTION, prov);
      if(mode)
         {
         providers.push_back(prov); // available
         }
      }
   return providers;
   }

}
/*
* PBKDF
* (C) 2012 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_PBKDF1)
#endif

#if defined(BOTAN_HAS_PBKDF2)
#endif

#if defined(BOTAN_HAS_PGP_S2K)
#endif

namespace Botan {

std::unique_ptr<PBKDF> PBKDF::create(const std::string& algo_spec,
                                     const std::string& provider)
   {
   const SCAN_Name req(algo_spec);

#if defined(BOTAN_HAS_PBKDF2)
   if(req.algo_name() == "PBKDF2")
      {
      // TODO OpenSSL

      if(provider.empty() || provider == "base")
         {
         if(auto mac = MessageAuthenticationCode::create(req.arg(0)))
            return std::unique_ptr<PBKDF>(new PKCS5_PBKDF2(mac.release()));

         if(auto mac = MessageAuthenticationCode::create("HMAC(" + req.arg(0) + ")"))
            return std::unique_ptr<PBKDF>(new PKCS5_PBKDF2(mac.release()));
         }

      return nullptr;
      }
#endif

#if defined(BOTAN_HAS_PBKDF1)
   if(req.algo_name() == "PBKDF1" && req.arg_count() == 1)
      {
      if(auto hash = HashFunction::create(req.arg(0)))
         return std::unique_ptr<PBKDF>(new PKCS5_PBKDF1(hash.release()));

      }
#endif

#if defined(BOTAN_HAS_PGP_S2K)
   if(req.algo_name() == "OpenPGP-S2K" && req.arg_count() == 1)
      {
      if(auto hash = HashFunction::create(req.arg(0)))
         return std::unique_ptr<PBKDF>(new OpenPGP_S2K(hash.release()));
      }
#endif

   BOTAN_UNUSED(req);
   BOTAN_UNUSED(provider);

   return nullptr;
   }

//static
std::unique_ptr<PBKDF>
PBKDF::create_or_throw(const std::string& algo,
                             const std::string& provider)
   {
   if(auto pbkdf = PBKDF::create(algo, provider))
      {
      return pbkdf;
      }
   throw Lookup_Error("PBKDF", algo, provider);
   }

std::vector<std::string> PBKDF::providers(const std::string& algo_spec)
   {
   return probe_providers_of<PBKDF>(algo_spec, { "base", "openssl" });
   }

void PBKDF::pbkdf_timed(uint8_t out[], size_t out_len,
                        const std::string& passphrase,
                        const uint8_t salt[], size_t salt_len,
                        std::chrono::milliseconds msec,
                        size_t& iterations) const
   {
   iterations = pbkdf(out, out_len, passphrase, salt, salt_len, 0, msec);
   }

void PBKDF::pbkdf_iterations(uint8_t out[], size_t out_len,
                             const std::string& passphrase,
                             const uint8_t salt[], size_t salt_len,
                             size_t iterations) const
   {
   if(iterations == 0)
      throw Invalid_Argument(name() + ": Invalid iteration count");

   const size_t iterations_run = pbkdf(out, out_len, passphrase,
                                       salt, salt_len, iterations,
                                       std::chrono::milliseconds(0));
   BOTAN_ASSERT_EQUAL(iterations, iterations_run, "Expected PBKDF iterations");
   }

secure_vector<uint8_t> PBKDF::pbkdf_iterations(size_t out_len,
                                            const std::string& passphrase,
                                            const uint8_t salt[], size_t salt_len,
                                            size_t iterations) const
   {
   secure_vector<uint8_t> out(out_len);
   pbkdf_iterations(out.data(), out_len, passphrase, salt, salt_len, iterations);
   return out;
   }

secure_vector<uint8_t> PBKDF::pbkdf_timed(size_t out_len,
                                       const std::string& passphrase,
                                       const uint8_t salt[], size_t salt_len,
                                       std::chrono::milliseconds msec,
                                       size_t& iterations) const
   {
   secure_vector<uint8_t> out(out_len);
   pbkdf_timed(out.data(), out_len, passphrase, salt, salt_len, msec, iterations);
   return out;
   }

}
/*
* (C) 2018 Ribose Inc
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_PBKDF2)
#endif

#if defined(BOTAN_HAS_PGP_S2K)
#endif

#if defined(BOTAN_HAS_SCRYPT)
#endif

#if defined(BOTAN_HAS_ARGON2)
#endif

#if defined(BOTAN_HAS_PBKDF_BCRYPT)
#endif

namespace Botan {

std::unique_ptr<PasswordHashFamily> PasswordHashFamily::create(const std::string& algo_spec,
                                     const std::string& provider)
   {
   const SCAN_Name req(algo_spec);

#if defined(BOTAN_HAS_PBKDF2)
   if(req.algo_name() == "PBKDF2")
      {
      // TODO OpenSSL

      if(provider.empty() || provider == "base")
         {
         if(auto mac = MessageAuthenticationCode::create(req.arg(0)))
            return std::unique_ptr<PasswordHashFamily>(new PBKDF2_Family(mac.release()));

         if(auto mac = MessageAuthenticationCode::create("HMAC(" + req.arg(0) + ")"))
            return std::unique_ptr<PasswordHashFamily>(new PBKDF2_Family(mac.release()));
         }

      return nullptr;
      }
#endif

#if defined(BOTAN_HAS_SCRYPT)
   if(req.algo_name() == "Scrypt")
      {
      return std::unique_ptr<PasswordHashFamily>(new Scrypt_Family);
      }
#endif

#if defined(BOTAN_HAS_ARGON2)
   if(req.algo_name() == "Argon2d")
      {
      return std::unique_ptr<PasswordHashFamily>(new Argon2_Family(0));
      }
   else if(req.algo_name() == "Argon2i")
      {
      return std::unique_ptr<PasswordHashFamily>(new Argon2_Family(1));
      }
   else if(req.algo_name() == "Argon2id")
      {
      return std::unique_ptr<PasswordHashFamily>(new Argon2_Family(2));
      }
#endif

#if defined(BOTAN_HAS_PBKDF_BCRYPT)
   if(req.algo_name() == "Bcrypt-PBKDF")
      {
      return std::unique_ptr<PasswordHashFamily>(new Bcrypt_PBKDF_Family);
      }
#endif

#if defined(BOTAN_HAS_PGP_S2K)
   if(req.algo_name() == "OpenPGP-S2K" && req.arg_count() == 1)
      {
      if(auto hash = HashFunction::create(req.arg(0)))
         {
         return std::unique_ptr<PasswordHashFamily>(new RFC4880_S2K_Family(hash.release()));
         }
      }
#endif

   BOTAN_UNUSED(req);
   BOTAN_UNUSED(provider);

   return nullptr;
   }

//static
std::unique_ptr<PasswordHashFamily>
PasswordHashFamily::create_or_throw(const std::string& algo,
                             const std::string& provider)
   {
   if(auto pbkdf = PasswordHashFamily::create(algo, provider))
      {
      return pbkdf;
      }
   throw Lookup_Error("PasswordHashFamily", algo, provider);
   }

std::vector<std::string> PasswordHashFamily::providers(const std::string& algo_spec)
   {
   return probe_providers_of<PasswordHashFamily>(algo_spec, { "base", "openssl" });
   }

}
/*
* PBKDF2
* (C) 1999-2007 Jack Lloyd
* (C) 2018 Ribose Inc
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

void pbkdf2_set_key(MessageAuthenticationCode& prf,
                    const char* password,
                    size_t password_len)
   {
   try
      {
      prf.set_key(cast_char_ptr_to_uint8(password), password_len);
      }
   catch(Invalid_Key_Length&)
      {
      throw Invalid_Argument("PBKDF2 cannot accept passphrase of the given size");
      }
   }

}

size_t
pbkdf2(MessageAuthenticationCode& prf,
       uint8_t out[],
       size_t out_len,
       const std::string& password,
       const uint8_t salt[], size_t salt_len,
       size_t iterations,
       std::chrono::milliseconds msec)
   {
   if(iterations == 0)
      {
      iterations = PBKDF2(prf, out_len, msec).iterations();
      }

   PBKDF2 pbkdf2(prf, iterations);

   pbkdf2.derive_key(out, out_len,
                     password.c_str(), password.size(),
                     salt, salt_len);

   return iterations;
   }

namespace {

size_t tune_pbkdf2(MessageAuthenticationCode& prf,
                   size_t output_length,
                   uint32_t msec)
   {
   if(output_length == 0)
      output_length = 1;

   const size_t prf_sz = prf.output_length();
   BOTAN_ASSERT_NOMSG(prf_sz > 0);
   secure_vector<uint8_t> U(prf_sz);

   const size_t trial_iterations = 2000;

   // Short output ensures we only need a single PBKDF2 block

   Timer timer("PBKDF2");

   const auto tune_time = BOTAN_PBKDF_TUNING_TIME;

   prf.set_key(nullptr, 0);

   timer.run_until_elapsed(tune_time, [&]() {
      uint8_t out[12] = { 0 };
      uint8_t salt[12] = { 0 };
      pbkdf2(prf, out, sizeof(out), salt, sizeof(salt), trial_iterations);
      });

   if(timer.events() == 0)
      return trial_iterations;

   const uint64_t duration_nsec = timer.value() / timer.events();

   const uint64_t desired_nsec = static_cast<uint64_t>(msec) * 1000000;

   if(duration_nsec > desired_nsec)
      return trial_iterations;

   const size_t blocks_needed = (output_length + prf_sz - 1) / prf_sz;

   const size_t multiplier = static_cast<size_t>(desired_nsec / duration_nsec / blocks_needed);

   if(multiplier == 0)
      return trial_iterations;
   else
      return trial_iterations * multiplier;
   }

}

void pbkdf2(MessageAuthenticationCode& prf,
            uint8_t out[],
            size_t out_len,
            const uint8_t salt[],
            size_t salt_len,
            size_t iterations)
   {
   if(iterations == 0)
      throw Invalid_Argument("PBKDF2: Invalid iteration count");

   clear_mem(out, out_len);

   if(out_len == 0)
      return;

   const size_t prf_sz = prf.output_length();
   BOTAN_ASSERT_NOMSG(prf_sz > 0);

   secure_vector<uint8_t> U(prf_sz);

   uint32_t counter = 1;
   while(out_len)
      {
      const size_t prf_output = std::min<size_t>(prf_sz, out_len);

      prf.update(salt, salt_len);
      prf.update_be(counter++);
      prf.final(U.data());

      xor_buf(out, U.data(), prf_output);

      for(size_t i = 1; i != iterations; ++i)
         {
         prf.update(U);
         prf.final(U.data());
         xor_buf(out, U.data(), prf_output);
         }

      out_len -= prf_output;
      out += prf_output;
      }
   }

// PBKDF interface
size_t
PKCS5_PBKDF2::pbkdf(uint8_t key[], size_t key_len,
                    const std::string& password,
                    const uint8_t salt[], size_t salt_len,
                    size_t iterations,
                    std::chrono::milliseconds msec) const
   {
   if(iterations == 0)
      {
      iterations = PBKDF2(*m_mac, key_len, msec).iterations();
      }

   PBKDF2 pbkdf2(*m_mac, iterations);

   pbkdf2.derive_key(key, key_len,
                     password.c_str(), password.size(),
                     salt, salt_len);

   return iterations;
   }

std::string PKCS5_PBKDF2::name() const
   {
   return "PBKDF2(" + m_mac->name() + ")";
   }

PBKDF* PKCS5_PBKDF2::clone() const
   {
   return new PKCS5_PBKDF2(m_mac->clone());
   }

// PasswordHash interface

PBKDF2::PBKDF2(const MessageAuthenticationCode& prf, size_t olen, std::chrono::milliseconds msec) :
   m_prf(prf.clone()),
   m_iterations(tune_pbkdf2(*m_prf, olen, static_cast<uint32_t>(msec.count())))
   {}

std::string PBKDF2::to_string() const
   {
   return "PBKDF2(" + m_prf->name() + "," + std::to_string(m_iterations) + ")";
   }

void PBKDF2::derive_key(uint8_t out[], size_t out_len,
                        const char* password, const size_t password_len,
                        const uint8_t salt[], size_t salt_len) const
   {
   pbkdf2_set_key(*m_prf, password, password_len);
   pbkdf2(*m_prf, out, out_len, salt, salt_len, m_iterations);
   }

std::string PBKDF2_Family::name() const
   {
   return "PBKDF2(" + m_prf->name() + ")";
   }

std::unique_ptr<PasswordHash> PBKDF2_Family::tune(size_t output_len, std::chrono::milliseconds msec, size_t) const
   {
   return std::unique_ptr<PasswordHash>(new PBKDF2(*m_prf, output_len, msec));
   }

std::unique_ptr<PasswordHash> PBKDF2_Family::default_params() const
   {
   return std::unique_ptr<PasswordHash>(new PBKDF2(*m_prf, 150000));
   }

std::unique_ptr<PasswordHash> PBKDF2_Family::from_params(size_t iter, size_t, size_t) const
   {
   return std::unique_ptr<PasswordHash>(new PBKDF2(*m_prf, iter));
   }

std::unique_ptr<PasswordHash> PBKDF2_Family::from_iterations(size_t iter) const
   {
   return std::unique_ptr<PasswordHash>(new PBKDF2(*m_prf, iter));
   }

}
/*
* (C) 2016 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_AUTO_SEEDING_RNG)
#endif

namespace Botan {

void RandomNumberGenerator::randomize_with_ts_input(uint8_t output[], size_t output_len)
   {
   if(this->accepts_input())
      {
      /*
      Form additional input which is provided to the PRNG implementation
      to paramaterize the KDF output.
      */
      uint8_t additional_input[16] = { 0 };
      store_le(OS::get_system_timestamp_ns(), additional_input);
      store_le(OS::get_high_resolution_clock(), additional_input + 8);

      this->randomize_with_input(output, output_len, additional_input, sizeof(additional_input));
      }
   else
      {
      this->randomize(output, output_len);
      }
   }

void RandomNumberGenerator::randomize_with_input(uint8_t output[], size_t output_len,
                                                 const uint8_t input[], size_t input_len)
   {
   this->add_entropy(input, input_len);
   this->randomize(output, output_len);
   }

size_t RandomNumberGenerator::reseed(Entropy_Sources& srcs,
                                     size_t poll_bits,
                                     std::chrono::milliseconds poll_timeout)
   {
   if(this->accepts_input())
      {
      return srcs.poll(*this, poll_bits, poll_timeout);
      }
   else
      {
      return 0;
      }
   }

void RandomNumberGenerator::reseed_from_rng(RandomNumberGenerator& rng, size_t poll_bits)
   {
   if(this->accepts_input())
      {
      secure_vector<uint8_t> buf(poll_bits / 8);
      rng.randomize(buf.data(), buf.size());
      this->add_entropy(buf.data(), buf.size());
      }
   }

RandomNumberGenerator* RandomNumberGenerator::make_rng()
   {
#if defined(BOTAN_HAS_AUTO_SEEDING_RNG)
   return new AutoSeeded_RNG;
#else
   throw Not_Implemented("make_rng failed, no AutoSeeded_RNG in this build");
#endif
   }

#if defined(BOTAN_TARGET_OS_HAS_THREADS)

#if defined(BOTAN_HAS_AUTO_SEEDING_RNG)
Serialized_RNG::Serialized_RNG() : m_rng(new AutoSeeded_RNG) {}
#else
Serialized_RNG::Serialized_RNG()
   {
   throw Not_Implemented("Serialized_RNG default constructor failed: AutoSeeded_RNG disabled in build");
   }
#endif

#endif

}
/*
* SHA-{224,256}
* (C) 1999-2010,2017 Jack Lloyd
*     2007 FlexSecure GmbH
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

std::string sha256_provider()
   {
#if defined(BOTAN_HAS_SHA2_32_X86)
   if(CPUID::has_intel_sha())
      {
      return "shani";
      }
#endif

#if defined(BOTAN_HAS_SHA2_32_X86_BMI2)
   if(CPUID::has_bmi2())
      {
      return "bmi2";
      }
#endif

#if defined(BOTAN_HAS_SHA2_32_ARMV8)
   if(CPUID::has_arm_sha2())
      {
      return "armv8";
      }
#endif

   return "base";
   }

}

std::unique_ptr<HashFunction> SHA_224::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new SHA_224(*this));
   }

std::unique_ptr<HashFunction> SHA_256::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new SHA_256(*this));
   }

/*
* SHA-256 F1 Function
*
* Use a macro as many compilers won't inline a function this big,
* even though it is much faster if inlined.
*/
#define SHA2_32_F(A, B, C, D, E, F, G, H, M1, M2, M3, M4, magic) do {               \
   uint32_t A_rho = rotr<2>(A) ^ rotr<13>(A) ^ rotr<22>(A); \
   uint32_t E_rho = rotr<6>(E) ^ rotr<11>(E) ^ rotr<25>(E); \
   uint32_t M2_sigma = rotr<17>(M2) ^ rotr<19>(M2) ^ (M2 >> 10);    \
   uint32_t M4_sigma = rotr<7>(M4) ^ rotr<18>(M4) ^ (M4 >> 3);      \
   H += magic + E_rho + ((E & F) ^ (~E & G)) + M1;                                  \
   D += H;                                                                          \
   H += A_rho + ((A & B) | ((A | B) & C));                                          \
   M1 += M2_sigma + M3 + M4_sigma;                                                  \
   } while(0);

/*
* SHA-224 / SHA-256 compression function
*/
void SHA_256::compress_digest(secure_vector<uint32_t>& digest,
                              const uint8_t input[], size_t blocks)
   {
#if defined(BOTAN_HAS_SHA2_32_X86)
   if(CPUID::has_intel_sha())
      {
      return SHA_256::compress_digest_x86(digest, input, blocks);
      }
#endif

#if defined(BOTAN_HAS_SHA2_32_X86_BMI2)
   if(CPUID::has_bmi2())
      {
      return SHA_256::compress_digest_x86_bmi2(digest, input, blocks);
      }
#endif

#if defined(BOTAN_HAS_SHA2_32_ARMV8)
   if(CPUID::has_arm_sha2())
      {
      return SHA_256::compress_digest_armv8(digest, input, blocks);
      }
#endif

   uint32_t A = digest[0], B = digest[1], C = digest[2],
            D = digest[3], E = digest[4], F = digest[5],
            G = digest[6], H = digest[7];

   for(size_t i = 0; i != blocks; ++i)
      {
      uint32_t W00 = load_be<uint32_t>(input,  0);
      uint32_t W01 = load_be<uint32_t>(input,  1);
      uint32_t W02 = load_be<uint32_t>(input,  2);
      uint32_t W03 = load_be<uint32_t>(input,  3);
      uint32_t W04 = load_be<uint32_t>(input,  4);
      uint32_t W05 = load_be<uint32_t>(input,  5);
      uint32_t W06 = load_be<uint32_t>(input,  6);
      uint32_t W07 = load_be<uint32_t>(input,  7);
      uint32_t W08 = load_be<uint32_t>(input,  8);
      uint32_t W09 = load_be<uint32_t>(input,  9);
      uint32_t W10 = load_be<uint32_t>(input, 10);
      uint32_t W11 = load_be<uint32_t>(input, 11);
      uint32_t W12 = load_be<uint32_t>(input, 12);
      uint32_t W13 = load_be<uint32_t>(input, 13);
      uint32_t W14 = load_be<uint32_t>(input, 14);
      uint32_t W15 = load_be<uint32_t>(input, 15);

      SHA2_32_F(A, B, C, D, E, F, G, H, W00, W14, W09, W01, 0x428A2F98);
      SHA2_32_F(H, A, B, C, D, E, F, G, W01, W15, W10, W02, 0x71374491);
      SHA2_32_F(G, H, A, B, C, D, E, F, W02, W00, W11, W03, 0xB5C0FBCF);
      SHA2_32_F(F, G, H, A, B, C, D, E, W03, W01, W12, W04, 0xE9B5DBA5);
      SHA2_32_F(E, F, G, H, A, B, C, D, W04, W02, W13, W05, 0x3956C25B);
      SHA2_32_F(D, E, F, G, H, A, B, C, W05, W03, W14, W06, 0x59F111F1);
      SHA2_32_F(C, D, E, F, G, H, A, B, W06, W04, W15, W07, 0x923F82A4);
      SHA2_32_F(B, C, D, E, F, G, H, A, W07, W05, W00, W08, 0xAB1C5ED5);
      SHA2_32_F(A, B, C, D, E, F, G, H, W08, W06, W01, W09, 0xD807AA98);
      SHA2_32_F(H, A, B, C, D, E, F, G, W09, W07, W02, W10, 0x12835B01);
      SHA2_32_F(G, H, A, B, C, D, E, F, W10, W08, W03, W11, 0x243185BE);
      SHA2_32_F(F, G, H, A, B, C, D, E, W11, W09, W04, W12, 0x550C7DC3);
      SHA2_32_F(E, F, G, H, A, B, C, D, W12, W10, W05, W13, 0x72BE5D74);
      SHA2_32_F(D, E, F, G, H, A, B, C, W13, W11, W06, W14, 0x80DEB1FE);
      SHA2_32_F(C, D, E, F, G, H, A, B, W14, W12, W07, W15, 0x9BDC06A7);
      SHA2_32_F(B, C, D, E, F, G, H, A, W15, W13, W08, W00, 0xC19BF174);

      SHA2_32_F(A, B, C, D, E, F, G, H, W00, W14, W09, W01, 0xE49B69C1);
      SHA2_32_F(H, A, B, C, D, E, F, G, W01, W15, W10, W02, 0xEFBE4786);
      SHA2_32_F(G, H, A, B, C, D, E, F, W02, W00, W11, W03, 0x0FC19DC6);
      SHA2_32_F(F, G, H, A, B, C, D, E, W03, W01, W12, W04, 0x240CA1CC);
      SHA2_32_F(E, F, G, H, A, B, C, D, W04, W02, W13, W05, 0x2DE92C6F);
      SHA2_32_F(D, E, F, G, H, A, B, C, W05, W03, W14, W06, 0x4A7484AA);
      SHA2_32_F(C, D, E, F, G, H, A, B, W06, W04, W15, W07, 0x5CB0A9DC);
      SHA2_32_F(B, C, D, E, F, G, H, A, W07, W05, W00, W08, 0x76F988DA);
      SHA2_32_F(A, B, C, D, E, F, G, H, W08, W06, W01, W09, 0x983E5152);
      SHA2_32_F(H, A, B, C, D, E, F, G, W09, W07, W02, W10, 0xA831C66D);
      SHA2_32_F(G, H, A, B, C, D, E, F, W10, W08, W03, W11, 0xB00327C8);
      SHA2_32_F(F, G, H, A, B, C, D, E, W11, W09, W04, W12, 0xBF597FC7);
      SHA2_32_F(E, F, G, H, A, B, C, D, W12, W10, W05, W13, 0xC6E00BF3);
      SHA2_32_F(D, E, F, G, H, A, B, C, W13, W11, W06, W14, 0xD5A79147);
      SHA2_32_F(C, D, E, F, G, H, A, B, W14, W12, W07, W15, 0x06CA6351);
      SHA2_32_F(B, C, D, E, F, G, H, A, W15, W13, W08, W00, 0x14292967);

      SHA2_32_F(A, B, C, D, E, F, G, H, W00, W14, W09, W01, 0x27B70A85);
      SHA2_32_F(H, A, B, C, D, E, F, G, W01, W15, W10, W02, 0x2E1B2138);
      SHA2_32_F(G, H, A, B, C, D, E, F, W02, W00, W11, W03, 0x4D2C6DFC);
      SHA2_32_F(F, G, H, A, B, C, D, E, W03, W01, W12, W04, 0x53380D13);
      SHA2_32_F(E, F, G, H, A, B, C, D, W04, W02, W13, W05, 0x650A7354);
      SHA2_32_F(D, E, F, G, H, A, B, C, W05, W03, W14, W06, 0x766A0ABB);
      SHA2_32_F(C, D, E, F, G, H, A, B, W06, W04, W15, W07, 0x81C2C92E);
      SHA2_32_F(B, C, D, E, F, G, H, A, W07, W05, W00, W08, 0x92722C85);
      SHA2_32_F(A, B, C, D, E, F, G, H, W08, W06, W01, W09, 0xA2BFE8A1);
      SHA2_32_F(H, A, B, C, D, E, F, G, W09, W07, W02, W10, 0xA81A664B);
      SHA2_32_F(G, H, A, B, C, D, E, F, W10, W08, W03, W11, 0xC24B8B70);
      SHA2_32_F(F, G, H, A, B, C, D, E, W11, W09, W04, W12, 0xC76C51A3);
      SHA2_32_F(E, F, G, H, A, B, C, D, W12, W10, W05, W13, 0xD192E819);
      SHA2_32_F(D, E, F, G, H, A, B, C, W13, W11, W06, W14, 0xD6990624);
      SHA2_32_F(C, D, E, F, G, H, A, B, W14, W12, W07, W15, 0xF40E3585);
      SHA2_32_F(B, C, D, E, F, G, H, A, W15, W13, W08, W00, 0x106AA070);

      SHA2_32_F(A, B, C, D, E, F, G, H, W00, W14, W09, W01, 0x19A4C116);
      SHA2_32_F(H, A, B, C, D, E, F, G, W01, W15, W10, W02, 0x1E376C08);
      SHA2_32_F(G, H, A, B, C, D, E, F, W02, W00, W11, W03, 0x2748774C);
      SHA2_32_F(F, G, H, A, B, C, D, E, W03, W01, W12, W04, 0x34B0BCB5);
      SHA2_32_F(E, F, G, H, A, B, C, D, W04, W02, W13, W05, 0x391C0CB3);
      SHA2_32_F(D, E, F, G, H, A, B, C, W05, W03, W14, W06, 0x4ED8AA4A);
      SHA2_32_F(C, D, E, F, G, H, A, B, W06, W04, W15, W07, 0x5B9CCA4F);
      SHA2_32_F(B, C, D, E, F, G, H, A, W07, W05, W00, W08, 0x682E6FF3);
      SHA2_32_F(A, B, C, D, E, F, G, H, W08, W06, W01, W09, 0x748F82EE);
      SHA2_32_F(H, A, B, C, D, E, F, G, W09, W07, W02, W10, 0x78A5636F);
      SHA2_32_F(G, H, A, B, C, D, E, F, W10, W08, W03, W11, 0x84C87814);
      SHA2_32_F(F, G, H, A, B, C, D, E, W11, W09, W04, W12, 0x8CC70208);
      SHA2_32_F(E, F, G, H, A, B, C, D, W12, W10, W05, W13, 0x90BEFFFA);
      SHA2_32_F(D, E, F, G, H, A, B, C, W13, W11, W06, W14, 0xA4506CEB);
      SHA2_32_F(C, D, E, F, G, H, A, B, W14, W12, W07, W15, 0xBEF9A3F7);
      SHA2_32_F(B, C, D, E, F, G, H, A, W15, W13, W08, W00, 0xC67178F2);

      A = (digest[0] += A);
      B = (digest[1] += B);
      C = (digest[2] += C);
      D = (digest[3] += D);
      E = (digest[4] += E);
      F = (digest[5] += F);
      G = (digest[6] += G);
      H = (digest[7] += H);

      input += 64;
      }
   }

std::string SHA_224::provider() const
   {
   return sha256_provider();
   }

std::string SHA_256::provider() const
   {
   return sha256_provider();
   }

/*
* SHA-224 compression function
*/
void SHA_224::compress_n(const uint8_t input[], size_t blocks)
   {
   SHA_256::compress_digest(m_digest, input, blocks);
   }

/*
* Copy out the digest
*/
void SHA_224::copy_out(uint8_t output[])
   {
   copy_out_vec_be(output, output_length(), m_digest);
   }

/*
* Clear memory of sensitive data
*/
void SHA_224::clear()
   {
   MDx_HashFunction::clear();
   m_digest[0] = 0xC1059ED8;
   m_digest[1] = 0x367CD507;
   m_digest[2] = 0x3070DD17;
   m_digest[3] = 0xF70E5939;
   m_digest[4] = 0xFFC00B31;
   m_digest[5] = 0x68581511;
   m_digest[6] = 0x64F98FA7;
   m_digest[7] = 0xBEFA4FA4;
   }

/*
* SHA-256 compression function
*/
void SHA_256::compress_n(const uint8_t input[], size_t blocks)
   {
   SHA_256::compress_digest(m_digest, input, blocks);
   }

/*
* Copy out the digest
*/
void SHA_256::copy_out(uint8_t output[])
   {
   copy_out_vec_be(output, output_length(), m_digest);
   }

/*
* Clear memory of sensitive data
*/
void SHA_256::clear()
   {
   MDx_HashFunction::clear();
   m_digest[0] = 0x6A09E667;
   m_digest[1] = 0xBB67AE85;
   m_digest[2] = 0x3C6EF372;
   m_digest[3] = 0xA54FF53A;
   m_digest[4] = 0x510E527F;
   m_digest[5] = 0x9B05688C;
   m_digest[6] = 0x1F83D9AB;
   m_digest[7] = 0x5BE0CD19;
   }

}
/*
* Stream Ciphers
* (C) 2015,2016 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_CHACHA)
#endif

#if defined(BOTAN_HAS_SALSA20)
#endif

#if defined(BOTAN_HAS_SHAKE_CIPHER)
#endif

#if defined(BOTAN_HAS_CTR_BE)
#endif

#if defined(BOTAN_HAS_OFB)
#endif

#if defined(BOTAN_HAS_RC4)
#endif

#if defined(BOTAN_HAS_OPENSSL)
#endif

namespace Botan {

std::unique_ptr<StreamCipher> StreamCipher::create(const std::string& algo_spec,
                                                   const std::string& provider)
   {
   const SCAN_Name req(algo_spec);

#if defined(BOTAN_HAS_CTR_BE)
   if((req.algo_name() == "CTR-BE" || req.algo_name() == "CTR") && req.arg_count_between(1,2))
      {
      if(provider.empty() || provider == "base")
         {
         auto cipher = BlockCipher::create(req.arg(0));
         if(cipher)
            {
            size_t ctr_size = req.arg_as_integer(1, cipher->block_size());
            return std::unique_ptr<StreamCipher>(new CTR_BE(cipher.release(), ctr_size));
            }
         }
      }
#endif

#if defined(BOTAN_HAS_CHACHA)
   if(req.algo_name() == "ChaCha")
      {
      if(provider.empty() || provider == "base")
         return std::unique_ptr<StreamCipher>(new ChaCha(req.arg_as_integer(0, 20)));
      }

   if(req.algo_name() == "ChaCha20")
      {
      if(provider.empty() || provider == "base")
         return std::unique_ptr<StreamCipher>(new ChaCha(20));
      }
#endif

#if defined(BOTAN_HAS_SALSA20)
   if(req.algo_name() == "Salsa20")
      {
      if(provider.empty() || provider == "base")
         return std::unique_ptr<StreamCipher>(new Salsa20);
      }
#endif

#if defined(BOTAN_HAS_SHAKE_CIPHER)
   if(req.algo_name() == "SHAKE-128" || req.algo_name() == "SHAKE-128-XOF")
      {
      if(provider.empty() || provider == "base")
         return std::unique_ptr<StreamCipher>(new SHAKE_128_Cipher);
      }
#endif

#if defined(BOTAN_HAS_OFB)
   if(req.algo_name() == "OFB" && req.arg_count() == 1)
      {
      if(provider.empty() || provider == "base")
         {
         if(auto c = BlockCipher::create(req.arg(0)))
            return std::unique_ptr<StreamCipher>(new OFB(c.release()));
         }
      }
#endif

#if defined(BOTAN_HAS_RC4)

   if(req.algo_name() == "RC4" ||
      req.algo_name() == "ARC4" ||
      req.algo_name() == "MARK-4")
      {
      const size_t skip = (req.algo_name() == "MARK-4") ? 256 : req.arg_as_integer(0, 0);

#if defined(BOTAN_HAS_OPENSSL)
      if(provider.empty() || provider == "openssl")
         {
         return std::unique_ptr<StreamCipher>(make_openssl_rc4(skip));
         }
#endif

      if(provider.empty() || provider == "base")
         {
         return std::unique_ptr<StreamCipher>(new RC4(skip));
         }
      }

#endif

   BOTAN_UNUSED(req);
   BOTAN_UNUSED(provider);

   return nullptr;
   }

//static
std::unique_ptr<StreamCipher>
StreamCipher::create_or_throw(const std::string& algo,
                             const std::string& provider)
   {
   if(auto sc = StreamCipher::create(algo, provider))
      {
      return sc;
      }
   throw Lookup_Error("Stream cipher", algo, provider);
   }

std::vector<std::string> StreamCipher::providers(const std::string& algo_spec)
   {
   return probe_providers_of<StreamCipher>(algo_spec, {"base", "openssl"});
   }

}
/*
* Runtime assertion checking
* (C) 2010,2012,2018 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

void throw_invalid_argument(const char* message,
                            const char* func,
                            const char* file)
   {
   std::ostringstream format;
   format << message << " in " << func << ":" << file;
   throw Invalid_Argument(format.str());
   }

void throw_invalid_state(const char* expr,
                         const char* func,
                         const char* file)
   {
   std::ostringstream format;
   format << "Invalid state: " << expr << " was false in " << func << ":" << file;
   throw Invalid_State(format.str());
   }

void assertion_failure(const char* expr_str,
                       const char* assertion_made,
                       const char* func,
                       const char* file,
                       int line)
   {
   std::ostringstream format;

   format << "False assertion ";

   if(assertion_made && assertion_made[0] != 0)
      format << "'" << assertion_made << "' (expression " << expr_str << ") ";
   else
      format << expr_str << " ";

   if(func)
      format << "in " << func << " ";

   format << "@" << file << ":" << line;

   throw Internal_Error(format.str());
   }

}
/*
* Calendar Functions
* (C) 1999-2010,2017 Jack Lloyd
* (C) 2015 Simon Warta (Kullo GmbH)
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <ctime>
#include <iomanip>
#include <stdlib.h>

namespace Botan {

namespace {

std::tm do_gmtime(std::time_t time_val)
   {
   std::tm tm;

#if defined(BOTAN_TARGET_OS_HAS_WIN32)
   ::gmtime_s(&tm, &time_val); // Windows
#elif defined(BOTAN_TARGET_OS_HAS_POSIX1)
   ::gmtime_r(&time_val, &tm); // Unix/SUSv2
#else
   std::tm* tm_p = std::gmtime(&time_val);
   if (tm_p == nullptr)
      throw Encoding_Error("time_t_to_tm could not convert");
   tm = *tm_p;
#endif

   return tm;
   }

/*
Portable replacement for timegm, _mkgmtime, etc

Algorithm due to Howard Hinnant

See https://howardhinnant.github.io/date_algorithms.html#days_from_civil
for details and explaination. The code is slightly simplified by our assumption
that the date is at least 1970, which is sufficient for our purposes.
*/
size_t days_since_epoch(uint32_t year, uint32_t month, uint32_t day)
   {
   if(month <= 2)
      year -= 1;
   const uint32_t era = year / 400;
   const uint32_t yoe = year - era * 400;      // [0, 399]
   const uint32_t doy = (153*(month + (month > 2 ? -3 : 9)) + 2)/5 + day-1;  // [0, 365]
   const uint32_t doe = yoe * 365 + yoe/4 - yoe/100 + doy;         // [0, 146096]
   return era * 146097 + doe - 719468;
   }

}

std::chrono::system_clock::time_point calendar_point::to_std_timepoint() const
   {
   if(get_year() < 1970)
      throw Invalid_Argument("calendar_point::to_std_timepoint() does not support years before 1970");

   // 32 bit time_t ends at January 19, 2038
   // https://msdn.microsoft.com/en-us/library/2093ets1.aspx
   // Throw after 2037 if 32 bit time_t is used

   BOTAN_IF_CONSTEXPR(sizeof(std::time_t) == 4)
      {
      if(get_year() > 2037)
         {
         throw Invalid_Argument("calendar_point::to_std_timepoint() does not support years after 2037 on this system");
         }
      }

   // This upper bound is completely arbitrary
   if(get_year() >= 2400)
      {
      throw Invalid_Argument("calendar_point::to_std_timepoint() does not support years after 2400");
      }

   const uint64_t seconds_64 = (days_since_epoch(get_year(), get_month(), get_day()) * 86400) +
                                (get_hour() * 60 * 60) + (get_minutes() * 60) + get_seconds();

   const time_t seconds_time_t = static_cast<time_t>(seconds_64);

   if(seconds_64 - seconds_time_t != 0)
      {
      throw Invalid_Argument("calendar_point::to_std_timepoint time_t overflow");
      }

   return std::chrono::system_clock::from_time_t(seconds_time_t);
   }

std::string calendar_point::to_string() const
   {
   // desired format: <YYYY>-<MM>-<dd>T<HH>:<mm>:<ss>
   std::stringstream output;
   output << std::setfill('0')
          << std::setw(4) << get_year() << "-"
          << std::setw(2) << get_month() << "-"
          << std::setw(2) << get_day() << "T"
          << std::setw(2) << get_hour() << ":"
          << std::setw(2) << get_minutes() << ":"
          << std::setw(2) << get_seconds();
   return output.str();
   }


calendar_point calendar_value(
   const std::chrono::system_clock::time_point& time_point)
   {
   std::tm tm = do_gmtime(std::chrono::system_clock::to_time_t(time_point));

   return calendar_point(tm.tm_year + 1900,
                         tm.tm_mon + 1,
                         tm.tm_mday,
                         tm.tm_hour,
                         tm.tm_min,
                         tm.tm_sec);
   }

}
/*
* Character Set Handling
* (C) 1999-2007 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <cctype>

namespace Botan {

namespace {

void append_utf8_for(std::string& s, uint32_t c)
   {
   if(c >= 0xD800 && c < 0xE000)
      throw Decoding_Error("Invalid Unicode character");

   if(c <= 0x7F)
      {
      const uint8_t b0 = static_cast<uint8_t>(c);
      s.push_back(static_cast<char>(b0));
      }
   else if(c <= 0x7FF)
      {
      const uint8_t b0 = 0xC0 | static_cast<uint8_t>(c >> 6);
      const uint8_t b1 = 0x80 | static_cast<uint8_t>(c & 0x3F);
      s.push_back(static_cast<char>(b0));
      s.push_back(static_cast<char>(b1));
      }
   else if(c <= 0xFFFF)
      {
      const uint8_t b0 = 0xE0 | static_cast<uint8_t>(c >> 12);
      const uint8_t b1 = 0x80 | static_cast<uint8_t>((c >> 6) & 0x3F);
      const uint8_t b2 = 0x80 | static_cast<uint8_t>(c & 0x3F);
      s.push_back(static_cast<char>(b0));
      s.push_back(static_cast<char>(b1));
      s.push_back(static_cast<char>(b2));
      }
   else if(c <= 0x10FFFF)
      {
      const uint8_t b0 = 0xF0 | static_cast<uint8_t>(c >> 18);
      const uint8_t b1 = 0x80 | static_cast<uint8_t>((c >> 12) & 0x3F);
      const uint8_t b2 = 0x80 | static_cast<uint8_t>((c >> 6) & 0x3F);
      const uint8_t b3 = 0x80 | static_cast<uint8_t>(c & 0x3F);
      s.push_back(static_cast<char>(b0));
      s.push_back(static_cast<char>(b1));
      s.push_back(static_cast<char>(b2));
      s.push_back(static_cast<char>(b3));
      }
   else
      throw Decoding_Error("Invalid Unicode character");

   }

}

std::string ucs2_to_utf8(const uint8_t ucs2[], size_t len)
   {
   if(len % 2 != 0)
      throw Decoding_Error("Invalid length for UCS-2 string");

   const size_t chars = len / 2;

   std::string s;
   for(size_t i = 0; i != chars; ++i)
      {
      const uint16_t c = load_be<uint16_t>(ucs2, i);
      append_utf8_for(s, c);
      }

   return s;
   }

std::string ucs4_to_utf8(const uint8_t ucs4[], size_t len)
   {
   if(len % 4 != 0)
      throw Decoding_Error("Invalid length for UCS-4 string");

   const size_t chars = len / 4;

   std::string s;
   for(size_t i = 0; i != chars; ++i)
      {
      const uint32_t c = load_be<uint32_t>(ucs4, i);
      append_utf8_for(s, c);
      }

   return s;
   }

/*
* Convert from UTF-8 to ISO 8859-1
*/
std::string utf8_to_latin1(const std::string& utf8)
   {
   std::string iso8859;

   size_t position = 0;
   while(position != utf8.size())
      {
      const uint8_t c1 = static_cast<uint8_t>(utf8[position++]);

      if(c1 <= 0x7F)
         {
         iso8859 += static_cast<char>(c1);
         }
      else if(c1 >= 0xC0 && c1 <= 0xC7)
         {
         if(position == utf8.size())
            throw Decoding_Error("UTF-8: sequence truncated");

         const uint8_t c2 = static_cast<uint8_t>(utf8[position++]);
         const uint8_t iso_char = ((c1 & 0x07) << 6) | (c2 & 0x3F);

         if(iso_char <= 0x7F)
            throw Decoding_Error("UTF-8: sequence longer than needed");

         iso8859 += static_cast<char>(iso_char);
         }
      else
         throw Decoding_Error("UTF-8: Unicode chars not in Latin1 used");
      }

   return iso8859;
   }

namespace Charset {

namespace {

/*
* Convert from UCS-2 to ISO 8859-1
*/
std::string ucs2_to_latin1(const std::string& ucs2)
   {
   if(ucs2.size() % 2 == 1)
      throw Decoding_Error("UCS-2 string has an odd number of bytes");

   std::string latin1;

   for(size_t i = 0; i != ucs2.size(); i += 2)
      {
      const uint8_t c1 = ucs2[i];
      const uint8_t c2 = ucs2[i+1];

      if(c1 != 0)
         throw Decoding_Error("UCS-2 has non-Latin1 characters");

      latin1 += static_cast<char>(c2);
      }

   return latin1;
   }

/*
* Convert from ISO 8859-1 to UTF-8
*/
std::string latin1_to_utf8(const std::string& iso8859)
   {
   std::string utf8;
   for(size_t i = 0; i != iso8859.size(); ++i)
      {
      const uint8_t c = static_cast<uint8_t>(iso8859[i]);

      if(c <= 0x7F)
         utf8 += static_cast<char>(c);
      else
         {
         utf8 += static_cast<char>((0xC0 | (c >> 6)));
         utf8 += static_cast<char>((0x80 | (c & 0x3F)));
         }
      }
   return utf8;
   }

}

/*
* Perform character set transcoding
*/
std::string transcode(const std::string& str,
                      Character_Set to, Character_Set from)
   {
   if(to == LOCAL_CHARSET)
      to = LATIN1_CHARSET;
   if(from == LOCAL_CHARSET)
      from = LATIN1_CHARSET;

   if(to == from)
      return str;

   if(from == LATIN1_CHARSET && to == UTF8_CHARSET)
      return latin1_to_utf8(str);
   if(from == UTF8_CHARSET && to == LATIN1_CHARSET)
      return utf8_to_latin1(str);
   if(from == UCS2_CHARSET && to == LATIN1_CHARSET)
      return ucs2_to_latin1(str);

   throw Invalid_Argument("Unknown transcoding operation from " +
                          std::to_string(from) + " to " + std::to_string(to));
   }

/*
* Check if a character represents a digit
*/
bool is_digit(char c)
   {
   if(c == '0' || c == '1' || c == '2' || c == '3' || c == '4' ||
      c == '5' || c == '6' || c == '7' || c == '8' || c == '9')
      return true;
   return false;
   }

/*
* Check if a character represents whitespace
*/
bool is_space(char c)
   {
   if(c == ' ' || c == '\t' || c == '\n' || c == '\r')
      return true;
   return false;
   }

/*
* Convert a character to a digit
*/
uint8_t char2digit(char c)
   {
   switch(c)
      {
      case '0': return 0;
      case '1': return 1;
      case '2': return 2;
      case '3': return 3;
      case '4': return 4;
      case '5': return 5;
      case '6': return 6;
      case '7': return 7;
      case '8': return 8;
      case '9': return 9;
      }

   throw Invalid_Argument("char2digit: Input is not a digit character");
   }

/*
* Convert a digit to a character
*/
char digit2char(uint8_t b)
   {
   switch(b)
      {
      case 0: return '0';
      case 1: return '1';
      case 2: return '2';
      case 3: return '3';
      case 4: return '4';
      case 5: return '5';
      case 6: return '6';
      case 7: return '7';
      case 8: return '8';
      case 9: return '9';
      }

   throw Invalid_Argument("digit2char: Input is not a digit");
   }

/*
* Case-insensitive character comparison
*/
bool caseless_cmp(char a, char b)
   {
   return (std::tolower(static_cast<unsigned char>(a)) ==
           std::tolower(static_cast<unsigned char>(b)));
   }

}

}
/*
* (C) 2018 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace CT {

secure_vector<uint8_t> copy_output(CT::Mask<uint8_t> bad_input,
                                   const uint8_t input[],
                                   size_t input_length,
                                   size_t offset)
   {
   if(input_length == 0)
      return secure_vector<uint8_t>();

   /*
   * Ensure at runtime that offset <= input_length. This is an invalid input,
   * but we can't throw without using the poisoned value. Instead, if it happens,
   * set offset to be equal to the input length (so output_bytes becomes 0 and
   * the returned vector is empty)
   */
   const auto valid_offset = CT::Mask<size_t>::is_lte(offset, input_length);
   offset = valid_offset.select(offset, input_length);

   const size_t output_bytes = input_length - offset;

   secure_vector<uint8_t> output(input_length);

   /*
   Move the desired output bytes to the front using a slow (O^n)
   but constant time loop that does not leak the value of the offset
   */
   for(size_t i = 0; i != input_length; ++i)
      {
      /*
      start index from i rather than 0 since we know j must be >= i + offset
      to have any effect, and starting from i does not reveal information
      */
      for(size_t j = i; j != input_length; ++j)
         {
         const uint8_t b = input[j];
         const auto is_eq = CT::Mask<size_t>::is_equal(j, offset + i);
         output[i] |= is_eq.if_set_return(b);
         }
      }

   bad_input.if_set_zero_out(output.data(), output.size());

   CT::unpoison(output.data(), output.size());
   CT::unpoison(output_bytes);

   /*
   This is potentially not const time, depending on how std::vector is
   implemented. But since we are always reducing length, it should
   just amount to setting the member var holding the length.
   */
   output.resize(output_bytes);
   return output;
   }

secure_vector<uint8_t> strip_leading_zeros(const uint8_t in[], size_t length)
   {
   size_t leading_zeros = 0;

   auto only_zeros = Mask<uint8_t>::set();

   for(size_t i = 0; i != length; ++i)
      {
      only_zeros &= CT::Mask<uint8_t>::is_zero(in[i]);
      leading_zeros += only_zeros.if_set_return(1);
      }

   return copy_output(CT::Mask<uint8_t>::cleared(), in, length, leading_zeros);
   }

}

}
/*
* DataSource
* (C) 1999-2007 Jack Lloyd
*     2005 Matthew Gregan
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <istream>

#if defined(BOTAN_TARGET_OS_HAS_FILESYSTEM)
  #include <fstream>
#endif

namespace Botan {

/*
* Read a single byte from the DataSource
*/
size_t DataSource::read_byte(uint8_t& out)
   {
   return read(&out, 1);
   }

/*
* Peek a single byte from the DataSource
*/
size_t DataSource::peek_byte(uint8_t& out) const
   {
   return peek(&out, 1, 0);
   }

/*
* Discard the next N bytes of the data
*/
size_t DataSource::discard_next(size_t n)
   {
   uint8_t buf[64] = { 0 };
   size_t discarded = 0;

   while(n)
      {
      const size_t got = this->read(buf, std::min(n, sizeof(buf)));
      discarded += got;
      n -= got;

      if(got == 0)
         break;
      }

   return discarded;
   }

/*
* Read from a memory buffer
*/
size_t DataSource_Memory::read(uint8_t out[], size_t length)
   {
   const size_t got = std::min<size_t>(m_source.size() - m_offset, length);
   copy_mem(out, m_source.data() + m_offset, got);
   m_offset += got;
   return got;
   }

bool DataSource_Memory::check_available(size_t n)
   {
   return (n <= (m_source.size() - m_offset));
   }

/*
* Peek into a memory buffer
*/
size_t DataSource_Memory::peek(uint8_t out[], size_t length,
                               size_t peek_offset) const
   {
   const size_t bytes_left = m_source.size() - m_offset;
   if(peek_offset >= bytes_left) return 0;

   const size_t got = std::min(bytes_left - peek_offset, length);
   copy_mem(out, &m_source[m_offset + peek_offset], got);
   return got;
   }

/*
* Check if the memory buffer is empty
*/
bool DataSource_Memory::end_of_data() const
   {
   return (m_offset == m_source.size());
   }

/*
* DataSource_Memory Constructor
*/
DataSource_Memory::DataSource_Memory(const std::string& in) :
   m_source(cast_char_ptr_to_uint8(in.data()),
            cast_char_ptr_to_uint8(in.data()) + in.length()),
   m_offset(0)
   {
   }

/*
* Read from a stream
*/
size_t DataSource_Stream::read(uint8_t out[], size_t length)
   {
   m_source.read(cast_uint8_ptr_to_char(out), length);
   if(m_source.bad())
      throw Stream_IO_Error("DataSource_Stream::read: Source failure");

   const size_t got = static_cast<size_t>(m_source.gcount());
   m_total_read += got;
   return got;
   }

bool DataSource_Stream::check_available(size_t n)
   {
   const std::streampos orig_pos = m_source.tellg();
   m_source.seekg(0, std::ios::end);
   const size_t avail = static_cast<size_t>(m_source.tellg() - orig_pos);
   m_source.seekg(orig_pos);
   return (avail >= n);
   }

/*
* Peek into a stream
*/
size_t DataSource_Stream::peek(uint8_t out[], size_t length, size_t offset) const
   {
   if(end_of_data())
      throw Invalid_State("DataSource_Stream: Cannot peek when out of data");

   size_t got = 0;

   if(offset)
      {
      secure_vector<uint8_t> buf(offset);
      m_source.read(cast_uint8_ptr_to_char(buf.data()), buf.size());
      if(m_source.bad())
         throw Stream_IO_Error("DataSource_Stream::peek: Source failure");
      got = static_cast<size_t>(m_source.gcount());
      }

   if(got == offset)
      {
      m_source.read(cast_uint8_ptr_to_char(out), length);
      if(m_source.bad())
         throw Stream_IO_Error("DataSource_Stream::peek: Source failure");
      got = static_cast<size_t>(m_source.gcount());
      }

   if(m_source.eof())
      m_source.clear();
   m_source.seekg(m_total_read, std::ios::beg);

   return got;
   }

/*
* Check if the stream is empty or in error
*/
bool DataSource_Stream::end_of_data() const
   {
   return (!m_source.good());
   }

/*
* Return a human-readable ID for this stream
*/
std::string DataSource_Stream::id() const
   {
   return m_identifier;
   }

#if defined(BOTAN_TARGET_OS_HAS_FILESYSTEM)

/*
* DataSource_Stream Constructor
*/
DataSource_Stream::DataSource_Stream(const std::string& path,
                                     bool use_binary) :
   m_identifier(path),
   m_source_memory(new std::ifstream(path, use_binary ? std::ios::binary : std::ios::in)),
   m_source(*m_source_memory),
   m_total_read(0)
   {
   if(!m_source.good())
      {
      throw Stream_IO_Error("DataSource: Failure opening file " + path);
      }
   }

#endif

/*
* DataSource_Stream Constructor
*/
DataSource_Stream::DataSource_Stream(std::istream& in,
                                     const std::string& name) :
   m_identifier(name),
   m_source(in),
   m_total_read(0)
   {
   }

DataSource_Stream::~DataSource_Stream()
   {
   // for ~unique_ptr
   }

}
/*
* (C) 2017 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

std::string to_string(ErrorType type)
   {
   switch(type)
      {
      case ErrorType::Unknown:
         return "Unknown";
      case ErrorType::SystemError:
         return "SystemError";
      case ErrorType::NotImplemented:
         return "NotImplemented";
      case ErrorType::OutOfMemory:
         return "OutOfMemory";
      case ErrorType::InternalError:
         return "InternalError";
      case ErrorType::IoError:
         return "IoError";
      case ErrorType::InvalidObjectState :
         return "InvalidObjectState";
      case ErrorType::KeyNotSet:
         return "KeyNotSet";
      case ErrorType::InvalidArgument:
         return "InvalidArgument";
      case ErrorType::InvalidKeyLength:
         return "InvalidKeyLength";
      case ErrorType::InvalidNonceLength:
         return "InvalidNonceLength";
      case ErrorType::LookupError:
         return "LookupError";
      case ErrorType::EncodingFailure:
         return "EncodingFailure";
      case ErrorType::DecodingFailure:
         return "DecodingFailure";
      case ErrorType::TLSError:
         return "TLSError";
      case ErrorType::HttpError:
         return "HttpError";
      case ErrorType::InvalidTag:
         return "InvalidTag";
      case ErrorType::RoughtimeError:
         return "RoughtimeError";
      case ErrorType::OpenSSLError :
         return "OpenSSLError";
      case ErrorType::CommonCryptoError:
         return "CommonCryptoError";
      case ErrorType::Pkcs11Error:
         return "Pkcs11Error";
      case ErrorType::TPMError:
         return "TPMError";
      case ErrorType::DatabaseError:
         return "DatabaseError";
      case ErrorType::ZlibError :
         return "ZlibError";
      case ErrorType::Bzip2Error:
         return "Bzip2Error" ;
      case ErrorType::LzmaError:
         return "LzmaError";
      }

   // No default case in above switch so compiler warns
   return "Unrecognized Botan error";
   }

Exception::Exception(const std::string& msg) : m_msg(msg)
   {}

Exception::Exception(const std::string& msg, const std::exception& e) :
   m_msg(msg + " failed with " + std::string(e.what()))
   {}

Exception::Exception(const char* prefix, const std::string& msg) :
   m_msg(std::string(prefix) + " " + msg)
   {}

Invalid_Argument::Invalid_Argument(const std::string& msg) :
   Exception(msg)
   {}

Invalid_Argument::Invalid_Argument(const std::string& msg, const std::string& where) :
   Exception(msg + " in " + where)
   {}

Invalid_Argument::Invalid_Argument(const std::string& msg, const std::exception& e) :
   Exception(msg, e) {}

Lookup_Error::Lookup_Error(const std::string& type,
                           const std::string& algo,
                           const std::string& provider) :
   Exception("Unavailable " + type + " " + algo +
             (provider.empty() ? std::string("") : (" for provider " + provider)))
   {}

Internal_Error::Internal_Error(const std::string& err) :
   Exception("Internal error: " + err)
   {}

Invalid_Key_Length::Invalid_Key_Length(const std::string& name, size_t length) :
   Invalid_Argument(name + " cannot accept a key of length " +
                    std::to_string(length))
   {}

Invalid_IV_Length::Invalid_IV_Length(const std::string& mode, size_t bad_len) :
   Invalid_Argument("IV length " + std::to_string(bad_len) +
                    " is invalid for " + mode)
   {}

Key_Not_Set::Key_Not_Set(const std::string& algo) :
   Invalid_State("Key not set in " + algo)
   {}

Policy_Violation::Policy_Violation(const std::string& err) :
   Invalid_State("Policy violation: " + err) {}

PRNG_Unseeded::PRNG_Unseeded(const std::string& algo) :
   Invalid_State("PRNG not seeded: " + algo)
   {}

Algorithm_Not_Found::Algorithm_Not_Found(const std::string& name) :
   Lookup_Error("Could not find any algorithm named \"" + name + "\"")
   {}

No_Provider_Found::No_Provider_Found(const std::string& name) :
   Exception("Could not find any provider for algorithm named \"" + name + "\"")
   {}

Provider_Not_Found::Provider_Not_Found(const std::string& algo, const std::string& provider) :
   Lookup_Error("Could not find provider '" + provider + "' for " + algo)
   {}

Invalid_Algorithm_Name::Invalid_Algorithm_Name(const std::string& name):
   Invalid_Argument("Invalid algorithm name: " + name)
   {}

Encoding_Error::Encoding_Error(const std::string& name) :
   Invalid_Argument("Encoding error: " + name)
   {}

Decoding_Error::Decoding_Error(const std::string& name) :
   Invalid_Argument(name)
   {}

Decoding_Error::Decoding_Error(const std::string& msg, const std::exception& e) :
   Invalid_Argument(msg, e)
   {}

Decoding_Error::Decoding_Error(const std::string& name, const char* exception_message) :
   Invalid_Argument(name + " failed with exception " + exception_message) {}

Invalid_Authentication_Tag::Invalid_Authentication_Tag(const std::string& msg) :
   Exception("Invalid authentication tag: " + msg)
   {}

Invalid_OID::Invalid_OID(const std::string& oid) :
   Decoding_Error("Invalid ASN.1 OID: " + oid)
   {}

Stream_IO_Error::Stream_IO_Error(const std::string& err) :
   Exception("I/O error: " + err)
   {}

System_Error::System_Error(const std::string& msg, int err_code) :
   Exception(msg + " error code " + std::to_string(err_code)),
   m_error_code(err_code)
   {}

Self_Test_Failure::Self_Test_Failure(const std::string& err) :
   Internal_Error("Self test failed: " + err)
   {}

Not_Implemented::Not_Implemented(const std::string& err) :
   Exception("Not implemented", err)
   {}

}
/*
* (C) 2015,2017,2019 Jack Lloyd
* (C) 2015 Simon Warta (Kullo GmbH)
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <deque>
#include <memory>

#if defined(BOTAN_TARGET_OS_HAS_POSIX1)
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <dirent.h>
  #include <functional>
#elif defined(BOTAN_TARGET_OS_HAS_WIN32)
  #define NOMINMAX 1
  #define _WINSOCKAPI_ // stop windows.h including winsock.h
  #include <windows.h>
#endif

namespace Botan {

namespace {

#if defined(BOTAN_TARGET_OS_HAS_POSIX1)

std::vector<std::string> impl_readdir(const std::string& dir_path)
   {
   std::vector<std::string> out;
   std::deque<std::string> dir_list;
   dir_list.push_back(dir_path);

   while(!dir_list.empty())
      {
      const std::string cur_path = dir_list[0];
      dir_list.pop_front();

      std::unique_ptr<DIR, std::function<int (DIR*)>> dir(::opendir(cur_path.c_str()), ::closedir);

      if(dir)
         {
         while(struct dirent* dirent = ::readdir(dir.get()))
            {
            const std::string filename = dirent->d_name;
            if(filename == "." || filename == "..")
               continue;
            const std::string full_path = cur_path + "/" + filename;

            struct stat stat_buf;

            if(::stat(full_path.c_str(), &stat_buf) == -1)
               continue;

            if(S_ISDIR(stat_buf.st_mode))
               dir_list.push_back(full_path);
            else if(S_ISREG(stat_buf.st_mode))
               out.push_back(full_path);
            }
         }
      }

   return out;
   }

#elif defined(BOTAN_TARGET_OS_HAS_WIN32)

std::vector<std::string> impl_win32(const std::string& dir_path)
   {
   std::vector<std::string> out;
   std::deque<std::string> dir_list;
   dir_list.push_back(dir_path);

   while(!dir_list.empty())
      {
      const std::string cur_path = dir_list[0];
      dir_list.pop_front();

      WIN32_FIND_DATAA find_data;
      HANDLE dir = ::FindFirstFileA((cur_path + "/*").c_str(), &find_data);

      if(dir != INVALID_HANDLE_VALUE)
         {
         do
            {
            const std::string filename = find_data.cFileName;
            if(filename == "." || filename == "..")
               continue;
            const std::string full_path = cur_path + "/" + filename;

            if(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
               {
               dir_list.push_back(full_path);
               }
            else
               {
               out.push_back(full_path);
               }
            }
         while(::FindNextFileA(dir, &find_data));
         }

      ::FindClose(dir);
      }

   return out;
}
#endif

}

bool has_filesystem_impl()
   {
#if defined(BOTAN_TARGET_OS_HAS_POSIX1)
   return true;
#elif defined(BOTAN_TARGET_OS_HAS_WIN32)
   return true;
#else
   return false;
#endif
   }

std::vector<std::string> get_files_recursive(const std::string& dir)
   {
   std::vector<std::string> files;

#if defined(BOTAN_TARGET_OS_HAS_POSIX1)
   files = impl_readdir(dir);
#elif defined(BOTAN_TARGET_OS_HAS_WIN32)
   files = impl_win32(dir);
#else
   BOTAN_UNUSED(dir);
   throw No_Filesystem_Access();
#endif

   std::sort(files.begin(), files.end());

   return files;
   }

}
/*
* (C) 2017 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <cstdlib>
#include <new>

#if defined(BOTAN_HAS_LOCKING_ALLOCATOR)
#endif

namespace Botan {

BOTAN_MALLOC_FN void* allocate_memory(size_t elems, size_t elem_size)
   {
   if(elems == 0 || elem_size == 0)
      return nullptr;

#if defined(BOTAN_HAS_LOCKING_ALLOCATOR)
   if(void* p = mlock_allocator::instance().allocate(elems, elem_size))
      return p;
#endif

   void* ptr = std::calloc(elems, elem_size);
   if(!ptr)
      throw std::bad_alloc();
   return ptr;
   }

void deallocate_memory(void* p, size_t elems, size_t elem_size)
   {
   if(p == nullptr)
      return;

   secure_scrub_memory(p, elems * elem_size);

#if defined(BOTAN_HAS_LOCKING_ALLOCATOR)
   if(mlock_allocator::instance().deallocate(p, elems, elem_size))
      return;
#endif

   std::free(p);
   }

void initialize_allocator()
   {
#if defined(BOTAN_HAS_LOCKING_ALLOCATOR)
   mlock_allocator::instance();
#endif
   }

uint8_t ct_compare_u8(const uint8_t x[],
                      const uint8_t y[],
                      size_t len)
   {
   volatile uint8_t difference = 0;

   for(size_t i = 0; i != len; ++i)
      difference |= (x[i] ^ y[i]);

   return CT::Mask<uint8_t>::is_zero(difference).value();
   }

}
/*
* OS and machine specific utility functions
* (C) 2015,2016,2017,2018 Jack Lloyd
* (C) 2016 Daniel Neus
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#include <chrono>

#if defined(BOTAN_TARGET_OS_HAS_THREADS)
  #include <thread>
#endif

#if defined(BOTAN_TARGET_OS_HAS_EXPLICIT_BZERO)
  #include <string.h>
#endif

#if defined(BOTAN_TARGET_OS_HAS_POSIX1)
  #include <sys/types.h>
  #include <sys/resource.h>
  #include <sys/mman.h>
  #include <signal.h>
  #include <setjmp.h>
  #include <unistd.h>
  #include <errno.h>
  #include <termios.h>
  #undef B0
#endif

#if defined(BOTAN_TARGET_OS_IS_EMSCRIPTEN)
  #include <emscripten/emscripten.h>
#endif

#if defined(BOTAN_TARGET_OS_HAS_GETAUXVAL) || defined(BOTAN_TARGET_OS_IS_ANDROID) || \
  defined(BOTAN_TARGET_OS_HAS_ELF_AUX_INFO)
  #include <sys/auxv.h>
#endif

#if defined(BOTAN_TARGET_OS_HAS_WIN32)
  #define NOMINMAX 1
  #define _WINSOCKAPI_ // stop windows.h including winsock.h
  #include <windows.h>
#endif

#if defined(BOTAN_TARGET_OS_IS_ANDROID)
  #include <elf.h>
  extern "C" char **environ;
#endif

#if defined(BOTAN_TARGET_OS_IS_IOS) || defined(BOTAN_TARGET_OS_IS_MACOS)
  #include <mach/vm_statistics.h>
#endif

namespace Botan {

// Not defined in OS namespace for historical reasons
void secure_scrub_memory(void* ptr, size_t n)
   {
#if defined(BOTAN_TARGET_OS_HAS_RTLSECUREZEROMEMORY)
   ::RtlSecureZeroMemory(ptr, n);

#elif defined(BOTAN_TARGET_OS_HAS_EXPLICIT_BZERO)
   ::explicit_bzero(ptr, n);

#elif defined(BOTAN_TARGET_OS_HAS_EXPLICIT_MEMSET)
   (void)::explicit_memset(ptr, 0, n);

#elif defined(BOTAN_USE_VOLATILE_MEMSET_FOR_ZERO) && (BOTAN_USE_VOLATILE_MEMSET_FOR_ZERO == 1)
   /*
   Call memset through a static volatile pointer, which the compiler
   should not elide. This construct should be safe in conforming
   compilers, but who knows. I did confirm that on x86-64 GCC 6.1 and
   Clang 3.8 both create code that saves the memset address in the
   data segment and unconditionally loads and jumps to that address.
   */
   static void* (*const volatile memset_ptr)(void*, int, size_t) = std::memset;
   (memset_ptr)(ptr, 0, n);
#else

   volatile uint8_t* p = reinterpret_cast<volatile uint8_t*>(ptr);

   for(size_t i = 0; i != n; ++i)
      p[i] = 0;
#endif
   }

uint32_t OS::get_process_id()
   {
#if defined(BOTAN_TARGET_OS_HAS_POSIX1)
   return ::getpid();
#elif defined(BOTAN_TARGET_OS_HAS_WIN32)
   return ::GetCurrentProcessId();
#elif defined(BOTAN_TARGET_OS_IS_INCLUDEOS) || defined(BOTAN_TARGET_OS_IS_LLVM) || defined(BOTAN_TARGET_OS_IS_NONE)
   return 0; // truly no meaningful value
#else
   #error "Missing get_process_id"
#endif
   }

unsigned long OS::get_auxval(unsigned long id)
   {
#if defined(BOTAN_TARGET_OS_HAS_GETAUXVAL)
   return ::getauxval(id);
#elif defined(BOTAN_TARGET_OS_IS_ANDROID) && defined(BOTAN_TARGET_ARCH_IS_ARM32)

   if(id == 0)
      return 0;

   char **p = environ;

   while(*p++ != nullptr)
      ;

   Elf32_auxv_t *e = reinterpret_cast<Elf32_auxv_t*>(p);

   while(e != nullptr)
      {
      if(e->a_type == id)
         return e->a_un.a_val;
      e++;
      }

   return 0;
#elif defined(BOTAN_TARGET_OS_HAS_ELF_AUX_INFO)
   unsigned long auxinfo = 0;
   ::elf_aux_info(id, &auxinfo, sizeof(auxinfo));
   return auxinfo;
#else
   BOTAN_UNUSED(id);
   return 0;
#endif
   }

bool OS::running_in_privileged_state()
   {
#if defined(AT_SECURE)
   return OS::get_auxval(AT_SECURE) != 0;
#elif defined(BOTAN_TARGET_OS_HAS_POSIX1)
   return (::getuid() != ::geteuid()) || (::getgid() != ::getegid());
#else
   return false;
#endif
   }

uint64_t OS::get_cpu_cycle_counter()
   {
   uint64_t rtc = 0;

#if defined(BOTAN_TARGET_OS_HAS_WIN32)
   LARGE_INTEGER tv;
   ::QueryPerformanceCounter(&tv);
   rtc = tv.QuadPart;

#elif defined(BOTAN_USE_GCC_INLINE_ASM)

#if defined(BOTAN_TARGET_CPU_IS_X86_FAMILY)

   if(CPUID::has_rdtsc())
      {
      uint32_t rtc_low = 0, rtc_high = 0;
      asm volatile("rdtsc" : "=d" (rtc_high), "=a" (rtc_low));
      rtc = (static_cast<uint64_t>(rtc_high) << 32) | rtc_low;
      }

#elif defined(BOTAN_TARGET_ARCH_IS_PPC64)

   for(;;)
      {
      uint32_t rtc_low = 0, rtc_high = 0, rtc_high2 = 0;
      asm volatile("mftbu %0" : "=r" (rtc_high));
      asm volatile("mftb %0" : "=r" (rtc_low));
      asm volatile("mftbu %0" : "=r" (rtc_high2));

      if(rtc_high == rtc_high2)
         {
         rtc = (static_cast<uint64_t>(rtc_high) << 32) | rtc_low;
         break;
         }
      }

#elif defined(BOTAN_TARGET_ARCH_IS_ALPHA)
   asm volatile("rpcc %0" : "=r" (rtc));

   // OpenBSD does not trap access to the %tick register
#elif defined(BOTAN_TARGET_ARCH_IS_SPARC64) && !defined(BOTAN_TARGET_OS_IS_OPENBSD)
   asm volatile("rd %%tick, %0" : "=r" (rtc));

#elif defined(BOTAN_TARGET_ARCH_IS_IA64)
   asm volatile("mov %0=ar.itc" : "=r" (rtc));

#elif defined(BOTAN_TARGET_ARCH_IS_S390X)
   asm volatile("stck 0(%0)" : : "a" (&rtc) : "memory", "cc");

#elif defined(BOTAN_TARGET_ARCH_IS_HPPA)
   asm volatile("mfctl 16,%0" : "=r" (rtc)); // 64-bit only?

#else
   //#warning "OS::get_cpu_cycle_counter not implemented"
#endif

#endif

   return rtc;
   }

size_t OS::get_cpu_total()
   {
#if defined(BOTAN_TARGET_OS_HAS_POSIX1) && defined(_SC_NPROCESSORS_CONF)
   const long res = ::sysconf(_SC_NPROCESSORS_CONF);
   if(res > 0)
      return static_cast<size_t>(res);
#endif

#if defined(BOTAN_TARGET_OS_HAS_THREADS)
   return static_cast<size_t>(std::thread::hardware_concurrency());
#else
   return 1;
#endif
   }

size_t OS::get_cpu_available()
   {
#if defined(BOTAN_TARGET_OS_HAS_POSIX1) && defined(_SC_NPROCESSORS_ONLN)
   const long res = ::sysconf(_SC_NPROCESSORS_ONLN);
   if(res > 0)
      return static_cast<size_t>(res);
#endif

   return OS::get_cpu_total();
   }

uint64_t OS::get_high_resolution_clock()
   {
   if(uint64_t cpu_clock = OS::get_cpu_cycle_counter())
      return cpu_clock;

#if defined(BOTAN_TARGET_OS_IS_EMSCRIPTEN)
   return emscripten_get_now();
#endif

   /*
   If we got here either we either don't have an asm instruction
   above, or (for x86) RDTSC is not available at runtime. Try some
   clock_gettimes and return the first one that works, or otherwise
   fall back to std::chrono.
   */

#if defined(BOTAN_TARGET_OS_HAS_CLOCK_GETTIME)

   // The ordering here is somewhat arbitrary...
   const clockid_t clock_types[] = {
#if defined(CLOCK_MONOTONIC_HR)
      CLOCK_MONOTONIC_HR,
#endif
#if defined(CLOCK_MONOTONIC_RAW)
      CLOCK_MONOTONIC_RAW,
#endif
#if defined(CLOCK_MONOTONIC)
      CLOCK_MONOTONIC,
#endif
#if defined(CLOCK_PROCESS_CPUTIME_ID)
      CLOCK_PROCESS_CPUTIME_ID,
#endif
#if defined(CLOCK_THREAD_CPUTIME_ID)
      CLOCK_THREAD_CPUTIME_ID,
#endif
   };

   for(clockid_t clock : clock_types)
      {
      struct timespec ts;
      if(::clock_gettime(clock, &ts) == 0)
         {
         return (static_cast<uint64_t>(ts.tv_sec) * 1000000000) + static_cast<uint64_t>(ts.tv_nsec);
         }
      }
#endif

   // Plain C++11 fallback
   auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
   return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
   }

uint64_t OS::get_system_timestamp_ns()
   {
#if defined(BOTAN_TARGET_OS_HAS_CLOCK_GETTIME)
   struct timespec ts;
   if(::clock_gettime(CLOCK_REALTIME, &ts) == 0)
      {
      return (static_cast<uint64_t>(ts.tv_sec) * 1000000000) + static_cast<uint64_t>(ts.tv_nsec);
      }
#endif

   auto now = std::chrono::system_clock::now().time_since_epoch();
   return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
   }

size_t OS::system_page_size()
   {
   const size_t default_page_size = 4096;

#if defined(BOTAN_TARGET_OS_HAS_POSIX1)
   long p = ::sysconf(_SC_PAGESIZE);
   if(p > 1)
      return static_cast<size_t>(p);
   else
      return default_page_size;
#elif defined(BOTAN_TARGET_OS_HAS_VIRTUAL_LOCK)
   BOTAN_UNUSED(default_page_size);
   SYSTEM_INFO sys_info;
   ::GetSystemInfo(&sys_info);
   return sys_info.dwPageSize;
#else
   return default_page_size;
#endif
   }

size_t OS::get_memory_locking_limit()
   {
#if defined(BOTAN_TARGET_OS_HAS_POSIX1) && defined(BOTAN_TARGET_OS_HAS_POSIX_MLOCK) && defined(RLIMIT_MEMLOCK)
   /*
   * If RLIMIT_MEMLOCK is not defined, likely the OS does not support
   * unprivileged mlock calls.
   *
   * Linux defaults to only 64 KiB of mlockable memory per process
   * (too small) but BSDs offer a small fraction of total RAM (more
   * than we need). Bound the total mlock size to 512 KiB which is
   * enough to run the entire test suite without spilling to non-mlock
   * memory (and thus presumably also enough for many useful
   * programs), but small enough that we should not cause problems
   * even if many processes are mlocking on the same machine.
   */
   const size_t user_req = read_env_variable_sz("BOTAN_MLOCK_POOL_SIZE", BOTAN_MLOCK_ALLOCATOR_MAX_LOCKED_KB);

   const size_t mlock_requested = std::min<size_t>(user_req, BOTAN_MLOCK_ALLOCATOR_MAX_LOCKED_KB);

   if(mlock_requested > 0)
      {
      struct ::rlimit limits;

      ::getrlimit(RLIMIT_MEMLOCK, &limits);

      if(limits.rlim_cur < limits.rlim_max)
         {
         limits.rlim_cur = limits.rlim_max;
         ::setrlimit(RLIMIT_MEMLOCK, &limits);
         ::getrlimit(RLIMIT_MEMLOCK, &limits);
         }

      return std::min<size_t>(limits.rlim_cur, mlock_requested * 1024);
      }

#elif defined(BOTAN_TARGET_OS_HAS_VIRTUAL_LOCK)
   SIZE_T working_min = 0, working_max = 0;
   if(!::GetProcessWorkingSetSize(::GetCurrentProcess(), &working_min, &working_max))
      {
      return 0;
      }

   // According to Microsoft MSDN:
   // The maximum number of pages that a process can lock is equal to the number of pages in its minimum working set minus a small overhead
   // In the book "Windows Internals Part 2": the maximum lockable pages are minimum working set size - 8 pages
   // But the information in the book seems to be inaccurate/outdated
   // I've tested this on Windows 8.1 x64, Windows 10 x64 and Windows 7 x86
   // On all three OS the value is 11 instead of 8
   const size_t overhead = OS::system_page_size() * 11;
   if(working_min > overhead)
      {
      const size_t lockable_bytes = working_min - overhead;
      return std::min<size_t>(lockable_bytes, BOTAN_MLOCK_ALLOCATOR_MAX_LOCKED_KB * 1024);
      }
#endif

   // Not supported on this platform
   return 0;
   }

bool OS::read_env_variable(std::string& value_out, const std::string& name)
   {
   value_out = "";

   if(running_in_privileged_state())
      return false;

#if defined(BOTAN_TARGET_OS_HAS_WIN32) && defined(BOTAN_BUILD_COMPILER_IS_MSVC)
   char val[128] = { 0 };
   size_t req_size = 0;
   if(getenv_s(&req_size, val, sizeof(val), name.c_str()) == 0)
      {
      value_out = std::string(val, req_size);
      return true;
      }
#else
   if(const char* val = std::getenv(name.c_str()))
      {
      value_out = val;
      return true;
      }
#endif

   return false;
   }

size_t OS::read_env_variable_sz(const std::string& name, size_t def)
   {
   std::string value;
   if(read_env_variable(value, name))
      {
      try
         {
         const size_t val = std::stoul(value, nullptr);
         return val;
         }
      catch(std::exception&) { /* ignore it */ }
      }

   return def;
   }

#if defined(BOTAN_TARGET_OS_HAS_POSIX1) && defined(BOTAN_TARGET_OS_HAS_POSIX_MLOCK)

namespace {

int get_locked_fd()
   {
#if defined(BOTAN_TARGET_OS_IS_IOS) || defined(BOTAN_TARGET_OS_IS_MACOS)
   // On Darwin, tagging anonymous pages allows vmmap to track these.
   // Allowed from 240 to 255 for userland applications
   static constexpr int default_locked_fd = 255;
   int locked_fd = default_locked_fd;

   if(size_t locked_fdl = OS::read_env_variable_sz("BOTAN_LOCKED_FD", default_locked_fd))
      {
      if(locked_fdl < 240 || locked_fdl > 255)
         {
         locked_fdl = default_locked_fd;
         }
      locked_fd = static_cast<int>(locked_fdl);
      }
   return VM_MAKE_TAG(locked_fd);
#else
   return -1;
#endif
   }

}

#endif

std::vector<void*> OS::allocate_locked_pages(size_t count)
   {
   std::vector<void*> result;

#if (defined(BOTAN_TARGET_OS_HAS_POSIX1) && defined(BOTAN_TARGET_OS_HAS_POSIX_MLOCK)) || defined(BOTAN_TARGET_OS_HAS_VIRTUAL_LOCK)

   result.reserve(count);

   const size_t page_size = OS::system_page_size();

#if defined(BOTAN_TARGET_OS_HAS_POSIX1) && defined(BOTAN_TARGET_OS_HAS_POSIX_MLOCK)
   static const int locked_fd = get_locked_fd();
#endif

   for(size_t i = 0; i != count; ++i)
      {
      void* ptr = nullptr;

#if defined(BOTAN_TARGET_OS_HAS_POSIX1) && defined(BOTAN_TARGET_OS_HAS_POSIX_MLOCK)

#if !defined(MAP_ANONYMOUS)
   #define MAP_ANONYMOUS MAP_ANON
#endif

#if !defined(MAP_NOCORE)
#if defined(MAP_CONCEAL)
   #define MAP_NOCORE MAP_CONCEAL
#else
   #define MAP_NOCORE 0
#endif
#endif

#if !defined(PROT_MAX)
   #define PROT_MAX(p) 0
#endif
      const int pflags = PROT_READ | PROT_WRITE;

      ptr = ::mmap(nullptr, 3*page_size,
                   pflags | PROT_MAX(pflags),
                   MAP_ANONYMOUS | MAP_PRIVATE | MAP_NOCORE,
                   /*fd=*/locked_fd, /*offset=*/0);

      if(ptr == MAP_FAILED)
         {
         continue;
         }

      // lock the data page
      if(::mlock(static_cast<uint8_t*>(ptr) + page_size, page_size) != 0)
         {
         ::munmap(ptr, 3*page_size);
         continue;
         }

#if defined(MADV_DONTDUMP)
      // we ignore errors here, as DONTDUMP is just a bonus
      ::madvise(static_cast<uint8_t*>(ptr) + page_size, page_size, MADV_DONTDUMP);
#endif

#elif defined(BOTAN_TARGET_OS_HAS_VIRTUAL_LOCK)
      ptr = ::VirtualAlloc(nullptr, 3*page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

      if(ptr == nullptr)
         continue;

      if(::VirtualLock(static_cast<uint8_t*>(ptr) + page_size, page_size) == 0)
         {
         ::VirtualFree(ptr, 0, MEM_RELEASE);
         continue;
         }
#endif

      std::memset(ptr, 0, 3*page_size); // zero data page and both guard pages

      // Make guard page preceeding the data page
      page_prohibit_access(static_cast<uint8_t*>(ptr));
      // Make guard page following the data page
      page_prohibit_access(static_cast<uint8_t*>(ptr) + 2*page_size);

      result.push_back(static_cast<uint8_t*>(ptr) + page_size);
      }
#else
   BOTAN_UNUSED(count);
#endif

   return result;
   }

void OS::page_allow_access(void* page)
   {
#if defined(BOTAN_TARGET_OS_HAS_POSIX1)
   const size_t page_size = OS::system_page_size();
   ::mprotect(page, page_size, PROT_READ | PROT_WRITE);
#elif defined(BOTAN_TARGET_OS_HAS_VIRTUAL_LOCK)
   const size_t page_size = OS::system_page_size();
   DWORD old_perms = 0;
   ::VirtualProtect(page, page_size, PAGE_READWRITE, &old_perms);
   BOTAN_UNUSED(old_perms);
#else
   BOTAN_UNUSED(page);
#endif
   }

void OS::page_prohibit_access(void* page)
   {
#if defined(BOTAN_TARGET_OS_HAS_POSIX1)
   const size_t page_size = OS::system_page_size();
   ::mprotect(page, page_size, PROT_NONE);
#elif defined(BOTAN_TARGET_OS_HAS_VIRTUAL_LOCK)
   const size_t page_size = OS::system_page_size();
   DWORD old_perms = 0;
   ::VirtualProtect(page, page_size, PAGE_NOACCESS, &old_perms);
   BOTAN_UNUSED(old_perms);
#else
   BOTAN_UNUSED(page);
#endif
   }

void OS::free_locked_pages(const std::vector<void*>& pages)
   {
   const size_t page_size = OS::system_page_size();

   for(size_t i = 0; i != pages.size(); ++i)
      {
      void* ptr = pages[i];

      secure_scrub_memory(ptr, page_size);

      // ptr points to the data page, guard pages are before and after
      page_allow_access(static_cast<uint8_t*>(ptr) - page_size);
      page_allow_access(static_cast<uint8_t*>(ptr) + page_size);

#if defined(BOTAN_TARGET_OS_HAS_POSIX1) && defined(BOTAN_TARGET_OS_HAS_POSIX_MLOCK)
      ::munlock(ptr, page_size);
      ::munmap(static_cast<uint8_t*>(ptr) - page_size, 3*page_size);
#elif defined(BOTAN_TARGET_OS_HAS_VIRTUAL_LOCK)
      ::VirtualUnlock(ptr, page_size);
      ::VirtualFree(static_cast<uint8_t*>(ptr) - page_size, 0, MEM_RELEASE);
#endif
      }
   }

#if defined(BOTAN_TARGET_OS_HAS_POSIX1) && !defined(BOTAN_TARGET_OS_IS_EMSCRIPTEN)

namespace {

static ::sigjmp_buf g_sigill_jmp_buf;

void botan_sigill_handler(int)
   {
   siglongjmp(g_sigill_jmp_buf, /*non-zero return value*/1);
   }

}

#endif

int OS::run_cpu_instruction_probe(std::function<int ()> probe_fn)
   {
   volatile int probe_result = -3;

#if defined(BOTAN_TARGET_OS_HAS_POSIX1) && !defined(BOTAN_TARGET_OS_IS_EMSCRIPTEN)
   struct sigaction old_sigaction;
   struct sigaction sigaction;

   sigaction.sa_handler = botan_sigill_handler;
   sigemptyset(&sigaction.sa_mask);
   sigaction.sa_flags = 0;

   int rc = ::sigaction(SIGILL, &sigaction, &old_sigaction);

   if(rc != 0)
      throw System_Error("run_cpu_instruction_probe sigaction failed", errno);

   rc = sigsetjmp(g_sigill_jmp_buf, /*save sigs*/1);

   if(rc == 0)
      {
      // first call to sigsetjmp
      probe_result = probe_fn();
      }
   else if(rc == 1)
      {
      // non-local return from siglongjmp in signal handler: return error
      probe_result = -1;
      }

   // Restore old SIGILL handler, if any
   rc = ::sigaction(SIGILL, &old_sigaction, nullptr);
   if(rc != 0)
      throw System_Error("run_cpu_instruction_probe sigaction restore failed", errno);

#else
   BOTAN_UNUSED(probe_fn);
#endif

   return probe_result;
   }

std::unique_ptr<OS::Echo_Suppression> OS::suppress_echo_on_terminal()
   {
#if defined(BOTAN_TARGET_OS_HAS_POSIX1)
   class POSIX_Echo_Suppression : public Echo_Suppression
      {
      public:
         POSIX_Echo_Suppression()
            {
            m_stdin_fd = fileno(stdin);
            if(::tcgetattr(m_stdin_fd, &m_old_termios) != 0)
               throw System_Error("Getting terminal status failed", errno);

            struct termios noecho_flags = m_old_termios;
            noecho_flags.c_lflag &= ~ECHO;
            noecho_flags.c_lflag |= ECHONL;

            if(::tcsetattr(m_stdin_fd, TCSANOW, &noecho_flags) != 0)
               throw System_Error("Clearing terminal echo bit failed", errno);
            }

         void reenable_echo() override
            {
            if(m_stdin_fd > 0)
               {
               if(::tcsetattr(m_stdin_fd, TCSANOW, &m_old_termios) != 0)
                  throw System_Error("Restoring terminal echo bit failed", errno);
               m_stdin_fd = -1;
               }
            }

         ~POSIX_Echo_Suppression()
            {
            try
               {
               reenable_echo();
               }
            catch(...)
               {
               }
            }

      private:
         int m_stdin_fd;
         struct termios m_old_termios;
      };

   return std::unique_ptr<Echo_Suppression>(new POSIX_Echo_Suppression);

#elif defined(BOTAN_TARGET_OS_HAS_WIN32)

   class Win32_Echo_Suppression : public Echo_Suppression
      {
      public:
         Win32_Echo_Suppression()
            {
            m_input_handle = ::GetStdHandle(STD_INPUT_HANDLE);
            if(::GetConsoleMode(m_input_handle, &m_console_state) == 0)
               throw System_Error("Getting console mode failed", ::GetLastError());

            DWORD new_mode = ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT;
            if(::SetConsoleMode(m_input_handle, new_mode) == 0)
               throw System_Error("Setting console mode failed", ::GetLastError());
            }

         void reenable_echo() override
            {
            if(m_input_handle != INVALID_HANDLE_VALUE)
               {
               if(::SetConsoleMode(m_input_handle, m_console_state) == 0)
                  throw System_Error("Setting console mode failed", ::GetLastError());
               m_input_handle = INVALID_HANDLE_VALUE;
               }
            }

         ~Win32_Echo_Suppression()
            {
            try
               {
               reenable_echo();
               }
            catch(...)
               {
               }
            }

      private:
         HANDLE m_input_handle;
         DWORD m_console_state;
      };

   return std::unique_ptr<Echo_Suppression>(new Win32_Echo_Suppression);

#else

   // Not supported on this platform, return null
   return std::unique_ptr<Echo_Suppression>();
#endif
   }

}
/*
* Various string utils and parsing functions
* (C) 1999-2007,2013,2014,2015,2018 Jack Lloyd
* (C) 2015 Simon Warta (Kullo GmbH)
* (C) 2017 René Korthaus, Rohde & Schwarz Cybersecurity
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <limits>
#include <set>

#if defined(BOTAN_HAS_ASN1)
#endif

namespace Botan {

uint16_t to_uint16(const std::string& str)
   {
   const uint32_t x = to_u32bit(str);

   if(x >> 16)
      throw Invalid_Argument("Integer value exceeds 16 bit range");

   return static_cast<uint16_t>(x);
   }

uint32_t to_u32bit(const std::string& str)
   {
   // std::stoul is not strict enough. Ensure that str is digit only [0-9]*
   for(const char chr : str)
      {
      if(chr < '0' || chr > '9')
         {
         std::string chrAsString(1, chr);
         throw Invalid_Argument("String contains non-digit char: " + chrAsString);
         }
      }

   const unsigned long int x = std::stoul(str);

   if(sizeof(unsigned long int) > 4)
      {
      // x might be uint64
      if (x > std::numeric_limits<uint32_t>::max())
         {
         throw Invalid_Argument("Integer value of " + str + " exceeds 32 bit range");
         }
      }

   return static_cast<uint32_t>(x);
   }

/*
* Convert a string into a time duration
*/
uint32_t timespec_to_u32bit(const std::string& timespec)
   {
   if(timespec.empty())
      return 0;

   const char suffix = timespec[timespec.size()-1];
   std::string value = timespec.substr(0, timespec.size()-1);

   uint32_t scale = 1;

   if(Charset::is_digit(suffix))
      value += suffix;
   else if(suffix == 's')
      scale = 1;
   else if(suffix == 'm')
      scale = 60;
   else if(suffix == 'h')
      scale = 60 * 60;
   else if(suffix == 'd')
      scale = 24 * 60 * 60;
   else if(suffix == 'y')
      scale = 365 * 24 * 60 * 60;
   else
      throw Decoding_Error("timespec_to_u32bit: Bad input " + timespec);

   return scale * to_u32bit(value);
   }

/*
* Parse a SCAN-style algorithm name
*/
std::vector<std::string> parse_algorithm_name(const std::string& namex)
   {
   if(namex.find('(') == std::string::npos &&
      namex.find(')') == std::string::npos)
      return std::vector<std::string>(1, namex);

   std::string name = namex, substring;
   std::vector<std::string> elems;
   size_t level = 0;

   elems.push_back(name.substr(0, name.find('(')));
   name = name.substr(name.find('('));

   for(auto i = name.begin(); i != name.end(); ++i)
      {
      char c = *i;

      if(c == '(')
         ++level;
      if(c == ')')
         {
         if(level == 1 && i == name.end() - 1)
            {
            if(elems.size() == 1)
               elems.push_back(substring.substr(1));
            else
               elems.push_back(substring);
            return elems;
            }

         if(level == 0 || (level == 1 && i != name.end() - 1))
            throw Invalid_Algorithm_Name(namex);
         --level;
         }

      if(c == ',' && level == 1)
         {
         if(elems.size() == 1)
            elems.push_back(substring.substr(1));
         else
            elems.push_back(substring);
         substring.clear();
         }
      else
         substring += c;
      }

   if(!substring.empty())
      throw Invalid_Algorithm_Name(namex);

   return elems;
   }

std::vector<std::string> split_on(const std::string& str, char delim)
   {
   return split_on_pred(str, [delim](char c) { return c == delim; });
   }

std::vector<std::string> split_on_pred(const std::string& str,
                                       std::function<bool (char)> pred)
   {
   std::vector<std::string> elems;
   if(str.empty()) return elems;

   std::string substr;
   for(auto i = str.begin(); i != str.end(); ++i)
      {
      if(pred(*i))
         {
         if(!substr.empty())
            elems.push_back(substr);
         substr.clear();
         }
      else
         substr += *i;
      }

   if(substr.empty())
      throw Invalid_Argument("Unable to split string: " + str);
   elems.push_back(substr);

   return elems;
   }

/*
* Join a string
*/
std::string string_join(const std::vector<std::string>& strs, char delim)
   {
   std::string out = "";

   for(size_t i = 0; i != strs.size(); ++i)
      {
      if(i != 0)
         out += delim;
      out += strs[i];
      }

   return out;
   }

/*
* Parse an ASN.1 OID string
*/
std::vector<uint32_t> parse_asn1_oid(const std::string& oid)
   {
#if defined(BOTAN_HAS_ASN1)
   return OID(oid).get_components();
#else
   BOTAN_UNUSED(oid);
   throw Not_Implemented("ASN1 support not available");
#endif
   }

/*
* X.500 String Comparison
*/
bool x500_name_cmp(const std::string& name1, const std::string& name2)
   {
   auto p1 = name1.begin();
   auto p2 = name2.begin();

   while((p1 != name1.end()) && Charset::is_space(*p1)) ++p1;
   while((p2 != name2.end()) && Charset::is_space(*p2)) ++p2;

   while(p1 != name1.end() && p2 != name2.end())
      {
      if(Charset::is_space(*p1))
         {
         if(!Charset::is_space(*p2))
            return false;

         while((p1 != name1.end()) && Charset::is_space(*p1)) ++p1;
         while((p2 != name2.end()) && Charset::is_space(*p2)) ++p2;

         if(p1 == name1.end() && p2 == name2.end())
            return true;
         if(p1 == name1.end() || p2 == name2.end())
            return false;
         }

      if(!Charset::caseless_cmp(*p1, *p2))
         return false;
      ++p1;
      ++p2;
      }

   while((p1 != name1.end()) && Charset::is_space(*p1)) ++p1;
   while((p2 != name2.end()) && Charset::is_space(*p2)) ++p2;

   if((p1 != name1.end()) || (p2 != name2.end()))
      return false;
   return true;
   }

/*
* Convert a decimal-dotted string to binary IP
*/
uint32_t string_to_ipv4(const std::string& str)
   {
   std::vector<std::string> parts = split_on(str, '.');

   if(parts.size() != 4)
      throw Decoding_Error("Invalid IP string " + str);

   uint32_t ip = 0;

   for(auto part = parts.begin(); part != parts.end(); ++part)
      {
      uint32_t octet = to_u32bit(*part);

      if(octet > 255)
         throw Decoding_Error("Invalid IP string " + str);

      ip = (ip << 8) | (octet & 0xFF);
      }

   return ip;
   }

/*
* Convert an IP address to decimal-dotted string
*/
std::string ipv4_to_string(uint32_t ip)
   {
   std::string str;

   for(size_t i = 0; i != sizeof(ip); ++i)
      {
      if(i)
         str += ".";
      str += std::to_string(get_byte(i, ip));
      }

   return str;
   }

std::string erase_chars(const std::string& str, const std::set<char>& chars)
   {
   std::string out;

   for(auto c: str)
      if(chars.count(c) == 0)
         out += c;

   return out;
   }

std::string replace_chars(const std::string& str,
                          const std::set<char>& chars,
                          char to_char)
   {
   std::string out = str;

   for(size_t i = 0; i != out.size(); ++i)
      if(chars.count(out[i]))
         out[i] = to_char;

   return out;
   }

std::string replace_char(const std::string& str, char from_char, char to_char)
   {
   std::string out = str;

   for(size_t i = 0; i != out.size(); ++i)
      if(out[i] == from_char)
         out[i] = to_char;

   return out;
   }

std::string tolower_string(const std::string& in)
   {
   std::string s = in;
   for(size_t i = 0; i != s.size(); ++i)
      {
      const int cu = static_cast<unsigned char>(s[i]);
      if(std::isalpha(cu))
         s[i] = static_cast<char>(std::tolower(cu));
      }
   return s;
   }

bool host_wildcard_match(const std::string& issued_, const std::string& host_)
   {
   const std::string issued = tolower_string(issued_);
   const std::string host = tolower_string(host_);

   if(host.empty() || issued.empty())
      return false;

   /*
   If there are embedded nulls in your issued name
   Well I feel bad for you son
   */
   if(std::count(issued.begin(), issued.end(), char(0)) > 0)
      return false;

   // If more than one wildcard, then issued name is invalid
   const size_t stars = std::count(issued.begin(), issued.end(), '*');
   if(stars > 1)
      return false;

   // '*' is not a valid character in DNS names so should not appear on the host side
   if(std::count(host.begin(), host.end(), '*') != 0)
      return false;

   // Similarly a DNS name can't end in .
   if(host[host.size() - 1] == '.')
      return false;

   // And a host can't have an empty name component, so reject that
   if(host.find("..") != std::string::npos)
      return false;

   // Exact match: accept
   if(issued == host)
      {
      return true;
      }

   /*
   Otherwise it might be a wildcard

   If the issued size is strictly longer than the hostname size it
   couldn't possibly be a match, even if the issued value is a
   wildcard. The only exception is when the wildcard ends up empty
   (eg www.example.com matches www*.example.com)
   */
   if(issued.size() > host.size() + 1)
      {
      return false;
      }

   // If no * at all then not a wildcard, and so not a match
   if(stars != 1)
      {
      return false;
      }

   /*
   Now walk through the issued string, making sure every character
   matches. When we come to the (singular) '*', jump forward in the
   hostname by the corresponding amount. We know exactly how much
   space the wildcard takes because it must be exactly `len(host) -
   len(issued) + 1 chars`.

   We also verify that the '*' comes in the leftmost component, and
   doesn't skip over any '.' in the hostname.
   */
   size_t dots_seen = 0;
   size_t host_idx = 0;

   for(size_t i = 0; i != issued.size(); ++i)
      {
      dots_seen += (issued[i] == '.');

      if(issued[i] == '*')
         {
         // Fail: wildcard can only come in leftmost component
         if(dots_seen > 0)
            {
            return false;
            }

         /*
         Since there is only one * we know the tail of the issued and
         hostname must be an exact match. In this case advance host_idx
         to match.
         */
         const size_t advance = (host.size() - issued.size() + 1);

         if(host_idx + advance > host.size()) // shouldn't happen
            return false;

         // Can't be any intervening .s that we would have skipped
         if(std::count(host.begin() + host_idx,
                       host.begin() + host_idx + advance, '.') != 0)
            return false;

         host_idx += advance;
         }
      else
         {
         if(issued[i] != host[host_idx])
            {
            return false;
            }

         host_idx += 1;
         }
      }

   // Wildcard issued name must have at least 3 components
   if(dots_seen < 2)
      {
      return false;
      }

   return true;
   }

}
/*
* Simple config/test file reader
* (C) 2013,2014,2015 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

std::string clean_ws(const std::string& s)
   {
   const char* ws = " \t\n";
   auto start = s.find_first_not_of(ws);
   auto end = s.find_last_not_of(ws);

   if(start == std::string::npos)
      return "";

   if(end == std::string::npos)
      return s.substr(start, end);
   else
      return s.substr(start, start + end + 1);
   }

std::map<std::string, std::string> read_cfg(std::istream& is)
   {
   std::map<std::string, std::string> kv;
   size_t line = 0;

   while(is.good())
      {
      std::string s;

      std::getline(is, s);

      ++line;

      if(s.empty() || s[0] == '#')
         continue;

      s = clean_ws(s.substr(0, s.find('#')));

      if(s.empty())
         continue;

      auto eq = s.find("=");

      if(eq == std::string::npos || eq == 0 || eq == s.size() - 1)
         throw Decoding_Error("Bad read_cfg input '" + s + "' on line " + std::to_string(line));

      const std::string key = clean_ws(s.substr(0, eq));
      const std::string val = clean_ws(s.substr(eq + 1, std::string::npos));

      kv[key] = val;
      }

   return kv;
   }

}
/*
* (C) 2018 Ribose Inc
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

std::map<std::string, std::string> read_kv(const std::string& kv)
   {
   std::map<std::string, std::string> m;
   if(kv == "")
      return m;

   std::vector<std::string> parts;

   try
      {
      parts = split_on(kv, ',');
      }
   catch(std::exception&)
      {
      throw Invalid_Argument("Bad KV spec");
      }

   bool escaped = false;
   bool reading_key = true;
   std::string cur_key;
   std::string cur_val;

   for(char c : kv)
      {
      if(c == '\\' && !escaped)
         {
         escaped = true;
         }
      else if(c == ',' && !escaped)
         {
         if(cur_key.empty())
            throw Invalid_Argument("Bad KV spec empty key");

         if(m.find(cur_key) != m.end())
            throw Invalid_Argument("Bad KV spec duplicated key");
         m[cur_key] = cur_val;
         cur_key = "";
         cur_val = "";
         reading_key = true;
         }
      else if(c == '=' && !escaped)
         {
         if(reading_key == false)
            throw Invalid_Argument("Bad KV spec unexpected equals sign");
         reading_key = false;
         }
      else
         {
         if(reading_key)
            cur_key += c;
         else
            cur_val += c;

         if(escaped)
            escaped = false;
         }
      }

   if(!cur_key.empty())
      {
      if(reading_key == false)
         {
         if(m.find(cur_key) != m.end())
            throw Invalid_Argument("Bad KV spec duplicated key");
         m[cur_key] = cur_val;
         }
      else
         throw Invalid_Argument("Bad KV spec incomplete string");
      }

   return m;
   }

}
/*
* (C) 2018 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

void Timer::start()
   {
   stop();
   m_timer_start = OS::get_system_timestamp_ns();
   m_cpu_cycles_start = OS::get_cpu_cycle_counter();
   }

void Timer::stop()
   {
   if(m_timer_start)
      {
      if(m_cpu_cycles_start != 0)
         {
         const uint64_t cycles_taken = OS::get_cpu_cycle_counter() - m_cpu_cycles_start;
         if(cycles_taken > 0)
            {
            m_cpu_cycles_used += static_cast<size_t>(cycles_taken * m_clock_cycle_ratio);
            }
         }

      const uint64_t now = OS::get_system_timestamp_ns();

      if(now > m_timer_start)
         {
         const uint64_t dur = now - m_timer_start;

         m_time_used += dur;

         if(m_event_count == 0)
            {
            m_min_time = m_max_time = dur;
            }
         else
            {
            m_max_time = std::max(m_max_time, dur);
            m_min_time = std::min(m_min_time, dur);
            }
         }

      m_timer_start = 0;
      ++m_event_count;
      }
   }

bool Timer::operator<(const Timer& other) const
   {
   if(this->doing() != other.doing())
      return (this->doing() < other.doing());

   return (this->get_name() < other.get_name());
   }

std::string Timer::to_string() const
   {
   if(m_custom_msg.size() > 0)
      {
      return m_custom_msg;
      }
   else if(this->buf_size() == 0)
      {
      return result_string_ops();
      }
   else
      {
      return result_string_bps();
      }
   }

std::string Timer::result_string_bps() const
   {
   const size_t MiB = 1024 * 1024;

   const double MiB_total = static_cast<double>(events()) / MiB;
   const double MiB_per_sec = MiB_total / seconds();

   std::ostringstream oss;
   oss << get_name();

   if(!doing().empty())
      {
      oss << " " << doing();
      }

   if(buf_size() > 0)
      {
      oss << " buffer size " << buf_size() << " bytes:";
      }

   if(events() == 0)
      oss << " " << "N/A";
   else
      oss << " " << std::fixed << std::setprecision(3) << MiB_per_sec << " MiB/sec";

   if(cycles_consumed() != 0)
      {
      const double cycles_per_byte = static_cast<double>(cycles_consumed()) / events();
      oss << " " << std::fixed << std::setprecision(2) << cycles_per_byte << " cycles/byte";
      }

   oss << " (" << MiB_total << " MiB in " << milliseconds() << " ms)\n";

   return oss.str();
   }

std::string Timer::result_string_ops() const
   {
   std::ostringstream oss;

   oss << get_name() << " ";

   if(events() == 0)
      {
      oss << "no events\n";
      }
   else
      {
      oss << static_cast<uint64_t>(events_per_second())
          << ' ' << doing() << "/sec; "
          << std::setprecision(2) << std::fixed
          << ms_per_event() << " ms/op";

      if(cycles_consumed() != 0)
         {
         const double cycles_per_op = static_cast<double>(cycles_consumed()) / events();
         const int precision = (cycles_per_op < 10000) ? 2 : 0;
         oss << " " << std::fixed << std::setprecision(precision) << cycles_per_op << " cycles/op";
         }

      oss << " (" << events() << " " << (events() == 1 ? "op" : "ops")
          << " in " << milliseconds() << " ms)\n";
      }

   return oss.str();
   }

}
/*
* Version Information
* (C) 1999-2013,2015 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

/*
  These are intentionally compiled rather than inlined, so an
  application running against a shared library can test the true
  version they are running against.
*/

#define QUOTE(name) #name
#define STR(macro) QUOTE(macro)

const char* short_version_cstr()
   {
   return STR(BOTAN_VERSION_MAJOR) "."
          STR(BOTAN_VERSION_MINOR) "."
          STR(BOTAN_VERSION_PATCH)
#if defined(BOTAN_VERSION_SUFFIX)
          STR(BOTAN_VERSION_SUFFIX)
#endif
      ;
   }

const char* version_cstr()
   {

   /*
   It is intentional that this string is a compile-time constant;
   it makes it much easier to find in binaries.
   */

   return "Botan " STR(BOTAN_VERSION_MAJOR) "."
                   STR(BOTAN_VERSION_MINOR) "."
                   STR(BOTAN_VERSION_PATCH)
#if defined(BOTAN_VERSION_SUFFIX)
                   STR(BOTAN_VERSION_SUFFIX)
#endif
                   " ("
#if defined(BOTAN_UNSAFE_FUZZER_MODE)
                   "UNSAFE FUZZER MODE BUILD "
#endif
                   BOTAN_VERSION_RELEASE_TYPE
#if (BOTAN_VERSION_DATESTAMP != 0)
                   ", dated " STR(BOTAN_VERSION_DATESTAMP)
#endif
                   ", revision " BOTAN_VERSION_VC_REVISION
                   ", distribution " BOTAN_DISTRIBUTION_INFO ")";
   }

#undef STR
#undef QUOTE

/*
* Return the version as a string
*/
std::string version_string()
   {
   return std::string(version_cstr());
   }

std::string short_version_string()
   {
   return std::string(short_version_cstr());
   }

uint32_t version_datestamp() { return BOTAN_VERSION_DATESTAMP; }

/*
* Return parts of the version as integers
*/
uint32_t version_major() { return BOTAN_VERSION_MAJOR; }
uint32_t version_minor() { return BOTAN_VERSION_MINOR; }
uint32_t version_patch() { return BOTAN_VERSION_PATCH; }

std::string runtime_version_check(uint32_t major,
                                  uint32_t minor,
                                  uint32_t patch)
   {
   if(major != version_major() || minor != version_minor() || patch != version_patch())
      {
      std::ostringstream oss;
      oss << "Warning: linked version (" << short_version_string() << ")"
          << " does not match version built against "
          << "(" << major << '.' << minor << '.' << patch << ")\n";
      return oss.str();
      }

   return "";
   }

}
