#pragma once

/*
*	<*intrin.h>-like wrapper functions for pipeline 1, multiply-add, and multimedia
*	instructions. Functions taking an immediate value as an argument have macro versions
*	exposing this functionality. C++ convenience wrappers are provided as well.
*
*	These functions are declared 'always_inline'. You should not take their addresses or call
*	them using function pointers.
* 
*	The '__m128i' analogues are split up by integer type. There are plenty of no-op conversion
*	functions. This is done to make integer type mismatches easier to notice. Do not store this
*	type in memory and do not take the address of a variable using them. Treat every variable
*	as though it were declared with the 'register' keyword.
* 
*	This library is using a safe mode with well-defined semantics for every function. This has
*	considerable overhead but guarantees correct results even in debug mode (-O0). Define
*	'PS2INTRIN_UNSAFE' to disable safe mode and 'PS2INTRIN_SILENCE_UNSAFE' to silence the
*	warning. Unsafe mode assumes that all variables of 'm128*' type reside in registers at all
*	times and that surrounding code does not modify the LO/HI/SA registers. This is always false
*	in debug mode. Generated code should be checked manually for correctness.
*	
*	A note for developers looking to use these functions:
*	The EE Core Multimedia Instructions have the most support for these types: 'int16_t',
*	'int32_t' and 'uint32_t'. If you plan on using these functions, you should make sure your
*	data is in this format. The variants operating on 64-bit values are mostly just 32-bit values
*	plus 32 bits of sign extension, not real 64-bit integers.
*/

#ifndef _EE
#error "This header only deals with EE-Core intrinsics and should not be used for the IOP or other architectures."
#endif

#include <stdint.h>
#include <string.h>

#if defined(PS2INTRIN_UNSAFE) && !defined(PS2INTRIN_SILENCE_UNSAFE)
#pragma message "Using unsafe ps2intrin mode. Check correctness of generated code."
#endif

// This is mostly here to make working with IntelliSense easier.
#if defined(_MSC_VER)
#define FORCEINLINE __forceinline inline
#define CONST __declspec(noalias)
#define UNSEQUENCED __declspec(noalias)
#define PURE __declspec(noalias)
#define REPRODUCIBLE __declspec(noalias)
#define ALIGNAS16 __declspec(align(16))
#define DECLAREVECTOR_SAFE(base, name) typedef __declspec(align(16)) struct { uint64_t lo; uint64_t hi; } name
#define DECLAREVECTOR_UNSAFE(base, name) typedef __declspec(align(16)) struct { uint64_t v; } name
#define MODE_TI
#elif defined(__GNUC__)
// Always inline this function into the caller. Must be both
// '(always_inline)' and 'inline' to guarantee inlining, as per gcc warning.
#define FORCEINLINE __attribute__ ((always_inline)) inline
// This function only reads arguments and returns a value.
#define CONST __attribute__ ((const))
// This function only reads arguments, returns a value and reads or writes memory through pointers
// in function arguments.
#define UNSEQUENCED __attribute__ ((unsequenced))
// This function only reads arguments, global state and returns a value.
#define PURE __attribute__ ((pure))
// This function only reads arguments, global state, returns a value and reads or writes memory
// through pointers in function arguments.
#define REPRODUCIBLE __attribute__ ((reproducible))
// This type must be aligned to a 16-byte boundary
#define ALIGNAS16 __attribute__ ((aligned(16)))
// In unsafe mode any 16-byte vector actually uses a struct with a single uint64_t in it. This
// ensures 16 byte size and alignment, but means an additional (zero) register has to be used.
#define DECLAREVECTOR_UNSAFE(base, name) typedef struct { uint64_t v; } __attribute__ ((aligned(16))) name
// In safe mode any 16-byte vector uses a struct of 2 uint64_t to be able to name both the upper and
// lower 8 bytes.
#define DECLAREVECTOR_SAFE(base, name) typedef struct { uint64_t lo; uint64_t hi; } __attribute__ ((aligned(16))) name
// Integer size of 128 bits specifier
#define MODE_TI __attribute__((mode(TI)))
#else
#warning "Could not determine 'always_inline' analog for current compiler"
#define FORCEINLINE
#define CONST
#define UNSEQUENCED
#define PURE
#define REPRODUCIBLE
#define ALIGNAS16
#define DECLAREVECTOR(base, name) typedef struct { uint64_t lo; uint64_t hi; } name
#define MODE_TI
#endif

#ifdef PS2INTRIN_UNSAFE
#define DECLAREVECTOR DECLAREVECTOR_UNSAFE
#else
#define DECLAREVECTOR DECLAREVECTOR_SAFE
#endif


#ifdef __cplusplus
extern "C" {
#endif

	// Types

	/// @brief Signed 128-bit integer
	///
	/// Actually uses the low 64 bits of 2 registers. Functions using this type will always
	/// combine both parts into a single 128-bit value regardless of whether PS2INTRIN_UNSAFE
	/// is defined.
	typedef int ALIGNAS16 int128_t MODE_TI;

	/// @brief Unsigned 128-bit integer
	///
	/// Actually uses the low 64 bits of 2 registers. Functions using this type will always
	/// combine both parts into a single 128-bit value regardless of whether PS2INTRIN_UNSAFE
	/// is defined.
	typedef unsigned ALIGNAS16 uint128_t MODE_TI;

	/// @brief Type containing state of LO/HI registers. Used in safe mode to maintain expected
	/// values regardless of other instructions executed around desired intrinsics.
	/// Completely ignored if PS2INTRIN_UNSAFE is defined.
	typedef struct
	{
#ifndef PS2INTRIN_UNSAFE
		uint64_t lo[2];
		uint64_t hi[2];
#endif
	} lohi_state_t;

	/// @brief Type containing state of SA register. Used in safe mode to maintain expected
	/// value regardless of other instructions executed around desired intrinsics.
	/// Completely ignored if PS2INTRIN_UNSAFE is defined.
	typedef struct
	{
		uint64_t sa;
	} sa_state_t;

	/// @brief Result type of signed 32-bit multiplication
	typedef struct
	{
		int32_t lo;
		int32_t hi;
	} mul_i32_result_t;

	/// @brief Result type of unsigned 32-bit multiplication
	typedef struct
	{
		uint32_t lo;
		uint32_t hi;
	} mul_u32_result_t;

	/// @brief Result type of signed 32-bit division with remainder
	typedef struct
	{
		int32_t quotient;
		int32_t remainder;
	} divrem_i32_result_t;

	/// @brief Result type of unsigned 32-bit division with remainder
	typedef struct
	{
		uint32_t quotient;
		uint32_t remainder;
	} divrem_u32_result_t;

	/// @brief A 128-bit value containing 16 signed 8-bit values.
	/// Loads and stores of this type must be aligned 16 to bytes. Otherwise a different memory
	/// address is fetched/written.
	DECLAREVECTOR(int8_t, m128i8);

	/// @brief A 128-bit value containing 16 unsigned 8-bit values.
	/// Loads and stores of this type must be aligned 16 to bytes. Otherwise a different memory
	/// address is fetched/written.
	DECLAREVECTOR(uint8_t, m128u8);

	/// @brief A 128-bit value containing 8 signed 16-bit values.
	/// Loads and stores of this type must be aligned 16 to bytes. Otherwise a different memory
	/// address is fetched/written.
	DECLAREVECTOR(int16_t, m128i16);

	/// @brief A 128-bit value containing 8 unsigned 16-bit values.
	/// Loads and stores of this type must be aligned 16 to bytes. Otherwise a different memory
	/// address is fetched/written.
	DECLAREVECTOR(uint16_t, m128u16);

	/// @brief A 128-bit value containing 4 signed 32-bit values.
	/// Loads and stores of this type must be aligned 16 to bytes. Otherwise a different memory
	/// address is fetched/written.
	DECLAREVECTOR(int32_t, m128i32);

	/// @brief A 128-bit value containing 4 unsigned 32-bit values.
	/// Loads and stores of this type must be aligned 16 to bytes. Otherwise a different memory
	/// address is fetched/written.
	DECLAREVECTOR(uint32_t, m128u32);

	/// @brief A 128-bit value containing 2 signed 64-bit values.
	/// Loads and stores of this type must be aligned 16 to bytes. Otherwise a different memory
	/// address is fetched/written.
	/// 
	/// Note: Some functions operate on these values. However, often the EE Core hardware
	/// actually operates on 32-bit values with the upper 32 bits being sign-extension. This is
	/// also noted on the specific functions this applies to.
	DECLAREVECTOR(int64_t, m128i64);

	/// @brief A 128-bit value containing 2 unsigned 64-bit values.
	/// Loads and stores of this type must be aligned 16 to bytes. Otherwise a different memory
	/// address is fetched/written.
	/// 
	/// Note: Some functions operate on these values. However, often the EE Core hardware
	/// actually operates on 32-bit values with the upper 32 bits being sign-extension. This is
	/// also noted on the specific functions this applies to.
	DECLAREVECTOR(uint64_t, m128u64);

	/// @brief A 128-bit value containing 1 signed 128-bit value.
	/// Loads and stores of this type must be aligned 16 to bytes. Otherwise a different memory
	/// address is fetched/written.
	/// 
	/// Note that 'int128_t' can be assigned using a 'qword_t' from <tamtypes.h>. Use this if you
	/// already have a 'qword_t' you want to use, plus a cast.
	DECLAREVECTOR(int128_t, m128i128);

	/// @brief A 128-bit value containing 1 unsigned 128-bit value.
	/// Loads and stores of this type must be aligned 16 to bytes. Otherwise a different memory
	/// address is fetched/written.
	/// 
	/// Note that 'uint128_t' can be assigned using a 'qword_t' from <tamtypes.h>. Use this if
	/// you already have a 'qword_t' you want to use, plus a cast.
	DECLAREVECTOR(uint128_t, m128u128);


	// General Purpose

	/// @brief BREAK : BREAKpoint
	/// 
	/// Issue a breakpoint exception.
	/// 
	/// Has a 20-bit field for custom code, but can't be used here without compiler magic.
	/// (It would need to be encoded in the instruction itself, not an argument).
	FORCEINLINE void breakpoint()
	{
		// implicitly volatile
		asm("break");
	}

	/// @brief PREF : PREFetch
	/// 
	/// Prefetch data from memory to the cache.
	/// 
	/// Can set specific hint value. Only 0 is supported as this value, other values are
	/// reserved. Possible values: [0, 32). Prefer using the function 'prefetch' instead.
	/// 
	/// Does nothing on uncached memory locations.
	/// 
	/// Not allowed on memory locations not present in the TLB. Not recently used pages may not
	/// be present in the TLB, reducing effectiveness. Prefetch may not happen when the memory
	/// bus is used.
	/// @param address The address to prefetch. Must be a variable convertible to 'const void*'.
	/// @param hint The hint value to use. Must be an integer literal.
#define PREF(address, hint)																			\
	asm(												/* implicitly volatile	*/					\
		"pref	%c[Hint],%a[Address]"																\
		:												/* output operands	*/						\
		: [Hint] "n" (hint), [Address] "ZD" (address)	/* input operands	Per GCC documentation,	\
														ZD refers to an address suitable for the	\
														'prefetch' instruction	*/					\
	)

	/// @brief PREF : PREFetch
	/// 
	/// Prefetch data from memory to the cache.
	/// 
	/// Uses hint value 0.
	/// 
	/// Does nothing on uncached memory locations.
	/// 
	/// Not allowed on memory locations not present in the TLB. Not recently used pages may not
	/// be present in the TLB, reducing effectiveness. Prefetch may not happen when the memory
	/// bus is used.
	/// @param address The address to prefetch
	FORCEINLINE void prefetch(const void* address)
	{
		PREF(address, 0);
	}


	// LO/HI registers

	/// @brief Construct a 'lohi_state_t' with the current values of the LO/HI registers.
	/// 
	/// Optional, provided as a way of accessing those registers without first setting values.
	/// Obtained values are unspecified. They depend on preceding code and can change depending
	/// on compiler optimization.
	/// 
	/// This function reads global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	FORCEINLINE REPRODUCIBLE void lohi_state_construct(lohi_state_t* state)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;
#else
		asm volatile(	/* must be volatile so gcc cannot assume the values are always the same	*/
			"pmflo	%[Lo0]\n\t"
			"pmfhi	%[Hi0]\n\t"
			"pcpyud	%[Lo1],%[Lo0],%[Lo0]\n\t"
			"pcpyud	%[Hi1],%[Hi0],%[Hi0]"
			: [Lo0] "=r" (state->lo[0]),									/* output operands	*/
			  [Lo1] "=r" (state->lo[1]),
			  [Hi0] "=r" (state->hi[0]),
			  [Hi1] "=r" (state->hi[1])
		);
#endif
	}

	/// @brief Destroy a 'lohi_state_t' by writing back the contained values to the LO/HI
	/// registers.
	/// 
	/// Optional, provided as a way of accessing those registers while affecting global state.
	/// Effect of written vales is unspecified and depends on following code. Can change depending
	/// on compiler optimizations.
	/// 
	/// This function writes global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	FORCEINLINE void lohi_state_destruct(lohi_state_t* state)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;
#else
		asm volatile(			/* must be volatile so gcc cannot discard an unused store	*/
			"pcpyld	%[Lo0],%[Lo1],%[Lo0]\n\t"
			"pcpyld	%[Hi0],%[Hi1],%[Hi0]\n\t"
			"pmtlo	%[Lo0]\n\t"
			"pmthi	%[Hi0]"
			:															/* output operands	*/
			: [Lo0] "r" (state->lo[0]),									/* input opoerands	*/
			  [Lo1] "r" (state->lo[1]),
			  [Hi0] "r" (state->hi[0]),
			  [Hi1] "r" (state->hi[1])
			: "lo", "hi"														/* clobbers	*/
		);
#endif
	}

	/// @brief MTLO : Move To LO register
	/// 
	/// Set LO0 to 0.
	/// 
	/// This function writes to global state (LO0).
	/// @param state Additional state used in safe mode. May not be NULL.
	FORCEINLINE void setzero_lo0(lohi_state_t* state)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// implicitly volatile
		asm(
			"mtlo	$0"
			:										/* output operands	*/
			:										/* input operands	*/
			: "lo"									/* clobbers lo register	*/
		);
#else
		state->lo[0] = 0;
#endif
	}

	/// @brief MTHI : Move To HI register
	/// 
	/// Set HI0 to 0.
	/// 
	/// This function writes to global state (HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	FORCEINLINE void setzero_hi0(lohi_state_t* state)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// implicitly volatile
		asm(
			"mthi	$0"
			:										/* output operands	*/
			:										/* input operands	*/
			: "hi"									/* clobbers hi register	*/
		);
#else
		state->hi[0] = 0;
#endif
	}

	/// @brief MTLO1 : Move To LO1 register
	/// 
	/// Set LO1 to 0.
	/// 
	/// This function writes to global state (LO1).
	/// @param state Additional state used in safe mode. May not be NULL.
	FORCEINLINE void setzero_lo1(lohi_state_t* state)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// implicitly volatile
		asm(
			"mtlo1	$0"
			:										/* output operands	*/
			:										/* input operands	*/
			: "lo"									/* clobbers lo register	*/
		);
#else
		state->lo[1] = 0;
#endif
	}

	/// @brief MTHI1 : Move To HI1 register
	/// 
	/// Set HI1 to 0.
	/// 
	/// This function writes to global state (HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	FORCEINLINE void setzero_hi1(lohi_state_t* state)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// implicitly volatile
		asm(
			"mthi1	$0"
			:										/* output operands	*/
			:										/* input operands	*/
			: "hi"									/* clobbers hi register	*/
		);
#else
		state->hi[1] = 0;
#endif
	}

	/// @brief MTLO : Move To LO register
	/// MTHI : Move To HI register
	/// 
	/// Set LO0 and HI0 to 0.
	/// 
	/// This function writes to global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	FORCEINLINE void setzero_lohi0(lohi_state_t* state)
	{
		setzero_lo0(state);
		setzero_hi0(state);
	}

	/// @brief MTLO1 : Move To LO1 register
	/// MTHI1 : Move To HI1 register
	/// 
	/// Set LO1 and HI1 to 0.
	/// 
	/// This function writes to global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	FORCEINLINE void setzero_lohi1(lohi_state_t* state)
	{
		setzero_lo1(state);
		setzero_hi1(state);
	}


	/// @brief MFLO : Move From LO register
	/// 
	/// Get the value of the LO0 register, treated as a sign-extended 32-bit value.
	/// 
	/// This produces a stall if the value is being written to by another instruction and it has
	/// not finished yet. See functions implementing 'MULT', 'MADD' and 'DIV'.
	/// 
	/// This function reads global state (LO0)
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return The value stored in LO0 (LO bits 0..63)
	FORCEINLINE PURE int32_t load_lo0_32(lohi_state_t* state)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		int32_t result = 0;

		// I'd like to tell gcc that this asm reads from the LO register but using a variable bound
		// to the "l" constraint sets LO first and then inserts this asm after. As it stands, this
		// needs to be declared volatile to ensure gcc doesn't think the result is always the same.
		asm volatile(
			"mflo	%[Result]"
			: [Result] "=r" (result)		// output operands
		);

		return result;
#else
		return (int32_t)((int64_t)state->lo[0]);	// not equivalent, this will fix the high 32 bits
#endif
	}

	/// @brief MFLO : Move From LO register
	/// 
	/// Get the value of the LO0 register, treated as a 64-bit value.
	/// 
	/// This produces a stall if the value is being written to by another instruction and it has
	/// not finished yet. See functions implementing 'MULT', 'MADD' and 'DIV'.
	/// 
	/// This function reads global state (LO0)
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return The value stored in LO0 (LO bits 0..63)
	FORCEINLINE PURE int64_t load_lo0_64(lohi_state_t* state)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		int64_t result = 0;

		// I'd like to tell gcc that this asm reads from the LO register but using a variable bound
		// to the "l" constraint sets LO first and then inserts this asm after. As it stands, this
		// needs to be declared volatile to ensure gcc doesn't think the result is always the same.
		asm volatile(
			"mflo	%[Result]"
			: [Result] "=r" (result)		// output operands
		);

		return result;
#else
		return (int64_t)state->lo[0];
#endif
	}

	/// @brief MFHI : Move From HI register
	/// 
	/// Get the value of the HI0 register, treated as a sign-extended 32-bit value.
	/// 
	/// This produces a stall if the value is being written to by another instruction and it has
	/// not finished yet. See functions implementing 'MULT', 'MADD' and 'DIV'.
	/// 
	/// This function reads global state (HI0)
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return The value stored in HI0 (HI bits 0..63)
	FORCEINLINE PURE int32_t load_hi0_32(lohi_state_t* state)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		int32_t result = 0;

		// I'd like to tell gcc that this asm reads from the HI register but using a variable bound
		// to the "h" constraint sets HI first and then inserts this asm after. As it stands, this
		// needs to be declared volatile to ensure gcc doesn't think the result is always the same.
		asm volatile(
			"mfhi	%[Result]"
			: [Result] "=r" (result)		// output operands
		);

		return result;
#else
		return (int32_t)((int64_t)state->hi[0]);	// not equivalent, this will fix the high 32 bits
#endif
	}

	/// @brief MFHI : Move From HI register
	/// 
	/// Get the value of the HI0 register, treated as a 64-bit value.
	/// 
	/// This produces a stall if the value is being written to by another instruction and it has
	/// not finished yet. See functions implementing 'MULT', 'MADD' and 'DIV'.
	/// 
	/// This function reads global state (HI0)
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return The value stored in HI0 (HI bits 0..63)
	FORCEINLINE PURE int64_t load_hi0_64(lohi_state_t* state)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		int64_t result = 0;

		// I'd like to tell gcc that this asm reads from the HI register but using a variable bound
		// to the "h" constraint sets HI first and then inserts this asm after. As it stands, this
		// needs to be declared volatile to ensure gcc doesn't think the result is always the same.
		asm volatile(
			"mfhi	%[Result]"
			: [Result] "=r" (result)		// output operands
		);

		return result;
#else
		return (int64_t)state->hi[0];
#endif
	}

	/// @brief MFLO1 : Move From LO1 register
	/// 
	/// Get the value of the LO1 register, treated as a sign-extended 32-bit value.
	/// 
	/// This produces a stall if the value is being written to by another instruction and it has
	/// not finished yet. See functions implementing 'MULT', 'MADD' and 'DIV'.
	/// 
	/// This function reads global state (LO1)
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return The value stored in LO1 (LO bits 64..127)
	FORCEINLINE PURE int32_t load_lo1_32(lohi_state_t* state)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		int32_t result = 0;

		// I'd like to tell gcc that this asm reads from the LO register but using a variable bound
		// to the "l" constraint sets LO first and then inserts this asm after. As it stands, this
		// needs to be declared volatile to ensure gcc doesn't think the result is always the same.
		asm volatile(
			"mflo1	%[Result]"
			: [Result] "=r" (result)		// output operands
		);

		return result;
#else
		return (int32_t)((int64_t)state->lo[1]);	// not equivalent, this will fix the high 32 bits
#endif
	}

	/// @brief MFLO1 : Move From LO1 register
	/// 
	/// Get the value of the LO1 register, treated as a 64-bit value.
	/// 
	/// This produces a stall if the value is being written to by another instruction and it has
	/// not finished yet. See functions implementing 'MULT', 'MADD' and 'DIV'.
	/// 
	/// This function reads global state (LO1)
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return The value stored in LO1 (LO bits 64..127)
	FORCEINLINE PURE int64_t load_lo1_64(lohi_state_t* state)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		int64_t result = 0;

		// I'd like to tell gcc that this asm reads from the LO register but using a variable bound
		// to the "l" constraint sets LO first and then inserts this asm after. As it stands, this
		// needs to be declared volatile to ensure gcc doesn't think the result is always the same.
		asm volatile(
			"mflo1	%[Result]"
			: [Result] "=r" (result)		// output operands
		);

		return result;
#else
		return (int64_t)state->lo[1];
#endif
	}

	/// @brief MFHI1 : Move From HI1 register
	/// 
	/// Get the value of the HI1 register, treated as a sign-extended 32-bit value.
	/// 
	/// This produces a stall if the value is being written to by another instruction and it has
	/// not finished yet. See functions implementing 'MULT', 'MADD' and 'DIV'.
	/// 
	/// This function reads global state (HI1)
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return The value stored in HI1 (HI bits 64..127)
	FORCEINLINE PURE int32_t load_hi1_32(lohi_state_t* state)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		int32_t result = 0;

		// I'd like to tell gcc that this asm reads from the HI register but using a variable bound
		// to the "h" constraint sets HI first and then inserts this asm after. As it stands, this
		// needs to be declared volatile to ensure gcc doesn't think the result is always the same.
		asm volatile(
			"mfhi1	%[Result]"
			: [Result] "=r" (result)		// output operands
		);

		return result;
#else
		return (int32_t)((int64_t)state->hi[1]);	// not equivalent, this will fix the high 32 bits
#endif
	}

	/// @brief MFHI1 : Move From HI1 register
	/// 
	/// Get the value of the HI1 register, treated as a 64-bit value.
	/// 
	/// This produces a stall if the value is being written to by another instruction and it has
	/// not finished yet. See functions implementing 'MULT', 'MADD' and 'DIV'.
	/// 
	/// This function reads global state (HI1)
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return The value stored in HI1 (HI bits 64..127)
	FORCEINLINE PURE int64_t load_hi1_64(lohi_state_t* state)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		int64_t result = 0;

		// I'd like to tell gcc that this asm reads from the HI register but using a variable bound
		// to the "h" constraint sets HI first and then inserts this asm after. As it stands, this
		// needs to be declared volatile to ensure gcc doesn't think the result is always the same.
		asm volatile(
			"mfhi1	%[Result]"
			: [Result] "=r" (result)		// output operands
		);

		return result;
#else
		return (int64_t)state->hi[1];
#endif
	}

	/// @brief MFLO : Move From LO register
	/// MFHI : Move From HI register
	/// 
	/// Concatenate the values of LO0 and HI0, each interpreted as a 32-bit integer, into a single
	/// 64-bit value.
	/// 
	/// This produces a stall if the value is being written to by another instruction and it has
	/// not finished yet. See functions implementing 'MULT', 'MADD' and 'DIV'.
	/// 
	/// This function reads global state (LO0/HI0)
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return Concatenated LO and HI register values
	FORCEINLINE PURE int64_t load_lohi0_32(lohi_state_t* state)
	{
		int64_t result = load_hi0_32(state);
		result <<= 32;
		result |= load_lo0_32(state);
		return result;
	}

	/// @brief MFLO1 : Move From LO1 register
	/// MFHI1 : Move From HI1 register
	/// 
	/// Concatenate the values of LO1 and HI1, each interpreted as a 32-bit integer, into a single
	/// 64-bit value.
	/// 
	/// This produces a stall if the value is being written to by another instruction and it has
	/// not finished yet. See functions implementing 'MULT', 'MADD' and 'DIV'.
	/// 
	/// This function reads global state (LO1/HI1)
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return Concatenated LO and HI register values
	FORCEINLINE PURE int64_t load_lohi1_32(lohi_state_t* state)
	{
		int64_t result = load_hi1_32(state);
		result <<= 32;
		result |= load_lo1_32(state);
		return result;
	}


	/// @brief MTLO : Move To LO register
	/// 
	/// Store a value to the LO0 register.
	/// 
	/// This function writes to global state (LO0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param value Value to store in LO0 (LO bits 0..63)
	FORCEINLINE void store_lo0(lohi_state_t* state, int64_t value)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// I'd like to tell gcc that this asm writes to the LO register but using a variable bound
		// to the "l" constraint will not be used, discarding the asm statement. As it stands, this
		// needs to be declared volatile to ensure gcc doesn't think the store is unobserved.
		asm volatile(					// would be implicitly volatile
			"mtlo	%[Value]"
			:							// output operands
			: [Value] "r" (value)		// input operands
			: "lo"						// clobbers
		);
#else
		state->lo[0] = value;
#endif
	}

	/// @brief MTHI : Move To HI register
	/// 
	/// Store a value to the HI0 register.
	/// 
	/// This function writes to global state (HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param value Value to store in HI0 (HI bits 0..63)
	FORCEINLINE void store_hi0(lohi_state_t* state, int64_t value)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// I'd like to tell gcc that this asm writes to the HI register but using a variable bound
		// to the "h" constraint will not be used, discarding the asm statement. As it stands, this
		// needs to be declared volatile to ensure gcc doesn't think the store is unobserved.
		asm volatile(					// would be implicitly volatile
			"mthi	%[Value]"
			:							// output operands
			: [Value] "r" (value)		// input operands
			: "hi"						// clobbers
		);
#else
		state->hi[0] = value;
#endif
	}

	/// @brief MTLO1 : Move To LO1 register
	/// 
	/// Store a value to the LO1 register.
	/// 
	/// This function writes to global state (LO1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param value Value to store in LO1 (LO bits 64..127)
	FORCEINLINE void store_lo1(lohi_state_t* state, int64_t value)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// I'd like to tell gcc that this asm writes to the LO register but using a variable bound
		// to the "l" constraint will not be used, discarding the asm statement. As it stands, this
		// needs to be declared volatile to ensure gcc doesn't think the store is unobserved.
		asm volatile(					// would be implicitly volatile
			"mtlo1	%[Value]"
			:							// output operands
			: [Value] "r" (value)		// input operands
			: "lo"						// clobbers
		);
#else
		state->lo[1] = value;
#endif
	}

	/// @brief MTHI1 : Move To HI1 register
	/// 
	/// Store a value to the HI1 register.
	/// 
	/// This function writes to global state (HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param value Value to store in HI1 (HI bits 64..127)
	FORCEINLINE void store_hi1(lohi_state_t* state, int64_t value)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// I'd like to tell gcc that this asm writes to the HI register but using a variable bound
		// to the "h" constraint will not be used, discarding the asm statement. As it stands, this
		// needs to be declared volatile to ensure gcc doesn't think the store is unobserved.
		asm volatile(					// would be implicitly volatile
			"mthi1	%[Value]"
			:							// output operands
			: [Value] "r" (value)		// input operands
			: "hi"						// clobbers
		);
#else
		state->hi[1] = value;
#endif
	}

	
	// Funnel Shift

	/// @brief Construct a 'sa_state_t' using the value currently stored in the SA register.
	/// 
	/// Optional, provided as means of accessing that register without first setting a value.
	/// Obtained value is unspecified and depends on preceding code.
	/// 
	/// This function reads global state (SA).
	/// @param state Additional state used in safe mode. May not be NULL.
	FORCEINLINE REPRODUCIBLE void sa_state_construct(sa_state_t* state)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;
#else
		asm volatile( /* must be volatile so gcc cannot assume the result is always the same	*/
			"mfsa	%[Value]"
			: [Value] "=r" (state->sa)										/* output operands	*/
		);
#endif
	}

	/// @brief Destroy a 'sa_state_t' by writing the value currently stored to the SA register.
	/// 
	/// Optional, provided as means of writing globally to that register. Effect of written
	/// value is unspecified and depends on following code.
	/// 
	/// This function writes to global state (SA).
	/// @param state Additional state used in safe mode. May not be NULL.
	FORCEINLINE void sa_state_destruct(sa_state_t* state)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;
#else
		asm volatile(		/* must be volatile so gcc cannot assume the store is unobserved	*/
			"mtsa	%[Value]"
			:																/* output operands	*/
			: [Value] "r" (state->sa)										/* input operands	*/
			: /*"sa"*/																/* clobbers	*/
		);
#endif
	}

	/// @brief MFSA : Move From Shift Amount register
	/// 
	/// Get the value of the shift amount register.
	/// 
	/// The value is only used to preserve the register across context switches. It is meaningless
	/// unless written back to the SA register using 'store_sa'. Do not assume the result has any
	/// particular value. Set the shift amount register to a meaningful value by using 'set_sa_8'
	/// or 'set_sa_16'.
	/// 
	/// This function reads global state (SA).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return The value stored in SA (SA bits 0..63)
	FORCEINLINE PURE uint64_t load_sa(sa_state_t* state)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		uint64_t result = 0;

		// I'd like to tell gcc that this asm reads from the SA register but I don't know of any
		// constraint that fixes a variable to that register, much less how to use it here (see
		// other load_lo/hi functions). As it stands, this needs to be declared volatile to ensure
		// gcc doesn't think the result is always the same.
		asm volatile(
			"mfsa	%[Result]"
			: [Result] "=r" (result)		// output operands
		);

		return result;
#else
		return state->sa;
#endif
	}

	/// @brief MTSA : Move To Shift Amount register
	/// 
	/// Store a value to the shift amount register.
	/// 
	/// The value is only used to preserve the register across context switches. Don't use a value
	/// unless read from the SA register using 'load_sa'. Do not assume the result has any
	/// particular value. Set the shift amount register to a meaningful value by using 'set_sa_8'
	/// or 'set_sa_16'.
	/// 
	/// Note that the 3 preceding instructions may not be any of the following:
	/// MFSA, MTSAB, MTSAH, QFSRV
	/// 
	/// This function writes to global state (SA).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param value Value to store in SA
	FORCEINLINE void store_sa(sa_state_t* state, uint64_t value)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// I'd like to tell gcc that this asm writes to the SA register but I don't know of any
		// constraint that fixes a variable to that register, much less how to use it here (see
		// 'load_sa'). As it stands, this needs to be declared volatile to ensure gcc doesn't think
		// the write is unobserved (and would be implicitly).
		asm volatile(
			"mtsa	%[Value]"
			:							// output operands
			: [Value] "r" (value)		// input operands
			: /*"sa"*/					// clobbers
		);
#else
		state->sa = value;
#endif
	}

	// MTSAB_BOTH
#ifdef PS2INTRIN_UNSAFE
	/// @brief MTSAB : Move byte count To Shift Amount register (Byte)
	/// 
	/// Set a byte shift count in the shift amount register.
	/// 
	/// The values of 'variable' and 'immediate' are XOR'ed together. The resulting value is the
	/// amount of bytes 'byte_shift_logical_right' will shift by.
	/// 
	/// Allowable values for 'variable' and 'immediate' are [0,15]. Only the lower 4 bits are used,
	/// others are ignored.
	/// 
	/// Use this macro if you require both the variable and constant values usable by the 'mtsab'
	/// instruction. Otherwise, prefer 'MTSAB_IMMEDIATE' if you only have a constant value and
	/// 'set_sa_8' if you only have a variable shift amount.
	/// 
	/// Note that the 3 preceding instructions may not be any of the following:
	/// MFSA, QFSRV
	/// 
	/// This macro writes to global state (SA).
	/// @param state Additional state used in safe mode. Must be an expression of type
	/// 'sa_state_t*'. May not be NULL.
	/// @param variable The variable byte-amount to set up the shift amount register for. Should be
	/// the name of a variable of type 'unsigned'.
	/// @param immediate The compile time constant byte-amount to set up the shift amount register
	/// for. Should be an integer literal.
	#define MTSAB_BOTH(state, variable, immediate)													\
	{																								\
		(void)(state);																				\
		asm volatile(									/*	volatile, same reason as 'store_sa'	*/	\
			"mtsab	%[Variable],%c[Immediate]"														\
			:															/*	output parameters	*/	\
			: [Variable] "r" (variable), [Immediate] "n" (immediate)	/*	input parameters	*/	\
			: /*"sa"*/													/*	clobbers	*/			\
		);																							\
	}
#else
	/// @brief MTSAB : Move byte count To Shift Amount register (Byte)
	/// 
	/// Set a byte shift count in the shift amount register.
	/// 
	/// The values of 'variable' and 'immediate' are XOR'ed together. The resulting value is the
	/// amount of bytes 'byte_shift_logical_right' will shift by.
	/// 
	/// Allowable values for 'variable' and 'immediate' are [0,15]. Only the lower 4 bits are used,
	/// others are ignored.
	/// 
	/// Use this macro if you require both the variable and constant values usable by the 'mtsab'
	/// instruction. Otherwise, prefer 'MTSAB_IMMEDIATE' if you only have a constant value and
	/// 'set_sa_8' if you only have a variable shift amount.
	/// 
	/// Note that the 3 preceding instructions may not be any of the following:
	/// MFSA, QFSRV
	/// 
	/// This macro writes to global state (SA).
	/// @param state Additional state used in safe mode. Must be an expression of type
	/// 'sa_state_t*'. May not be NULL.
	/// @param variable The variable byte-amount to set up the shift amount register for. Should be
	/// the name of a variable of type 'unsigned'.
	/// @param immediate The compile time constant byte-amount to set up the shift amount register
	/// for. Should be an integer literal.
	#define MTSAB_BOTH(state, variable, immediate)													\
	{																								\
		uint64_t result = 0;																		\
		uint64_t tmp = 0;																			\
		asm volatile(																				\
			"mfsa	%[Tmp]\n\t"																		\
			"nop\n\tnop\n\tnop\n\tmtsab	%[Variable],%c[Immediate]\n\t"	/*	timing nops	*/			\
			"mfsa	%[Result]\n\t"																	\
			"nop\n\tnop\n\tnop\n\tmtsa	%[Tmp]\n\t"						/*	timing nops	*/			\
			: [Result] "=r" (result), [Tmp] "=&r" (tmp)					/*	output parameters	*/	\
			: [Variable] "r" (variable), [Immediate] "n" (immediate)	/*	input parameters	*/	\
		);																							\
		(state)->sa = result;																		\
	}
#endif

	// MTSAB_IMMEDIATE
#ifdef PS2INTRIN_UNSAFE
	/// @brief MTSAB : Move byte count To Shift Amount register (Byte)
	/// 
	/// Set a byte shift count in the shift amount register.
	/// 
	/// The 'immediate' value is the amount of bytes 'byte_shift_logical_right' will shift by.
	/// 
	/// Allowable values for 'immediate' are [0,15]. Only the lower 4 bits are used, others are
	/// ignored.
	/// 
	/// Use this macro if your shift amount is known at compile time. Using this macro does not use
	/// a general purpose register. Otherwise, prefer 'MTSAB_BOTH' or 'set_sa_8'.
	/// 
	/// Note that the 3 preceding instructions may not be any of the following:
	/// MFSA, QFSRV
	/// 
	/// This macro writes to global state (SA).
	/// @param state Additional state used in safe mode. Must be an expression of type
	/// 'sa_state_t*'. May not be NULL.
	/// @param immediate The compile time constant byte-amount to set up the shift amount register
	/// for. Should be an integer literal.
	#define MTSAB_IMMEDIATE(state, immediate)														\
	{																								\
		(void)state;																				\
		asm volatile(									/*	volatile, same reason as 'store_sa'	*/	\
			"mtsab	$0,%c[Immediate]"																\
			:															/*	output parameters	*/	\
			: [Immediate] "n" (immediate)								/*	input parameters	*/	\
			: /*"sa"*/													/*	clobbers	*/			\
		);																							\
	}
#else
	/// @brief MTSAB : Move byte count To Shift Amount register (Byte)
	/// 
	/// Set a byte shift count in the shift amount register.
	/// 
	/// The 'immediate' value is the amount of bytes 'byte_shift_logical_right' will shift by.
	/// 
	/// Allowable values for 'immediate' are [0,15]. Only the lower 4 bits are used, others are
	/// ignored.
	/// 
	/// Use this macro if your shift amount is known at compile time. Using this macro does not use
	/// a general purpose register. Otherwise, prefer 'MTSAB_BOTH' or 'set_sa_8'.
	/// 
	/// Note that the 3 preceding instructions may not be any of the following:
	/// MFSA, QFSRV
	/// 
	/// This macro writes to global state (SA).
	/// @param state Additional state used in safe mode. Must be an expression of type
	/// 'sa_state_t*'. May not be NULL.
	/// @param immediate The compile time constant byte-amount to set up the shift amount register
	/// for. Should be an integer literal.
#define MTSAB_IMMEDIATE(state, immediate)															\
	{																								\
		uint64_t result = 0;																		\
		uint64_t tmp = 0;																			\
		asm volatile(																				\
			"mfsa	%[Tmp]\n\t"																		\
			"nop\n\tnop\n\tnop\n\tmtsab	$0,%c[Immediate]\n\t"			/*	timing nops	*/			\
			"mfsa	%[Result]\n\t"																	\
			"nop\n\tnop\n\tnop\n\tmtsa	%[Tmp]\n\t"						/*	timing nops	*/			\
			: [Result] "=r" (result), [Tmp] "=&r" (tmp)					/*	output parameters	*/	\
			: [Immediate] "n" (immediate)								/*	input parameters	*/	\
		);																							\
		(state)->sa = result;																		\
	}
#endif

	/// @brief MTSAB : Move byte count To Shift Amount register (Byte)
	/// 
	/// Set a byte shift count in the shift amount register.
	/// 
	/// Allowable values for 'byte_amount' are [0,15]. Only the lower 4 bits are used, others are
	/// ignored.
	/// 
	/// Use this function if your byte shift amount is calculated by the program. Otherwise, prefer
	/// 'MTSAB_BOTH' or 'MTSAB_IMMEDIATE'.
	/// 
	/// Note that the 3 preceding instructions may not be any of the following:
	/// MFSA, QFSRV
	/// 
	/// This function writes to global state (SA).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param byte_amount The byte-amount to set up the shift amount register for.
	FORCEINLINE void set_sa_8(sa_state_t* state, unsigned byte_amount)
	{
		MTSAB_BOTH(state, byte_amount, 0);
	}

	// MTSAH_BOTH
#ifdef PS2INTRIN_UNSAFE
	/// @brief MTSAH : Move halfword count To Shift Amount register (Halfword)
	/// 
	/// Set a halfword shift count in the shift amount register.
	/// 
	/// The values of 'variable' and 'immediate' are XOR'ed together. The resulting value is the
	/// amount of halfwords 'byte_shift_logical_right' will shift by.
	/// 
	/// Allowable values for 'variable' and 'immediate' are [0,7]. Only the lower 3 bits are used,
	/// others are ignored.
	/// 
	/// Use this macro if you require both the variable and constant values usable by the 'mtsah'
	/// instruction. Otherwise, prefer 'MTSAH_IMMEDIATE' if you only have a constant value and
	/// 'set_sa_16' if you only have a variable shift amount.
	/// 
	/// Note that the 3 preceding instructions may not be any of the following:
	/// MFSA, QFSRV
	/// 
	/// This macro writes to global state (SA).
	/// @param state Additional state used in safe mode. Must be an expression of type
	/// 'sa_state_t*'. May not be NULL.
	/// @param variable The variable halfword-amount to set up the shift amount register for. Should be
	/// the name of a variable of type 'unsigned'.
	/// @param immediate The compile time constant halfword-amount to set up the shift amount register
	/// for. Should be an integer literal.
	#define MTSAH_BOTH(state, variable, immediate)													\
	{																								\
		(void)state;																				\
		asm volatile(									/*	volatile, same reason as 'store_sa'	*/	\
			"mtsah	%[Variable],%c[Immediate]"														\
			:															/*	output parameters	*/	\
			: [Variable] "r" (variable), [Immediate] "n" (immediate)	/*	input parameters	*/	\
			: /*"sa"*/													/*	clobbers	*/			\
		);																							\
	}
#else
	/// @brief MTSAH : Move halfword count To Shift Amount register (Halfword)
	/// 
	/// Set a halfword shift count in the shift amount register.
	/// 
	/// The values of 'variable' and 'immediate' are XOR'ed together. The resulting value is the
	/// amount of halfwords 'byte_shift_logical_right' will shift by.
	/// 
	/// Allowable values for 'variable' and 'immediate' are [0,7]. Only the lower 3 bits are used,
	/// others are ignored.
	/// 
	/// Use this macro if you require both the variable and constant values usable by the 'mtsah'
	/// instruction. Otherwise, prefer 'MTSAH_IMMEDIATE' if you only have a constant value and
	/// 'set_sa_16' if you only have a variable shift amount.
	/// 
	/// Note that the 3 preceding instructions may not be any of the following:
	/// MFSA, QFSRV
	/// 
	/// This macro writes to global state (SA).
	/// @param state Additional state used in safe mode. Must be an expression of type
	/// 'sa_state_t*'. May not be NULL.
	/// @param variable The variable halfword-amount to set up the shift amount register for. Should be
	/// the name of a variable of type 'unsigned'.
	/// @param immediate The compile time constant halfword-amount to set up the shift amount register
	/// for. Should be an integer literal.
	#define MTSAH_BOTH(state, variable, immediate)													\
	{																								\
		uint64_t result = 0;																		\
		uint64_t tmp = 0;																			\
		asm volatile(																				\
			"mfsa	%[Tmp]\n\t"																		\
			"nop\n\tnop\n\tnop\n\tmtsah	%[Variable],%c[Immediate]\n\t"	/*	timing nops	*/			\
			"mfsa	%[Result]\n\t"																	\
			"nop\n\tnop\n\tnop\n\tmtsa	%[Tmp]\n\t"						/*	timing nops	*/			\
			: [Result] "=r" (result), [Tmp] "=&r" (tmp)					/*	output parameters	*/	\
			: [Variable] "r" (variable), [Immediate] "n" (immediate)	/*	input parameters	*/	\
		);																							\
		(state)->sa = result;																		\
	}
#endif

	// MTSAH_IMMEDIATE
#ifdef PS2INTRIN_UNSAFE
	/// @brief MTSAH : Move halfword count To Shift Amount register (Halfword)
	/// 
	/// Set a halfword shift count in the shift amount register.
	/// 
	/// The 'immediate' value is the amount of halfwords 'byte_shift_logical_right' will shift by.
	/// 
	/// Allowable values for 'immediate' are [0,7]. Only the lower 3 bits are used, others are
	/// ignored.
	/// 
	/// Use this macro if your shift amount is known at compile time. Using this macro does not use
	/// a general purpose register. Otherwise, prefer 'MTSAH_BOTH' or 'set_sa_16'.
	/// 
	/// Note that the 3 preceding instructions may not be any of the following:
	/// MFSA, QFSRV
	/// 
	/// This macro writes to global state (SA).
	/// @param state Additional state used in safe mode. Must be an expression of type
	/// 'sa_state_t*'. May not be NULL.
	/// @param immediate The compile time constant halfword-amount to set up the shift amount register
	/// for. Should be an integer literal.
	#define MTSAH_IMMEDIATE(state, immediate)														\
	{																								\
		(void)state;																				\
		asm volatile(									/*	volatile, same reason as 'store_sa'	*/	\
			"mtsah	$0,%c[Immediate]"																\
			:															/*	output parameters	*/	\
			: [Immediate] "n" (immediate)								/*	input parameters	*/	\
			: /*"sa"*/													/*	clobbers	*/			\
		);																							\
	}
#else
	/// @brief MTSAH : Move halfword count To Shift Amount register (Halfword)
	/// 
	/// Set a halfword shift count in the shift amount register.
	/// 
	/// The 'immediate' value is the amount of halfwords 'byte_shift_logical_right' will shift by.
	/// 
	/// Allowable values for 'immediate' are [0,7]. Only the lower 3 bits are used, others are
	/// ignored.
	/// 
	/// Use this macro if your shift amount is known at compile time. Using this macro does not use
	/// a general purpose register. Otherwise, prefer 'MTSAH_BOTH' or 'set_sa_16'.
	/// 
	/// Note that the 3 preceding instructions may not be any of the following:
	/// MFSA, QFSRV
	/// 
	/// This macro writes to global state (SA).
	/// @param state Additional state used in safe mode. Must be an expression of type
	/// 'sa_state_t*'. May not be NULL.
	/// @param immediate The compile time constant halfword-amount to set up the shift amount register
	/// for. Should be an integer literal.
	#define MTSAH_IMMEDIATE(state, immediate)														\
	{																								\
		uint64_t result = 0;																		\
		uint64_t tmp = 0;																			\
		asm volatile(																				\
			"mfsa	%[Tmp]\n\t"																		\
			"nop\n\tnop\n\tnop\n\tmtsah	$0,%c[Immediate]\n\t"			/*	timing nops	*/			\
			"mfsa	%[Result]\n\t"																	\
			"nop\n\tnop\n\tnop\n\tmtsa	%[Tmp]\n\t"						/*	timing nops	*/			\
			: [Result] "=r" (result), [Tmp] "=&r" (tmp)					/*	output parameters	*/	\
			: [Immediate] "n" (immediate)								/*	input parameters	*/	\
		);																							\
		(state)->sa = result;																		\
	}
#endif

	/// @brief MTSAH : Move halfword count To Shift Amount register (Halfword)
	/// 
	/// Set a halfword shift count in the shift amount register.
	/// 
	/// Allowable values for 'halfword_amount' are [0,7]. Only the lower 3 bits are used, others are
	/// ignored.
	/// 
	/// Use this function if your byte shift amount is calculated by the program. Otherwise, prefer
	/// 'MTSAH_BOTH' or 'MTSAH_IMMEDIATE'.
	/// 
	/// This function writes to global state (SA).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param halfword_amount The halfword-amount to set up the shift amount register for.
	FORCEINLINE void set_sa_16(sa_state_t* state, unsigned halfword_amount)
	{
		MTSAH_BOTH(state, halfword_amount, 0);
	}

	/// @brief QFSRV : Quadword Funnel Shift Right Variable
	/// 
	/// Concatenate ('upper' | 'lower') into a 256-bit value. Then shift the result right by an
	/// amount specified in the Shift Amount Register (SA). Since that register can only be set to
	/// multiples of bytes or halfwords, only bytes can be shifted at a time.
	/// 
	/// Note: If you can decode the value of the SA register you might be able to set specific bit
	/// amounts after all (using 'load_sa' and 'store_sa'). This is undocumented.
	/// 
	/// With specific values in the SA register a left shift and rotations can be achieved. For a
	/// left shift 'x' bytes use 'mtsab' with '16-x' as the argument, then use qfsvr with your
	/// data in the 'upper' position and '0' for 'lower'.
	/// 
	/// This function reads global state (SA).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param upper The upper 128 bits of the temporary value getting shifted right.
	/// @param lower The lower 128 bits of the temporary value getting shifted right.
	/// @return The lower 128 bits of the temporary value after shifting.
	FORCEINLINE PURE uint128_t byte_shift_logical_right(sa_state_t* state, uint128_t upper, uint128_t lower)
	{
		uint64_t upperlo = upper & 0xFFFFFFFFFFFFFFFF;
		uint64_t upperhi = (upper >> 64) & 0xFFFFFFFFFFFFFFFF;
		uint64_t lowerlo = lower & 0xFFFFFFFFFFFFFFFF;
		uint64_t lowerhi = (lower >> 64) & 0xFFFFFFFFFFFFFFFF;
		uint64_t resultboth = 0;
		uint64_t resulthi = 0;

#ifdef PS2INTRIN_UNSAFE
		(void)state;
		asm(
			"pcpyld	%[ResultBoth],%[UpperHi],%[UpperLo]\n\t"
			"pcpyld	%[LowerLo],%[LowerHi],%[LowerLo]\n\t"
			"qfsrv	%[ResultBoth],%[ResultBoth],%[LowerLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultBoth],%[ResultBoth]"
			: [LowerLo] "=r" (lowerlo),
			  [ResultBoth] "=r" (resultboth),
			  [ResultHi] "=&r" (resulthi)					/* clobber leads to better codegen	*/
			: [UpperHi] "r" (upperhi),
			  [UpperLo] "r1" (upperlo),
			  [LowerHi] "r" (lowerhi),
			  "r0" (lowerlo)
		);
#else
		uint64_t tmp = 0;
		asm(
			"mfsa	%[Tmp]\n\t"
			"pcpyld	%[ResultBoth],%[UpperHi],%[UpperLo]\n\t"
			"pcpyld	%[LowerLo],%[LowerHi],%[LowerLoR]\n\t"
			"nop\n\tmtsa	%[State]\n\t"										/* timing nop	*/
			"qfsrv	%[ResultBoth],%[ResultBoth],%[LowerLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultBoth],%[ResultBoth]\n\t"
			"nop\n\tnop\n\tmtsa	%[Tmp]\n\t"										/* timing nops	*/
			: [LowerLo] "=r" (lowerlo),
			  [ResultBoth] "=r" (resultboth),
			  [ResultHi] "=&r" (resulthi),					/* clobber leads to better codegen	*/
			  [Tmp] "=&r" (tmp)								/* clobber needed					*/
			: [UpperHi] "r" (upperhi),
			  [UpperLo] "r1" (upperlo),					/* can be same register as resultboth	*/
			  [LowerHi] "r" (lowerhi),
			  [LowerLoR] "r0" (lowerlo),			/* can be same register as written lowerlo	*/
			  [State] "r" (state->sa)
		);
#endif

		uint64_t resultlo = resultboth & 0xFFFFFFFFFFFFFFFF;

		uint128_t result = resulthi;
		result <<= 64;
		result |= resultlo;
		return result;
	}


	// Asynchronous and Pipeline 1 instructions

	/// @brief MULT : MULTiply word
	/// 
	/// Multiply 32-bit signed integers.
	/// 
	/// This uses the 3-operand version of the instruction, returning the low 32 bits of the 64 bit
	/// result in a register. Both the LO and HI register are still written to. Use this version if
	/// you need the low 32 bits of the 64-bit multiplication result.
	/// 
	/// Integer multiplication happens asynchonously. Reading the result before it is finished will
	/// stall the EE Core. This applies to the LO and HI registers as well as the return value of
	/// this function.
	/// 
	/// This function writes to global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First Multiplicand
	/// @param b Second Multiplicand
	/// @return Low 32 bits of the 64 bit multiplication result
	FORCEINLINE int32_t mullo0_i32_start(lohi_state_t* state, int32_t a, int32_t b)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;
		int32_t lo = 0;

		asm volatile(
			"mult	%[Lo],%[A],%[B]"
			: [Lo] "=r" (lo)
			: [A] "%r" (a),
			  [B] "r" (b)
			: "lo", "hi"
		);

		return lo;
#else
		int32_t lo = 0;
		int64_t tmplo = 0;
		int64_t tmphi = 0;

		asm(
			"mflo	%[TmpLo]\n\t"
			"mfhi	%[TmpHi]\n\t"
			"mult	%[Lo],%[A],%[B]\n\t"
			"mflo	%[StateLo]\n\t"
			"mfhi	%[StateHi]\n\t"
			"mtlo	%[TmpLo]\n\t"
			"mthi	%[TmpHi]"
			: [Lo] "=r" (lo),
			  [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo] "=r" (state->lo[0]),
			  [StateHi] "=r" (state->hi[0])
			: [A] "%r" (a),
			  [B] "r" (b)
		);

		return lo;
#endif
	}

	/// @brief MULT : MULTiply word
	/// 
	/// Multiply 32-bit signed integers.
	/// 
	/// This uses the 2-operand version of the instruction, writing only to the LO0 and HI0
	/// register. Use this version if you are only interested in the high 32 bits of the 64-bit
	/// multiplication result.
	/// 
	/// Integer multiplication happens asynchonously. Reading the result before it is finished will
	/// stall the EE Core.
	/// 
	/// This function writes to global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First Multiplicand
	/// @param b Second Multiplicand
	FORCEINLINE void mulhi0_i32_start(lohi_state_t* state, int32_t a, int32_t b)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		asm volatile(
			"mult	%[A],%[B]"
			: 
			: [A] "%r" (a),
			  [B] "r" (b)
			: "lo", "hi"
		);
#else
		int64_t tmplo = 0;
		int64_t tmphi = 0;

		asm(
			"mflo	%[TmpLo]\n\t"
			"mfhi	%[TmpHi]\n\t"
			"mult	%[A],%[B]\n\t"
			"mflo	%[StateLo]\n\t"
			"mfhi	%[StateHi]\n\t"
			"mtlo	%[TmpLo]\n\t"
			"mthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo] "=r" (state->lo[0]),
			  [StateHi] "=r" (state->hi[0])
			: [A] "%r" (a),
			  [B] "r" (b)
		);
#endif
	}

	/// @brief MULT : MULTiply word
	/// 
	/// Finish an asynchronous multiplication by reading both low and high 32 bits of the 64-bit
	/// result. Use this function if you started a multiplication using 'mulhi0_i32_start' but do
	/// want the low 32 bit result after all.
	/// 
	/// This function reads global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return Struct containing low and high 32 bits of 64-bit multiplication result
	FORCEINLINE PURE mul_i32_result_t mul0_i32_finish(lohi_state_t* state)
	{
		mul_i32_result_t result = { 0, 0 };

		result.lo = load_lo0_32(state);
		result.hi = load_hi0_32(state);

		return result;
	}

	/// @brief MULT : MULTiply word
	/// 
	/// Finish an asynchronous multiplication by reading the high 32 bits of the 64-bit result,
	/// reusing the low 32 bits from 'mullo0_i32_start'. Prefer this function if you need both
	/// parts as a struct.
	/// 
	/// This function reads global state (HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param lo Low 32 bits of multiplication result obtained from 'mullo0_i32_start'
	/// @return Struct containing low and high 32 bits of 64-bit multiplication result
	FORCEINLINE PURE mul_i32_result_t mul0_i32_finish_lo(lohi_state_t* state, int32_t lo)
	{
		mul_i32_result_t result = { 0, 0 };

		result.lo = lo;
		result.hi = load_hi0_32(state);

		return result;
	}

	/// @brief MULT : MULTiply word
	/// 
	/// Finish an asynchronous multiplication by reading the high 32 bits of the 64-bit result.
	/// Prefer this function if you need both parts of the result separately or just the high part.
	/// 
	/// This function reads global state (HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return High 32 bits of 64-bit multiplication result
	FORCEINLINE PURE int32_t mulhi0_i32_finish(lohi_state_t* state)
	{
		return load_hi0_32(state);
	}

	/// @brief MULT : MULTiply word
	/// 
	/// Multiply 32-bit signed integers.
	/// 
	/// Helper function to both start and finish a multiplication. This function is known to not
	/// be optimal in regards to throughput. Refer to the documentation of 'mullo0_i32_start'.
	/// 
	/// This function writes to global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First Multiplicand
	/// @param b Second Multiplicand
	/// @return Struct containing low and high 32 bits of 64-bit multiplication result
	FORCEINLINE mul_i32_result_t mul0_i32(lohi_state_t* state, int32_t a, int32_t b)
	{
		return mul0_i32_finish_lo(state, mullo0_i32_start(state, a, b));
	}

	/// @brief MULTU : MULTiply Unsigned word
	/// 
	/// Multiply 32-bit unsigned integers.
	/// 
	/// This uses the 3-operand version of the instruction, returning the low 32 bits of the 64 bit
	/// result in a register. Both the LO and HI register are still written to. Use this version if
	/// you need the low 32 bits of the 64-bit multiplication result.
	/// 
	/// Integer multiplication happens asynchonously. Reading the result before it is finished will
	/// stall the EE Core. This applies to the LO and HI registers as well as the return value of
	/// this function.
	/// 
	/// This function writes to global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First Multiplicand
	/// @param b Second Multiplicand
	/// @return Low 32 bits of the 64 bit multiplication result
	FORCEINLINE uint32_t mullo0_u32_start(lohi_state_t* state, uint32_t a, uint32_t b)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;
		int32_t lo = 0;

		asm volatile(
			"multu	%[Lo],%[A],%[B]"
			: [Lo] "=r" (lo)
			: [A] "%r" (a), [B] "r" (b)
			: "lo", "hi"
		);

		return lo;
#else
		int32_t lo = 0;
		int64_t tmplo = 0;
		int64_t tmphi = 0;

		asm(
			"mflo	%[TmpLo]\n\t"
			"mfhi	%[TmpHi]\n\t"
			"multu	%[Lo],%[A],%[B]\n\t"
			"mflo	%[StateLo]\n\t"
			"mfhi	%[StateHi]\n\t"
			"mtlo	%[TmpLo]\n\t"
			"mthi	%[TmpHi]"
			: [Lo] "=r" (lo),
			  [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo] "=r" (state->lo[0]),
			  [StateHi] "=r" (state->hi[0])
			: [A] "%r" (a),
			  [B] "r" (b)
		);

		return lo;
#endif
	}

	/// @brief MULTU : MULTiply Unsigned word
	/// 
	/// Multiply 32-bit unsigned integers.
	/// 
	/// This uses the 2-operand version of the instruction, writing only to the LO0 and HI0
	/// register. Use this version if you are only interested in the high 32 bits of the 64-bit
	/// multiplication result.
	/// 
	/// Integer multiplication happens asynchonously. Reading the result before it is finished will
	/// stall the EE Core.
	/// 
	/// This function writes to global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First Multiplicand
	/// @param b Second Multiplicand
	FORCEINLINE void mulhi0_u32_start(lohi_state_t* state, uint32_t a, uint32_t b)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		asm volatile(
			"multu	%[A],%[B]"
			:
			: [A] "%r" (a),
			  [B] "r" (b)
			: "lo", "hi"
		);
#else
		int64_t tmplo = 0;
		int64_t tmphi = 0;

		asm(
			"mflo	%[TmpLo]\n\t"
			"mfhi	%[TmpHi]\n\t"
			"multu	%[A],%[B]\n\t"
			"mflo	%[StateLo]\n\t"
			"mfhi	%[StateHi]\n\t"
			"mtlo	%[TmpLo]\n\t"
			"mthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo] "=r" (state->lo[0]),
			  [StateHi] "=r" (state->hi[0])
			: [A] "%r" (a),
			  [B] "r" (b)
		);
#endif
	}

	/// @brief MULTU : MULTiply Unsigned word
	/// 
	/// Finish an asynchronous multiplication by reading both low and high 32 bits of the 64-bit
	/// result. Use this function if you started a multiplication using 'mulhi0_u32_start' but do
	/// want the low 32 bit result after all.
	/// 
	/// This function reads global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return Struct containing low and high 32 bits of 64-bit multiplication result
	FORCEINLINE PURE mul_u32_result_t mul0_u32_finish(lohi_state_t* state)
	{
		mul_u32_result_t result = { 0, 0 };

		result.lo = load_lo0_32(state);
		result.hi = load_hi0_32(state);

		return result;
	}

	/// @brief MULTU : MULTiply Unsigned word
	/// 
	/// Finish an asynchronous multiplication by reading the high 32 bits of the 64-bit result,
	/// reusing the low 32 bits from 'mullo0_u32_start'. Prefer this function if you need both
	/// parts as a struct.
	/// 
	/// This function reads global state (HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param lo Low 32 bits of multiplication result obtained from 'mullo0_u32_start'
	/// @return Struct containing low and high 32 bits of 64-bit multiplication result
	FORCEINLINE PURE mul_u32_result_t mul0_u32_finish_lo(lohi_state_t* state, uint32_t lo)
	{
		mul_u32_result_t result = { 0, 0 };

		result.lo = lo;
		result.hi = load_hi0_32(state);

		return result;
	}

	/// @brief MULTU : MULTiply Unsigned word
	/// 
	/// Finish an asynchronous multiplication by reading the high 32 bits of the 64-bit result.
	/// Prefer this function if you need both parts of the result separately or just the high part.
	/// 
	/// This function reads global state (HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return High 32 bits of 64-bit multiplication result
	FORCEINLINE PURE uint32_t mulhi0_u32_finish(lohi_state_t* state)
	{
		return load_hi0_32(state);
	}

	/// @brief MULTU : MULTiply Unsigned word
	/// 
	/// Multiply 32-bit unsigned integers.
	/// 
	/// Helper function to both start and finish a multiplication. This function is known to not
	/// be optimal in regards to throughput. Refer to the documentation of 'mullo0_u32_start'.
	/// 
	/// This function writes to global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First Multiplicand
	/// @param b Second Multiplicand
	/// @return Struct containing low and high 32 bits of 64-bit multiplication result
	FORCEINLINE mul_u32_result_t mul0_u32(lohi_state_t* state, uint32_t a, uint32_t b)
	{
		return mul0_u32_finish_lo(state, mullo0_u32_start(state, a, b));
	}

	/// @brief MULT1 : MULTiply word pipeline 1
	/// 
	/// Multiply 32-bit signed integers.
	/// 
	/// This uses the 3-operand version of the instruction, returning the low 32 bits of the 64 bit
	/// result in a register. Both the LO and HI register are still written to. Use this version if
	/// you need the low 32 bits of the 64-bit multiplication result.
	/// 
	/// Integer multiplication happens asynchonously. Reading the result before it is finished will
	/// stall the EE Core. This applies to the LO and HI registers as well as the return value of
	/// this function.
	/// 
	/// This function writes to global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First Multiplicand
	/// @param b Second Multiplicand
	/// @return Low 32 bits of the 64 bit multiplication result
	FORCEINLINE int32_t mullo1_i32_start(lohi_state_t* state, int32_t a, int32_t b)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;
		int32_t lo = 0;

		asm volatile(
			"mult1	%[Lo],%[A],%[B]"
			: [Lo] "=r" (lo)
			: [A] "%r" (a),
			  [B] "r" (b)
			: "lo", "hi"
		);

		return lo;
#else
		int32_t lo = 0;
		int64_t tmplo = 0;
		int64_t tmphi = 0;

		asm(
			"mflo1	%[TmpLo]\n\t"
			"mfhi1	%[TmpHi]\n\t"
			"mult1	%[Lo],%[A],%[B]\n\t"
			"mflo1	%[StateLo]\n\t"
			"mfhi1	%[StateHi]\n\t"
			"mtlo1	%[TmpLo]\n\t"
			"mthi1	%[TmpHi]"
			: [Lo] "=r" (lo),
			  [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo] "=r" (state->lo[1]),
			  [StateHi] "=r" (state->hi[1])
			: [A] "%r" (a),
			  [B] "r" (b)
		);

		return lo;
#endif
	}

	/// @brief MULT1 : MULTiply word pipeline 1
	/// 
	/// Multiply 32-bit signed integers.
	/// 
	/// This uses the 2-operand version of the instruction, writing only to the LO0 and HI0
	/// register. Use this version if you are only interested in the high 32 bits of the 64-bit
	/// multiplication result.
	/// 
	/// Integer multiplication happens asynchonously. Reading the result before it is finished will
	/// stall the EE Core.
	/// 
	/// This function writes to global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First Multiplicand
	/// @param b Second Multiplicand
	FORCEINLINE void mulhi1_i32_start(lohi_state_t* state, int32_t a, int32_t b)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		asm volatile(
			"mult1	%[A],%[B]"
			:
			: [A] "%r" (a),
			  [B] "r" (b)
			: "lo", "hi"
		);
#else
		int64_t tmplo = 0;
		int64_t tmphi = 0;

		asm(
			"mflo1	%[TmpLo]\n\t"
			"mfhi1	%[TmpHi]\n\t"
			"mult1	%[A],%[B]\n\t"
			"mflo1	%[StateLo]\n\t"
			"mfhi1	%[StateHi]\n\t"
			"mtlo1	%[TmpLo]\n\t"
			"mthi1	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo] "=r" (state->lo[1]),
			  [StateHi] "=r" (state->hi[1])
			: [A] "%r" (a),
			  [B] "r" (b)
		);
#endif
	}

	/// @brief MULT : MULTiply word word pipeline 1
	/// 
	/// Finish an asynchronous multiplication by reading both low and high 32 bits of the 64-bit
	/// result. Use this function if you started a multiplication using 'mulhi0_i32_start' but do
	/// want the low 32 bit result after all.
	/// 
	/// This function reads global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return Struct containing low and high 32 bits of 64-bit multiplication result
	FORCEINLINE PURE mul_i32_result_t mul1_i32_finish(lohi_state_t* state)
	{
		mul_i32_result_t result = { 0, 0 };

		result.lo = load_lo1_32(state);
		result.hi = load_hi1_32(state);

		return result;
	}

	/// @brief MULT1 : MULTiply word pipeline 1
	/// 
	/// Finish an asynchronous multiplication by reading the high 32 bits of the 64-bit result,
	/// reusing the low 32 bits from 'mullo1_i32_start'. Prefer this function if you need both
	/// parts as a struct.
	/// 
	/// This function reads global state (HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param lo Low 32 bits of multiplication result obtained from 'mullo1_i32_start'
	/// @return Struct containing low and high 32 bits of 64-bit multiplication result
	FORCEINLINE PURE mul_i32_result_t mul1_i32_finish_lo(lohi_state_t* state, int32_t lo)
	{
		mul_i32_result_t result = { 0, 0 };

		result.lo = lo;
		result.hi = load_hi1_32(state);

		return result;
	}

	/// @brief MULT1 : MULTiply word pipeline 1
	/// 
	/// Finish an asynchronous multiplication by reading the high 32 bits of the 64-bit result.
	/// Prefer this function if you need both parts of the result separately or just the high part.
	/// 
	/// This function reads global state (HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return High 32 bits of 64-bit multiplication result
	FORCEINLINE PURE int32_t mulhi1_i32_finish(lohi_state_t* state)
	{
		return load_hi1_32(state);
	}

	/// @brief MULT1 : MULTiply word pipeline 1
	/// 
	/// Multiply 32-bit signed integers.
	/// 
	/// Helper function to both start and finish a multiplication. This function is known to not
	/// be optimal in regards to throughput. Refer to the documentation of 'mullo1_i32_start'.
	/// 
	/// This function writes to global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First Multiplicand
	/// @param b Second Multiplicand
	/// @return Struct containing low and high 32 bits of 64-bit multiplication result
	FORCEINLINE mul_i32_result_t mul1_i32(lohi_state_t* state, int32_t a, int32_t b)
	{
		return mul1_i32_finish_lo(state, mullo1_i32_start(state, a, b));
	}

	/// @brief MULTU1 : MULTiply Unsigned word pipeline 1
	/// 
	/// Multiply 32-bit unsigned integers.
	/// 
	/// This uses the 3-operand version of the instruction, returning the low 32 bits of the 64 bit
	/// result in a register. Both the LO and HI register are still written to. Use this version if
	/// you need the low 32 bits of the 64-bit multiplication result.
	/// 
	/// Integer multiplication happens asynchonously. Reading the result before it is finished will
	/// stall the EE Core. This applies to the LO and HI registers as well as the return value of
	/// this function.
	/// 
	/// This function writes to global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First Multiplicand
	/// @param b Second Multiplicand
	/// @return Low 32 bits of the 64 bit multiplication result
	FORCEINLINE uint32_t mullo1_u32_start(lohi_state_t* state, uint32_t a, uint32_t b)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;
		int32_t lo = 0;

		asm volatile(
			"multu1	%[Lo],%[A],%[B]"
			: [Lo] "=r" (lo)
			: [A] "%r" (a),
			  [B] "r" (b)
			: "lo", "hi"
		);

		return lo;
#else
		int32_t lo = 0;
		int64_t tmplo = 0;
		int64_t tmphi = 0;

		asm(
			"mflo1	%[TmpLo]\n\t"
			"mfhi1	%[TmpHi]\n\t"
			"multu1	%[Lo],%[A],%[B]\n\t"
			"mflo1	%[StateLo]\n\t"
			"mfhi1	%[StateHi]\n\t"
			"mtlo1	%[TmpLo]\n\t"
			"mthi1	%[TmpHi]"
			: [Lo] "=r" (lo),
			  [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo] "=r" (state->lo[1]),
			  [StateHi] "=r" (state->hi[1])
			: [A] "%r" (a),
			  [B] "r" (b)
		);

		return lo;
#endif
	}

	/// @brief MULTU1 : MULTiply Unsigned word pipeline 1
	/// 
	/// Multiply 32-bit unsigned integers.
	/// 
	/// This uses the 2-operand version of the instruction, writing only to the LO0 and HI0
	/// register. Use this version if you are only interested in the high 32 bits of the 64-bit
	/// multiplication result.
	/// 
	/// Integer multiplication happens asynchonously. Reading the result before it is finished will
	/// stall the EE Core.
	/// 
	/// This function writes to global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First Multiplicand
	/// @param b Second Multiplicand
	FORCEINLINE void mulhi1_u32_start(lohi_state_t* state, uint32_t a, uint32_t b)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		asm volatile(
			"multu1	%[A],%[B]"
			:
			: [A] "%r" (a),
			  [B] "r" (b)
			: "lo", "hi"
		);
#else
		int64_t tmplo = 0;
		int64_t tmphi = 0;

		asm(
			"mflo1	%[TmpLo]\n\t"
			"mfhi1	%[TmpHi]\n\t"
			"multu1	%[A],%[B]\n\t"
			"mflo1	%[StateLo]\n\t"
			"mfhi1	%[StateHi]\n\t"
			"mtlo1	%[TmpLo]\n\t"
			"mthi1	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo] "=r" (state->lo[1]),
			  [StateHi] "=r" (state->hi[1])
			: [A] "%r" (a),
			  [B] "r" (b)
		);
#endif
	}

	/// @brief MULTU1 : MULTiply Unsigned word pipeline 1
	/// 
	/// Finish an asynchronous multiplication by reading both low and high 32 bits of the 64-bit
	/// result. Use this function if you started a multiplication using 'mulhi1_u32_start' but do
	/// want the low 32 bit result after all.
	/// 
	/// This function reads global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return Struct containing low and high 32 bits of 64-bit multiplication result
	FORCEINLINE PURE mul_u32_result_t mul1_u32_finish(lohi_state_t* state)
	{
		mul_u32_result_t result = { 0, 0 };

		result.lo = load_lo1_32(state);
		result.hi = load_hi1_32(state);

		return result;
	}

	/// @brief MULTU1 : MULTiply Unsigned word pipeline 1
	/// 
	/// Finish an asynchronous multiplication by reading the high 32 bits of the 64-bit result,
	/// reusing the low 32 bits from 'mullo1_u32_start'. Prefer this function if you need both
	/// parts as a struct.
	/// 
	/// This function reads global state (HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param lo Low 32 bits of multiplication result obtained from 'mullo1_u32_start'
	/// @return Struct containing low and high 32 bits of 64-bit multiplication result
	FORCEINLINE PURE mul_u32_result_t mul1_u32_finish_lo(lohi_state_t* state, uint32_t lo)
	{
		mul_u32_result_t result = { 0, 0 };

		result.lo = lo;
		result.hi = load_hi1_32(state);

		return result;
	}

	/// @brief MULTU1 : MULTiply Unsigned word pipeline 1
	/// 
	/// Finish an asynchronous multiplication by reading the high 32 bits of the 64-bit result.
	/// Prefer this function if you need both parts of the result separately or just the high part.
	/// 
	/// This function reads global state (HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return High 32 bits of 64-bit multiplication result
	FORCEINLINE PURE uint32_t mulhi1_u32_finish(lohi_state_t* state)
	{
		return load_hi1_32(state);
	}

	/// @brief MULTU1 : MULTiply Unsigned word pipeline 1
	/// 
	/// Multiply 32-bit unsigned integers.
	/// 
	/// Helper function to both start and finish a multiplication. This function is known to not
	/// be optimal in regards to throughput. Refer to the documentation of 'mullo1_u32_start'.
	/// 
	/// This function writes to global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First Multiplicand
	/// @param b Second Multiplicand
	/// @return Struct containing low and high 32 bits of 64-bit multiplication result
	FORCEINLINE mul_u32_result_t mul1_u32(lohi_state_t* state, uint32_t a, uint32_t b)
	{
		return mul1_u32_finish_lo(state, mullo1_u32_start(state, a, b));
	}


	/// @brief MADD : Multiply-ADD word
	/// 
	/// Multiply signed 32-bit values and accumulate into the LO/HI registers.
	/// 
	/// This uses the 3 operand form of the instruction. Use this if you only need the low 32 bits
	/// of the 64-bit accumulator. If you do not need that value use 'fma0_i32' instead.
	/// 
	/// Integer fused-multiply-add is processed asynchronously. Attempting to read from LO0, HI0 or
	/// the return value will cause a stall if this instruction is not finished.
	/// 
	/// This function writes to global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First multiplicand
	/// @param b Second multiplicand
	/// @return Low 32 bits (LO0) of the accumulator after the fused-multiply-add operation
	FORCEINLINE int32_t fma0_i32_lo(lohi_state_t* state, int32_t a, int32_t b)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;
		int32_t lo = 0;

		asm volatile(
			"madd	%[Lo],%[A],%[B]"
			: [Lo] "=r" (lo)
			: [A] "%r" (a),
			  [B] "r" (b)
			: "lo", "hi"
		);

		return lo;
#else
		int32_t lo = 0;
		int64_t tmplo = 0;
		int64_t tmphi = 0;

		asm(
			"mflo	%[TmpLo]\n\t"
			"mfhi	%[TmpHi]\n\t"
			"mtlo	%[StateLo]\n\t"
			"mthi	%[StateHi]\n\t"
			"madd	%[Lo],%[A],%[B]\n\t"
			"mflo	%[StateLo]\n\t"
			"mfhi	%[StateHi]\n\t"
			"mtlo	%[TmpLo]\n\t"
			"mthi	%[TmpHi]"
			: [Lo] "=r" (lo),
			  [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo] "+r" (state->lo[0]),
			  [StateHi] "+r" (state->hi[0])
			: [A] "%r" (a),
			  [B] "r" (b)
		);

		return lo;
#endif
	}

	/// @brief MADD : Multiply-ADD word
	/// 
	/// Multiply signed 32-bit values and accumulate into the LO/HI registers.
	/// 
	/// This uses the 2 operand form of the instruction. Use this version if you are doing many
	/// fused-multiply-add with the same accumulator in a loop.
	/// 
	/// Integer fused-multiply-add is processed asynchronously. Attempting to read from LO0 or HI0
	/// will cause a stall if this instruction is not finished.
	/// 
	/// This function writes to global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First multiplicand
	/// @param b Second multiplicand
	FORCEINLINE void fma0_i32(lohi_state_t* state, int32_t a, int32_t b)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		asm volatile(
			"madd	%[A],%[B]"
			:
			: [A] "%r" (a),
			  [B] "r" (b)
			: "lo", "hi"
		);
#else
		int64_t tmplo = 0;
		int64_t tmphi = 0;

		asm(
			"mflo	%[TmpLo]\n\t"
			"mfhi	%[TmpHi]\n\t"
			"mtlo	%[StateLo]\n\t"
			"mthi	%[StateHi]\n\t"
			"madd	%[A],%[B]\n\t"
			"mflo	%[StateLo]\n\t"
			"mfhi	%[StateHi]\n\t"
			"mtlo	%[TmpLo]\n\t"
			"mthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo] "+r" (state->lo[0]),
			  [StateHi] "+r" (state->hi[0])
			: [A] "%r" (a),
			  [B] "r" (b)
		);
#endif
	}

	/// @brief MADD : Multiply-ADD word
	/// 
	/// Multiply signed 32-bit values and accumulate into the LO/HI registers.
	/// 
	/// This uses the 3 operand form of the instruction. The upper part of the accumulator is
	/// loaded and combined with the low part into a 64-bit result and returned.
	/// 
	/// This function is known to be suboptimal in regards to throughput. See documentation of
	/// 'fma0_i32' for details.
	/// 
	/// This function writes to global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First multiplicand
	/// @param b Second multiplicand
	/// @return Struct containing bot the low and high 32 bits of the accumulator after the
	/// fused-multiply-add operation.
	FORCEINLINE mul_i32_result_t fma0_i32_finish(lohi_state_t* state, int32_t a, int32_t b)
	{
		mul_i32_result_t result = { 0, 0 };

		result.lo = fma0_i32_lo(state, a, b);
		result.hi = load_hi0_32(state);

		return result;
	}

	/// @brief MADDU : Multiply-ADD Unsigned word
	/// 
	/// Multiply unsigned 32-bit values and accumulate into the LO/HI registers.
	/// 
	/// This uses the 3 operand form of the instruction. Use this if you only need the low 32 bits
	/// of the 64-bit accumulator. If you do not need that value use 'fma0_u32' instead.
	/// 
	/// Integer fused-multiply-add is processed asynchronously. Attempting to read from LO0, HI0 or
	/// the return value will cause a stall if this instruction is not finished.
	/// 
	/// This function writes to global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First multiplicand
	/// @param b Second multiplicand
	/// @return Low 32 bits (LO0) of the accumulator after the fused-multiply-add operation
	FORCEINLINE uint32_t fma0_u32_lo(lohi_state_t* state, uint32_t a, uint32_t b)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;
		int32_t lo = 0;

		asm volatile(
			"maddu	%[Lo],%[A],%[B]"
			: [Lo] "=r" (lo)
			: [A] "%r" (a),
			  [B] "r" (b)
			: "lo", "hi"
		);

		return lo;
#else
		int32_t lo = 0;
		int64_t tmplo = 0;
		int64_t tmphi = 0;

		asm(
			"mflo	%[TmpLo]\n\t"
			"mfhi	%[TmpHi]\n\t"
			"mtlo	%[StateLo]\n\t"
			"mthi	%[StateHi]\n\t"
			"maddu	%[Lo],%[A],%[B]\n\t"
			"mflo	%[StateLo]\n\t"
			"mfhi	%[StateHi]\n\t"
			"mtlo	%[TmpLo]\n\t"
			"mthi	%[TmpHi]"
			: [Lo] "=r" (lo),
			  [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo] "+r" (state->lo[0]),
			  [StateHi] "+r" (state->hi[0])
			: [A] "%r" (a),
			  [B] "r" (b)
		);

		return lo;
#endif
	}

	/// @brief MADDU : Multiply-ADD Unsigned word
	/// 
	/// Multiply unsigned 32-bit values and accumulate into the LO/HI registers.
	/// 
	/// This uses the 2 operand form of the instruction. Use this version if you are doing many
	/// fused-multiply-add with the same accumulator in a loop.
	/// 
	/// Integer fused-multiply-add is processed asynchronously. Attempting to read from LO0 or HI0
	/// will cause a stall if this instruction is not finished.
	/// 
	/// This function writes to global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First multiplicand
	/// @param b Second multiplicand
	FORCEINLINE void fma0_u32(lohi_state_t* state, uint32_t a, uint32_t b)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		asm volatile(
			"maddu	%[A],%[B]"
			:
			: [A] "%r" (a),
			  [B] "r" (b)
			: "lo", "hi"
		);
#else
		int64_t tmplo = 0;
		int64_t tmphi = 0;

		asm(
			"mflo	%[TmpLo]\n\t"
			"mfhi	%[TmpHi]\n\t"
			"mtlo	%[StateLo]\n\t"
			"mthi	%[StateHi]\n\t"
			"maddu	%[A],%[B]\n\t"
			"mflo	%[StateLo]\n\t"
			"mfhi	%[StateHi]\n\t"
			"mtlo	%[TmpLo]\n\t"
			"mthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo] "+r" (state->lo[0]),
			  [StateHi] "+r" (state->hi[0])
			: [A] "%r" (a),
			  [B] "r" (b)
		);
#endif
	}

	/// @brief MADDU : Multiply-ADD Unsigned word
	/// 
	/// Multiply unsigned 32-bit values and accumulate into the LO/HI registers.
	/// 
	/// This uses the 3 operand form of the instruction. The upper part of the accumulator is
	/// loaded and combined with the low part into a 64-bit result and returned.
	/// 
	/// This function is known to be suboptimal in regards to throughput. See documentation of
	/// 'fma0_u32' for details.
	/// 
	/// This function writes to global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First multiplicand
	/// @param b Second multiplicand
	/// @return Struct containing bot the low and high 32 bits of the accumulator after the
	/// fused-multiply-add operation.
	FORCEINLINE mul_u32_result_t fma0_u32_finish(lohi_state_t* state, uint32_t a, uint32_t b)
	{
		mul_u32_result_t result = { 0, 0 };

		result.lo = fma0_u32_lo(state, a, b);
		result.hi = load_hi0_32(state);

		return result;
	}

	/// @brief MADD1 : Multiply-ADD word pipeline 1
	/// 
	/// Multiply signed 32-bit values and accumulate into the LO/HI registers.
	/// 
	/// This uses the 3 operand form of the instruction. Use this if you only need the low 32 bits
	/// of the 64-bit accumulator. If you do not need that value use 'fma1_i32' instead.
	/// 
	/// Integer fused-multiply-add is processed asynchronously. Attempting to read from LO1, HI1 or
	/// the return value will cause a stall if this instruction is not finished.
	/// 
	/// This function writes to global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First multiplicand
	/// @param b Second multiplicand
	/// @return Low 32 bits (LO1) of the accumulator after the fused-multiply-add operation
	FORCEINLINE int32_t fma1_i32_lo(lohi_state_t* state, int32_t a, int32_t b)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;
		int32_t lo = 0;

		asm volatile(
			"madd1	%[Lo],%[A],%[B]"
			: [Lo] "=r" (lo)
			: [A] "%r" (a),
			  [B] "r" (b)
			: "lo", "hi"
		);

		return lo;
#else
		int32_t lo = 0;
		int64_t tmplo = 0;
		int64_t tmphi = 0;

		asm(
			"mflo1	%[TmpLo]\n\t"
			"mfhi1	%[TmpHi]\n\t"
			"mtlo1	%[StateLo]\n\t"
			"mthi1	%[StateHi]\n\t"
			"madd1	%[Lo],%[A],%[B]\n\t"
			"mflo1	%[StateLo]\n\t"
			"mfhi1	%[StateHi]\n\t"
			"mtlo1	%[TmpLo]\n\t"
			"mthi1	%[TmpHi]"
			: [Lo] "=r" (lo),
			  [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo] "+r" (state->lo[1]),
			  [StateHi] "+r" (state->hi[1])
			: [A] "%r" (a),
			  [B] "r" (b)
		);

		return lo;
#endif
	}

	/// @brief MADD1 : Multiply-ADD word pipeline 1
	/// 
	/// Multiply signed 32-bit values and accumulate into the LO/HI registers.
	/// 
	/// This uses the 2 operand form of the instruction. Use this version if you are doing many
	/// fused-multiply-add with the same accumulator in a loop.
	/// 
	/// Integer fused-multiply-add is processed asynchronously. Attempting to read from LO1 or HI1
	/// will cause a stall if this instruction is not finished.
	/// 
	/// This function writes to global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First multiplicand
	/// @param b Second multiplicand
	FORCEINLINE void fma1_i32(lohi_state_t* state, int32_t a, int32_t b)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		asm volatile(
			"madd1	%[A],%[B]"
			:
			: [A] "%r" (a),
			  [B] "r" (b)
			: "lo", "hi"
		);
#else
		int64_t tmplo = 0;
		int64_t tmphi = 0;

		asm(
			"mflo1	%[TmpLo]\n\t"
			"mfhi1	%[TmpHi]\n\t"
			"mtlo1	%[StateLo]\n\t"
			"mthi1	%[StateHi]\n\t"
			"madd1	%[A],%[B]\n\t"
			"mflo1	%[StateLo]\n\t"
			"mfhi1	%[StateHi]\n\t"
			"mtlo1	%[TmpLo]\n\t"
			"mthi1	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo] "+r" (state->lo[1]),
			  [StateHi] "+r" (state->hi[1])
			: [A] "%r" (a),
			  [B] "r" (b)
		);
#endif
	}

	/// @brief MADD1 : Multiply-ADD word pipeline 1
	/// 
	/// Multiply signed 32-bit values and accumulate into the LO/HI registers.
	/// 
	/// This uses the 3 operand form of the instruction. The upper part of the accumulator is
	/// loaded and combined with the low part into a 64-bit result and returned.
	/// 
	/// This function is known to be suboptimal in regards to throughput. See documentation of
	/// 'fma1_i32' for details.
	/// 
	/// This function writes to global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First multiplicand
	/// @param b Second multiplicand
	/// @return Struct containing bot the low and high 32 bits of the accumulator after the
	/// fused-multiply-add operation.
	FORCEINLINE mul_i32_result_t fma1_i32_finish(lohi_state_t* state, int32_t a, int32_t b)
	{
		mul_i32_result_t result = { 0, 0 };

		result.lo = fma1_i32_lo(state, a, b);
		result.hi = load_hi1_32(state);

		return result;
	}

	/// @brief MADDU1 : Multiply-ADD Unsigned word pipeline 1
	/// 
	/// Multiply unsigned 32-bit values and accumulate into the LO/HI registers.
	/// 
	/// This uses the 3 operand form of the instruction. Use this if you only need the low 32 bits
	/// of the 64-bit accumulator. If you do not need that value use 'fma1_u32' instead.
	/// 
	/// Integer fused-multiply-add is processed asynchronously. Attempting to read from LO1, HI1 or
	/// the return value will cause a stall if this instruction is not finished.
	/// 
	/// This function writes to global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First multiplicand
	/// @param b Second multiplicand
	/// @return Low 32 bits (LO1) of the accumulator after the fused-multiply-add operation
	FORCEINLINE uint32_t fma1_u32_lo(lohi_state_t* state, uint32_t a, uint32_t b)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;
		int32_t lo = 0;

		asm volatile(
			"maddu1	%[Lo],%[A],%[B]"
			: [Lo] "=r" (lo)
			: [A] "%r" (a),
			  [B] "r" (b)
			: "lo", "hi"
		);

		return lo;
#else
		int32_t lo = 0;
		int64_t tmplo = 0;
		int64_t tmphi = 0;

		asm(
			"mflo1	%[TmpLo]\n\t"
			"mfhi1	%[TmpHi]\n\t"
			"mtlo1	%[StateLo]\n\t"
			"mthi1	%[StateHi]\n\t"
			"maddu1	%[Lo],%[A],%[B]\n\t"
			"mflo1	%[StateLo]\n\t"
			"mfhi1	%[StateHi]\n\t"
			"mtlo1	%[TmpLo]\n\t"
			"mthi1	%[TmpHi]"
			: [Lo] "=r" (lo),
			  [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo] "+r" (state->lo[1]),
			  [StateHi] "+r" (state->hi[1])
			: [A] "%r" (a),
			  [B] "r" (b)
		);

		return lo;
#endif
	}

	/// @brief MADDU1 : Multiply-ADD Unsigned word pipeline 1
	/// 
	/// Multiply unsigned 32-bit values and accumulate into the LO/HI registers.
	/// 
	/// This uses the 2 operand form of the instruction. Use this version if you are doing many
	/// fused-multiply-add with the same accumulator in a loop.
	/// 
	/// Integer fused-multiply-add is processed asynchronously. Attempting to read from LO1 or HI1
	/// will cause a stall if this instruction is not finished.
	/// 
	/// This function writes to global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First multiplicand
	/// @param b Second multiplicand
	FORCEINLINE void fma1_u32(lohi_state_t* state, uint32_t a, uint32_t b)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		asm volatile(
			"maddu1	%[A],%[B]"
			:
			: [A] "%r" (a),
			  [B] "r" (b)
			: "lo", "hi"
		);
#else
		int64_t tmplo = 0;
		int64_t tmphi = 0;

		asm(
			"mflo1	%[TmpLo]\n\t"
			"mfhi1	%[TmpHi]\n\t"
			"mtlo1	%[StateLo]\n\t"
			"mthi1	%[StateHi]\n\t"
			"maddu1	%[A],%[B]\n\t"
			"mflo1	%[StateLo]\n\t"
			"mfhi1	%[StateHi]\n\t"
			"mtlo1	%[TmpLo]\n\t"
			"mthi1	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo] "+r" (state->lo[1]),
			  [StateHi] "+r" (state->hi[1])
			: [A] "%r" (a),
			  [B] "r" (b)
		);
#endif
	}

	/// @brief MADDU1 : Multiply-ADD Unsigned word pipeline 1
	/// 
	/// Multiply unsigned 32-bit values and accumulate into the LO/HI registers.
	/// 
	/// This uses the 3 operand form of the instruction. The upper part of the accumulator is
	/// loaded and combined with the low part into a 64-bit result and returned.
	/// 
	/// This function is known to be suboptimal in regards to throughput. See documentation of
	/// 'fma1_u32' for details.
	/// 
	/// This function writes to global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param a First multiplicand
	/// @param b Second multiplicand
	/// @return Struct containing bot the low and high 32 bits of the accumulator after the
	/// fused-multiply-add operation.
	FORCEINLINE mul_u32_result_t fma1_u32_finish(lohi_state_t* state, uint32_t a, uint32_t b)
	{
		mul_u32_result_t result = { 0, 0 };

		result.lo = fma1_u32_lo(state, a, b);
		result.hi = load_hi1_32(state);

		return result;
	}


	/// @brief DIV : DIVide word
	/// 
	/// Start a division. The results will be stored in the LO0/HI0 registers.
	/// 
	/// Division by 0 results in undefined values.
	/// No exception occurs.
	/// 
	/// Dividing INT_MIN by -1 results in INT_MIN as the quotient and 0 as the remainder.
	/// No overflow exception will occur.
	/// 
	/// Result signedness table:
	///		Dividend	|	Divisor	|	Quotient (LO0)	|	Remainder (HI0)
	///		+			|	+		|	+				|	+
	///		+			|	-		|	-				|	+
	///		-			|	+		|	-				|	-
	///		-			|	-		|	+				|	-
	/// 
	/// As integer division is done asynchronously on the EE Core you should issue the division
	/// instruction before checking for erroneous inputs like div-by-0 to improve throughput.
	/// Reading from the result registers (LO0/HI0) will cause a stall if the result is not
	/// ready at that point in time.
	/// The Undefined Behaviour definition of C/C++ does not allow a div-by-0 check to happen
	/// in regular code. The code snippet:
	/// 
	///		int foo(int x, int y) {
	///			int z = x / y;
	///			if (y == 0) {
	///				return 0;
	///			}
	///			return z;
	///		}
	/// 
	/// would be reduced to 'return x / y;' in all cases. This is because div-by-0 is
	/// Undefined Behaviour, so it can be assummed to not occur. This means y is never 0 at the
	/// point of the division and therefor can never be 0 after, which is when it is checked.
	/// Use this function to get around this limitation. Query the result of the operation using
	/// 'divrem0_i32_finish'.
	/// 
	/// This function writes to global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param dividend Number to divide
	/// @param divisor Number to divide by
	FORCEINLINE void divrem0_i32_start(lohi_state_t* state, int32_t dividend, int32_t divisor)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;
		// this needs an extra destination $0 register due to a quirk in the compiler
		asm volatile(
			"div	$0,%[Dividend],%[Divisor]"
			:																/* output operands	*/
			: [Dividend] "r" (dividend),									/* input operands	*/
			  [Divisor] "r" (divisor)
			: "lo", "hi"
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"mflo	%[TmpLo]\n\t"
			"mfhi	%[TmpHi]\n\t"
			"div	$0,%[Dividend],%[Divisor]\n\t"
			"mflo	%[ResLo]\n\t"
			"mfhi	%[ResHi]\n\t"
			"mtlo	%[TmpLo]\n\t"
			"mthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [ResLo] "=r" (state->lo[0]),
			  [ResHi] "=r" (state->hi[0])
			: [Dividend] "r" (dividend),
			  [Divisor] "r" (divisor)
		);
#endif
	}

	/// @brief DIV : DIVide word
	/// 
	/// Finish the division. Results will be read from LO0/HI0 registers.
	/// 
	/// See decumentation of 'divrem0_i32_start'.
	/// 
	/// This function reads global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return struct containing quotient and remainder of preceding division.
	FORCEINLINE PURE divrem_i32_result_t divrem0_i32_finish(lohi_state_t* state)
	{
		divrem_i32_result_t result = { 0, 0 };

		result.quotient = load_lo0_32(state);
		result.remainder = load_hi0_32(state);

		return result;
	}

	/// @brief DIV : DIVide word
	/// 
	/// Convenience function doing both starting and finishing a division.
	/// 
	/// Division by 0 results in undefined results.
	/// No exception occurs.
	/// 
	/// Dividing INT_MIN by -1 results in INT_MIN as the quotient and 0 as the remainder.
	/// No overflow exception will occur.
	/// 
	/// Result signedness table:
	///		Dividend	|	Divisor	|	Quotient (LO0)	|	Remainder (HI0)
	///		+			|	+		|	+				|	+
	///		+			|	-		|	-				|	+
	///		-			|	+		|	-				|	-
	///		-			|	-		|	+				|	-
	/// 
	/// This function is not optimal in regards to throughput. Refer to the documentation of
	/// 'divrem0_i32_start'.
	/// 
	/// This function writes to global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param dividend Number to divide
	/// @param divisor Number to divide by
	/// @return struct containing quotient and remainder of division.
	FORCEINLINE divrem_i32_result_t divrem0_i32(lohi_state_t* state, int32_t dividend, int32_t divisor)
	{
		divrem0_i32_start(state, dividend, divisor);
		return divrem0_i32_finish(state);
	}

	/// @brief DIVU : DIVide Unsigned word
	/// 
	/// Start a division. The results will be stored in the LO0/HI0 registers.
	/// 
	/// Division by 0 results in undefined values.
	/// No exception occurs.
	/// 
	/// As integer division is done asynchronously on the EE Core you should issue the division
	/// instruction before checking for erroneous inputs like div-by-0 to improve throughput.
	/// Reading from the result registers (LO0/HI0) will cause a stall if the result is not
	/// ready at that point in time.
	/// The Undefined Behaviour definition of C/C++ does not allow a div-by-0 check to happen
	/// in regular code. The code snippet:
	/// 
	///		unsigned foo(unsigned x, unsigned y) {
	///			unsigned z = x / y;
	///			if (y == 0) {
	///				return 0;
	///			}
	///			return z;
	///		}
	/// 
	/// would be reduced to 'return x / y;' in all cases. This is because div-by-0 is
	/// Undefined Behaviour, so it can be assummed to not occur. This means y is never 0 at the
	/// point of the division and therefor can never be 0 after, which is when it is checked.
	/// Use this function to get around this limitation. Query the result of the operation using
	/// 'divrem0_u32_finish'.
	/// 
	/// This function writes to global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param dividend Number to divide
	/// @param divisor Number to divide by
	FORCEINLINE void divrem0_u32_start(lohi_state_t* state, uint32_t dividend, uint32_t divisor)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;
		// this needs an extra destination $0 register due to a quirk in the compiler
		asm volatile(
			"divu	$0,%[Dividend],%[Divisor]"
			:																/* output operands	*/
			: [Dividend] "r" (dividend),									/* input operands	*/
			  [Divisor] "r" (divisor)
			: "lo", "hi"
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"mflo	%[TmpLo]\n\t"
			"mfhi	%[TmpHi]\n\t"
			"divu	$0,%[Dividend],%[Divisor]\n\t"
			"mflo	%[ResLo]\n\t"
			"mfhi	%[ResHi]\n\t"
			"mtlo	%[TmpLo]\n\t"
			"mthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [ResLo] "=r" (state->lo[0]),
			  [ResHi] "=r" (state->hi[0])
			: [Dividend] "r" (dividend),
			  [Divisor] "r" (divisor)
		);
#endif
	}

	/// @brief DIVU : DIVide Unsigned word
	/// 
	/// Finish the division. Results will be read from LO0/HI0 registers.
	/// 
	/// See decumentation of 'divrem0_u32_start'.
	/// 
	/// This function reads global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return struct containing quotient and remainder of preceding division.
	FORCEINLINE PURE divrem_u32_result_t divrem0_u32_finish(lohi_state_t* state)
	{
		divrem_u32_result_t result = { 0, 0 };

		result.quotient = load_lo0_32(state);
		result.remainder = load_hi0_32(state);

		return result;
	}

	/// @brief DIVU : DIVide Unsigned word
	/// 
	/// Convenience function doing both starting and finishing a division.
	/// 
	/// Division by 0 results in undefined results.
	/// No exception occurs.
	/// 
	/// This function is not optimal in regards to throughput. Refer to the documentation of
	/// 'divrem0_u32_start'.
	/// 
	/// This function writes to global state (LO0/HI0).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param dividend Number to divide
	/// @param divisor Number to divide by
	/// @return struct containing quotient and remainder of division.
	FORCEINLINE divrem_u32_result_t divrem0_u32(lohi_state_t* state, uint32_t dividend, uint32_t divisor)
	{
		divrem0_u32_start(state, dividend, divisor);
		return divrem0_u32_finish(state);
	}

	/// @brief DIV1 : Divide Word Pipeline 1
	/// 
	/// Start a division. The results will be stored in the LO1/HI1 registers.
	/// 
	/// Division by 0 results in undefined results.
	/// No exception occurs.
	/// 
	/// Dividing INT_MIN by -1 results in INT_MIN as the quotient and 0 as the remainder.
	/// No overflow exception will occur.
	/// 
	/// Result signedness table:
	///		Dividend	|	Divisor	|	Quotient (LO1)	|	Remainder (HI1)
	///		+			|	+		|	+				|	+
	///		+			|	-		|	-				|	+
	///		-			|	+		|	-				|	-
	///		-			|	-		|	+				|	-
	/// 
	/// As integer division is done asynchronously on the EE Core you should issue the division
	/// instruction before checking for erroneous inputs like div-by-0 to improve throughput.
	/// Reading from the result registers (LO1/HI1) will cause a stall if the result is not
	/// ready at that point in time.
	/// The Undefined Behaviour definition of C/C++ does not allow a div-by-0 check to happen
	/// in regular code. The code snippet:
	/// 
	///		int foo(int x, int y) {
	///			int z = x / y;
	///			if (y == 0) {
	///				return 0;
	///			}
	///			return z;
	///		}
	/// 
	/// would be reduced to 'return x / y;' in all cases. This is because div-by-0 is
	/// Undefined Behaviour, so it can be assummed to not occur. This means y is never 0 at the
	/// point of the division and therefor can never be 0 after, which is when it is checked.
	/// Use this function to get around this limitation. Query the result of the operation using
	/// 'divrem1_i32_finish'.
	/// 
	/// This function writes to global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param dividend Number to divide
	/// @param divisor Number to divide by
	FORCEINLINE void divrem1_i32_start(lohi_state_t* state, int32_t dividend, int32_t divisor)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;
		// this needs an extra destination $0 register due to a quirk in the compiler
		asm volatile(
			"div1	$0,%[Dividend],%[Divisor]"
			:																/* output operands	*/
			: [Dividend] "r" (dividend),									/* input operands	*/
			  [Divisor] "r" (divisor)
			: "lo", "hi"
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"mflo1	%[TmpLo]\n\t"
			"mfhi1	%[TmpHi]\n\t"
			"div1	$0,%[Dividend],%[Divisor]\n\t"
			"mflo1	%[ResLo]\n\t"
			"mfhi1	%[ResHi]\n\t"
			"mtlo1	%[TmpLo]\n\t"
			"mthi1	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			[TmpHi] "=&r" (tmphi),
			[ResLo] "=r" (state->lo[1]),
			[ResHi] "=r" (state->hi[1])
			: [Dividend] "r" (dividend),
			[Divisor] "r" (divisor)
		);
#endif
	}

	/// @brief DIV1 : Divide Word Pipeline 1
	/// 
	/// Finish the division. Results will be read from LO1/HI1 registers.
	/// 
	/// See decumentation of 'divrem1_i32_start'.
	/// 
	/// This function reads global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return struct containing quotient and remainder of preceding division.
	FORCEINLINE PURE divrem_i32_result_t divrem1_i32_finish(lohi_state_t* state)
	{
		divrem_i32_result_t result = { 0, 0 };

		result.quotient = load_lo1_32(state);
		result.remainder = load_hi1_32(state);

		return result;
	}

	/// @brief DIV1 : Divide Word Pipeline 1
	/// 
	/// Convenience function doing both starting and finishing a pipeline 1 division.
	/// 
	/// Division by 0 results in undefined results.
	/// No exception occurs.
	/// 
	/// Dividing INT_MIN by -1 results in INT_MIN as the quotient and 0 as the remainder.
	/// No overflow exception will occur.
	/// 
	/// Result signedness table:
	///		Dividend	|	Divisor	|	Quotient (LO1)	|	Remainder (HI1)
	///		+			|	+		|	+				|	+
	///		+			|	-		|	-				|	+
	///		-			|	+		|	-				|	-
	///		-			|	-		|	+				|	-
	/// 
	/// This function is not optimal in regards to throughput. Refer to the documentation of
	/// 'divrem1_i32_start'.
	/// 
	/// This function writes to global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param dividend Number to divide
	/// @param divisor Number to divide by
	/// @return struct containing quotient and remainder of division.
	FORCEINLINE divrem_i32_result_t divrem1_i32(lohi_state_t* state, int32_t dividend, int32_t divisor)
	{
		divrem1_i32_start(state, dividend, divisor);
		return divrem1_i32_finish(state);
	}

	/// @brief DIVU1 : DIVide Unsigned word pipeline 1
	/// 
	/// Start a division. The results will be stored in the LO1/HI1 registers.
	/// 
	/// Division by 0 results in undefined results.
	/// No exception occurs.
	/// 
	/// As integer division is done asynchronously on the EE Core you should issue the division
	/// instruction before checking for erroneous inputs like div-by-0 to improve throughput.
	/// Reading from the result registers (LO1/HI1) will cause a stall if the result is not
	/// ready at that point in time.
	/// The Undefined Behaviour definition of C/C++ does not allow a div-by-0 check to happen
	/// in regular code. The code snippet:
	/// 
	///		unsigned foo(unsigned x, unsigned y) {
	///			unsigned z = x / y;
	///			if (y == 0) {
	///				return 0;
	///			}
	///			return z;
	///		}
	/// 
	/// would be reduced to 'return x / y;' in all cases. This is because div-by-0 is
	/// Undefined Behaviour, so it can be assummed to not occur. This means y is never 0 at the
	/// point of the division and therefor can never be 0 after, which is when it is checked.
	/// Use this function to get around this limitation. Query the result of the operation using
	/// 'divrem1_i32_finish'.
	/// 
	/// This function writes to global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param dividend Number to divide
	/// @param divisor Number to divide by
	FORCEINLINE void divrem1_u32_start(lohi_state_t* state, uint32_t dividend, uint32_t divisor)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;
		// this needs an extra destination $0 register due to a quirk in the compiler
		asm volatile(
			"divu1	$0,%[Dividend],%[Divisor]"
			:																/* output operands	*/
			: [Dividend] "r" (dividend),									/* input operands	*/
			  [Divisor] "r" (divisor)
			: "lo", "hi"
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"mflo1	%[TmpLo]\n\t"
			"mfhi1	%[TmpHi]\n\t"
			"divu1	$0,%[Dividend],%[Divisor]\n\t"
			"mflo1	%[ResLo]\n\t"
			"mfhi1	%[ResHi]\n\t"
			"mtlo1	%[TmpLo]\n\t"
			"mthi1	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [ResLo] "=r" (state->lo[1]),
			  [ResHi] "=r" (state->hi[1])
			: [Dividend] "r" (dividend),
			  [Divisor] "r" (divisor)
		);
#endif
	}

	/// @brief DIVU1 : DIVide Unsigned word pipeline 1
	/// 
	/// Finish the division. Results will be read from LO1/HI1 registers.
	/// 
	/// See decumentation of 'divrem1_i32_start'.
	/// 
	/// This function reads global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return struct containing quotient and remainder of preceding division.
	FORCEINLINE PURE divrem_u32_result_t divrem1_u32_finish(lohi_state_t* state)
	{
		divrem_u32_result_t result = { 0, 0 };

		result.quotient = load_lo1_32(state);
		result.remainder = load_hi1_32(state);

		return result;
	}

	/// @brief DIVU1 : DIVide Unsigned word pipeline 1
	/// 
	/// Convenience function doing both starting and finishing a pipeline 1 division.
	/// 
	/// Division by 0 results in undefined results.
	/// No exception occurs.
	/// 
	/// This function is not optimal in regards to throughput. Refer to the documentation of
	/// 'divrem1_u32_start'.
	/// 
	/// This function writes to global state (LO1/HI1).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param dividend Number to divide
	/// @param divisor Number to divide by
	/// @return struct containing quotient and remainder of division.
	FORCEINLINE divrem_u32_result_t divrem1_u32(lohi_state_t* state, uint32_t dividend, uint32_t divisor)
	{
		divrem1_u32_start(state, dividend, divisor);
		return divrem1_u32_finish(state);
	}


	// Multimedia instructions

	/// @brief Create a 128-bit packed integer of all 0.
	/// @return 128-bit packed integer containing 16 8-bit signed integers
	FORCEINLINE CONST m128i8 mm_setzero_epi8()
	{
#ifdef PS2INTRIN_UNSAFE
		m128i8 result = { 0 };

		asm(
			"por	%[Result],$0,$0"
			: [Result] "=r" (result.v)
		);

		return result;
#else
		m128i8 result = { 0, 0 };
		return result;
#endif
	}

	/// @brief Create a 128-bit packed integer of all 0.
	/// @return 128-bit packed integer containing 16 8-bit unsigned integers
	FORCEINLINE CONST m128u8 mm_setzero_epu8()
	{
#ifdef PS2INTRIN_UNSAFE
		m128u8 result = { 0 };

		asm(
			"por	%[Result],$0,$0"
			: [Result] "=r" (result.v)
		);

		return result;
#else
		m128u8 result = { 0, 0 };
		return result;
#endif
	}

	/// @brief Create a 128-bit packed integer of all 0.
	/// @return 128-bit packed integer containing 8 16-bit signed integers
	FORCEINLINE CONST m128i16 mm_setzero_epi16()
	{
#ifdef PS2INTRIN_UNSAFE
		m128i16 result = { 0 };

		asm(
			"por	%[Result],$0,$0"
			: [Result] "=r" (result.v)
		);

		return result;
#else
		m128i16 result = { 0, 0 };
		return result;
#endif
	}

	/// @brief Create a 128-bit packed integer of all 0.
	/// @return 128-bit packed integer containing 8 16-bit unsigned integers
	FORCEINLINE CONST m128u16 mm_setzero_epu16()
	{
#ifdef PS2INTRIN_UNSAFE
		m128u16 result = { 0 };

		asm(
			"por	%[Result],$0,$0"
			: [Result] "=r" (result.v)
		);

		return result;
#else
		m128u16 result = { 0, 0 };
		return result;
#endif
	}

	/// @brief Create a 128-bit packed integer of all 0.
	/// @return 128-bit packed integer containing 4 32-bit signed integers
	FORCEINLINE CONST m128i32 mm_setzero_epi32()
	{
#ifdef PS2INTRIN_UNSAFE
		m128i32 result = { 0 };

		asm(
			"por	%[Result],$0,$0"
			: [Result] "=r" (result.v)
		);

		return result;
#else
		m128i32 result = { 0, 0 };
		return result;
#endif
	}

	/// @brief Create a 128-bit packed integer of all 0.
	/// @return 128-bit packed integer containing 4 32-bit unsigned integers
	FORCEINLINE CONST m128u32 mm_setzero_epu32()
	{
#ifdef PS2INTRIN_UNSAFE
		m128u32 result = { 0 };

		asm(
			"por	%[Result],$0,$0"
			: [Result] "=r" (result.v)
		);

		return result;
#else
		m128u32 result = { 0, 0 };
		return result;
#endif
	}

	/// @brief Create a 128-bit packed integer of all 0.
	/// @return 128-bit packed integer containing 2 64-bit signed integers
	FORCEINLINE CONST m128i64 mm_setzero_epi64()
	{
#ifdef PS2INTRIN_UNSAFE
		m128i64 result = { 0 };

		asm(
			"por	%[Result],$0,$0"
			: [Result] "=r" (result.v)
		);

		return result;
#else
		m128i64 result = { 0, 0 };
		return result;
#endif
	}

	/// @brief Create a 128-bit packed integer of all 0.
	/// @return 128-bit packed integer containing 2 64-bit unsigned integers
	FORCEINLINE CONST m128u64 mm_setzero_epu64()
	{
#ifdef PS2INTRIN_UNSAFE
		m128u64 result = { 0 };

		asm(
			"por	%[Result],$0,$0"
			: [Result] "=r" (result.v)
		);

		return result;
#else
		m128u64 result = { 0, 0 };
		return result;
#endif
	}

	/// @brief Create a 128-bit packed integer of all 0.
	/// @return 128-bit packed integer containing 1 128-bit signed integers
	FORCEINLINE CONST m128i128 mm_setzero_epi128()
	{
#ifdef PS2INTRIN_UNSAFE
		m128i128 result = { 0 };

		asm(
			"por	%[Result],$0,$0"
			: [Result] "=r" (result.v)
		);

		return result;
#else
		m128i128 result = { 0, 0 };
		return result;
#endif
	}

	/// @brief Create a 128-bit packed integer of all 0.
	/// @return 128-bit packed integer containing 1 128-bit unsigned integers
	FORCEINLINE CONST m128u128 mm_setzero_epu128()
	{
#ifdef PS2INTRIN_UNSAFE
		m128u128 result = { 0 };

		asm(
			"por	%[Result],$0,$0"
			: [Result] "=r" (result.v)
		);

		return result;
#else
		m128u128 result = { 0, 0 };
		return result;
#endif
	}

	// LQ
#ifdef PS2INTRIN_UNSAFE
	#define LQ(result, address)																		\
	asm(																							\
		"lq	%[Result],%[Address]"																	\
		: [Result] "=r" ((result).v)					/*	output operands	*/						\
		: [Address] "o" (*(const char (*)[16]) address)	/*	input operands	Tell GCC that this is	\
																reading 16 bytes from *address	*/	\
	)
#else
	#define LQ(result, address)																		\
	asm(																							\
		"lq	%[ResultLo],%[Address]\n\t"																\
		"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"												\
		: [ResultLo] "=r" ((result).lo),				/*	output operands	*/						\
		  [ResultHi] "=r" ((result).hi)																\
		: [Address] "o" (*(const char (*)[16]) address)	/*	input operands	Tell GCC that this is	\
																reading 16 bytes from *address	*/	\
	)
#endif

	/// @brief LQ : Load Quadword
	/// 
	/// Load 16 packed signed 8-bit integers from memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, loading unintended values.
	/// 
	/// This function reads global state (*'p')
	/// @param p Memory location to load integer data from.
	/// @return Packed integer data loaded from given memory location.
	FORCEINLINE UNSEQUENCED m128i8 mm_load_epi8(const m128i8* p)
	{
		m128i8 result = {};

		LQ(result, p);

		return result;
	}

	/// @brief LQ : Load Quadword
	/// 
	/// Load 16 packed unsigned 8-bit integers from memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, loading unintended values.
	/// 
	/// This function reads global state (*'p')
	/// @param p Memory location to load integer data from.
	/// @return Packed integer data loaded from given memory location.
	FORCEINLINE UNSEQUENCED m128u8 mm_load_epu8(const m128u8* p)
	{
		m128u8 result = {};

		LQ(result, p);

		return result;
	}

	/// @brief LQ : Load Quadword
	/// 
	/// Load 8 packed signed 16-bit integers from memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, loading unintended values.
	/// 
	/// This function reads global state (*'p')
	/// @param p Memory location to load integer data from.
	/// @return Packed integer data loaded from given memory location.
	FORCEINLINE UNSEQUENCED m128i16 mm_load_epi16(const m128i16* p)
	{
		m128i16 result = {};

		LQ(result, p);

		return result;
	}

	/// @brief LQ : Load Quadword
	/// 
	/// Load 8 packed unsigned 16-bit integers from memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, loading unintended values.
	/// 
	/// This function reads global state (*'p')
	/// @param p Memory location to load integer data from.
	/// @return Packed integer data loaded from given memory location.
	FORCEINLINE UNSEQUENCED m128u16 mm_load_epu16(const m128u16* p)
	{
		m128u16 result = {};

		LQ(result, p);

		return result;
	}

	/// @brief LQ : Load Quadword
	/// 
	/// Load 4 packed signed 32-bit integers from memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, loading unintended values.
	/// 
	/// This function reads global state (*'p')
	/// @param p Memory location to load integer data from.
	/// @return Packed integer data loaded from given memory location.
	FORCEINLINE UNSEQUENCED m128i32 mm_load_epi32(const m128i32* p)
	{
		m128i32 result = {};

		LQ(result, p);

		return result;
	}

	/// @brief LQ : Load Quadword
	/// 
	/// Load 4 packed unsigned 32-bit integers from memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, loading unintended values.
	/// 
	/// This function reads global state (*'p')
	/// @param p Memory location to load integer data from.
	/// @return Packed integer data loaded from given memory location.
	FORCEINLINE UNSEQUENCED m128u32 mm_load_epu32(const m128u32* p)
	{
		m128u32 result = {};

		LQ(result, p);

		return result;
	}

	/// @brief LQ : Load Quadword
	/// 
	/// Load 2 packed signed 64-bit integers from memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, loading unintended values.
	/// 
	/// This function reads global state (*'p')
	/// @param p Memory location to load integer data from.
	/// @return Packed integer data loaded from given memory location.
	FORCEINLINE UNSEQUENCED m128i64 mm_load_epi64(const m128i64* p)
	{
		m128i64 result = {};

		LQ(result, p);

		return result;
	}

	/// @brief LQ : Load Quadword
	/// 
	/// Load 2 packed unsigned 64-bit integers from memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, loading unintended values.
	/// 
	/// This function reads global state (*'p')
	/// @param p Memory location to load integer data from.
	/// @return Packed integer data loaded from given memory location.
	FORCEINLINE UNSEQUENCED m128u64 mm_load_epu64(const m128u64* p)
	{
		m128u64 result = {};

		LQ(result, p);

		return result;
	}

	/// @brief LQ : Load Quadword
	/// 
	/// Load 1 signed 128-bit integer from memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, loading unintended values.
	/// 
	/// This function reads global state (*'p')
	/// @param p Memory location to load integer data from.
	/// @return Packed integer data loaded from given memory location.
	FORCEINLINE UNSEQUENCED m128i128 mm_load_epi128(const m128i128* p)
	{
		m128i128 result = {};

		LQ(result, p);

		return result;
	}

	/// @brief LQ : Load Quadword
	/// 
	/// Load 1 unsigned 128-bit integer from memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, loading unintended values.
	/// 
	/// This function reads global state (*'p')
	/// @param p Memory location to load integer data from.
	/// @return Packed integer data loaded from given memory location.
	FORCEINLINE UNSEQUENCED m128u128 mm_load_epu128(const m128u128* p)
	{
		m128u128 result = {};

		LQ(result, p);

		return result;
	}

#undef LQ

	/// @brief LQ : Load Quadword
	/// 
	/// Load 1 int128_t from memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, loading unintended values.
	/// 
	/// This function reads global state (*'p')
	/// @param p Memory location to load integer data from.
	/// @return Packed integer data loaded from given memory location.
	FORCEINLINE UNSEQUENCED int128_t mm_load_i128(const int128_t* p)
	{
		uint64_t lo = 0;
		uint64_t hi = 0;

		asm(
			"lq	%[ResultLo],%[Address]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (lo),							/*	output operands	*/
			  [ResultHi] "=r" (hi)
			: [Address] "o" (*(const char (*)[16]) p)		/*	input operands	Tell GCC that this is
																	reading 16 bytes from *address	*/
		);

		uint128_t result = hi;
		result <<= 64;
		result |= lo;

		return (int128_t)result;
	}

	/// @brief LQ : Load Quadword
	/// 
	/// Load 1 uint128_t from memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, loading unintended values.
	/// 
	/// This function reads global state (*'p')
	/// @param p Memory location to load integer data from.
	/// @return Packed integer data loaded from given memory location.
	FORCEINLINE UNSEQUENCED uint128_t mm_load_u128(const uint128_t* p)
	{
		uint64_t lo = 0;
		uint64_t hi = 0;

		asm(
			"lq	%[ResultLo],%[Address]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (lo),							/*	output operands	*/
			  [ResultHi] "=r" (hi)
			: [Address] "o" (*(const char (*)[16]) p)		/*	input operands	Tell GCC that this is
																	reading 16 bytes from *address	*/
		);

		uint128_t result = hi;
		result <<= 64;
		result |= lo;

		return result;
	}


// SQ
#ifdef PS2INTRIN_UNSAFE
	#define SQ(address, value)																		\
	asm(																							\
		"sq	%[Value],%[Address]"																	\
		: [Address] "=o" (*(char (*)[16]) address)		/*	output operands	Tell GCC that this is	\
															writing 16 bytes to *address	*/		\
		: [Value] "r" ((value).v)						/*	input operands	*/						\
	)
#else
	#define SQ(address, value)																		\
	asm(																							\
		"pcpyld	%[ValueLo],%[ValueHi],%[ValueLo]\n\t"												\
		"sq	%[ValueLo],%[Address]"																	\
		: [Address] "=o" (*(char (*)[16]) address)		/*	output operands	Tell GCC that this is	\
															writing 16 bytes to *address	*/		\
		: [ValueLo] "r" ((value).lo),					/*	input operands	*/						\
		  [ValueHi] "r" ((value).hi)																\
	)
#endif

	/// @brief SQ : Store Quadword
	/// 
	/// Store 16 packed signed 8-bit integers to memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, storing to an unintended address.
	/// 
	/// This function writes to global state (*'address')
	/// @param p Address to store to.
	/// @return Packed integer data to store to the memory location.
	FORCEINLINE UNSEQUENCED void mm_store_epi8(m128i8* p, m128i8 value)
	{
		SQ(p, value);
	}

	/// @brief SQ : Store Quadword
	/// 
	/// Store 16 packed unsigned 8-bit integers to memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, storing to an unintended address.
	/// 
	/// This function writes to global state (*'address')
	/// @param p Address to store to.
	/// @return Packed integer data to store to the memory location.
	FORCEINLINE UNSEQUENCED void mm_store_epu8(m128u8* p, m128u8 value)
	{
		SQ(p, value);
	}

	/// @brief SQ : Store Quadword
	/// 
	/// Store 8 packed signed 16-bit integers to memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, storing to an unintended address.
	/// 
	/// This function writes to global state (*'address')
	/// @param p Address to store to.
	/// @return Packed integer data to store to the memory location.
	FORCEINLINE UNSEQUENCED void mm_store_epi16(m128i16* p, m128i16 value)
	{
		SQ(p, value);
	}

	/// @brief SQ : Store Quadword
	/// 
	/// Store 8 packed unsigned 16-bit integers to memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, storing to an unintended address.
	/// 
	/// This function writes to global state (*'address')
	/// @param p Address to store to.
	/// @return Packed integer data to store to the memory location.
	FORCEINLINE UNSEQUENCED void mm_store_epu16(m128u16* p, m128u16 value)
	{
		SQ(p, value);
	}

	/// @brief SQ : Store Quadword
	/// 
	/// Store 4 packed signed 32-bit integers to memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, storing to an unintended address.
	/// 
	/// This function writes to global state (*'address')
	/// @param p Address to store to.
	/// @return Packed integer data to store to the memory location.
	FORCEINLINE UNSEQUENCED void mm_store_epi32(m128i32* p, m128i32 value)
	{
		SQ(p, value);
	}

	/// @brief SQ : Store Quadword
	/// 
	/// Store 4 packed unsigned 32-bit integers to memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, storing to an unintended address.
	/// 
	/// This function writes to global state (*'address')
	/// @param p Address to store to.
	/// @return Packed integer data to store to the memory location.
	FORCEINLINE UNSEQUENCED void mm_store_epu32(m128u32* p, m128u32 value)
	{
		SQ(p, value);
	}

	/// @brief SQ : Store Quadword
	/// 
	/// Store 2 packed signed 64-bit integers to memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, storing to an unintended address.
	/// 
	/// This function writes to global state (*'address')
	/// @param p Address to store to.
	/// @return Packed integer data to store to the memory location.
	FORCEINLINE UNSEQUENCED void mm_store_epi64(m128i64* p, m128i64 value)
	{
		SQ(p, value);
	}

	/// @brief SQ : Store Quadword
	/// 
	/// Store 2 packed unsigned 64-bit integers to memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, storing to an unintended address.
	/// 
	/// This function writes to global state (*'address')
	/// @param p Address to store to.
	/// @return Packed integer data to store to the memory location.
	FORCEINLINE UNSEQUENCED void mm_store_epu64(m128u64* p, m128u64 value)
	{
		SQ(p, value);
	}

	/// @brief SQ : Store Quadword
	/// 
	/// Store 1 packed signed 128-bit integers to memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, storing to an unintended address.
	/// 
	/// This function writes to global state (*'address')
	/// @param p Address to store to.
	/// @return Packed integer data to store to the memory location.
	FORCEINLINE UNSEQUENCED void mm_store_epi128(m128i128* p, m128i128 value)
	{
		SQ(p, value);
	}

	/// @brief SQ : Store Quadword
	/// 
	/// Store 1 packed unsigned 128-bit integers to memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, storing to an unintended address.
	/// 
	/// This function writes to global state (*'address')
	/// @param p Address to store to.
	/// @return Packed integer data to store to the memory location.
	FORCEINLINE UNSEQUENCED void mm_store_epu128(m128u128* p, m128u128 value)
	{
		SQ(p, value);
	}

#undef SQ

	/// @brief SQ : Store Quadword
	/// 
	/// Store 1 int128_t to memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, storing to an unintended address.
	/// 
	/// This function writes to global state (*'address')
	/// @param p Address to store to.
	/// @return Packed integer data to store to the memory location.
	FORCEINLINE UNSEQUENCED void mm_store_i128(int128_t* p, int128_t value)
	{
		asm(
			"pcpyld	%[ValueLo],%[ValueHi],%[ValueLo]\n\t"
			"sq	%[ValueLo],%[Address]"
			: [Address] "=o" (*(char (*)[16]) p)			/*	output operands	Tell GCC that this is
																writing 16 bytes to *address	*/
			: [ValueLo] "r" ((uint64_t)value),					/*	input operands	*/
			  [ValueHi] "r" ((uint64_t)(value >> 64))
		);
	}

	/// @brief SQ : Store Quadword
	/// 
	/// Store 1 uint128_t to memory.
	/// 
	/// The memory location must aligned on a 16-byte boundary. Otherwise the next 16-byte boundary
	/// below the given memory location is used instead, storing to an unintended address.
	/// 
	/// This function writes to global state (*'address')
	/// @param p Address to store to.
	/// @return Packed integer data to store to the memory location.
	FORCEINLINE UNSEQUENCED void mm_store_u128(uint128_t* p, uint128_t value)
	{
		asm(
			"pcpyld	%[ValueLo],%[ValueHi],%[ValueLo]\n\t"
			"sq	%[ValueLo],%[Address]"
			: [Address] "=o" (*(char (*)[16]) p)			/*	output operands	Tell GCC that this is
																writing 16 bytes to *address	*/
			: [ValueLo] "r" ((uint64_t)value),					/*	input operands	*/
			  [ValueHi] "r" ((uint64_t)(value >> 64))
		);
	}


	/// @brief Set a 128-bit packed integer type with given values.
	/// 
	/// This is a convenience function and corresponds to a multitude of instructions.
	/// 
	/// Ordering of the arguments is where they would be as if loaded from memory in order of their
	/// name. So the code:
	/// 
	///		int8_t arr[16] = { r0, r1, r2, ... , r15 };
	///		result = mm_load_epi8(arr);
	/// 
	/// would result in the same initialized packed integer type. (Omitting alignment/casting).
	/// @param r15 Signed 8-bit value to put in the highest position of the result
	/// @param r14 
	/// @param r13 
	/// @param r12 
	/// @param r11 
	/// @param r10 
	/// @param r9 
	/// @param r8 
	/// @param r7 
	/// @param r6 
	/// @param r5 
	/// @param r4 
	/// @param r3 
	/// @param r2 
	/// @param r1 
	/// @param r0 Signed 8-bit value to put in the lowest position of the result
	/// @return Packed integer type initialized to the given values
	FORCEINLINE CONST m128i8 mm_set_epi8(int8_t r15, int8_t r14, int8_t r13, int8_t r12, int8_t r11, int8_t r10, int8_t r9, int8_t r8,
										 int8_t r7, int8_t r6, int8_t r5, int8_t r4, int8_t r3, int8_t r2, int8_t r1, int8_t r0)
	{
		m128i8 result = {};

		uint64_t lo = r7  & 0xFF;
		uint64_t hi = r15 & 0xFF;
		lo <<= 8;
		lo |= r6  & 0xFF;
		hi <<= 8;
		hi |= r14 & 0xFF;
		lo <<= 8;
		lo |= r5  & 0xFF;
		hi <<= 8;
		hi |= r13 & 0xFF;
		lo <<= 8;
		lo |= r4  & 0xFF;
		hi <<= 8;
		hi |= r12 & 0xFF;
		lo <<= 8;
		lo |= r3  & 0xFF;
		hi <<= 8;
		hi |= r11 & 0xFF;
		lo <<= 8;
		lo |= r2  & 0xFF;
		hi <<= 8;
		hi |= r10 & 0xFF;
		lo <<= 8;
		lo |= r1  & 0xFF;
		hi <<= 8;
		hi |= r9  & 0xFF;
		lo <<= 8;
		lo |= r0  & 0xFF;
		hi <<= 8;
		hi |= r8  & 0xFF;

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pcpyld	%[Result],%[Upper],%[Lower]"
			: [Result] "=r" (result.v)
			: [Upper] "r" (hi), [Lower] "r" (lo)
		);
#else
		result.lo = lo;
		result.hi = hi;
#endif

		return result;
	}

	/// @brief Set a 128-bit packed integer type with given values.
	/// 
	/// This is a convenience function and corresponds to a multitude of instructions.
	/// 
	/// Ordering of the arguments is where they would be as if loaded from memory in order of their
	/// name. So the code:
	/// 
	///		uint8_t arr[16] = { r0, r1, r2, ... , r15 };
	///		result = mm_load_epu8(arr);
	/// 
	/// would result in the same initialized packed integer type. (Omitting alignment/casting).
	/// @param r15 Unsigned 8-bit value to put in the highest position of the result
	/// @param r14 
	/// @param r13 
	/// @param r12 
	/// @param r11 
	/// @param r10 
	/// @param r9 
	/// @param r8 
	/// @param r7 
	/// @param r6 
	/// @param r5 
	/// @param r4 
	/// @param r3 
	/// @param r2 
	/// @param r1 
	/// @param r0 Unsigned 8-bit value to put in the lowest position of the result
	/// @return Packed integer type initialized to the given values
	FORCEINLINE CONST m128u8 mm_set_epu8(uint8_t r15, uint8_t r14, uint8_t r13, uint8_t r12, uint8_t r11, uint8_t r10, uint8_t r9, uint8_t r8,
										 uint8_t r7, uint8_t r6, uint8_t r5, uint8_t r4, uint8_t r3, uint8_t r2, uint8_t r1, uint8_t r0)
	{
		m128u8 result = {};

		uint64_t lo = r7  & 0xFF;
		uint64_t hi = r15 & 0xFF;
		lo <<= 8;
		lo |= r6  & 0xFF;
		hi <<= 8;
		hi |= r14 & 0xFF;
		lo <<= 8;
		lo |= r5  & 0xFF;
		hi <<= 8;
		hi |= r13 & 0xFF;
		lo <<= 8;
		lo |= r4  & 0xFF;
		hi <<= 8;
		hi |= r12 & 0xFF;
		lo <<= 8;
		lo |= r3  & 0xFF;
		hi <<= 8;
		hi |= r11 & 0xFF;
		lo <<= 8;
		lo |= r2  & 0xFF;
		hi <<= 8;
		hi |= r10 & 0xFF;
		lo <<= 8;
		lo |= r1  & 0xFF;
		hi <<= 8;
		hi |= r9  & 0xFF;
		lo <<= 8;
		lo |= r0  & 0xFF;
		hi <<= 8;
		hi |= r8  & 0xFF;

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pcpyld	%[Result],%[Upper],%[Lower]"
			: [Result] "=r" (result.v)
			: [Upper] "r" (hi), [Lower] "r" (lo)
		);
#else
		result.lo = lo;
		result.hi = hi;
#endif

		return result;
	}

	/// @brief Set a 128-bit packed integer type with given values.
	/// 
	/// This is a convenience function and corresponds to a multitude of instructions.
	/// 
	/// Ordering of the arguments is where they would be as if loaded from memory in order of their
	/// name. So the code:
	/// 
	///		int16_t arr[8] = { r0, r1, r2, ... , r7 };
	///		result = mm_load_epi16(arr);
	/// 
	/// would result in the same initialized packed integer type. (Omitting alignment/casting).
	/// @param r7 Signed 16-bit value to put in the highest position of the result
	/// @param r6 
	/// @param r5 
	/// @param r4 
	/// @param r3 
	/// @param r2 
	/// @param r1 
	/// @param r0 Signed 16-bit value to put in the lowest position of the result
	/// @return Packed integer type initialized to the given values
	FORCEINLINE CONST m128i16 mm_set_epi16(int16_t r7, int16_t r6, int16_t r5, int16_t r4, int16_t r3, int16_t r2, int16_t r1, int16_t r0)
	{
		m128i16 result = {};

		uint64_t lo = r3 & 0xFFFF;
		uint64_t hi = r7 & 0xFFFF;
		lo <<= 16;
		lo |= r2 & 0xFFFF;
		hi <<= 16;
		hi |= r6 & 0xFFFF;
		lo <<= 16;
		lo |= r1 & 0xFFFF;
		hi <<= 16;
		hi |= r5 & 0xFFFF;
		lo <<= 16;
		lo |= r0 & 0xFFFF;
		hi <<= 16;
		hi |= r4 & 0xFFFF;

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pcpyld	%[Result],%[Upper],%[Lower]"
			: [Result] "=r" (result.v)
			: [Upper] "r" (hi), [Lower] "r" (lo)
		);
#else
		result.lo = lo;
		result.hi = hi;
#endif

		return result;
	}

	/// @brief Set a 128-bit packed integer type with given values.
	/// 
	/// This is a convenience function and corresponds to a multitude of instructions.
	/// 
	/// Ordering of the arguments is where they would be as if loaded from memory in order of their
	/// name. So the code:
	/// 
	///		uint16_t arr[8] = { r0, r1, r2, ... , r7 };
	///		result = mm_load_epu16(arr);
	/// 
	/// would result in the same initialized packed integer type. (Omitting alignment/casting).
	/// @param r7 Unsigned 16-bit value to put in the highest position of the result
	/// @param r6 
	/// @param r5 
	/// @param r4 
	/// @param r3 
	/// @param r2 
	/// @param r1 
	/// @param r0 Unsigned 16-bit value to put in the lowest position of the result
	/// @return Packed integer type initialized to the given values
	FORCEINLINE CONST m128u16 mm_set_epu16(uint16_t r7, uint16_t r6, uint16_t r5, uint16_t r4, uint16_t r3, uint16_t r2, uint16_t r1, uint16_t r0)
	{
		m128u16 result = {};

		uint64_t lo = r3 & 0xFFFF;
		uint64_t hi = r7 & 0xFFFF;
		lo <<= 16;
		lo |= r2 & 0xFFFF;
		hi <<= 16;
		hi |= r6 & 0xFFFF;
		lo <<= 16;
		lo |= r1 & 0xFFFF;
		hi <<= 16;
		hi |= r5 & 0xFFFF;
		lo <<= 16;
		lo |= r0 & 0xFFFF;
		hi <<= 16;
		hi |= r4 & 0xFFFF;

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pcpyld	%[Result],%[Upper],%[Lower]"
			: [Result] "=r" (result.v)
			: [Upper] "r" (hi), [Lower] "r" (lo)
		);
#else
		result.lo = lo;
		result.hi = hi;
#endif

		return result;
	}

	/// @brief Set a 128-bit packed integer type with given values.
	/// 
	/// This is a convenience function and corresponds to a multitude of instructions.
	/// 
	/// Ordering of the arguments is where they would be as if loaded from memory in order of their
	/// name. So the code:
	/// 
	///		int32_t arr[16] = { r0, r1, r2, r3 };
	///		result = mm_load_epi32(arr);
	/// 
	/// would result in the same initialized packed integer type. (Omitting alignment/casting).
	/// @param r3 Signed 32-bit value to put in the highest position of the result
	/// @param r2 
	/// @param r1 
	/// @param r0 Signed 32-bit value to put in the lowest position of the result
	/// @return Packed integer type initialized to the given values
	FORCEINLINE CONST m128i32 mm_set_epi32(int32_t r3, int32_t r2, int32_t r1, int32_t r0)
	{
		m128i32 result = {};

		uint64_t lo = r1 & 0xFFFFFFFF;
		uint64_t hi = r3 & 0xFFFFFFFF;
		lo <<= 32;
		lo |= r0 & 0xFFFFFFFF;
		hi <<= 32;
		hi |= r2 & 0xFFFFFFFF;

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pcpyld	%[Result],%[Upper],%[Lower]"
			: [Result] "=r" (result.v)
			: [Upper] "r" (hi), [Lower] "r" (lo)
		);
#else
		result.lo = lo;
		result.hi = hi;
#endif

		return result;
	}

	/// @brief Set a 128-bit packed integer type with given values.
	/// 
	/// This is a convenience function and corresponds to a multitude of instructions.
	/// 
	/// Ordering of the arguments is where they would be as if loaded from memory in order of their
	/// name. So the code:
	/// 
	///		uint32_t arr[4] = { r0, r1, r2, r3 };
	///		result = mm_load_epu32(arr);
	/// 
	/// would result in the same initialized packed integer type. (Omitting alignment/casting).
	/// @param r3 Unsigned 32-bit value to put in the highest position of the result
	/// @param r2 
	/// @param r1 
	/// @param r0 Unsigned 32-bit value to put in the lowest position of the result
	/// @return Packed integer type initialized to the given values
	FORCEINLINE CONST m128u32 mm_set_epu32(uint32_t r3, uint32_t r2, uint32_t r1, uint32_t r0)
	{
		m128u32 result = {};

		uint64_t lo = r1 & 0xFFFFFFFF;
		uint64_t hi = r3 & 0xFFFFFFFF;
		lo <<= 32;
		lo |= r0 & 0xFFFFFFFF;
		hi <<= 32;
		hi |= r2 & 0xFFFFFFFF;

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pcpyld	%[Result],%[Upper],%[Lower]"
			: [Result] "=r" (result.v)
			: [Upper] "r" (hi), [Lower] "r" (lo)
		);
#else
		result.lo = lo;
		result.hi = hi;
#endif

		return result;
	}

	/// @brief Set a 128-bit packed integer type with given values.
	/// 
	/// This is a convenience function and corresponds to a multitude of instructions.
	/// 
	/// Ordering of the arguments is where they would be as if loaded from memory in order of their
	/// name. So the code:
	/// 
	///		int64_t arr[2] = { r0, r1 };
	///		result = mm_load_epi64(arr);
	/// 
	/// would result in the same initialized packed integer type. (Omitting alignment/casting).
	/// @param r1 Signed 64-bit value to put in the highest position of the result
	/// @param r0 Signed 64-bit value to put in the lowest position of the result
	/// @return Packed integer type initialized to the given values
	FORCEINLINE CONST m128i64 mm_set_epi64(int64_t r1, int64_t r0)
	{
		m128i64 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pcpyld	%[Result],%[Upper],%[Lower]"
			: [Result] "=r" (result.v)
			: [Upper] "r" (r1), [Lower] "r" (r0)
		);
#else
		result.lo = r0;
		result.hi = r1;
#endif

		return result;
	}

	/// @brief Set a 128-bit packed integer type with given values.
	/// 
	/// This is a convenience function and corresponds to a multitude of instructions.
	/// 
	/// Ordering of the arguments is where they would be as if loaded from memory in order of their
	/// name. So the code:
	/// 
	///		uint64_t arr[2] = { r0, r1 };
	///		result = mm_load_epu64(arr);
	/// 
	/// would result in the same initialized packed integer type. (Omitting alignment/casting).
	/// @param r1 Unsigned 64-bit value to put in the highest position of the result
	/// @param r0 Unsigned 64-bit value to put in the lowest position of the result
	/// @return Packed integer type initialized to the given values
	FORCEINLINE CONST m128u64 mm_set_epu64(uint64_t r1, uint64_t r0)
	{
		m128u64 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pcpyld	%[Result],%[Upper],%[Lower]"
			: [Result] "=r" (result.v)
			: [Upper] "r" (r1), [Lower] "r" (r0)
		);
#else
		result.lo = r0;
		result.hi = r1;
#endif

		return result;
	}

	/// @brief Set a 128-bit packed integer type with given values.
	/// 
	/// This is a convenience function. Note that qword_t is convertible to int128_t.
	/// @param r0 Signed 128-bit value to put in the result
	/// @return Packed integer type initialized to the given values
	FORCEINLINE CONST m128i128 mm_set_epi128(int128_t r0)
	{
		m128i128 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pcpyld	%[Result],%[Upper],%[Lower]"
			: [Result] "=r" (result.v)
			: [Upper] "r" ((uint64_t)(r0 >> 64)), [Lower] "r" ((uint64_t)(r0))
		);
#else
		result.lo = r0;
		result.hi = r0 >> 64;
#endif

		return result;
	}

	/// @brief Set a 128-bit packed integer type with given values.
	/// 
	/// This is a convenience function. Note that qword_t is convertible to uint128_t.
	/// @param r0 Unsigned 128-bit value to put in the result
	/// @return Packed integer type initialized to the given values
	FORCEINLINE CONST m128u128 mm_set_epu128(uint128_t r0)
	{
		m128u128 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pcpyld	%[Result],%[Upper],%[Lower]"
			: [Result] "=r" (result.v)
			: [Upper] "r" ((uint64_t)(r0 >> 64)), [Lower] "r" ((uint64_t)(r0))
		);
#else
		result.lo = r0;
		result.hi = r0 >> 64;
#endif

		return result;
	}
	

	// CAST
#ifdef PS2INTRIN_UNSAFE
	/// Conversion functions
	///
	/// All of these functions are no-ops and follow the naming convention 'mm_cast<To>_<From>'
	#define CAST(NameTo, NameFrom)																	\
	FORCEINLINE CONST m128 ## NameTo mm_castep ## NameTo ## _ep ## NameFrom (m128 ## NameFrom v) {	\
		m128 ## NameTo result = { v.v };																\
		return result;																				\
	}
#else
	/// Conversion functions
	///
	/// All of these functions are no-ops and follow the naming convention 'mm_cast<To>_<From>'
	#define CAST(NameTo, NameFrom)																	\
	FORCEINLINE CONST m128 ## NameTo mm_castep ## NameTo ## _ep ## NameFrom (m128 ## NameFrom v) {	\
		m128 ## NameTo result = { v.lo, v.hi };														\
		return result;																				\
	}
#endif

	CAST(u8, i8);
	CAST(i16, i8);
	CAST(u16, i8);
	CAST(i32, i8);
	CAST(u32, i8);
	CAST(i64, i8);
	CAST(u64, i8);
	CAST(i128, i8);
	CAST(u128, i8);

	CAST(i8, u8);
	CAST(i16, u8);
	CAST(u16, u8);
	CAST(i32, u8);
	CAST(u32, u8);
	CAST(i64, u8);
	CAST(u64, u8);
	CAST(i128, u8);
	CAST(u128, u8);

	CAST(i8, i16);
	CAST(u8, i16);
	CAST(u16, i16);
	CAST(i32, i16);
	CAST(u32, i16);
	CAST(i64, i16);
	CAST(u64, i16);
	CAST(i128, i16);
	CAST(u128, i16);

	CAST(i8, u16);
	CAST(u8, u16);
	CAST(i16, u16);
	CAST(i32, u16);
	CAST(u32, u16);
	CAST(i64, u16);
	CAST(u64, u16);
	CAST(i128, u16);
	CAST(u128, u16);

	CAST(i8, i32);
	CAST(u8, i32);
	CAST(i16, i32);
	CAST(u16, i32);
	CAST(u32, i32);
	CAST(i64, i32);
	CAST(u64, i32);
	CAST(i128, i32);
	CAST(u128, i32);

	CAST(i8, u32);
	CAST(u8, u32);
	CAST(i16, u32);
	CAST(u16, u32);
	CAST(i32, u32);
	CAST(i64, u32);
	CAST(u64, u32);
	CAST(i128, u32);
	CAST(u128, u32);

	CAST(i8, i64);
	CAST(u8, i64);
	CAST(i16, i64);
	CAST(u16, i64);
	CAST(i32, i64);
	CAST(u32, i64);
	CAST(u64, i64);
	CAST(i128, i64);
	CAST(u128, i64);

	CAST(i8, u64);
	CAST(u8, u64);
	CAST(i16, u64);
	CAST(u16, u64);
	CAST(i32, u64);
	CAST(u32, u64);
	CAST(i64, u64);
	CAST(i128, u64);
	CAST(u128, u64);

	CAST(i8, i128);
	CAST(u8, i128);
	CAST(i16, i128);
	CAST(u16, i128);
	CAST(i32, i128);
	CAST(u32, i128);
	CAST(i64, i128);
	CAST(u64, i128);
	CAST(u128, i128);

	CAST(i8, u128);
	CAST(u8, u128);
	CAST(i16, u128);
	CAST(u16, u128);
	CAST(i32, u128);
	CAST(u32, u128);
	CAST(i64, u128);
	CAST(u64, u128);
	CAST(i128, u128);

#undef CAST

	/// @brief Broadcast Byte
	/// 
	/// Broadcast an 8-bit value to all 16 positions in a 128-bit value.
	/// @param v 8-bit value to broadcast
	/// @return 128-value with broadcasted values
	FORCEINLINE PURE m128i8 mm_broadcast_epi8(int8_t v)
	{
#ifdef PS2INTRIN_UNSAFE
		m128i8 result = {};

		asm(
			"pextlb	%[Result],%[Value],%[Value]\n\t"	/* double byte to fill lower halfword		*/
			"pcpyld	%[Result],%[Result],%[Result]\n\t"	/* put lower halfword in both doublewords	*/
			"pcpyh	%[Result],%[Result]"				/* broadcast lower halfword in both			*/
			: [Result] "=r" (result.v)
			: [Value] "r" (v)
		);

		return result;
#else
		return mm_set_epi8(v, v, v, v, v, v, v, v, v, v, v, v, v, v, v, v);
#endif
	}

	/// @brief Broadcast Byte
	/// 
	/// Broadcast an 8-bit value to all 16 positions in a 128-bit value.
	/// @param v 8-bit value to broadcast
	/// @return 128-value with broadcasted values
	FORCEINLINE PURE m128u8 mm_broadcast_epu8(uint8_t v)
	{
		return mm_castepu8_epi8(mm_broadcast_epi8((int8_t)v));
	}

	/// @brief Broadcast Halfword
	/// 
	/// Broadcast a 16-bit value to all 8 positions in a 128-bit value.
	/// @param v 16-bit value to broadcast
	/// @return 128-value with broadcasted values
	FORCEINLINE PURE m128i16 mm_broadcast_epi16(int16_t v)
	{
#ifdef PS2INTRIN_UNSAFE
		m128i16 result = {};

		asm(
			"pcpyld	%[Result],%[Value],%[Value]\n\t"	/* put lower halfword in both doublewords	*/
			"pcpyh	%[Result],%[Result]"				/* broadcast lower halfword in both			*/
			: [Result] "=r" (result.v)
			: [Value] "r" (v)
		);

		return result;
#else
		return mm_set_epi16(v, v, v, v, v, v, v, v);
#endif
	}

	/// @brief Broadcast Halfword
	/// 
	/// Broadcast a 16-bit value to all 8 positions in a 128-bit value.
	/// @param v 16-bit value to broadcast
	/// @return 128-value with broadcasted values
	FORCEINLINE PURE m128u16 mm_broadcast_epu16(uint16_t v)
	{
		return mm_castepu16_epi16(mm_broadcast_epi16((int16_t)v));
	}

	/// @brief Broadcast Word
	/// 
	/// Broadcast a 32-bit value to all 4 positions in a 128-bit value.
	/// @param v 32-bit value to broadcast
	/// @return 128-value with broadcasted values
	FORCEINLINE PURE m128i32 mm_broadcast_epi32(int32_t v)
	{
#ifdef PS2INTRIN_UNSAFE
		m128i32 result = {};

		asm(
			"pextlw	%[Result],%[Value],%[Value]\n\t"
			"pcpyld	%[Result],%[Result],%[Result]"
			: [Result] "=r" (result.v)
			: [Value] "r" (v)
		);

		return result;
#else
		return mm_set_epi32(v, v, v, v);
#endif
	}

	/// @brief Broadcast Word
	/// 
	/// Broadcast a 32-bit value to all 4 positions in a 128-bit value.
	/// @param v 32-bit value to broadcast
	/// @return 128-value with broadcasted values
	FORCEINLINE PURE m128u32 mm_broadcast_epu32(uint32_t v)
	{
		return mm_castepu32_epi32(mm_broadcast_epi32((int32_t)v));
	}

	/// @brief Broadcast Doubleword
	/// 
	/// Broadcast a 64-bit value to all 2 positions in a 128-bit value.
	/// @param v 64-bit value to broadcast
	/// @return 128-value with broadcasted values
	FORCEINLINE PURE m128i64 mm_broadcast_epi64(int64_t v)
	{
#ifdef PS2INTRIN_UNSAFE
		m128i64 result = {};

		asm(
			"pcpyld	%[Result],%[Value],%[Value]"
			: [Result] "=r" (result.v)
			: [Value] "r" (v)
		);

		return result;
#else
		return mm_set_epi64(v, v);
#endif
	}

	/// @brief Broadcast Doubleword
	/// 
	/// Broadcast a 64-bit value to all 2 positions in a 128-bit value.
	/// @param v 64-bit value to broadcast
	/// @return 128-value with broadcasted values
	FORCEINLINE PURE m128u64 mm_broadcast_epu64(uint64_t v)
	{
		return mm_castepu64_epi64(mm_broadcast_epi64((int64_t)v));
	}


	/// @brief PMFLO : Parallel Move From LO register
	/// 
	/// Read the entire LO register and interpret its contents as signed 16-bit integers.
	/// 
	/// This function reads global state (LO).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return The current LO register
	FORCEINLINE PURE m128i16 mm_loadlo_epi16(lohi_state_t* state)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;
		m128i16 result = {};

		///	volatile for same reason as 'load_lo*'
		asm volatile(
			"pmflo	%[Result]"
			: [Result] "=r" (result.v)
		);

		return result;
#else
		m128i16 result = { state->lo[0], state->lo[1] };
		return result;
#endif
	}

	/// @brief PMFLO : Parallel Move From LO register
	/// 
	/// Read the entire LO register and interpret its contents as unsigned 16-bit integers.
	/// 
	/// This function reads global state (LO).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return The current LO register
	FORCEINLINE PURE m128u16 mm_loadlo_epu16(lohi_state_t* state)
	{
		return mm_castepu16_epi16(mm_loadlo_epi16(state));
	}

	/// @brief PMFLO : Parallel Move From LO register
	/// 
	/// Read the entire LO register and interpret its contents as unsigned 16-bit integers.
	/// 
	/// This function reads global state (LO).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return The current LO register
	FORCEINLINE PURE m128i32 mm_loadlo_epi32(lohi_state_t* state)
	{
		return mm_castepi32_epi16(mm_loadlo_epi16(state));
	}

	/// @brief PMFLO : Parallel Move From LO register
	/// 
	/// Read the entire LO register and interpret its contents as unsigned 16-bit integers.
	/// 
	/// This function reads global state (LO).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return The current LO register
	FORCEINLINE PURE m128u32 mm_loadlo_epu32(lohi_state_t* state)
	{
		return mm_castepu32_epi16(mm_loadlo_epi16(state));
	}

	/// @brief PMFHI : Parallel Move From HI register
	/// 
	/// Read the entire HI register and interpret its contents as unsigned 16-bit integers.
	/// 
	/// This function reads global state (HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return The current HI register
	FORCEINLINE PURE m128i16 mm_loadhi_epi16(lohi_state_t* state)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;
		m128i16 result = {};

		///	volatile for same reason as 'load_hi*'
		asm volatile(
			"pmfhi	%[Result]"
			: [Result] "=r" (result.v)
		);

		return result;
#else
		m128i16 result = { state->hi[0], state->hi[1] };
		return result;
#endif
	}

	/// @brief PMFHI : Parallel Move From HI register
	/// 
	/// Read the entire HI register and interpret its contents as unsigned 16-bit integers.
	/// 
	/// This function reads global state (HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return The current HI register
	FORCEINLINE PURE m128u16 mm_loadhi_epu16(lohi_state_t* state)
	{
		return mm_castepu16_epi16(mm_loadhi_epi16(state));
	}

	/// @brief PMFHI : Parallel Move From HI register
	/// 
	/// Read the entire HI register and interpret its contents as unsigned 16-bit integers.
	/// 
	/// This function reads global state (HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return The current HI register
	FORCEINLINE PURE m128i32 mm_loadhi_epi32(lohi_state_t* state)
	{
		return mm_castepi32_epi16(mm_loadhi_epi16(state));
	}

	/// @brief PMFHI : Parallel Move From HI register
	/// 
	/// Read the entire HI register and interpret its contents as unsigned 16-bit integers.
	/// 
	/// This function reads global state (HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return The current HI register
	FORCEINLINE PURE m128u32 mm_loadhi_epu32(lohi_state_t* state)
	{
		return mm_castepu32_epi16(mm_loadhi_epi16(state));
	}

	/// @brief PMFHL.LH : Parallel Move From Hi/Lo register; Lower Halfwords
	/// 
	/// Copy contents of both LO and HI registers. Assume LO and HI contain 8 16-bit values
	/// each, of which only the even positions are used. Store the 4 values from LO to positions
	/// 0, 1, 4 and 5 of the result. The 4 values from the HI register are put in positions 2, 3,
	/// 6 and 7. Finally, interpret the result as packed signed 16-bit integers.
	/// 
	/// Bitwise reordering:
	///		Result[ 15,   0]	=	LO[ 15,   0]
	///		Result[ 31,  16]	=	LO[ 47,  32]
	///		Result[ 47,  32]	=	HI[ 15,   0]
	///		Result[ 63,  48]	=	HI[ 47,  32]
	///		Result[ 79,  64]	=	LO[ 79,  64]
	///		Result[ 95,  80]	=	LO[111,  96]
	///		Result[111,  96]	=	HI[ 79,  64]
	///		Result[127, 112]	=	HI[111,  96]
	/// 
	/// This function reads global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return Rearranged values from the LO and HI registers
	FORCEINLINE PURE m128i16 mm_loadlohi_lower_epi16(lohi_state_t* state)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		(void)state;

		///	volatile for same reason as 'load_lo*'
		asm volatile(
			"pmfhl.lh	%[Result]"
			: [Result] "=r" (result.v)
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"pmflo	%[TmpLo]\n\t"
			"pmfhi	%[TmpHi]\n\t"
			"pcpyld	%[StateLo0],%[StateLo1],%[StateLo0]\n\t"
			"pcpyld	%[StateHi0],%[StateHi1],%[StateHi0]\n\t"
			"pmtlo	%[StateLo0]\n\t"
			"pmthi	%[StateHi0]\n\t"
			"pmfhl.lh	%[ResultLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]\n\t"
			"pmtlo	%[TmpLo]\n\t"
			"pmthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [ResultLo] "=r" (result.lo),
			  [ResultHi] "=r" (result.hi)
			: [StateLo0] "r" (state->lo[0]),
			  [StateLo1] "r" (state->lo[1]),
			  [StateHi0] "r" (state->hi[0]),
			  [StateHi1] "r" (state->hi[1])
		);
#endif

		return result;
	}

	/// @brief PMFHL.LH : Parallel Move From Hi/Lo register; Lower Halfwords
	/// 
	/// Copy contents of both LO and HI registers. Assume LO and HI contain 8 16-bit values
	/// each, of which only the even positions are used. Store the 4 values from LO to positions
	/// 0, 1, 4 and 5 of the result. The 4 values from the HI register are put in positions 2, 3,
	/// 6 and 7. Finally, interpret the result as packed signed 16-bit integers.
	/// 
	/// Bitwise reordering:
	///		Result[ 15,   0]	=	LO[ 15,   0]
	///		Result[ 31,  16]	=	LO[ 47,  32]
	///		Result[ 47,  32]	=	HI[ 15,   0]
	///		Result[ 63,  48]	=	HI[ 47,  32]
	///		Result[ 79,  64]	=	LO[ 79,  64]
	///		Result[ 95,  80]	=	LO[111,  96]
	///		Result[111,  96]	=	HI[ 79,  64]
	///		Result[127, 112]	=	HI[111,  96]
	/// 
	/// This function reads global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return Rearranged values from the LO and HI registers
	FORCEINLINE PURE m128u16 mm_loadlohi_lower_epu16(lohi_state_t* state)
	{
		return mm_castepu16_epi16(mm_loadlohi_lower_epi16(state));
	}

	/// @brief PMFHL.SH : Parallel Move From Hi/Lo register; Saturate lower Halfwords
	/// 
	/// Copy contents of both LO and HI registers. Assume LO and HI contain 4 32-bit values
	/// each, which get saturated to 16-bit signed values. Store the 4 values from LO to
	/// positions 0, 1, 4 and 5 of the result. The 4 values from the HI register are put in
	/// positions 2, 3, 6 and 7. Finally, interpret the result as packed signed 16-bit integers.
	/// 
	/// Bitwise reordering:
	///		Result[ 15,   0]	=	SaturateS16(LO[ 31,   0])
	///		Result[ 31,  16]	=	SaturateS16(LO[ 63,  32])
	///		Result[ 47,  32]	=	SaturateS16(HI[ 31,   0])
	///		Result[ 63,  48]	=	SaturateS16(HI[ 63,  32])
	///		Result[ 79,  64]	=	SaturateS16(LO[ 95,  64])
	///		Result[ 95,  80]	=	SaturateS16(LO[127,  96])
	///		Result[111,  96]	=	SaturateS16(HI[ 95,  64])
	///		Result[127, 112]	=	SaturateS16(HI[127,  96])
	/// 
	/// This function reads global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return Rearranged values from the LO and HI registers
	FORCEINLINE PURE m128i16 mm_loadslohi_lower_epi16(lohi_state_t* state)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		(void)state;

		///	volatile for same reason as 'load_lo*'
		asm volatile(
			"pmfhl.sh	%[Result]"
			: [Result] "=r" (result.v)
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"pmflo	%[TmpLo]\n\t"
			"pmfhi	%[TmpHi]\n\t"
			"pcpyld	%[StateLo0],%[StateLo1],%[StateLo0]\n\t"
			"pcpyld	%[StateHi0],%[StateHi1],%[StateHi0]\n\t"
			"pmtlo	%[StateLo0]\n\t"
			"pmthi	%[StateHi0]\n\t"
			"pmfhl.sh	%[ResultLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]\n\t"
			"pmtlo	%[TmpLo]\n\t"
			"pmthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [ResultLo] "=r" (result.lo),
			  [ResultHi] "=r" (result.hi)
			: [StateLo0] "r" (state->lo[0]),
			  [StateLo1] "r" (state->lo[1]),
			  [StateHi0] "r" (state->hi[0]),
			  [StateHi1] "r" (state->hi[1])
		);
#endif

		return result;
	}

	/// @brief PMFHL.LW : Parallel Move From Hi/Lo register; Lower Words
	/// 
	/// Copy contents of both LO and HI registers. Assume LO and HI contain 4 32-bit values
	/// each, of which only the even positions are used. Store the 2 values from LO to positions
	/// 0 and 2 of the result. The 2 values from the HI register are put in positions 1 and 3.
	/// Finally, interpret the result as packed signed 32-bit integers.
	/// 
	/// Bitwise reordering:
	///		Result[ 31,   0]	=	LO[ 31,   0]
	///		Result[ 63,  32]	=	HI[ 31,   0]
	///		Result[ 95,  64]	=	LO[ 95,  64]
	///		Result[127,  96]	=	HI[ 95,  64]
	/// 
	/// This function reads global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return Rearranged values from the LO and HI registers
	FORCEINLINE PURE m128i32 mm_loadlohi_lower_epi32(lohi_state_t* state)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		(void)state;

		///	volatile for same reason as 'load_lo*'
		asm volatile(
			"pmfhl.lw	%[Result]"
			: [Result] "=r" (result.v)
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"pmflo	%[TmpLo]\n\t"
			"pmfhi	%[TmpHi]\n\t"
			"pcpyld	%[StateLo0],%[StateLo1],%[StateLo0]\n\t"
			"pcpyld	%[StateHi0],%[StateHi1],%[StateHi0]\n\t"
			"pmtlo	%[StateLo0]\n\t"
			"pmthi	%[StateHi0]\n\t"
			"pmfhl.lw	%[ResultLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]\n\t"
			"pmtlo	%[TmpLo]\n\t"
			"pmthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [ResultLo] "=r" (result.lo),
			  [ResultHi] "=r" (result.hi)
			: [StateLo0] "r" (state->lo[0]),
			  [StateLo1] "r" (state->lo[1]),
			  [StateHi0] "r" (state->hi[0]),
			  [StateHi1] "r" (state->hi[1])
		);
#endif

		return result;
	}

	/// @brief PMFHL.LW : Parallel Move From Hi/Lo register; Lower Words
	/// 
	/// Copy contents of both LO and HI registers. Assume LO and HI contain 4 32-bit values
	/// each, of which only the even positions are used. Store the 2 values from LO to positions
	/// 0 and 2 of the result. The 2 values from the HI register are put in positions 1 and 3.
	/// Finally, interpret the result as packed unsigned 32-bit integers.
	/// 
	/// Bitwise reordering:
	///		Result[ 31,   0]	=	LO[ 31,   0]
	///		Result[ 63,  32]	=	HI[ 31,   0]
	///		Result[ 95,  64]	=	LO[ 95,  64]
	///		Result[127,  96]	=	HI[ 95,  64]
	/// 
	/// This function reads global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return Rearranged values from the LO and HI registers
	FORCEINLINE PURE m128u32 mm_loadlohi_lower_epu32(lohi_state_t* state)
	{
		return mm_castepu32_epi32(mm_loadlohi_lower_epi32(state));
	}

	/// @brief PMFHL.SLW : Parallel Move From Hi/Lo register; Saturate lower Words
	/// 
	/// Copy contents of both LO and HI registers. Assume LO and HI contain 4 32-bit values
	/// each, of which only the even positions are used. Concatenate the 32-bit value in HI with
	/// the corresponding element in LO to 2 temporary 64-bit values. Saturate these values to
	/// signed 32-bit values, then store them with sign extension as 64-bit signed numbers in the
	/// result.
	/// 
	/// Bitwise reordering: (where '|' means concatenation)
	///		Result[ 63,   0]	=	SaturateS32(HI[31,  0] | LO[31,  0])
	///		Result[127,  64]	=	SaturateS32(HI[95, 64] | LO[95, 64])
	/// 
	/// This function reads global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return Rearranged values from the LO and HI registers
	FORCEINLINE PURE m128i64 mm_loadslohi_lower_epi64(lohi_state_t* state)
	{
		m128i64 result = {};

#ifdef PS2INTRIN_UNSAFE
		(void)state;

		///	volatile for same reason as 'load_lo*'
		asm volatile(
			"pmfhl.slw	%[Result]"
			: [Result] "=r" (result.v)
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"pmflo	%[TmpLo]\n\t"
			"pmfhi	%[TmpHi]\n\t"
			"pcpyld	%[StateLo0],%[StateLo1],%[StateLo0]\n\t"
			"pcpyld	%[StateHi0],%[StateHi1],%[StateHi0]\n\t"
			"pmtlo	%[StateLo0]\n\t"
			"pmthi	%[StateHi0]\n\t"
			"pmfhl.slw	%[ResultLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]\n\t"
			"pmtlo	%[TmpLo]\n\t"
			"pmthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [ResultLo] "=r" (result.lo),
			  [ResultHi] "=r" (result.hi)
			: [StateLo0] "r" (state->lo[0]),
			  [StateLo1] "r" (state->lo[1]),
			  [StateHi0] "r" (state->hi[0]),
			  [StateHi1] "r" (state->hi[1])
		);
#endif

		return result;
	}

	/// @brief PMFHL.UW : Parallel Move From Hi/Lo register; Upper Words
	/// 
	/// Copy contents of both LO and HI registers. Assume LO and HI contain 4 32-bit values
	/// each, of which only the odd positions are used. Store the 2 values from LO to positions
	/// 0 and 2 of the result. The 2 values from the HI register are put in positions 1 and 3.
	/// Finally, interpret the result as packed signed 32-bit integers.
	/// 
	/// Bitwise reordering:
	///		Result[ 31,   0]	=	LO[ 63,  32]
	///		Result[ 63,  32]	=	HI[ 63,  32]
	///		Result[ 95,  64]	=	LO[127,  96]
	///		Result[127,  96]	=	HI[127,  96]
	/// 
	/// This function reads global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return Rearranged values from the LO and HI registers
	FORCEINLINE PURE m128i32 mm_loadlohi_upper_epi32(lohi_state_t* state)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		(void)state;

		///	volatile for same reason as 'load_lo*'
		asm volatile(
			"pmfhl.uw	%[Result]"
			: [Result] "=r" (result.v)
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"pmflo	%[TmpLo]\n\t"
			"pmfhi	%[TmpHi]\n\t"
			"pcpyld	%[StateLo0],%[StateLo1],%[StateLo0]\n\t"
			"pcpyld	%[StateHi0],%[StateHi1],%[StateHi0]\n\t"
			"pmtlo	%[StateLo0]\n\t"
			"pmthi	%[StateHi0]\n\t"
			"pmfhl.uw	%[ResultLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]\n\t"
			"pmtlo	%[TmpLo]\n\t"
			"pmthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [ResultLo] "=r" (result.lo),
			  [ResultHi] "=r" (result.hi)
			: [StateLo0] "r" (state->lo[0]),
			  [StateLo1] "r" (state->lo[1]),
			  [StateHi0] "r" (state->hi[0]),
			  [StateHi1] "r" (state->hi[1])
		);
#endif

		return result;
	}

	/// @brief PMFHL.UW : Parallel Move From Hi/Lo register; Upper Words
	/// 
	/// Copy contents of both LO and HI registers. Assume LO and HI contain 4 32-bit values
	/// each, of which only the odd positions are used. Store the 2 values from LO to positions
	/// 0 and 2 of the result. The 2 values from the HI register are put in positions 1 and 3.
	/// Finally, interpret the result as packed unsigned 32-bit integers.
	/// 
	/// Bitwise reordering:
	///		Result[ 31,   0]	=	LO[ 63,  32]
	///		Result[ 63,  32]	=	HI[ 63,  32]
	///		Result[ 95,  64]	=	LO[127,  96]
	///		Result[127,  96]	=	HI[127,  96]
	/// 
	/// This function reads global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @return Rearranged values from the LO and HI registers
	FORCEINLINE PURE m128u32 mm_loadlohi_upper_epu32(lohi_state_t* state)
	{
		return mm_castepu32_epi32(mm_loadlohi_upper_epi32(state));
	}

	/// @brief PMTLO : Parallel Move To LO register
	/// 
	/// Store 8 signed 16-bit values to the LO register.
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param v Value to store to LO
	FORCEINLINE void mm_storelo_epi16(lohi_state_t* state, m128i16 v)
	{
#ifdef PS2INTRIN_UNSAFE
		// volatile for same reason as store_lo*
		asm volatile(
			"pmtlo	%[Value]"
			:
			: [Value] "r" (v.v)
			: "lo"
		);
#else
		state->lo[0] = v.lo;
		state->lo[1] = v.hi;
#endif
	}

	/// @brief PMTLO : Parallel Move To LO register
	/// 
	/// Store 8 unsigned 16-bit values to the LO register.
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param v Value to store to LO
	FORCEINLINE void mm_storelo_epu16(lohi_state_t* state, m128u16 v)
	{
		mm_storelo_epi16(state, mm_castepi16_epu16(v));
	}

	/// @brief PMTLO : Parallel Move To LO register
	/// 
	/// Store 4 signed 32-bit values to the LO register.
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param v Value to store to LO
	FORCEINLINE void mm_storelo_epi32(lohi_state_t* state, m128i32 v)
	{
		mm_storelo_epi16(state, mm_castepi16_epi32(v));
	}

	/// @brief PMTLO : Parallel Move To LO register
	/// 
	/// Store 4 unsigned 32-bit values to the LO register.
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param v Value to store to LO
	FORCEINLINE void mm_storelo_epu32(lohi_state_t* state, m128u32 v)
	{
		mm_storelo_epi16(state, mm_castepi16_epu32(v));
	}

	/// @brief PMTLO : Parallel Move To LO register
	/// 
	/// Store 2 signed 64-bit values to the LO register.
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param v Value to store to LO
	FORCEINLINE void mm_storelo_epi64(lohi_state_t* state, m128i64 v)
	{
		mm_storelo_epi16(state, mm_castepi16_epi64(v));
	}

	/// @brief PMTLO : Parallel Move To LO register
	/// 
	/// Store 2 unsigned 64-bit values to the LO register.
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param v Value to store to LO
	FORCEINLINE void mm_storelo_epu64(lohi_state_t* state, m128u64 v)
	{
		mm_storelo_epi16(state, mm_castepi16_epu64(v));
	}

	/// @brief PMTLO : Parallel Move To LO register
	/// 
	/// Store 1 signed 128-bit value to the LO register.
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param v Value to store to LO
	FORCEINLINE void mm_storelo_epi128(lohi_state_t* state, m128i128 v)
	{
		mm_storelo_epi16(state, mm_castepi16_epi128(v));
	}

	/// @brief PMTLO : Parallel Move To LO register
	/// 
	/// Store 1 unsigned 128-bit value to the LO register.
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param v Value to store to LO
	FORCEINLINE void mm_storelo_epu128(lohi_state_t* state, m128u128 v)
	{
		mm_storelo_epi16(state, mm_castepi16_epu128(v));
	}

	/// @brief PMTHI : Parallel Move To HI register
	/// 
	/// Store 8 signed 16-bit values to the HI register.
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param v Value to store to HI
	FORCEINLINE void mm_storehi_epi16(lohi_state_t* state, m128i16 v)
	{
#ifdef PS2INTRIN_UNSAFE
		// volatile for same reason as store_hi*
		asm volatile(
			"pmthi	%[Value]"
			:
			: [Value] "r" (v.v)
			: "hi"
		);
#else
		state->hi[0] = v.lo;
		state->hi[1] = v.hi;
#endif
	}

	/// @brief PMTHI : Parallel Move To HI register
	/// 
	/// Store 8 unsigned 16-bit values to the HI register.
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param v Value to store to HI
	FORCEINLINE void mm_storehi_epu16(lohi_state_t* state, m128u16 v)
	{
		mm_storehi_epi16(state, mm_castepi16_epu16(v));
	}

	/// @brief PMTHI : Parallel Move To HI register
	/// 
	/// Store 4 signed 32-bit values to the HI register.
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param v Value to store to HI
	FORCEINLINE void mm_storehi_epi32(lohi_state_t* state, m128i32 v)
	{
		mm_storehi_epi16(state, mm_castepi16_epi32(v));
	}

	/// @brief PMTHI : Parallel Move To HI register
	/// 
	/// Store 4 unsigned 32-bit values to the HI register.
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param v Value to store to HI
	FORCEINLINE void mm_storehi_epu32(lohi_state_t* state, m128u32 v)
	{
		mm_storehi_epi16(state, mm_castepi16_epu32(v));
	}

	/// @brief PMTHI : Parallel Move To HI register
	/// 
	/// Store 2 signed 64-bit values to the HI register.
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param v Value to store to HI
	FORCEINLINE void mm_storehi_epi64(lohi_state_t* state, m128i64 v)
	{
		mm_storehi_epi16(state, mm_castepi16_epi64(v));
	}

	/// @brief PMTHI : Parallel Move To HI register
	/// 
	/// Store 2 unsigned 64-bit values to the HI register.
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param v Value to store to HI
	FORCEINLINE void mm_storehi_epu64(lohi_state_t* state, m128u64 v)
	{
		mm_storehi_epi16(state, mm_castepi16_epu64(v));
	}

	/// @brief PMTHI : Parallel Move To HI register
	/// 
	/// Store 1 signed 128-bit value to the HI register.
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param v Value to store to HI
	FORCEINLINE void mm_storehi_epi128(lohi_state_t* state, m128i128 v)
	{
		mm_storehi_epi16(state, mm_castepi16_epi128(v));
	}

	/// @brief PMTHI : Parallel Move To HI register
	/// 
	/// Store 1 unsigned 128-bit value to the HI register.
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param v Value to store to HI
	FORCEINLINE void mm_storehi_epu128(lohi_state_t* state, m128u128 v)
	{
		mm_storehi_epi16(state, mm_castepi16_epu128(v));
	}

	/// @brief PMTHL.LW : Parallel Move To Hi/Lo register; Lower Words
	/// 
	/// Move 4 32-bit values to both LO and HI registers. Values in even position are moved to
	/// even positions in the LO register and odd positions are moved to even positions in the HI
	/// register. Odd positions in both LO and HI are not changed.
	/// 
	/// Bitwise reordering:
	///		LO[ 31,   0]	=	Value[ 31,   0]
	///		HI[ 31,   0]	=	Value[ 63,  32]
	///		LO[ 95,  64]	=	Value[ 95,  64]
	///		HI[ 95,  64]	=	Value[127,  96]
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param v Packed values to store to LO and HI registers
	FORCEINLINE void mm_storelohi_epi32(lohi_state_t* state, m128i32 v)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// volatile for same reason as store_lo*
		asm volatile(
			"pmthl.lw	%[Value]"
			:
			: [Value] "r" (v.v)
			: "lo", "hi"
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"pmflo	%[TmpLo]\n\t"
			"pmfhi	%[TmpHi]\n\t"
			"pcpyld	%[StateLo0],%[StateLo1],%[StateLo0]\n\t"
			"pcpyld	%[StateHi0],%[StateHi1],%[StateHi0]\n\t"
			"pmtlo	%[StateLo0]\n\t"
			"pmthi	%[StateHi0]\n\t"
			"pcpyld	%[ValueLo],%[ValueHi],%[ValueLo]\n\t"
			"pmthl.lw	%[ValueLo]\n\t"
			"pmflo	%[StateLo0]\n\t"
			"pmfhi	%[StateHi0]\n\t"
			"pcpyud	%[StateLo1],%[StateLo0],%[StateLo0]\n\t"
			"pcpyud	%[StateHi1],%[StateHi0],%[StateHi0]\n\t"
			"pmtlo	%[TmpLo]\n\t"
			"pmthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo0] "+r" (state->lo[0]),
			  [StateLo1] "+r" (state->lo[1]),
			  [StateHi0] "+r" (state->hi[0]),
			  [StateHi1] "+r" (state->hi[1])
			: [ValueLo] "r" (v.lo),
			  [ValueHi] "r" (v.hi)
		);
#endif
	}

	/// @brief PMTHL.LW : Parallel Move To Hi/Lo register; Lower Words
	/// 
	/// Move 4 32-bit values to both LO and HI registers. Values in even position are moved to
	/// even positions in the LO register and odd positions are moved to even positions in the HI
	/// register. Odd positions in both LO and HI are not changed.
	/// 
	/// Bitwise reordering:
	///		LO[ 31,   0]	=	Value[ 31,   0]
	///		HI[ 31,   0]	=	Value[ 63,  32]
	///		LO[ 95,  64]	=	Value[ 95,  64]
	///		HI[ 95,  64]	=	Value[127,  96]
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param v Packed values to store to LO and HI registers
	FORCEINLINE void mm_storelohi_epu32(lohi_state_t* state, m128u32 v)
	{
		mm_storelohi_epi32(state, mm_castepi32_epu32(v));
	}


	/// @brief PAND : Parallel AND
	/// 
	/// Compute bitwise-AND of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-AND of both operands
	FORCEINLINE CONST m128i8 mm_and_epi8(m128i8 l, m128i8 r)
	{
		m128i8 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm("pand	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		result.lo = l.lo & r.lo;
		result.hi = l.hi & r.hi;
#endif

		return result;
	}

	/// @brief PAND : Parallel AND
	/// 
	/// Compute bitwise-AND of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-AND of both operands
	FORCEINLINE CONST m128u8 mm_and_epu8(m128u8 l, m128u8 r)
	{
		return mm_castepu8_epi8(mm_and_epi8(mm_castepi8_epu8(l), mm_castepi8_epu8(r)));
	}

	/// @brief PAND : Parallel AND
	/// 
	/// Compute bitwise-AND of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-AND of both operands
	FORCEINLINE CONST m128i16 mm_and_epi16(m128i16 l, m128i16 r)
	{
		return mm_castepi16_epi8(mm_and_epi8(mm_castepi8_epi16(l), mm_castepi8_epi16(r)));
	}

	/// @brief PAND : Parallel AND
	/// 
	/// Compute bitwise-AND of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-AND of both operands
	FORCEINLINE CONST m128u16 mm_and_epu16(m128u16 l, m128u16 r)
	{
		return mm_castepu16_epi8(mm_and_epi8(mm_castepi8_epu16(l), mm_castepi8_epu16(r)));
	}

	/// @brief PAND : Parallel AND
	/// 
	/// Compute bitwise-AND of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-AND of both operands
	FORCEINLINE CONST m128i32 mm_and_epi32(m128i32 l, m128i32 r)
	{
		return mm_castepi32_epi8(mm_and_epi8(mm_castepi8_epi32(l), mm_castepi8_epi32(r)));
	}

	/// @brief PAND : Parallel AND
	/// 
	/// Compute bitwise-AND of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-AND of both operands
	FORCEINLINE CONST m128u32 mm_and_epu32(m128u32 l, m128u32 r)
	{
		return mm_castepu32_epi8(mm_and_epi8(mm_castepi8_epu32(l), mm_castepi8_epu32(r)));
	}

	/// @brief PAND : Parallel AND
	/// 
	/// Compute bitwise-AND of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-AND of both operands
	FORCEINLINE CONST m128i64 mm_and_epi64(m128i64 l, m128i64 r)
	{
		return mm_castepi64_epi8(mm_and_epi8(mm_castepi8_epi64(l), mm_castepi8_epi64(r)));
	}

	/// @brief PAND : Parallel AND
	/// 
	/// Compute bitwise-AND of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-AND of both operands
	FORCEINLINE CONST m128u64 mm_and_epu64(m128u64 l, m128u64 r)
	{
		return mm_castepu64_epi8(mm_and_epi8(mm_castepi8_epu64(l), mm_castepi8_epu64(r)));
	}

	/// @brief PAND : Parallel AND
	/// 
	/// Compute bitwise-AND of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-AND of both operands
	FORCEINLINE CONST m128i128 mm_and_epi128(m128i128 l, m128i128 r)
	{
		return mm_castepi128_epi8(mm_and_epi8(mm_castepi8_epi128(l), mm_castepi8_epi128(r)));
	}

	/// @brief PAND : Parallel AND
	/// 
	/// Compute bitwise-AND of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-AND of both operands
	FORCEINLINE CONST m128u128 mm_and_epu128(m128u128 l, m128u128 r)
	{
		return mm_castepu128_epi8(mm_and_epi8(mm_castepi8_epu128(l), mm_castepi8_epu128(r)));
	}


	/// @brief POR : Parallel OR
	/// 
	/// Compute bitwise-OR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-OR of both operands
	FORCEINLINE CONST m128i8 mm_or_epi8(m128i8 l, m128i8 r)
	{
		m128i8 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm("por	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		result.lo = l.lo | r.lo;
		result.hi = l.hi | r.hi;
#endif
		
		return result;
	}

	/// @brief POR : Parallel OR
	/// 
	/// Compute bitwise-OR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-OR of both operands
	FORCEINLINE CONST m128u8 mm_or_epu8(m128u8 l, m128u8 r)
	{
		return mm_castepu8_epi8(mm_or_epi8(mm_castepi8_epu8(l), mm_castepi8_epu8(r)));
	}

	/// @brief POR : Parallel OR
	/// 
	/// Compute bitwise-OR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-OR of both operands
	FORCEINLINE CONST m128i16 mm_or_epi16(m128i16 l, m128i16 r)
	{
		return mm_castepi16_epi8(mm_or_epi8(mm_castepi8_epi16(l), mm_castepi8_epi16(r)));
	}

	/// @brief POR : Parallel OR
	/// 
	/// Compute bitwise-OR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-OR of both operands
	FORCEINLINE CONST m128u16 mm_or_epu16(m128u16 l, m128u16 r)
	{
		return mm_castepu16_epi8(mm_or_epi8(mm_castepi8_epu16(l), mm_castepi8_epu16(r)));
	}

	/// @brief POR : Parallel OR
	/// 
	/// Compute bitwise-OR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-OR of both operands
	FORCEINLINE CONST m128i32 mm_or_epi32(m128i32 l, m128i32 r)
	{
		return mm_castepi32_epi8(mm_or_epi8(mm_castepi8_epi32(l), mm_castepi8_epi32(r)));
	}

	/// @brief POR : Parallel OR
	/// 
	/// Compute bitwise-OR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-OR of both operands
	FORCEINLINE CONST m128u32 mm_or_epu32(m128u32 l, m128u32 r)
	{
		return mm_castepu32_epi8(mm_or_epi8(mm_castepi8_epu32(l), mm_castepi8_epu32(r)));
	}

	/// @brief POR : Parallel OR
	/// 
	/// Compute bitwise-OR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-OR of both operands
	FORCEINLINE CONST m128i64 mm_or_epi64(m128i64 l, m128i64 r)
	{
		return mm_castepi64_epi8(mm_or_epi8(mm_castepi8_epi64(l), mm_castepi8_epi64(r)));
	}

	/// @brief POR : Parallel OR
	/// 
	/// Compute bitwise-OR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-OR of both operands
	FORCEINLINE CONST m128u64 mm_or_epu64(m128u64 l, m128u64 r)
	{
		return mm_castepu64_epi8(mm_or_epi8(mm_castepi8_epu64(l), mm_castepi8_epu64(r)));
	}

	/// @brief POR : Parallel OR
	/// 
	/// Compute bitwise-OR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-OR of both operands
	FORCEINLINE CONST m128i128 mm_or_epi128(m128i128 l, m128i128 r)
	{
		return mm_castepi128_epi8(mm_or_epi8(mm_castepi8_epi128(l), mm_castepi8_epi128(r)));
	}

	/// @brief POR : Parallel OR
	/// 
	/// Compute bitwise-OR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-OR of both operands
	FORCEINLINE CONST m128u128 mm_or_epu128(m128u128 l, m128u128 r)
	{
		return mm_castepu128_epi8(mm_or_epi8(mm_castepi8_epu128(l), mm_castepi8_epu128(r)));
	}


	/// @brief PXOR : Parallel XOR
	/// 
	/// Compute bitwise-XOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-XOR of both operands
	FORCEINLINE CONST m128i8 mm_xor_epi8(m128i8 l, m128i8 r)
	{
		m128i8 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm("pxor	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		result.lo = l.lo ^ r.lo;
		result.hi = l.hi ^ r.hi;
#endif

		return result;
	}

	/// @brief PXOR : Parallel XOR
	/// 
	/// Compute bitwise-XOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-XOR of both operands
	FORCEINLINE CONST m128u8 mm_xor_epu8(m128u8 l, m128u8 r)
	{
		return mm_castepu8_epi8(mm_xor_epi8(mm_castepi8_epu8(l), mm_castepi8_epu8(r)));
	}

	/// @brief PXOR : Parallel XOR
	/// 
	/// Compute bitwise-XOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-XOR of both operands
	FORCEINLINE CONST m128i16 mm_xor_epi16(m128i16 l, m128i16 r)
	{
		return mm_castepi16_epi8(mm_xor_epi8(mm_castepi8_epi16(l), mm_castepi8_epi16(r)));
	}

	/// @brief PXOR : Parallel XOR
	/// 
	/// Compute bitwise-XOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-XOR of both operands
	FORCEINLINE CONST m128u16 mm_xor_epu16(m128u16 l, m128u16 r)
	{
		return mm_castepu16_epi8(mm_xor_epi8(mm_castepi8_epu16(l), mm_castepi8_epu16(r)));
	}

	/// @brief PXOR : Parallel XOR
	/// 
	/// Compute bitwise-XOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-XOR of both operands
	FORCEINLINE CONST m128i32 mm_xor_epi32(m128i32 l, m128i32 r)
	{
		return mm_castepi32_epi8(mm_xor_epi8(mm_castepi8_epi32(l), mm_castepi8_epi32(r)));
	}

	/// @brief PXOR : Parallel XOR
	/// 
	/// Compute bitwise-XOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-XOR of both operands
	FORCEINLINE CONST m128u32 mm_xor_epu32(m128u32 l, m128u32 r)
	{
		return mm_castepu32_epi8(mm_xor_epi8(mm_castepi8_epu32(l), mm_castepi8_epu32(r)));
	}

	/// @brief PXOR : Parallel XOR
	/// 
	/// Compute bitwise-XOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-XOR of both operands
	FORCEINLINE CONST m128i64 mm_xor_epi64(m128i64 l, m128i64 r)
	{
		return mm_castepi64_epi8(mm_xor_epi8(mm_castepi8_epi64(l), mm_castepi8_epi64(r)));
	}

	/// @brief PXOR : Parallel XOR
	/// 
	/// Compute bitwise-XOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-XOR of both operands
	FORCEINLINE CONST m128u64 mm_xor_epu64(m128u64 l, m128u64 r)
	{
		return mm_castepu64_epi8(mm_xor_epi8(mm_castepi8_epu64(l), mm_castepi8_epu64(r)));
	}

	/// @brief PXOR : Parallel XOR
	/// 
	/// Compute bitwise-XOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-XOR of both operands
	FORCEINLINE CONST m128i128 mm_xor_epi128(m128i128 l, m128i128 r)
	{
		return mm_castepi128_epi8(mm_xor_epi8(mm_castepi8_epi128(l), mm_castepi8_epi128(r)));
	}

	/// @brief PXOR : Parallel XOR
	/// 
	/// Compute bitwise-XOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-XOR of both operands
	FORCEINLINE CONST m128u128 mm_xor_epu128(m128u128 l, m128u128 r)
	{
		return mm_castepu128_epi8(mm_xor_epi8(mm_castepi8_epu128(l), mm_castepi8_epu128(r)));
	}


	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-NOR of both operands
	FORCEINLINE CONST m128i8 mm_nor_epi8(m128i8 l, m128i8 r)
	{
		m128i8 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm("pnor	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		result.lo = ~(l.lo | r.lo);
		result.hi = ~(l.hi | r.hi);
#endif

		return result;
	}

	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-NOR of both operands
	FORCEINLINE CONST m128u8 mm_nor_epu8(m128u8 l, m128u8 r)
	{
		return mm_castepu8_epi8(mm_nor_epi8(mm_castepi8_epu8(l), mm_castepi8_epu8(r)));
	}

	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-NOR of both operands
	FORCEINLINE CONST m128i16 mm_nor_epi16(m128i16 l, m128i16 r)
	{
		return mm_castepi16_epi8(mm_nor_epi8(mm_castepi8_epi16(l), mm_castepi8_epi16(r)));
	}

	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-NOR of both operands
	FORCEINLINE CONST m128u16 mm_nor_epu16(m128u16 l, m128u16 r)
	{
		return mm_castepu16_epi8(mm_nor_epi8(mm_castepi8_epu16(l), mm_castepi8_epu16(r)));
	}

	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-NOR of both operands
	FORCEINLINE CONST m128i32 mm_nor_epi32(m128i32 l, m128i32 r)
	{
		return mm_castepi32_epi8(mm_nor_epi8(mm_castepi8_epi32(l), mm_castepi8_epi32(r)));
	}

	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-NOR of both operands
	FORCEINLINE CONST m128u32 mm_nor_epu32(m128u32 l, m128u32 r)
	{
		return mm_castepu32_epi8(mm_nor_epi8(mm_castepi8_epu32(l), mm_castepi8_epu32(r)));
	}

	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-NOR of both operands
	FORCEINLINE CONST m128i64 mm_nor_epi64(m128i64 l, m128i64 r)
	{
		return mm_castepi64_epi8(mm_nor_epi8(mm_castepi8_epi64(l), mm_castepi8_epi64(r)));
	}

	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-NOR of both operands
	FORCEINLINE CONST m128u64 mm_nor_epu64(m128u64 l, m128u64 r)
	{
		return mm_castepu64_epi8(mm_nor_epi8(mm_castepi8_epu64(l), mm_castepi8_epu64(r)));
	}

	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-NOR of both operands
	FORCEINLINE CONST m128i128 mm_nor_epi128(m128i128 l, m128i128 r)
	{
		return mm_castepi128_epi8(mm_nor_epi8(mm_castepi8_epi128(l), mm_castepi8_epi128(r)));
	}

	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOR of 128-bit values.
	/// @param l First operand
	/// @param r Second operand
	/// @return Bitwise-NOR of both operands
	FORCEINLINE CONST m128u128 mm_nor_epu128(m128u128 l, m128u128 r)
	{
		return mm_castepu128_epi8(mm_nor_epi8(mm_castepi8_epu128(l), mm_castepi8_epu128(r)));
	}


	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOT of 128-bit values.
	/// @param v Operand
	/// @return Bitwise-NOT of operand
	FORCEINLINE CONST m128i8 mm_not_epi8(m128i8 v)
	{
		return mm_nor_epi8(v, v);
	}

	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOT of 128-bit values.
	/// @param v Operand
	/// @return Bitwise-NOT of operand
	FORCEINLINE CONST m128u8 mm_not_epu8(m128u8 v)
	{
		return mm_nor_epu8(v, v);
	}

	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOT of 128-bit values.
	/// @param v Operand
	/// @return Bitwise-NOT of operand
	FORCEINLINE CONST m128i16 mm_not_epi16(m128i16 v)
	{
		return mm_nor_epi16(v, v);
	}

	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOT of 128-bit values.
	/// @param v Operand
	/// @return Bitwise-NOT of operand
	FORCEINLINE CONST m128u16 mm_not_epu16(m128u16 v)
	{
		return mm_nor_epu16(v, v);
	}

	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOT of 128-bit values.
	/// @param v Operand
	/// @return Bitwise-NOT of operand
	FORCEINLINE CONST m128i32 mm_not_epi32(m128i32 v)
	{
		return mm_nor_epi32(v, v);
	}

	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOT of 128-bit values.
	/// @param v Operand
	/// @return Bitwise-NOT of operand
	FORCEINLINE CONST m128u32 mm_not_epu32(m128u32 v)
	{
		return mm_nor_epu32(v, v);
	}

	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOT of 128-bit values.
	/// @param v Operand
	/// @return Bitwise-NOT of operand
	FORCEINLINE CONST m128i64 mm_not_epi64(m128i64 v)
	{
		return mm_nor_epi64(v, v);
	}

	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOT of 128-bit values.
	/// @param v Operand
	/// @return Bitwise-NOT of operand
	FORCEINLINE CONST m128u64 mm_not_epu64(m128u64 v)
	{
		return mm_nor_epu64(v, v);
	}

	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOT of 128-bit values.
	/// @param v Operand
	/// @return Bitwise-NOT of operand
	FORCEINLINE CONST m128i128 mm_not_epi128(m128i128 v)
	{
		return mm_nor_epi128(v, v);
	}

	/// @brief PNOR : Parallel NOR
	/// 
	/// Compute bitwise-NOT of 128-bit values.
	/// @param v Operand
	/// @return Bitwise-NOT of operand
	FORCEINLINE CONST m128u128 mm_not_epu128(m128u128 v)
	{
		return mm_nor_epu128(v, v);
	}


	/// @brief PCEQB : Parallel Compare for EQual Byte
	/// 
	/// Compare 16 8-bit value pairs for equality. On equality, return 0xFF for that value pair,
	/// otherwise 0x00.
	/// @param l First operand
	/// @param r Second operand
	/// @return Equality result mask
	FORCEINLINE CONST m128i8 mm_cmpeq_epi8(m128i8 l, m128i8 r)
	{
		m128i8 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pceqb	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LLo],%[LHi],%[LLo]\n\t"
			"pcpyld	%[RLo],%[RHi],%[RLo]\n\t"
			"pceqb	%[ResultLo],%[LLo],%[RLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LLo] "r" (l.lo),
			  [LHi] "r" (l.hi),
			  [RLo] "r" (r.lo),
			  [RHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PCEQB : Parallel Compare for EQual Byte
	/// PNOR : Parallel NOR
	/// 
	/// Convenience function. Invert result of 'cmpeq'.
	/// @param l First operand
	/// @param r Second operand
	/// @return Inverted equality result mask
	FORCEINLINE CONST m128i8 mm_cmpneq_epi8(m128i8 l, m128i8 r)
	{
		return mm_not_epi8(mm_cmpeq_epi8(l, r));
	}

	/// @brief PCGTB : Parallel Compare for Greater Than Byte
	/// 
	/// Compare 16 signed 8-bit value pairs for a greater than relationship. Returns 0xFF for
	/// pairs where the value in 'l' is true greater than the value in 'r', and 0x00 otherwise.
	/// 
	/// Unsigned comparisons can be achieved by first adding '0x80' to every element in each
	/// operand.
	/// @param l Left operand
	/// @param r Right operand
	/// @return Comparison result mask
	FORCEINLINE CONST m128i8 mm_cmpgt_epi8(m128i8 l, m128i8 r)
	{
		m128i8 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pcgtb	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LLo],%[LHi],%[LLo]\n\t"
			"pcpyld	%[RLo],%[RHi],%[RLo]\n\t"
			"pcgtb	%[ResultLo],%[LLo],%[RLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LLo] "r" (l.lo),
			  [LHi] "r" (l.hi),
			  [RLo] "r" (r.lo),
			  [RHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PCGTB : Parallel Compare for Greater Than Byte
	/// 
	/// Compare 16 signed 8-bit value pairs for a less than relationship. Returns 0xFF for
	/// pairs where the value in 'l' is true less than the value in 'r', and 0x00 otherwise.
	/// 
	/// Unsigned comparisons can be achieved by first adding '0x80' to every element in each
	/// operand.
	/// @param l Left operand
	/// @param r Right operand
	/// @return Comparison result mask
	FORCEINLINE CONST m128i8 mm_cmplt_epi8(m128i8 l, m128i8 r)
	{
		return mm_cmpgt_epi8(r, l);
	}

	/// @brief PCGTB : Parallel Compare for Greater Than Byte
	/// PNOR : Parallel NOR
	/// 
	/// Compare 16 signed 8-bit value pairs for a less than or equal relationship. Returns 0xFF
	/// for pairs where the value in 'l' is less than or equal the value in 'r', and 0x00
	/// otherwise.
	/// 
	/// Unsigned comparisons can be achieved by first adding '0x80' to every element in each
	/// operand.
	/// @param l Left operand
	/// @param r Right operand
	/// @return Comparison result mask
	FORCEINLINE CONST m128i8 mm_cmple_epi8(m128i8 l, m128i8 r)
	{
		return mm_not_epi8(mm_cmpgt_epi8(l, r));
	}

	/// @brief PCGTB : Parallel Compare for Greater Than Byte
	/// PNOR : Parallel NOR
	/// 
	/// Compare 16 signed 8-bit value pairs for a greater than or equal relationship. Returns 0xFF
	/// for pairs where the value in 'l' is greater than or equal the value in 'r', and 0x00
	/// otherwise.
	/// 
	/// Unsigned comparisons can be achieved by first adding '0x80' to every element in each
	/// operand.
	/// @param l Left operand
	/// @param r Right operand
	/// @return Comparison result mask
	FORCEINLINE CONST m128i8 mm_cmpge_epi8(m128i8 l, m128i8 r)
	{
		return mm_not_epi8(mm_cmpgt_epi8(r, l));
	}

	/// @brief PCEQB : Parallel Compare for EQual Byte
	/// 
	/// Compare 16 8-bit value pairs for equality. On equality, return 0xFF for that value pair,
	/// otherwise 0x00.
	/// @param l First operand
	/// @param r Second operand
	/// @return Equality result mask
	FORCEINLINE CONST m128u8 mm_cmpeq_epu8(m128u8 l, m128u8 r)
	{
		return mm_castepu8_epi8(mm_cmpeq_epi8(mm_castepi8_epu8(l), mm_castepi8_epu8(r)));
	}

	/// @brief PCEQB : Parallel Compare for EQual Byte
	/// PNOR : Parallel NOR
	/// 
	/// Convenience function. Invert result of 'cmpeq'.
	/// @param l First operand
	/// @param r Second operand
	/// @return Inverted equality result mask
	FORCEINLINE CONST m128u8 mm_cmpneq_epu8(m128u8 l, m128u8 r)
	{
		return mm_not_epu8(mm_cmpeq_epu8(l, r));
	}

	/// @brief PCEQH : Parallel Compare for EQual Halfword
	/// 
	/// Compare 8 16-bit value pairs for equality. On equality, return 0xFFFF for that value pair,
	/// otherwise 0x0000.
	/// @param l First operand
	/// @param r Second operand
	/// @return Equality result mask
	FORCEINLINE CONST m128i16 mm_cmpeq_epi16(m128i16 l, m128i16 r)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pceqh	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LLo],%[LHi],%[LLo]\n\t"
			"pcpyld	%[RLo],%[RHi],%[RLo]\n\t"
			"pceqh	%[ResultLo],%[LLo],%[RLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LLo] "r" (l.lo),
			  [LHi] "r" (l.hi),
			  [RLo] "r" (r.lo),
			  [RHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PCEQB : Parallel Compare for EQual Byte
	/// PNOR : Parallel NOR
	/// 
	/// Convenience function. Invert result of 'cmpeq'.
	/// @param l First operand
	/// @param r Second operand
	/// @return Inverted equality result mask
	FORCEINLINE CONST m128i16 mm_cmpneq_epi16(m128i16 l, m128i16 r)
	{
		return mm_not_epi16(mm_cmpeq_epi16(l, r));
	}

	/// @brief PCGTH : Parallel Compare for Greater Than Halfword
	/// 
	/// Compare 8 signed 16-bit value pairs for a greater than relationship. Returns 0xFFFF for
	/// pairs where the value in 'l' is true greater than the value in 'r', and 0x0000 otherwise.
	/// 
	/// Unsigned comparisons can be achieved by first adding '0x8000' to every element in each
	/// operand.
	/// @param l Left operand
	/// @param r Right operand
	/// @return Comparison result mask
	FORCEINLINE CONST m128i16 mm_cmpgt_epi16(m128i16 l, m128i16 r)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pcgth	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LLo],%[LHi],%[LLo]\n\t"
			"pcpyld	%[RLo],%[RHi],%[RLo]\n\t"
			"pcgth	%[ResultLo],%[LLo],%[RLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LLo] "r" (l.lo),
			  [LHi] "r" (l.hi),
			  [RLo] "r" (r.lo),
			  [RHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PCGTH : Parallel Compare for Greater Than Halfword
	/// 
	/// Compare 8 signed 16-bit value pairs for a less than relationship. Returns 0xFFFF for
	/// pairs where the value in 'l' is true less than the value in 'r', and 0x0000 otherwise.
	/// 
	/// Unsigned comparisons can be achieved by first adding '0x8000' to every element in each
	/// operand.
	/// @param l Left operand
	/// @param r Right operand
	/// @return Comparison result mask
	FORCEINLINE CONST m128i16 mm_cmplt_epi16(m128i16 l, m128i16 r)
	{
		return mm_cmpgt_epi16(r, l);
	}

	/// @brief PCGTH : Parallel Compare for Greater Than Halfword
	/// PNOR : Parallel NOR
	/// 
	/// Compare 8 signed 16-bit value pairs for a less than or equal relationship. Returns 0xFFFF
	/// for pairs where the value in 'l' is less than or equal the value in 'r', and 0x0000
	/// otherwise.
	/// 
	/// Unsigned comparisons can be achieved by first adding '0x8000' to every element in each
	/// operand.
	/// @param l Left operand
	/// @param r Right operand
	/// @return Comparison result mask
	FORCEINLINE CONST m128i16 mm_cmple_epi16(m128i16 l, m128i16 r)
	{
		return mm_not_epi16(mm_cmpgt_epi16(l, r));
	}

	/// @brief PCGTH : Parallel Compare for Greater Than Halfword
	/// PNOR : Parallel NOR
	/// 
	/// Compare 8 signed 16-bit value pairs for a greater than or equal relationship. Returns 0xFFFF
	/// for pairs where the value in 'l' is greater than or equal the value in 'r', and 0x0000
	/// otherwise.
	/// 
	/// Unsigned comparisons can be achieved by first adding '0x8000' to every element in each
	/// operand.
	/// @param l Left operand
	/// @param r Right operand
	/// @return Comparison result mask
	FORCEINLINE CONST m128i16 mm_cmpge_epi16(m128i16 l, m128i16 r)
	{
		return mm_not_epi16(mm_cmpgt_epi16(r, l));
	}

	/// @brief PCEQH : Parallel Compare for EQual Halfword
	/// 
	/// Compare 8 16-bit value pairs for equality. On equality, return 0xFFFF for that value pair,
	/// otherwise 0x0000.
	/// @param l First operand
	/// @param r Second operand
	/// @return Equality result mask
	FORCEINLINE CONST m128u16 mm_cmpeq_epu16(m128u16 l, m128u16 r)
	{
		return mm_castepu16_epi16(mm_cmpeq_epi16(mm_castepi16_epu16(l), mm_castepi16_epu16(r)));
	}

	/// @brief PCEQB : Parallel Compare for EQual Byte
	/// PNOR : Parallel NOR
	/// 
	/// Convenience function. Invert result of 'cmpeq'.
	/// @param l First operand
	/// @param r Second operand
	/// @return Inverted equality result mask
	FORCEINLINE CONST m128u16 mm_cmpneq_epu16(m128u16 l, m128u16 r)
	{
		return mm_not_epu16(mm_cmpeq_epu16(l, r));
	}

	/// @brief PCEQW : Parallel Compare for EQual Word
	/// 
	/// Compare 4 32-bit value pairs for equality. On equality, return 0xFFFFFFFF for that value
	/// pair, otherwise 0x00000000.
	/// @param l First operand
	/// @param r Second operand
	/// @return Equality result mask
	FORCEINLINE CONST m128i32 mm_cmpeq_epi32(m128i32 l, m128i32 r)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pceqw	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LLo],%[LHi],%[LLo]\n\t"
			"pcpyld	%[RLo],%[RHi],%[RLo]\n\t"
			"pceqw	%[ResultLo],%[LLo],%[RLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LLo] "r" (l.lo),
			  [LHi] "r" (l.hi),
			  [RLo] "r" (r.lo),
			  [RHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PCEQB : Parallel Compare for EQual Byte
	/// PNOR : Parallel NOR
	/// 
	/// Convenience function. Invert result of 'cmpeq'.
	/// @param l First operand
	/// @param r Second operand
	/// @return Inverted equality result mask
	FORCEINLINE CONST m128i32 mm_cmpneq_epi32(m128i32 l, m128i32 r)
	{
		return mm_not_epi32(mm_cmpeq_epi32(l, r));
	}

	/// @brief PCGTW : Parallel Compare for Greater Than Word
	/// 
	/// Compare 4 signed 32-bit value pairs for a greater than relationship. Returns 0xFFFFFFFF
	/// for pairs where the value in 'l' is true greater than the value in 'r', and 0x00000000
	/// otherwise.
	/// 
	/// Unsigned comparisons can be achieved by first adding '0x80000000' to every element in each
	/// operand.
	/// @param l Left operand
	/// @param r Right operand
	/// @return Comparison result mask
	FORCEINLINE CONST m128i32 mm_cmpgt_epi32(m128i32 l, m128i32 r)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pcgtw	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LLo],%[LHi],%[LLo]\n\t"
			"pcpyld	%[RLo],%[RHi],%[RLo]\n\t"
			"pcgtw	%[ResultLo],%[LLo],%[RLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LLo] "r" (l.lo),
			  [LHi] "r" (l.hi),
			  [RLo] "r" (r.lo),
			  [RHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PCGTW : Parallel Compare for Greater Than Word
	/// 
	/// Compare 4 signed 32-bit value pairs for a less than relationship. Returns 0xFFFFFFFF for
	/// pairs where the value in 'l' is true less than the value in 'r', and 0x00000000 otherwise.
	/// 
	/// Unsigned comparisons can be achieved by first adding '0x80000000' to every element in each
	/// operand.
	/// @param l Left operand
	/// @param r Right operand
	/// @return Comparison result mask
	FORCEINLINE CONST m128i32 mm_cmplt_epi32(m128i32 l, m128i32 r)
	{
		return mm_cmpgt_epi32(r, l);
	}

	/// @brief PCGTW : Parallel Compare for Greater Than Word
	/// PNOR : Parallel NOR
	/// 
	/// Compare 4 signed 32-bit value pairs for a less than or equal relationship. Returns
	/// 0xFFFFFFFF for pairs where the value in 'l' is less than or equal the value in 'r', and
	/// 0x00000000 otherwise.
	/// 
	/// Unsigned comparisons can be achieved by first adding '0x80000000' to every element in each
	/// operand.
	/// @param l Left operand
	/// @param r Right operand
	/// @return Comparison result mask
	FORCEINLINE CONST m128i32 mm_cmple_epi32(m128i32 l, m128i32 r)
	{
		return mm_not_epi32(mm_cmpgt_epi32(l, r));
	}

	/// @brief PCGTW : Parallel Compare for Greater Than Word
	/// PNOR : Parallel NOR
	/// 
	/// Compare 4 signed 32-bit value pairs for a greater than or equal relationship. Returns
	/// 0xFFFFFFFF for pairs where the value in 'l' is greater than or equal the value in 'r', and
	/// 0x00000000 otherwise.
	/// 
	/// Unsigned comparisons can be achieved by first adding '0x80000000' to every element in each
	/// operand.
	/// @param l Left operand
	/// @param r Right operand
	/// @return Comparison result mask
	FORCEINLINE CONST m128i32 mm_cmpge_epi32(m128i32 l, m128i32 r)
	{
		return mm_not_epi32(mm_cmpgt_epi32(r, l));
	}

	/// @brief PCEQW : Parallel Compare for EQual Word
	/// 
	/// Compare 4 32-bit value pairs for equality. On equality, return 0xFFFFFFFF for that value
	/// pair, otherwise 0x00000000.
	/// @param l First operand
	/// @param r Second operand
	/// @return Equality result mask
	FORCEINLINE CONST m128u32 mm_cmpeq_epu32(m128u32 l, m128u32 r)
	{
		return mm_castepu32_epi32(mm_cmpeq_epi32(mm_castepi32_epu32(l), mm_castepi32_epu32(r)));
	}

	/// @brief PCEQB : Parallel Compare for EQual Byte
	/// PNOR : Parallel NOR
	/// 
	/// Convenience function. Invert result of 'cmpeq'.
	/// @param l First operand
	/// @param r Second operand
	/// @return Inverted equality result mask
	FORCEINLINE CONST m128u32 mm_cmpneq_epu32(m128u32 l, m128u32 r)
	{
		return mm_not_epu32(mm_cmpeq_epu32(l, r));
	}


	// PSLLH
#ifdef PS2INTRIN_UNSAFE
	/// @brief PSLLH : Parallel Shift Left Logical Halfword
	/// 
	/// Logically left shift 16-bit values. Shifts in '0's into the lower bits.
	/// @param result Variable identifier to store result to. Must be of type 'm128i16' or
	/// 'm128u16'
	/// @param value Variable identifier to use as source value. Must be of type 'm128i16' or
	/// 'm128u16'
	/// @param shift_amount Amount of bits to shift the source value left. Must be in range [0, 15]
	#define PSLLH(result, value, shift_amount)														\
		asm(																						\
			"psllh	%[Result],%[Value],%c[ShiftAmount]"												\
			: [Result] "=r" ((result).v)															\
			: [Value] "r" ((value).v),																\
			  [ShiftAmount] "n" (shift_amount)														\
		)
#else
	/// @brief PSLLH : Parallel Shift Left Logical Halfword
	/// 
	/// Logically left shift 16-bit values. Shifts in '0's into the lower bits.
	/// @param result Variable identifier to store result to. Must be of type 'm128i16' or
	/// 'm128u16'
	/// @param value Variable identifier to use as source value. Must be of type 'm128i16' or
	/// 'm128u16'
	/// @param shift_amount Amount of bits to shift the source value left. Must be in range [0, 15]
	#define PSLLH(result, value, shift_amount)														\
		asm(																						\
			"pcpyld	%[ResultLo],%[ValueHi],%[ValueLo]\n\t"											\
			"psllh	%[ResultLo],%[ResultLo],%c[ShiftAmount]\n\t"									\
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"											\
			: [ResultLo] "=r" ((result).lo),														\
			  [ResultHi] "=&r" ((result).hi)														\
			: [ValueLo] "r" ((value).lo),															\
			  [ValueHi] "r" ((value).hi),															\
			  [ShiftAmount] "n" (shift_amount)														\
		)
#endif

	// PSLLW
#ifdef PS2INTRIN_UNSAFE
	/// @brief PSLLW : Parallel Shift Left Logical Word
	/// 
	/// Logically left shift 32-bit values. Shifts in '0's into the lower bits.
	/// @param result Variable identifier to store result to. Must be of type 'm128i32' or
	/// 'm128u32'
	/// @param value Variable identifier to use as source value. Must be of type 'm128i32' or
	/// 'm128u32'
	/// @param shift_amount Amount of bits to shift the source value left. Must be in range [0, 31]
	#define PSLLW(result, value, shift_amount)														\
		asm(																						\
			"psllw	%[Result],%[Value],%c[ShiftAmount]"												\
			: [Result] "=r" ((result).v)															\
			: [Value] "r" ((value).v),																\
			  [ShiftAmount] "n" (shift_amount)														\
		)
#else
	/// @brief PSLLW : Parallel Shift Left Logical Word
	/// 
	/// Logically left shift 32-bit values. Shifts in '0's into the lower bits.
	/// @param result Variable identifier to store result to. Must be of type 'm128i32' or
	/// 'm128u32'
	/// @param value Variable identifier to use as source value. Must be of type 'm128i32' or
	/// 'm128u32'
	/// @param shift_amount Amount of bits to shift the source value left. Must be in range [0, 31]
	#define PSLLW(result, value, shift_amount)														\
		asm(																						\
			"pcpyld	%[ResultLo],%[ValueHi],%[ValueLo]\n\t"											\
			"psllw	%[ResultLo],%[ResultLo],%c[ShiftAmount]\n\t"									\
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"											\
			: [ResultLo] "=r" ((result).lo),														\
			  [ResultHi] "=&r" ((result).hi)														\
			: [ValueLo] "r" ((value).lo),															\
			  [ValueHi] "r" ((value).hi),															\
			  [ShiftAmount] "n" (shift_amount)														\
		)
#endif

	/// @brief PSLLVW : Parallel Shift Left Logical Variable Word
	/// 
	/// Treat 128-bit packed integer data as 2 sign-extended 32-bit values. Shift each value left
	/// while shifting in '0's by an amount equal to the corresponding value in 'shift_amount'.
	/// Only the low 5 bits in each value of 'shift_amount' are used, others are ignored.
	/// @param value Values to shift
	/// @param shift_amount Amount to shift values by
	/// @return Values shifted left by variable amount
	FORCEINLINE CONST m128i64 mm_sllv_epi64(m128i64 value, m128u64 shift_amount)
	{
		m128i64 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"psllvw	%[Result],%[Value],%[Amount]"
			: [Result] "=r" (result.v)
			: [Value] "r" (value.v),
			  [Amount] "r" (shift_amount.v)
		);
#else
		asm(
			"pcpyld	%[ValueLo],%[ValueHi],%[ValueLo]\n\t"
			"pcpyld	%[AmountLo],%[AmountHi],%[AmountLo]\n\t"
			"psllvw	%[ResultLo],%[ValueLo],%[AmountLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [ValueLo] "r" (value.lo),
			  [ValueHi] "r" (value.hi),
			  [AmountLo] "r" (shift_amount.lo),
			  [AmountHi] "r" (shift_amount.hi)
		);
#endif

		return result;
	}

	// PSRAH
#ifdef PS2INTRIN_UNSAFE
	/// @brief PSRAH : Parallel Shift Right Arithmetic Halfword
	/// 
	/// Arithmetically right shift 16-bit values. Shifts in sign bits into the upper bits.
	/// @param result Variable identifier to store result to. Must be of type 'm128i16'
	/// @param value Variable identifier to use as source value. Must be of type 'm128i16'
	/// @param shift_amount Amount of bits to shift the source value right. Must be in range [0, 15]
	#define PSRAH(result, value, shift_amount)														\
		asm(																						\
			"psrah	%[Result],%[Value],%c[ShiftAmount]"												\
			: [Result] "=r" ((result).v)															\
			: [Value] "r" ((value).v),																\
			  [ShiftAmount] "n" (shift_amount)														\
		)
#else
	/// @brief PSRAH : Parallel Shift Right Arithmetic Halfword
	/// 
	/// Arithmetically right shift 16-bit values. Shifts in sign bits into the upper bits.
	/// @param result Variable identifier to store result to. Must be of type 'm128i16'
	/// @param value Variable identifier to use as source value. Must be of type 'm128i16'
	/// @param shift_amount Amount of bits to shift the source value right. Must be in range [0, 15]
	#define PSRAH(result, value, shift_amount)														\
		asm(																						\
			"pcpyld	%[ResultLo],%[ValueHi],%[ValueLo]\n\t"											\
			"psrah	%[ResultLo],%[ResultLo],%c[ShiftAmount]\n\t"									\
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"											\
			: [ResultLo] "=r" ((result).lo),														\
			  [ResultHi] "=&r" ((result).hi)														\
			: [ValueLo] "r" ((value).lo),															\
			  [ValueHi] "r" ((value).hi),															\
			  [ShiftAmount] "n" (shift_amount)														\
		)
#endif

	// PSRAW
#ifdef PS2INTRIN_UNSAFE
	/// @brief PSRAW : Parallel Shift Right Arithmetic Word
	/// 
	/// Arithmetically right shift 32-bit values. Shifts in sign bits into the upper bits.
	/// @param result Variable identifier to store result to. Must be of type 'm128i32'
	/// @param value Variable identifier to use as source value. Must be of type 'm128i32'
	/// @param shift_amount Amount of bits to shift the source value left. Must be in range [0, 31]
	#define PSRAW(result, value, shift_amount)														\
		asm(																						\
			"psraw	%[Result],%[Value],%c[ShiftAmount]"												\
			: [Result] "=r" ((result).v)															\
			: [Value] "r" ((value).v),																\
			  [ShiftAmount] "n" (shift_amount)														\
		)
#else
	/// @brief PSRAW : Parallel Shift Right Arithmetic Word
	/// 
	/// Arithmetically right shift 32-bit values. Shifts in sign bits into the upper bits.
	/// @param result Variable identifier to store result to. Must be of type 'm128i32'
	/// @param value Variable identifier to use as source value. Must be of type 'm128i32'
	/// @param shift_amount Amount of bits to shift the source value left. Must be in range [0, 31]
	#define PSRAW(result, value, shift_amount)														\
		asm(																						\
			"pcpyld	%[ResultLo],%[ValueHi],%[ValueLo]\n\t"											\
			"psraw	%[ResultLo],%[ResultLo],%c[ShiftAmount]\n\t"									\
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"											\
			: [ResultLo] "=r" ((result).lo),														\
			  [ResultHi] "=&r" ((result).hi)														\
			: [ValueLo] "r" ((value).lo),															\
			  [ValueHi] "r" ((value).hi),															\
			  [ShiftAmount] "n" (shift_amount)														\
		)
#endif

	/// @brief PSRAVW : Parallel Shift Right Arithmetic Variable Word
	/// 
	/// Treat 128-bit packed integer data as 2 sign-extended 32-bit values. Shift each value
	/// right while shifting in sign bits by an amount equal to the corresponding value in
	/// 'shift_amount'. Only the low 5 bits in each value of 'shift_amount' are used, others
	/// are ignored.
	/// @param value Values to shift
	/// @param shift_amount Amount to shift values by
	/// @return Values shifted right by variable amount
	FORCEINLINE CONST m128i64 mm_srav_epi64(m128i64 value, m128u64 shift_amount)
	{
		m128i64 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"psravw	%[Result],%[Value],%[Amount]"
			: [Result] "=r" (result.v)
			: [Value] "r" (value.v),
			  [Amount] "r" (shift_amount.v)
		);
#else
		asm(
			"pcpyld	%[ValueLo],%[ValueHi],%[ValueLo]\n\t"
			"pcpyld	%[AmountLo],%[AmountHi],%[AmountLo]\n\t"
			"psravw	%[ResultLo],%[ValueLo],%[AmountLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [ValueLo] "r" (value.lo),
			  [ValueHi] "r" (value.hi),
			  [AmountLo] "r" (shift_amount.lo),
			  [AmountHi] "r" (shift_amount.hi)
		);
#endif

		return result;
	}

	// PSRLH
#ifdef PS2INTRIN_UNSAFE
	/// @brief PSRLH : Parallel Shift Right Logical Halfword
	/// 
	/// Logically right shift 16-bit values. Shifts in '0's into the upper bits.
	/// @param result Variable identifier to store result to. Must be of type 'm128u16'
	/// @param value Variable identifier to use as source value. Must be of type 'm128u16'
	/// @param shift_amount Amount of bits to shift the source value right. Must be in range [0, 15]
	#define PSRLH(result, value, shift_amount)														\
		asm(																						\
			"psrlh	%[Result],%[Value],%c[ShiftAmount]"												\
			: [Result] "=r" ((result).v)															\
			: [Value] "r" ((value).v),																\
			  [ShiftAmount] "n" (shift_amount)														\
		)
#else
	/// @brief PSRLH : Parallel Shift Right Logical Halfword
	/// 
	/// Logically right shift 16-bit values. Shifts in '0's into the upper bits.
	/// @param result Variable identifier to store result to. Must be of type 'm128u16'
	/// @param value Variable identifier to use as source value. Must be of type 'm128u16'
	/// @param shift_amount Amount of bits to shift the source value right. Must be in range [0, 15]
	#define PSRLH(result, value, shift_amount)														\
		asm(																						\
			"pcpyld	%[ResultLo],%[ValueHi],%[ValueLo]\n\t"											\
			"psrlh	%[ResultLo],%[ResultLo],%c[ShiftAmount]\n\t"									\
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"											\
			: [ResultLo] "=r" ((result).lo),														\
			  [ResultHi] "=&r" ((result).hi)														\
			: [ValueLo] "r" ((value).lo),															\
			  [ValueHi] "r" ((value).hi),															\
			  [ShiftAmount] "n" (shift_amount)														\
		)
#endif

	// PSRLW
#ifdef PS2INTRIN_UNSAFE
	/// @brief PSRLW : Parallel Shift Right Logical Word
	/// 
	/// Logically right shift 32-bit values. Shifts in '0's into the upper bits.
	/// @param result Variable identifier to store result to. Must be of type 'm128u32'
	/// @param value Variable identifier to use as source value. Must be of type 'm128u32'
	/// @param shift_amount Amount of bits to shift the source value left. Must be in range [0, 31]
	#define PSRLW(result, value, shift_amount)														\
		asm(																						\
			"psrlw	%[Result],%[Value],%c[ShiftAmount]"												\
			: [Result] "=r" ((result).v)															\
			: [Value] "r" ((value).v),																\
			  [ShiftAmount] "n" (shift_amount)														\
		)
#else
	/// @brief PSRLW : Parallel Shift Right Logical Word
	/// 
	/// Logically right shift 32-bit values. Shifts in '0's into the upper bits.
	/// @param result Variable identifier to store result to. Must be of type 'm128u32'
	/// @param value Variable identifier to use as source value. Must be of type 'm128u32'
	/// @param shift_amount Amount of bits to shift the source value left. Must be in range [0, 31]
	#define PSRLW(result, value, shift_amount)														\
		asm(																						\
			"pcpyld	%[ResultLo],%[ValueHi],%[ValueLo]\n\t"											\
			"psrlw	%[ResultLo],%[ResultLo],%c[ShiftAmount]\n\t"									\
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"											\
			: [ResultLo] "=r" ((result).lo),														\
			  [ResultHi] "=&r" ((result).hi)														\
			: [ValueLo] "r" ((value).lo),															\
			  [ValueHi] "r" ((value).hi),															\
			  [ShiftAmount] "n" (shift_amount)														\
		)
#endif

	/// @brief PSRAVW : Parallel Shift Right Logical Variable Word
	/// 
	/// Treat 128-bit packed integer data as 2 sign-extended 32-bit values. Shift each value
	/// right while shifting in '0's by an amount equal to the corresponding value in
	/// 'shift_amount'. Only the low 5 bits in each value of 'shift_amount' are used, others
	/// are ignored.
	/// @param value Values to shift
	/// @param shift_amount Amount to shift values by
	/// @return Values shifted right by variable amount
	FORCEINLINE CONST m128u64 mm_srlv_epu64(m128u64 value, m128u64 shift_amount)
	{
		m128u64 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"psrlvw	%[Result],%[Value],%[Amount]"
			: [Result] "=r" (result.v)
			: [Value] "r" (value.v),
			  [Amount] "r" (shift_amount.v)
		);
#else
		asm(
			"pcpyld	%[ValueLo],%[ValueHi],%[ValueLo]\n\t"
			"pcpyld	%[AmountLo],%[AmountHi],%[AmountLo]\n\t"
			"psrlvw	%[ResultLo],%[ValueLo],%[AmountLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [ValueLo] "r" (value.lo),
			  [ValueHi] "r" (value.hi),
			  [AmountLo] "r" (shift_amount.lo),
			  [AmountHi] "r" (shift_amount.hi)
		);
#endif

		return result;
	}


	/// @brief PABSH : Parallel ABSolute Halfword
	/// 
	/// Calculate the absolute value of 8 16-bit signed values.
	/// 
	/// If any of the values is '0x8000' then the result will be truncated to '0x7FFF', or one
	/// less than the accurate absolute value.
	/// @param v Values to calculate the absolute value of
	/// @return Absolute values of argument with truncation
	FORCEINLINE CONST m128i16 mm_abs_epi16(m128i16 v)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pabsh	%[Result],%[Value]"
			: [Result] "=r" (result.v)
			: [Value] "r" (v.v)
		);
#else
		asm(
			"pcpyld	%[ValueLo],%[ValueHi],%[ValueLo]\n\t"
			"pabsh	%[ResultLo],%[ValueLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [ValueLo] "r" (v.lo),
			  [ValueHi] "r" (v.hi)
		);
#endif

		return result;
	}

	/// @brief PABSW : Parallel ABSolute Word
	/// 
	/// Calculate the absolute value of 4 32-bit signed values.
	/// 
	/// If any of the values is '0x80000000' then the result will be truncated to '0x7FFFFFFF', or
	/// one less than the accurate absolute value.
	/// @param v Values to calculate the absolute value of
	/// @return Absolute values of argument with truncation
	FORCEINLINE CONST m128i32 mm_abs_epi32(m128i32 v)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pabsw	%[Result],%[Value]"
			: [Result] "=r" (result.v)
			: [Value] "r" (v.v)
		);
#else
		asm(
			"pcpyld	%[ValueLo],%[ValueHi],%[ValueLo]\n\t"
			"pabsw	%[ResultLo],%[ValueLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=r" (result.hi)
			: [ValueLo] "r" (v.lo),
			  [ValueHi] "r" (v.hi)
		);
#endif

		return result;
	}

	/// @brief PMAXH : Parallel MAXimum Halfword
	/// 
	/// Calculate the maximum value of 8 16-bit signed value pairs.
	/// 
	/// Unsigned maximum can be achieved by first adding '0x8000' to both operands and then
	/// subtracting it again after.
	/// @param l First operand
	/// @param r Second operand
	/// @return Maximum value of each value pair
	FORCEINLINE CONST m128i16 mm_max_epi16(m128i16 l, m128i16 r)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pmaxh	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"pmaxh	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PMAXW : Parallel MAXimum Word
	/// 
	/// Calculate the maximum value of 4 32-bit signed value pairs.
	/// 
	/// Unsigned maximum can be achieved by first adding '0x80000000' to both operands and then
	/// subtracting it again after.
	/// @param l First operand
	/// @param r Second operand
	/// @return Maximum value of each value pair
	FORCEINLINE CONST m128i32 mm_max_epi32(m128i32 l, m128i32 r)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pmaxw	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"pmaxw	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PMINH : Parallel MINimum Halfword
	/// 
	/// Calculate the minimum value of 8 16-bit signed value pairs.
	/// 
	/// Unsigned minimum can be achieved by first adding '0x8000' to both operands and then
	/// subtracting it again after.
	/// @param l First operand
	/// @param r Second operand
	/// @return Minimum value of each value pair
	FORCEINLINE CONST m128i16 mm_min_epi16(m128i16 l, m128i16 r)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pminh	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"pminh	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PMINW : Parallel MINimum Word
	/// 
	/// Calculate the minimum value of 4 32-bit signed value pairs.
	/// 
	/// Unsigned minimum can be achieved by first adding '0x80000000' to both operands and then
	/// subtracting it again after.
	/// @param l First operand
	/// @param r Second operand
	/// @return Minimum value of each value pair
	FORCEINLINE CONST m128i32 mm_min_epi32(m128i32 l, m128i32 r)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pminw	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"pminw	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}


	/// @brief PADDB : Parallel ADD Byte
	/// 
	/// Add 16 8-bit value pairs.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of addition of both operands
	FORCEINLINE CONST m128i8 mm_add_epi8(m128i8 l, m128i8 r)
	{
		m128i8 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"paddb	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"paddb	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PADDB : Parallel ADD Byte
	/// 
	/// Add 16 8-bit value pairs.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of addition of both operands
	FORCEINLINE CONST m128u8 mm_add_epu8(m128u8 l, m128u8 r)
	{
		return mm_castepu8_epi8(mm_add_epi8(mm_castepi8_epu8(l), mm_castepi8_epu8(r)));
	}
	
	/// @brief PADDH : Parallel ADD Halfword
	/// 
	/// Add 8 16-bit value pairs.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of addition of both operands
	FORCEINLINE CONST m128i16 mm_add_epi16(m128i16 l, m128i16 r)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"paddh	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"paddh	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PADDH : Parallel ADD Halfword
	/// 
	/// Add 8 16-bit value pairs.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of addition of both operands
	FORCEINLINE CONST m128u16 mm_add_epu16(m128u16 l, m128u16 r)
	{
		return mm_castepu16_epi16(mm_add_epi16(mm_castepi16_epu16(l), mm_castepi16_epu16(r)));
	}

	/// @brief PADDW : Parallel ADD Word
	/// 
	/// Add 4 32-bit value pairs.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of addition of both operands
	FORCEINLINE CONST m128i32 mm_add_epi32(m128i32 l, m128i32 r)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"paddw	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"paddw	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PADDW : Parallel ADD Word
	/// 
	/// Add 4 32-bit value pairs.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of addition of both operands
	FORCEINLINE CONST m128u32 mm_add_epu32(m128u32 l, m128u32 r)
	{
		return mm_castepu32_epi32(mm_add_epi32(mm_castepi32_epu32(l), mm_castepi32_epu32(r)));
	}

	/// @brief PADDSB : Parallel ADD Signed saturation Byte
	/// 
	/// Add 16 8-bit value pairs. Saturate the result to the extreme values of int8_t instead of
	/// wraparound.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of signed saturated addition of both operands
	FORCEINLINE CONST m128i8 mm_adds_epi8(m128i8 l, m128i8 r)
	{
		m128i8 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"paddsb	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"paddsb	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PADDUB : Parallel ADD Unsigned saturation Byte
	/// 
	/// Add 16 8-bit value pairs. Saturate the result to the extreme values of uint8_t instead of
	/// wraparound.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of unsigned saturated addition of both operands
	FORCEINLINE CONST m128u8 mm_adds_epu8(m128u8 l, m128u8 r)
	{
		m128u8 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"paddub	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"paddub	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PADDSH : Parallel ADD Signed saturation Halfword
	/// 
	/// Add 8 16-bit value pairs. Saturate the result to the extreme values of int16_t instead of
	/// wraparound.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of signed saturated addition of both operands
	FORCEINLINE CONST m128i16 mm_adds_epi16(m128i16 l, m128i16 r)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"paddsh	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"paddsh	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PADDUH : Parallel ADD Unsigned saturation Halfword
	/// 
	/// Add 8 16-bit value pairs. Saturate the result to the extreme values of uint16_t instead of
	/// wraparound.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of unsigned saturated addition of both operands
	FORCEINLINE CONST m128u16 mm_adds_epu16(m128u16 l, m128u16 r)
	{
		m128u16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"padduh	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"padduh	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PADDSW : Parallel ADD Signed saturation Word
	/// 
	/// Add 4 32-bit value pairs. Saturate the result to the extreme values of int32_t instead of
	/// wraparound.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of signed saturated addition of both operands
	FORCEINLINE CONST m128i32 mm_adds_epi32(m128i32 l, m128i32 r)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"paddsw	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"paddsw	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PADDUW : Parallel ADD Unsigned saturation Word
	/// 
	/// Add 4 32-bit value pairs. Saturate the result to the extreme values of uint32_t instead of
	/// wraparound.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of unsigned saturated addition of both operands
	FORCEINLINE CONST m128u32 mm_adds_epu32(m128u32 l, m128u32 r)
	{
		m128u32 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"padduw	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"padduw	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PADSBH : Parallel ADd/SuBtract Halfword
	/// 
	/// Split each group of 8 16-bit signed values into a high and a low group of 4. Valculate
	/// 'l + r' for the high group and 'l - r' for the low group. This uses regular non-saturating
	/// arithmethic.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of addition/subtraction of operands
	FORCEINLINE CONST m128i16 mm_addsub_epi16(m128i16 l, m128i16 r)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"padsbh	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"padsbh	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}


	/// @brief PSUBB : Parallel SUBtract Byte
	/// 
	/// Subtract 16 8-bit value pairs.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of subtraction of both operands
	FORCEINLINE CONST m128i8 mm_sub_epi8(m128i8 l, m128i8 r)
	{
		m128i8 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"psubb	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"psubb	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PSUBB : Parallel SUBtract Byte
	/// 
	/// Subtract 16 8-bit value pairs.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of subtraction of both operands
	FORCEINLINE CONST m128u8 mm_sub_epu8(m128u8 l, m128u8 r)
	{
		return mm_castepu8_epi8(mm_sub_epi8(mm_castepi8_epu8(l), mm_castepi8_epu8(r)));
	}

	/// @brief PSUBH : Parallel SUBtract Halfword
	/// 
	/// Subtract 8 16-bit value pairs.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of subtraction of both operands
	FORCEINLINE CONST m128i16 mm_sub_epi16(m128i16 l, m128i16 r)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"psubh	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"psubh	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PSUBH : Parallel SUBtract Halfword
	/// 
	/// Subtract 8 16-bit value pairs.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of subtraction of both operands
	FORCEINLINE CONST m128u16 mm_sub_epu16(m128u16 l, m128u16 r)
	{
		return mm_castepu16_epi16(mm_sub_epi16(mm_castepi16_epu16(l), mm_castepi16_epu16(r)));
	}

	/// @brief PSUBW : Parallel SUBtract Word
	/// 
	/// Subtract 4 32-bit value pairs.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of subtraction of both operands
	FORCEINLINE CONST m128i32 mm_sub_epi32(m128i32 l, m128i32 r)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"psubw	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"psubw	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PSUBW : Parallel SUBtract Word
	/// 
	/// Subtract 4 32-bit value pairs.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of subtraction of both operands
	FORCEINLINE CONST m128u32 mm_sub_epu32(m128u32 l, m128u32 r)
	{
		return mm_castepu32_epi32(mm_sub_epi32(mm_castepi32_epu32(l), mm_castepi32_epu32(r)));
	}

	/// @brief PSUBSB : Parallel SUBtract Signed saturation Byte
	/// 
	/// Subtract 16 8-bit value pairs. Saturate the result to the extreme values of int8_t instead
	/// of wraparound.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of signed saturated subtraction of both operands
	FORCEINLINE CONST m128i8 mm_subs_epi8(m128i8 l, m128i8 r)
	{
		m128i8 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"psubsb	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"psubsb	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PSUBUB : Parallel SUBtract Unsigned saturation Byte
	/// 
	/// Subtract 16 8-bit value pairs. Saturate the result to the extreme values of uint8_t
	/// instead of wraparound.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of unsigned saturated subtraction of both operands
	FORCEINLINE CONST m128u8 mm_subs_epu8(m128u8 l, m128u8 r)
	{
		m128u8 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"psubub	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"psubub	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PSUBSH : Parallel SUBtract Signed saturation Halfword
	/// 
	/// Subtract 8 16-bit value pairs. Saturate the result to the extreme values of int16_t
	/// instead of wraparound.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of signed saturated subtraction of both operands
	FORCEINLINE CONST m128i16 mm_subs_epi16(m128i16 l, m128i16 r)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"psubsh	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"psubsh	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PSUBUH : Parallel SUBtract Unsigned saturation Halfword
	/// 
	/// Subtract 8 16-bit value pairs. Saturate the result to the extreme values of uint16_t
	/// instead of wraparound.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of unsigned saturated addition of both operands
	FORCEINLINE CONST m128u16 mm_subs_epu16(m128u16 l, m128u16 r)
	{
		m128u16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"psubuh	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"psubuh	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PSUBSW : Parallel SUBtract Signed saturation Word
	/// 
	/// Subtract 4 32-bit value pairs. Saturate the result to the extreme values of int32_t
	/// instead of wraparound.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of signed saturated subtraction of both operands
	FORCEINLINE CONST m128i32 mm_subs_epi32(m128i32 l, m128i32 r)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"psubsw	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"psubsw	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PSUBUW : Parallel SUBtract Unsigned saturation Word
	/// 
	/// Subtract 4 32-bit value pairs. Saturate the result to the extreme values of uint32_t
	/// instead of wraparound.
	/// @param l First operand
	/// @param r Second operand
	/// @return Result of unsigned saturated subtraction of both operands
	FORCEINLINE CONST m128u32 mm_subs_epu32(m128u32 l, m128u32 r)
	{
		m128u32 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"psubuw	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "r" (l.v),
			  [Right] "r" (r.v)
		);
#else
		asm(
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"psubuw	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}


	/// @brief PMULTH : Parallel MULTiply Halfword
	/// 
	/// Multiply 8 pairs of 16-bit signed values.
	/// 
	/// Let each pair have a signed 32-bit product named p0..p7. Store p0, p2, p4 and p6 into the
	/// return value. Store p0, p1, p4 and p5 into LO and p2, p3, p6 and p7 into HI.
	/// 
	/// Bitwise result:
	///		Return value:	[ 31,   0] = p0
	///						[ 63,  32] = p2
	///						[ 95,  64] = p4
	///						[127,  96] = p6
	///		LO:				[ 31,   0] = p0
	///						[ 63,  32] = p1
	///						[ 95,  64] = p4
	///						[127,  96] = p5
	///		HI:				[ 31,   0] = p2
	///						[ 63,  32] = p3
	///						[ 95,  64] = p6
	///						[127,  96] = p7
	/// 
	/// Multiplication happens asynchronously. Reading from the return value, the LO or the HI
	/// register will stall the EE Core until the result is ready.
	/// 
	/// You can extract only the products not in the return value using 'mm_loadlohi_upper_epi32'.
	/// 
	/// This function writes to global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param l First operand
	/// @param r Second operand
	/// @return Multiplication result of value pairs in even positions
	FORCEINLINE m128i32 mm_mul_epi16(lohi_state_t* state, m128i16 l, m128i16 r)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// volatile because writes to LO/HI
		asm volatile(
			"pmulth	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
			: "lo", "hi"
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"pmflo	%[TmpLo]\n\t"
			"pmfhi	%[TmpHi]\n\t"
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"pmulth	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]\n\t"
			"pmflo	%[StateLo0]\n\t"
			"pmfhi	%[StateHi0]\n\t"
			"pcpyud	%[StateLo1],%[StateLo0],%[StateLo0]\n\t"
			"pcpyud	%[StateHi1],%[StateHi0],%[StateHi0]\n\t"
			"pmtlo	%[TmpLo]\n\t"
			"pmthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi),
			  [StateLo0] "=r" (state->lo[0]),
			  [StateLo1] "=r" (state->lo[1]),
			  [StateHi0] "=r" (state->hi[0]),
			  [StateHi1] "=r" (state->hi[1])
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PMULTW : Parallel MULTiply Word
	/// 
	/// Multiply 2 pairs of signed 32-bit values. The input operands are treated as sign-extended
	/// 32-bit values. The 64-bit products are written to the return value. The low 32 bits of
	/// the products are written to even positions of the LO register and sign-extended. The high
	/// 32 bits are written to even positions of the HI register and sign extended.
	/// 
	/// Multiplication happens asynchronously. Reading from the return value, the LO or the HI
	/// register will stall the EE Core until the result is ready.
	/// 
	/// This function writes to global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param l First operand
	/// @param r Second operand
	/// @return Multiplication result containing full 64-bit signed values
	FORCEINLINE m128i64 mm_mul_epi64(lohi_state_t* state, m128i64 l, m128i64 r)
	{
		m128i64 result = {};

#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// volatile because writes to LO/HI
		asm volatile(
			"pmultw	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
			: "lo", "hi"
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"pmflo	%[TmpLo]\n\t"
			"pmfhi	%[TmpHi]\n\t"
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"pmultw	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]\n\t"
			"pmflo	%[StateLo0]\n\t"
			"pmfhi	%[StateHi0]\n\t"
			"pcpyud	%[StateLo1],%[StateLo0],%[StateLo0]\n\t"
			"pcpyud	%[StateHi1],%[StateHi0],%[StateHi0]\n\t"
			"pmtlo	%[TmpLo]\n\t"
			"pmthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi),
			  [StateLo0] "=r" (state->lo[0]),
			  [StateLo1] "=r" (state->lo[1]),
			  [StateHi0] "=r" (state->hi[0]),
			  [StateHi1] "=r" (state->hi[1])
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PMULTUW : Parallel MULTiply Unsigned Word
	/// 
	/// Multiply 2 pairs of unsigned 32-bit values. The input operands are treated as sign-
	/// extended 32-bit values. The 64-bit products are written to the return value. The low 32
	/// bits of the products are written to even positions of the LO register and sign-extended.
	/// The high 32 bits are written to even positions of the HI register and sign extended.
	/// 
	/// Multiplication happens asynchronously. Reading from the return value, the LO or the HI
	/// register will stall the EE Core until the result is ready.
	/// 
	/// This function writes to global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param l First operand
	/// @param r Second operand
	/// @return Multiplication result containing full 64-bit unsigned values
	FORCEINLINE m128u64 mm_mul_epu64(lohi_state_t* state, m128u64 l, m128u64 r)
	{
		m128u64 result = {};

#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// volatile because writes to LO/HI
		asm volatile(
			"pmultuw	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
			: "lo", "hi"
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"pmflo	%[TmpLo]\n\t"
			"pmfhi	%[TmpHi]\n\t"
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"pmultuw	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]\n\t"
			"pmflo	%[StateLo0]\n\t"
			"pmfhi	%[StateHi0]\n\t"
			"pcpyud	%[StateLo1],%[StateLo0],%[StateLo0]\n\t"
			"pcpyud	%[StateHi1],%[StateHi0],%[StateHi0]\n\t"
			"pmtlo	%[TmpLo]\n\t"
			"pmthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi),
			  [StateLo0] "=r" (state->lo[0]),
			  [StateLo1] "=r" (state->lo[1]),
			  [StateHi0] "=r" (state->hi[0]),
			  [StateHi1] "=r" (state->hi[1])
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PMADDH : Parallel Multiply-ADD Halfword
	/// 
	/// Multiply 8 pairs of 16-bit signed values and accumulate.
	/// 
	/// Let each pair have a signed 32-bit product named p0..p7. Assign each product a 32-bit
	/// accumulator a0..a7 according to the output locations in LO/HI of 'mm_mul_epi16'. Add
	/// the product to the accumulator and store a0, a2, a4 and a6 into the return value. Store
	/// a0, a1, a4 and a5 into LO and a2, a3, a6 and a7 into HI.
	/// 
	/// Bitwise accumulation:
	///		LO:				[ 31,   0] = a0
	///						[ 63,  32] = a1
	///						[ 95,  64] = a4
	///						[127,  96] = a5
	///		HI:				[ 31,   0] = a2
	///						[ 63,  32] = a3
	///						[ 95,  64] = a6
	///						[127,  96] = a7
	///		Return value:	[ 31,   0] = LO[ 31,   0]
	///						[ 63,  32] = HI[ 31,   0]
	///						[ 95,  64] = LO[ 95,  64]
	///						[127,  96] = HI[ 95,  64]
	/// 
	/// Multiplication happens asynchronously. Reading from the return value, the LO or the HI
	/// register will stall the EE Core until the result is ready.
	/// 
	/// You can extract only the accumulators not in the return value using
	/// 'mm_loadlohi_upper_epi32'.
	/// 
	/// This function writes to global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param l First operand
	/// @param r Second operand
	/// @return Multipy-accumulate result of value pairs in even positions
	FORCEINLINE m128i32 mm_fma_epi16(lohi_state_t* state, m128i16 l, m128i16 r)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// volatile because writes to LO/HI
		asm volatile(
			"pmaddh	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
			: "lo", "hi"
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"pmflo	%[TmpLo]\n\t"
			"pmfhi	%[TmpHi]\n\t"
			"pcpyld	%[StateLo0],%[StateLo1],%[StateLo0]\n\t"
			"pcpyld	%[StateHi0],%[StateHi1],%[StateHi0]\n\t"
			"pmtlo	%[StateLo0]\n\t"
			"pmthi	%[StateHi0]\n\t"
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"pmaddh	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]\n\t"
			"pmflo	%[StateLo0]\n\t"
			"pmfhi	%[StateHi0]\n\t"
			"pcpyud	%[StateLo1],%[StateLo0],%[StateLo0]\n\t"
			"pcpyud	%[StateHi1],%[StateHi0],%[StateHi0]\n\t"
			"pmtlo	%[TmpLo]\n\t"
			"pmthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [ResultLo] "=r" (result.lo),
			  [ResultHi] "=r" (result.hi),
			  [StateLo0] "+r" (state->lo[0]),
			  [StateLo1] "+r" (state->lo[1]),
			  [StateHi0] "+r" (state->hi[0]),
			  [StateHi1] "+r" (state->hi[1])
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PMADDW : Parallel Multiply-ADD Word
	/// 
	/// Multiply 2 pairs of signed 32-bit values and accumulate. The input operands are treated
	/// as sign-extended 32-bit values. Assign each product a 64-bit accumulator a0..a1 in LO/HI
	/// according to the output locations of 'mm_mul_epi64'. The products are added to the
	/// accumulator and written to the return value. The low 32 bits of the accumulators are
	/// written to even positions of the LO register and sign-extended. The high 32 bits are
	/// written to even positions of the HI register and sign extended.
	/// 
	/// Multiplication happens asynchronously. Reading from the return value, the LO or the HI
	/// register will stall the EE Core until the result is ready.
	/// 
	/// This function writes to global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param l First operand
	/// @param r Second operand
	/// @return Multiply-accumulate result containing full 64-bit signed values
	FORCEINLINE m128i64 mm_fma_epi64(lohi_state_t* state, m128i64 l, m128i64 r)
	{
		m128i64 result = {};

#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// volatile because writes to LO/HI
		asm volatile(
			"pmaddw	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
			: "lo", "hi"
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"pmflo	%[TmpLo]\n\t"
			"pmfhi	%[TmpHi]\n\t"
			"pcpyld	%[StateLo0],%[StateLo1],%[StateLo0]\n\t"
			"pcpyld	%[StateHi0],%[StateHi1],%[StateHi0]\n\t"
			"pmtlo	%[StateLo0]\n\t"
			"pmthi	%[StateHi0]\n\t"
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"pmaddw	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]\n\t"
			"pmflo	%[StateLo0]\n\t"
			"pmfhi	%[StateHi0]\n\t"
			"pcpyud	%[StateLo1],%[StateLo0],%[StateLo0]\n\t"
			"pcpyud	%[StateHi1],%[StateHi0],%[StateHi0]\n\t"
			"pmtlo	%[TmpLo]\n\t"
			"pmthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [ResultLo] "=r" (result.lo),
			  [ResultHi] "=r" (result.hi),
			  [StateLo0] "+r" (state->lo[0]),
			  [StateLo1] "+r" (state->lo[1]),
			  [StateHi0] "+r" (state->hi[0]),
			  [StateHi1] "+r" (state->hi[1])
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PMADDUW : Parallel Multiply-ADD Unsigned Word
	/// 
	/// Multiply 2 pairs of unsigned 32-bit values and accumulate. The input operands are treated
	/// as sign-extended 32-bit values. Assign each product a 64-bit accumulator a0..a1 in LO/HI
	/// according to the output locations of 'mm_mul_epu64'. The products are added to the
	/// accumulator and written to the return value. The low 32 bits of the accumulators are
	/// written to even positions of the LO register and sign-extended. The high 32 bits are
	/// written to even positions of the HI register and sign extended.
	/// 
	/// Multiplication happens asynchronously. Reading from the return value, the LO or the HI
	/// register will stall the EE Core until the result is ready.
	/// 
	/// This function writes to global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param l First operand
	/// @param r Second operand
	/// @return Multiply-accumulate result containing full 64-bit unsigned values
	FORCEINLINE m128u64 mm_fma_epu64(lohi_state_t* state, m128u64 l, m128u64 r)
	{
		m128u64 result = {};

#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// volatile because writes to LO/HI
		asm volatile(
			"pmadduw	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
			: "lo", "hi"
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"pmflo	%[TmpLo]\n\t"
			"pmfhi	%[TmpHi]\n\t"
			"pcpyld	%[StateLo0],%[StateLo1],%[StateLo0]\n\t"
			"pcpyld	%[StateHi0],%[StateHi1],%[StateHi0]\n\t"
			"pmtlo	%[StateLo0]\n\t"
			"pmthi	%[StateHi0]\n\t"
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"pmadduw	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]\n\t"
			"pmflo	%[StateLo0]\n\t"
			"pmfhi	%[StateHi0]\n\t"
			"pcpyud	%[StateLo1],%[StateLo0],%[StateLo0]\n\t"
			"pcpyud	%[StateHi1],%[StateHi0],%[StateHi0]\n\t"
			"pmtlo	%[TmpLo]\n\t"
			"pmthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [ResultLo] "=r" (result.lo),
			  [ResultHi] "=r" (result.hi),
			  [StateLo0] "+r" (state->lo[0]),
			  [StateLo1] "+r" (state->lo[1]),
			  [StateHi0] "+r" (state->hi[0]),
			  [StateHi1] "+r" (state->hi[1])
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PMSUBH : Parallel Multiply-SUBtract Halfword
	/// 
	/// Works exactly as 'mm_fma_epi16' but subtracts the product from the accumulators instead.
	/// 
	/// Multiplication happens asynchronously. Reading from the return value, the LO or the HI
	/// register will stall the EE Core until the result is ready.
	/// 
	/// You can extract only the accumulators not in the return value using
	/// 'mm_loadlohi_upper_epi32'.
	/// 
	/// This function writes to global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param l First operand
	/// @param r Second operand
	/// @return Multipy-accumulate result of value pairs in even positions
	FORCEINLINE m128i32 mm_fms_epi16(lohi_state_t* state, m128i16 l, m128i16 r)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// volatile because writes to LO/HI
		asm volatile(
			"pmsubh	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
			: "lo", "hi"
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"pmflo	%[TmpLo]\n\t"
			"pmfhi	%[TmpHi]\n\t"
			"pcpyld	%[StateLo0],%[StateLo1],%[StateLo0]\n\t"
			"pcpyld	%[StateHi0],%[StateHi1],%[StateHi0]\n\t"
			"pmtlo	%[StateLo0]\n\t"
			"pmthi	%[StateHi0]\n\t"
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"pmsubh	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]\n\t"
			"pmflo	%[StateLo0]\n\t"
			"pmfhi	%[StateHi0]\n\t"
			"pcpyud	%[StateLo1],%[StateLo0],%[StateLo0]\n\t"
			"pcpyud	%[StateHi1],%[StateHi0],%[StateHi0]\n\t"
			"pmtlo	%[TmpLo]\n\t"
			"pmthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [ResultLo] "=r" (result.lo),
			  [ResultHi] "=r" (result.hi),
			  [StateLo0] "+r" (state->lo[0]),
			  [StateLo1] "+r" (state->lo[1]),
			  [StateHi0] "+r" (state->hi[0]),
			  [StateHi1] "+r" (state->hi[1])
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PMSUBW : Parallel Multiply-SUBtract Word
	/// 
	/// Works exactly as 'mm_fma_epi64' but subtracts the product from the accumulators instead.
	/// 
	/// Multiplication happens asynchronously. Reading from the return value, the LO or the HI
	/// register will stall the EE Core until the result is ready.
	/// 
	/// This function writes to global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param l First operand
	/// @param r Second operand
	/// @return Multiply-accumulate result containing full 64-bit signed values
	FORCEINLINE m128i64 mm_fms_epi64(lohi_state_t* state, m128i64 l, m128i64 r)
	{
		m128i64 result = {};

#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// volatile because writes to LO/HI
		asm volatile(
			"pmsubw	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
			: "lo", "hi"
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"pmflo	%[TmpLo]\n\t"
			"pmfhi	%[TmpHi]\n\t"
			"pcpyld	%[StateLo0],%[StateLo1],%[StateLo0]\n\t"
			"pcpyld	%[StateHi0],%[StateHi1],%[StateHi0]\n\t"
			"pmtlo	%[StateLo0]\n\t"
			"pmthi	%[StateHi0]\n\t"
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"pmsubw	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]\n\t"
			"pmflo	%[StateLo0]\n\t"
			"pmfhi	%[StateHi0]\n\t"
			"pcpyud	%[StateLo1],%[StateLo0],%[StateLo0]\n\t"
			"pcpyud	%[StateHi1],%[StateHi0],%[StateHi0]\n\t"
			"pmtlo	%[TmpLo]\n\t"
			"pmthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [ResultLo] "=r" (result.lo),
			  [ResultHi] "=r" (result.hi),
			  [StateLo0] "+r" (state->lo[0]),
			  [StateLo1] "+r" (state->lo[1]),
			  [StateHi0] "+r" (state->hi[0]),
			  [StateHi1] "+r" (state->hi[1])
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PHMADH : Parallel Horizontal Multiply-ADd Halfword
	/// 
	/// Split 8 signed 16-bit values into 4 groups of 2. Multiply corresponding values from both
	/// operands into intermediate 32-bit values. Add the 2 intermediate values within a group.
	/// The result of group 0 (input values at index 0 and 1) is written to the return value at
	/// index 0 and the LO register at bits [0, 31]. The result of group 1 (input values at index
	/// 2 and 3) is written to the return value at index 1 and the HI register at bits [0, 31].
	/// The result of group 2 (4 and 5) is at index 2 and in LO[64, 95]. The result of group 3 (6
	/// and 7) is at index 3 and in HI[64, 95].
	/// 
	/// Bitwise result:
	///		tmp0	=	l[0, 15] * r[0, 15] + l[16, 31] * r[16, 31]
	///		tmp1	=	l[0, 15] * r[0, 15] + l[16, 31] * r[16, 31]
	///		tmp2	=	l[0, 15] * r[0, 15] + l[16, 31] * r[16, 31]
	///		tmp3	=	l[0, 15] * r[0, 15] + l[16, 31] * r[16, 31]
	///		Return value[ 0,  31]	=	tmp0
	///		Return value[32,  63]	=	tmp1
	///		Return value[64,  95]	=	tmp2
	///		Return value[96, 127]	=	tmp3
	///		LO[ 0,  31]	=	tmp0
	///		LO[32,  63]	=	<undefined>
	///		LO[64,  95]	=	tmp2
	///		LO[96, 127]	=	<undefined>
	///		HI[ 0,  31]	=	tmp1
	///		HI[32,  63]	=	<undefined>
	///		HI[64,  95]	=	tmp3
	///		HI[96, 127]	=	<undefined>
	/// 
	/// Multiplication happens asynchronously. Reading from the return value, the LO or the HI
	/// register will stall the EE Core until the result is ready.
	/// 
	/// This function writes to global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param l First Operand
	/// @param r Second Operand
	/// @return Horizontal sums of multiplication products
	FORCEINLINE m128i32 mm_hmuladd_epi16(lohi_state_t* state, m128i16 l, m128i16 r)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// volatile because writes to LO/HI
		asm volatile(
			"phmadh	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
			: "lo", "hi"
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"pmflo	%[TmpLo]\n\t"
			"pmfhi	%[TmpHi]\n\t"
			"pcpyld	%[StateLo0],%[StateLo1],%[StateLo0]\n\t"
			"pcpyld	%[StateHi0],%[StateHi1],%[StateHi0]\n\t"
			"pmtlo	%[StateLo0]\n\t"
			"pmthi	%[StateHi0]\n\t"
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"phmadh	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]\n\t"
			"pmflo	%[StateLo0]\n\t"
			"pmfhi	%[StateHi0]\n\t"
			"pcpyud	%[StateLo1],%[StateLo0],%[StateLo0]\n\t"
			"pcpyud	%[StateHi1],%[StateHi0],%[StateHi0]\n\t"
			"pmtlo	%[TmpLo]\n\t"
			"pmthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [ResultLo] "=r" (result.lo),
			  [ResultHi] "=r" (result.hi),
			  [StateLo0] "+r" (state->lo[0]),
			  [StateLo1] "+r" (state->lo[1]),
			  [StateHi0] "+r" (state->hi[0]),
			  [StateHi1] "+r" (state->hi[1])
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}

	/// @brief PHMSBH : Parallel Horizontal Multiply-SuBtract Halfword
	/// 
	/// Split 8 signed 16-bit values into 4 groups of 2. Multiply corresponding values from both
	/// operands into intermediate 32-bit values. Subtract the 2 intermediate values within a
	/// group. The result of group 0 (input values at index 0 and 1) is written to the return
	/// value at index 0 and the LO register at bits [0, 31]. The result of group 1 (input values
	/// at index 2 and 3) is written to the return value at index 1 and the HI register at bits
	/// [0, 31]. The result of group 2 (4 and 5) is at index 2 and in LO[64, 95]. The result of
	/// group 3 (6 and 7) is at index 3 and in HI[64, 95].
	/// 
	/// Bitwise result:
	///		tmp0	=	l[0, 15] * r[0, 15] - l[16, 31] * r[16, 31]
	///		tmp1	=	l[0, 15] * r[0, 15] - l[16, 31] * r[16, 31]
	///		tmp2	=	l[0, 15] * r[0, 15] - l[16, 31] * r[16, 31]
	///		tmp3	=	l[0, 15] * r[0, 15] - l[16, 31] * r[16, 31]
	///		Return value[ 0,  31]	=	tmp0
	///		Return value[32,  63]	=	tmp1
	///		Return value[64,  95]	=	tmp2
	///		Return value[96, 127]	=	tmp3
	///		LO[ 0,  31]	=	tmp0
	///		LO[32,  63]	=	<undefined>
	///		LO[64,  95]	=	tmp2
	///		LO[96, 127]	=	<undefined>
	///		HI[ 0,  31]	=	tmp1
	///		HI[32,  63]	=	<undefined>
	///		HI[64,  95]	=	tmp3
	///		HI[96, 127]	=	<undefined>
	/// 
	/// Multiplication happens asynchronously. Reading from the return value, the LO or the HI
	/// register will stall the EE Core until the result is ready.
	/// 
	/// This function writes to global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param l First Operand
	/// @param r Second Operand
	/// @return Horizontal differences of multiplication products
	FORCEINLINE m128i32 mm_hmulsub_epi16(lohi_state_t* state, m128i16 l, m128i16 r)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// volatile because writes to LO/HI
		asm volatile(
			"phmsbh	%[Result],%[Left],%[Right]"
			: [Result] "=r" (result.v)
			: [Left] "%r" (l.v),
			  [Right] "r" (r.v)
			: "lo", "hi"
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"pmflo	%[TmpLo]\n\t"
			"pmfhi	%[TmpHi]\n\t"
			"pcpyld	%[StateLo0],%[StateLo1],%[StateLo0]\n\t"
			"pcpyld	%[StateHi0],%[StateHi1],%[StateHi0]\n\t"
			"pmtlo	%[StateLo0]\n\t"
			"pmthi	%[StateHi0]\n\t"
			"pcpyld	%[LeftLo],%[LeftHi],%[LeftLo]\n\t"
			"pcpyld	%[RightLo],%[RightHi],%[RightLo]\n\t"
			"phmsbh	%[ResultLo],%[LeftLo],%[RightLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]\n\t"
			"pmflo	%[StateLo0]\n\t"
			"pmfhi	%[StateHi0]\n\t"
			"pcpyud	%[StateLo1],%[StateLo0],%[StateLo0]\n\t"
			"pcpyud	%[StateHi1],%[StateHi0],%[StateHi0]\n\t"
			"pmtlo	%[TmpLo]\n\t"
			"pmthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [ResultLo] "=r" (result.lo),
			  [ResultHi] "=r" (result.hi),
			  [StateLo0] "+r" (state->lo[0]),
			  [StateLo1] "+r" (state->lo[1]),
			  [StateHi0] "+r" (state->hi[0]),
			  [StateHi1] "+r" (state->hi[1])
			: [LeftLo] "r" (l.lo),
			  [LeftHi] "r" (l.hi),
			  [RightLo] "r" (r.lo),
			  [RightHi] "r" (r.hi)
		);
#endif

		return result;
	}


	/// @brief PDIVW : Parallel DIVide Word
	/// 
	/// Treat each operand as containing 2 32-bit sign-extended values. Divide each pair and store
	/// the quotients in the LO register and the remainder in the HI register as sign-extended 32-
	/// bit values. See 'divrem0_i32_start' for further information regarding result signedness
	/// and overflow/div-by-0 behavior.
	/// 
	/// As integer division is done asynchronously on the EE Core you should issue the division
	/// instruction before checking for erroneous inputs like div-by-0 to improve throughput.
	/// Reading from the result registers (LO/HI) will cause a stall if the result is not ready at
	/// that point in time.
	/// 
	/// This function writes to global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param dividend Values to divide
	/// @param divisor Values to divide by
	FORCEINLINE void mm_divrem_epi64(lohi_state_t* state, m128i64 dividend, m128i64 divisor)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// volatile because writes to LO/HI
		asm volatile(
			"pdivw	%[Dividend],%[Divisor]"
			:
			: [Dividend] "r" (dividend.v),
			  [Divisor] "r" (divisor.v)
			: "lo", "hi"
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;
		
		asm(
			"pmflo	%[TmpLo]\n\t"
			"pmfhi	%[TmpHi]\n\t"
			"pcpyld	%[DividendLo],%[DividendHi],%[DividendLo]\n\t"
			"pcpyld	%[DivisorLo],%[DivisorHi],%[DivisorLo]\n\t"
			"pdivw	%[DividendLo],%[DivisorLo]\n\t"
			"pmflo	%[StateLo0]\n\t"
			"pmfhi	%[StateHi0]\n\t"
			"pcpyud	%[StateLo1],%[StateLo0],%[StateLo0]\n\t"
			"pcpyud	%[StateHi1],%[StateHi0],%[StateHi0]\n\t"
			"pmtlo	%[TmpLo]\n\t"
			"pmthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo0] "=r" (state->lo[0]),
			  [StateLo1] "=r" (state->lo[1]),
			  [StateHi0] "=r" (state->hi[0]),
			  [StateHi1] "=r" (state->hi[1])
			: [DividendLo] "r" (dividend.lo),
			  [DividendHi] "r" (dividend.hi),
			  [DivisorLo] "r" (divisor.lo),
			  [DivisorHi] "r" (divisor.hi)
		);
#endif
	}

	/// @brief PDIVUW : Parallel DIVide Unsigned Word
	/// 
	/// Treat each operand as containing 2 32-bit sign-extended values. Divide each pair and store
	/// the quotients in the LO register and the remainder in the HI register as sign-extended 32-
	/// bit values. See 'divrem0_u32_start' for further information regarding overflow/div-by-0
	/// behavior.
	/// 
	/// As integer division is done asynchronously on the EE Core you should issue the division
	/// instruction before checking for erroneous inputs like div-by-0 to improve throughput.
	/// Reading from the result registers (LO/HI) will cause a stall if the result is not ready at
	/// that point in time.
	/// 
	/// This function writes to global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param dividend Values to divide
	/// @param divisor Values to divide by
	FORCEINLINE void mm_divrem_epu64(lohi_state_t* state, m128u64 dividend, m128u64 divisor)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// volatile because writes to LO/HI
		asm volatile(
			"pdivuw	%[Dividend],%[Divisor]"
			:
			: [Dividend] "r" (dividend.v),
			  [Divisor] "r" (divisor.v)
			: "lo", "hi"
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"pmflo	%[TmpLo]\n\t"
			"pmfhi	%[TmpHi]\n\t"
			"pcpyld	%[DividendLo],%[DividendHi],%[DividendLo]\n\t"
			"pcpyld	%[DivisorLo],%[DivisorHi],%[DivisorLo]\n\t"
			"pdivuw	%[DividendLo],%[DivisorLo]\n\t"
			"pmflo	%[StateLo0]\n\t"
			"pmfhi	%[StateHi0]\n\t"
			"pcpyud	%[StateLo1],%[StateLo0],%[StateLo0]\n\t"
			"pcpyud	%[StateHi1],%[StateHi0],%[StateHi0]\n\t"
			"pmtlo	%[TmpLo]\n\t"
			"pmthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo0] "=r" (state->lo[0]),
			  [StateLo1] "=r" (state->lo[1]),
			  [StateHi0] "=r" (state->hi[0]),
			  [StateHi1] "=r" (state->hi[1])
			: [DividendLo] "r" (dividend.lo),
			  [DividendHi] "r" (dividend.hi),
			  [DivisorLo] "r" (divisor.lo),
			  [DivisorHi] "r" (divisor.hi)
		);
#endif
	}

	/// @brief PDIVBW : Parallel DIVide Broadcast Word
	/// 
	/// Divide 4 32-bit signed values by a single signed 16-bit value. Store the four quotients
	/// to the LO register and the remainders to the HI register.
	/// 
	/// As integer division is done asynchronously on the EE Core you should issue the division
	/// instruction before checking for erroneous inputs like div-by-0 to improve throughput.
	/// Reading from the result registers (LO/HI) will cause a stall if the result is not ready at
	/// that point in time.
	/// 
	/// This function writes to global state (LO/HI).
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param dividend Values to divide
	/// @param divisor Value to divide by
	FORCEINLINE void mm_divremb_epi32(lohi_state_t* state, m128i32 dividend, int16_t divisor)
	{
#ifdef PS2INTRIN_UNSAFE
		(void)state;

		// volatile because writes to LO/HI
		asm volatile(
			"pdivbw	%[Dividend],%[Divisor]"
			:
			: [Dividend] "r" (dividend.v),
			  [Divisor] "r" (divisor)
			: "lo", "hi"
		);
#else
		uint64_t tmplo = 0;
		uint64_t tmphi = 0;

		asm(
			"pmflo	%[TmpLo]\n\t"
			"pmfhi	%[TmpHi]\n\t"
			"pcpyld	%[DividendLo],%[DividendHi],%[DividendLo]\n\t"
			"pdivbw	%[DividendLo],%[Divisor]\n\t"
			"pmflo	%[StateLo0]\n\t"
			"pmfhi	%[StateHi0]\n\t"
			"pcpyud	%[StateLo1],%[StateLo0],%[StateLo0]\n\t"
			"pcpyud	%[StateHi1],%[StateHi0],%[StateHi0]\n\t"
			"pmtlo	%[TmpLo]\n\t"
			"pmthi	%[TmpHi]"
			: [TmpLo] "=&r" (tmplo),
			  [TmpHi] "=&r" (tmphi),
			  [StateLo0] "=r" (state->lo[0]),
			  [StateLo1] "=r" (state->lo[1]),
			  [StateHi0] "=r" (state->hi[0]),
			  [StateHi1] "=r" (state->hi[1])
			: [DividendLo] "r" (dividend.lo),
			  [DividendHi] "r" (dividend.hi),
			  [Divisor] "r" (divisor)
		);
#endif
	}


	/// @brief PCPYH : Parallel CoPY Halfword
	/// 
	/// Out of the 8 16-bit values in the input value select those at position 0 and 4. Broadcast
	/// the value at position 0 to positions 0, 1, 2 and 3; and the value at position 4 to
	/// positions 4, 5, 6, and 7 of the return value.
	/// @param v Value to broadcast from
	/// @return Broadcasted values
	FORCEINLINE CONST m128i16 mm_broadcast2_epi16(m128i16 v)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pcpyh	%[Result],%[Value]"
			: [Result] "=r" (result.v)
			: [Value] "r" (v.v)
		);
#else
		asm(
			"pcpyld	%[ValueLo],%[ValueHi],%[ValueLo]\n\t"
			"pcpyh	%[ResultLo],%[ValueLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [ValueLo] "r" (v.lo),
			  [ValueHi] "r" (v.hi)
		);
#endif

		return result;
	}

	/// @brief PCPYH : Parallel CoPY Halfword
	/// 
	/// Out of the 8 16-bit values in the input value select those at position 0 and 4. Broadcast
	/// the value at position 0 to positions 0, 1, 2 and 3; and the value at position 4 to
	/// positions 4, 5, 6, and 7 of the return value.
	/// @param v Value to broadcast from
	/// @return Broadcasted values
	FORCEINLINE CONST m128u16 mm_broadcast2_epu16(m128u16 v)
	{
		return mm_castepu16_epi16(mm_broadcast2_epi16(mm_castepi16_epu16(v)));
	}

	/// @brief PCPYLD : Parallel CoPY Lower Doubleword
	/// 
	/// Select the lower 64 bits of each input value and combine them into a 128-bit value.
	/// @param lower Value to put in the low 64 bits of the return value
	/// @param upper Value to put in the high 64 bits of the return value
	/// @return Combined values
	FORCEINLINE CONST m128i64 mm_unpacklo_epi64(m128i64 lower, m128i64 upper)
	{
		m128i64 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pcpyld	%[Result],%[Upper],%[Lower]"
			: [Result] "=r" (result.v)
			: [Upper] "r" (upper.v),
			  [Lower] "r" (lower.v)
		);
#else
		result.lo = lower.lo;
		result.hi = upper.lo;
#endif

		return result;
	}

	/// @brief PCPYLD : Parallel CoPY Lower Doubleword
	/// 
	/// Select the lower 64 bits of each input value and combine them into a 128-bit value.
	/// @param lower Value to put in the low 64 bits of the return value
	/// @param upper Value to put in the high 64 bits of the return value
	/// @return Combined values
	FORCEINLINE CONST m128u64 mm_unpacklo_epu64(m128u64 lower, m128u64 upper)
	{
		return mm_castepu64_epi64(mm_unpacklo_epi64(mm_castepi64_epu64(lower), mm_castepi64_epu64(upper)));
	}

	/// @brief PCPYUD : Parallel CoPY Upper Doubleword
	/// 
	/// Select the high 64 bits of each input value and combine them into a 128-bit value.
	/// @param lower Value to put in the low 64 bits of the return value
	/// @param upper Value to put in the high 64 bits of the return value
	/// @return Combined values
	FORCEINLINE CONST m128i64 mm_unpackhi_epi64(m128i64 lower, m128i64 upper)
	{
		m128i64 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pcpyud	%[Result],%[Lower],%[Upper]"
			: [Result] "=r" (result.v)
			: [Upper] "r" (upper.v),
			  [Lower] "r" (lower.v)
		);
#else
		result.lo = lower.hi;
		result.hi = upper.hi;
#endif

		return result;
	}

	/// @brief PCPYUD : Parallel CoPY Upper Doubleword
	/// 
	/// Select the high 64 bits of each input value and combine them into a 128-bit value.
	/// @param lower Value to put in the low 64 bits of the return value
	/// @param upper Value to put in the high 64 bits of the return value
	/// @return Combined values
	FORCEINLINE CONST m128u64 mm_unpackhi_epu64(m128u64 lower, m128u64 upper)
	{
		return mm_castepu64_epi64(mm_unpackhi_epi64(mm_castepi64_epu64(lower), mm_castepi64_epu64(upper)));
	}

	/// @brief PEXCH : Parallel EXchange Center Halfword
	/// 
	/// Split the 8 16-bit values into 2 groups of 4. The low group spans values at porition 0,
	/// 1, 2 and 3. The upper group spans 4, 5, 6 and 7. Exchange values in the center of each
	/// group. 1 will swap with 2 and 5 will swap with 6.
	/// @param v Values to exchange values in
	/// @return Values after exchanging center values
	FORCEINLINE CONST m128i16 mm_xchgcenter_epi16(m128i16 v)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pexch	%[Result],%[Value]"
			: [Result] "=r" (result.v)
			: [Value] "r" (v.v)
		);
#else
		asm(
			"pcpyld	%[ValueLo],%[ValueHi],%[ValueHi]\n\t"
			"pexch	%[ResultLo],%[ValueLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [ValueLo] "r" (v.lo),
			  [ValueHi] "r" (v.hi)
		);
#endif

		return result;
	}

	/// @brief PEXCH : Parallel EXchange Center Halfword
	/// 
	/// Split the 8 16-bit values into 2 groups of 4. The low group spans values at porition 0,
	/// 1, 2 and 3. The upper group spans 4, 5, 6 and 7. Exchange values in the center of each
	/// group. 1 will swap with 2 and 5 will swap with 6.
	/// @param v Values to exchange values in
	/// @return Values after exchanging center values
	FORCEINLINE CONST m128u16 mm_xchgcenter_epu16(m128u16 v)
	{
		return mm_castepu16_epi16(mm_xchgcenter_epi16(mm_castepi16_epu16(v)));
	}

	/// @brief PEXCW : Parallel EXchange Center Word
	/// 
	/// Take 4 32-bit values. Swap the central 2 values.
	/// @param v Value to exchange elements in
	/// @return Value after exchanging elements
	FORCEINLINE CONST m128i32 mm_xchgcenter_epi32(m128i32 v)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pexcw	%[Result],%[Value]"
			: [Result] "=r" (result.v)
			: [Value] "r" (v.v)
		);
#else
		asm(
			"pcpyld	%[ValueLo],%[ValueHi],%[ValueHi]\n\t"
			"pexcw	%[ResultLo],%[ValueLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [ValueLo] "r" (v.lo),
			  [ValueHi] "r" (v.hi)
		);
#endif

		return result;
	}

	/// @brief PEXCW : Parallel EXchange Center Word
	/// 
	/// Take 4 32-bit values. Swap the central 2 values.
	/// @param v Value to exchange elements in
	/// @return Value after exchanging elements
	FORCEINLINE CONST m128u32 mm_xchgcenter_epu32(m128u32 v)
	{
		return mm_castepu32_epi32(mm_xchgcenter_epi32(mm_castepi32_epu32(v)));
	}

	/// @brief PEXEH : Parallel EXchange Even Halfword
	/// 
	/// Split the 8 16-bit values into 2 groups of 4. The low group spans values at porition 0,
	/// 1, 2 and 3. The upper group spans 4, 5, 6 and 7. Exchange values in even positions of each
	/// group. 0 will swap with 2 and 4 will swap with 6.
	/// @param v Values to exchange values in
	/// @return Values after exchanging center values
	FORCEINLINE CONST m128i16 mm_xchgeven_epi16(m128i16 v)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pexeh	%[Result],%[Value]"
			: [Result] "=r" (result.v)
			: [Value] "r" (v.v)
		);
#else
		asm(
			"pcpyld	%[ValueLo],%[ValueHi],%[ValueHi]\n\t"
			"pexeh	%[ResultLo],%[ValueLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [ValueLo] "r" (v.lo),
			  [ValueHi] "r" (v.hi)
		);
#endif

		return result;
	}

	/// @brief PEXEH : Parallel EXchange Even Halfword
	/// 
	/// Split the 8 16-bit values into 2 groups of 4. The low group spans values at porition 0,
	/// 1, 2 and 3. The upper group spans 4, 5, 6 and 7. Exchange values in even positions of each
	/// group. 0 will swap with 2 and 4 will swap with 6.
	/// @param v Values to exchange values in
	/// @return Values after exchanging center values
	FORCEINLINE CONST m128u16 mm_xchgeven_epu16(m128u16 v)
	{
		return mm_castepu16_epi16(mm_xchgeven_epi16(mm_castepi16_epu16(v)));
	}

	/// @brief PEXEW : Parallel EXchange Even Word
	/// 
	/// Take 4 32-bit values. Swap the values in even positions, 0 and 2.
	/// @param v Value to exchange elements in
	/// @return Value after exchanging elements
	FORCEINLINE CONST m128i32 mm_xchgeven_epi32(m128i32 v)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pexew	%[Result],%[Value]"
			: [Result] "=r" (result.v)
			: [Value] "r" (v.v)
		);
#else
		asm(
			"pcpyld	%[ValueLo],%[ValueHi],%[ValueHi]\n\t"
			"pexew	%[ResultLo],%[ValueLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [ValueLo] "r" (v.lo),
			  [ValueHi] "r" (v.hi)
		);
#endif

		return result;
	}

	/// @brief PEXEW : Parallel EXchange Even Word
	/// 
	/// Take 4 32-bit values. Swap the values in even positions, 0 and 2.
	/// @param v Value to exchange elements in
	/// @return Value after exchanging elements
	FORCEINLINE CONST m128u32 mm_xchgeven_epu32(m128u32 v)
	{
		return mm_castepu32_epi32(mm_xchgeven_epi32(mm_castepi32_epu32(v)));
	}

	/// @brief PREVH : Parallel REVerse Halfword
	/// 
	/// Split the 8 16-bit values into 2 groups of 4. The low group spans values at porition 0,
	/// 1, 2 and 3. The upper group spans 4, 5, 6 and 7. Reverse the order within each group. The
	/// total order of the return value is: 3, 2, 1, 0, 7, 6, 5, 4.
	/// @param v Value to reverse elements in
	/// @return Value after reversing elements
	FORCEINLINE CONST m128i16 mm_reverse_epi16(m128i16 v)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"prevh	%[Result],%[Value]"
			: [Result] "=r" (result.v)
			: [Value] "r" (v.v)
		);
#else
		asm(
			"pcpyld	%[ValueLo],%[ValueHi],%[ValueHi]\n\t"
			"prevh	%[ResultLo],%[ValueLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [ValueLo] "r" (v.lo),
			  [ValueHi] "r" (v.hi)
		);
#endif

		return result;
	}

	/// @brief PREVH : Parallel REVerse Halfword
	/// 
	/// Split the 8 16-bit values into 2 groups of 4. The low group spans values at porition 0,
	/// 1, 2 and 3. The upper group spans 4, 5, 6 and 7. Reverse the order within each group. The
	/// total order of the return value is: 3, 2, 1, 0, 7, 6, 5, 4.
	/// @param v Value to reverse elements in
	/// @return Value after reversing elements
	FORCEINLINE CONST m128u16 mm_reverse_epu16(m128u16 v)
	{
		return mm_castepu16_epi16(mm_reverse_epi16(mm_castepi16_epu16(v)));
	}

	/// @brief PROT3W: Parallel ROTate 3 Words left
	/// 
	/// Select the lower 3 32-bit values from 4. Rotate those 3 words 1 position towards a lower
	/// position. Values at position 0, 1 and 2 go to location 2, 0 and 1.
	/// @param v Value to rotate words in
	/// @return Value after rotating words
	FORCEINLINE CONST m128i32 mm_rot3_epi32(m128i32 v)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"prot3w	%[Result],%[Value]"
			: [Result] "=r" (result.v)
			: [Value] "r" (v.v)
		);
#else
		asm(
			"pcpyld	%[ValueLo],%[ValueHi],%[ValueHi]\n\t"
			"prot3w	%[ResultLo],%[ValueLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [ValueLo] "r" (v.lo),
			  [ValueHi] "r" (v.hi)
		);
#endif

		return result;
	}

	/// @brief PROT3W: Parallel ROTate 3 Words left
	/// 
	/// Select the lower 3 32-bit values from 4. Rotate those 3 words 1 position towards a lower
	/// position. Values at position 0, 1 and 2 go to location 2, 0 and 1.
	/// @param v Value to rotate words in
	/// @return Value after rotating words
	FORCEINLINE CONST m128u32 mm_rot3_epu32(m128u32 v)
	{
		return mm_castepu32_epi32(mm_rot3_epi32(mm_castepi32_epu32(v)));
	}

	/// @brief PEXTLB : Parallel EXTend Lower from Byte
	/// 
	/// Use only the lower 8 bytes of both operands. Interleave them into the return value such
	/// that their relative ordering stays intact and the values from 'even' land in the even
	/// positions and the values from 'odd' land in the odd positions.
	/// 
	/// Bitwise result:
	///		Return value[ 0,  7]	=	even[0,  7]
	///		Return value[ 8, 15]	=	odd	[0,  7]
	///		Return value[16, 23]	=	even[8, 15]
	///		Return value[24, 31]	=	odd	[8, 15]
	///		...
	/// @param even Values to put in even positions in the return value
	/// @param odd Values to put in odd positions in the return value
	/// @return Interleaved values from both arguments
	FORCEINLINE CONST m128i8 mm_extlo_epi8(m128i8 even, m128i8 odd)
	{
		m128i8 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pextlb	%[Result],%[Odd],%[Even]"
			: [Result] "=r" (result.v)
			: [Even] "r" (even.v),
			  [Odd] "r" (odd.v)
		);
#else
		asm(
			"pcpyld	%[OddLo],%[OddHi],%[OddLo]\n\t"
			"pcpyld	%[EvenLo],%[EvenHi],%[EvenLo]\n\t"
			"pextlb	%[ResultLo],%[OddLo],%[EvenLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [OddLo] "r" (odd.lo),
			  [OddHi] "r" (odd.hi),
			  [EvenLo] "r" (even.lo),
			  [EvenHi] "r" (even.hi)
		);
#endif

		return result;
	}

	/// @brief PEXTLB : Parallel EXTend Lower from Byte
	/// 
	/// Use only the lower 8 bytes of both operands. Interleave them into the return value such
	/// that their relative ordering stays intact and the values from 'even' land in the even
	/// positions and the values from 'odd' land in the odd positions.
	/// 
	/// Bitwise result:
	///		Return value[ 0,  7]	=	even[0,  7]
	///		Return value[ 8, 15]	=	odd	[0,  7]
	///		Return value[16, 23]	=	even[8, 15]
	///		Return value[24, 31]	=	odd	[8, 15]
	///		...
	/// @param even Values to put in even positions in the return value
	/// @param odd Values to put in odd positions in the return value
	/// @return Interleaved values from both arguments
	FORCEINLINE CONST m128u8 mm_extlo_epu8(m128u8 even, m128u8 odd)
	{
		return mm_castepu8_epi8(mm_extlo_epi8(mm_castepi8_epu8(even), mm_castepi8_epu8(odd)));
	}

	/// @brief PEXTUB : Parallel EXTend Upper from Byte
	/// 
	/// Use only the upper 8 bytes of both operands. Interleave them into the return value such
	/// that their relative ordering stays intact and the values from 'even' land in the even
	/// positions and the values from 'odd' land in the odd positions.
	/// 
	/// Bitwise result:
	///		Return value[ 0,  7]	=	even[64, 71]
	///		Return value[ 8, 15]	=	odd	[64, 71]
	///		Return value[16, 23]	=	even[72, 79]
	///		Return value[24, 31]	=	odd	[72, 79]
	///		...
	/// @param even Values to put in even positions in the return value
	/// @param odd Values to put in odd positions in the return value
	/// @return Interleaved values from both arguments
	FORCEINLINE CONST m128i8 mm_exthi_epi8(m128i8 even, m128i8 odd)
	{
		m128i8 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pextub	%[Result],%[Odd],%[Even]"
			: [Result] "=r" (result.v)
			: [Even] "r" (even.v),
			  [Odd] "r" (odd.v)
		);
#else
		asm(
			"pcpyld	%[OddLo],%[OddHi],%[OddLo]\n\t"
			"pcpyld	%[EvenLo],%[EvenHi],%[EvenLo]\n\t"
			"pextub	%[ResultLo],%[OddLo],%[EvenLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [OddLo] "r" (odd.lo),
			  [OddHi] "r" (odd.hi),
			  [EvenLo] "r" (even.lo),
			  [EvenHi] "r" (even.hi)
		);
#endif

		return result;
	}

	/// @brief PEXTUB : Parallel EXTend Upper from Byte
	/// 
	/// Use only the upper 8 bytes of both operands. Interleave them into the return value such
	/// that their relative ordering stays intact and the values from 'even' land in the even
	/// positions and the values from 'odd' land in the odd positions.
	/// 
	/// Bitwise result:
	///		Return value[ 0,  7]	=	even[64, 71]
	///		Return value[ 8, 15]	=	odd	[64, 71]
	///		Return value[16, 23]	=	even[72, 79]
	///		Return value[24, 31]	=	odd	[72, 79]
	///		...
	/// @param even Values to put in even positions in the return value
	/// @param odd Values to put in odd positions in the return value
	/// @return Interleaved values from both arguments
	FORCEINLINE CONST m128u8 mm_exthi_epu8(m128u8 even, m128u8 odd)
	{
		return mm_castepu8_epi8(mm_exthi_epi8(mm_castepi8_epu8(even), mm_castepi8_epu8(odd)));
	}

	/// @brief PEXTLH : Parallel EXTend Lower from Halfword
	/// 
	/// Use only the lower 4 halfwords of both operands. Interleave them into the return value
	/// such that their relative ordering stays intact and the values from 'even' land in the even
	/// positions and the values from 'odd' land in the odd positions.
	/// 
	/// Bitwise result:
	///		Return value[ 0, 15]	=	even[ 0, 15]
	///		Return value[16, 31]	=	odd	[ 0, 15]
	///		Return value[32, 47]	=	even[16, 31]
	///		Return value[48, 63]	=	odd	[16, 31]
	///		...
	/// @param even Values to put in even positions in the return value
	/// @param odd Values to put in odd positions in the return value
	/// @return Interleaved values from both arguments
	FORCEINLINE CONST m128i16 mm_extlo_epi16(m128i16 even, m128i16 odd)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pextlh	%[Result],%[Odd],%[Even]"
			: [Result] "=r" (result.v)
			: [Even] "r" (even.v),
			  [Odd] "r" (odd.v)
		);
#else
		asm(
			"pcpyld	%[OddLo],%[OddHi],%[OddLo]\n\t"
			"pcpyld	%[EvenLo],%[EvenHi],%[EvenLo]\n\t"
			"pextlh	%[ResultLo],%[OddLo],%[EvenLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [OddLo] "r" (odd.lo),
			  [OddHi] "r" (odd.hi),
			  [EvenLo] "r" (even.lo),
			  [EvenHi] "r" (even.hi)
		);
#endif

		return result;
	}

	/// @brief PEXTLH : Parallel EXTend Lower from Halfword
	/// 
	/// Use only the lower 4 halfwords of both operands. Interleave them into the return value
	/// such that their relative ordering stays intact and the values from 'even' land in the even
	/// positions and the values from 'odd' land in the odd positions.
	/// 
	/// Bitwise result:
	///		Return value[ 0, 15]	=	even[ 0, 15]
	///		Return value[16, 31]	=	odd	[ 0, 15]
	///		Return value[32, 47]	=	even[16, 31]
	///		Return value[48, 63]	=	odd	[16, 31]
	///		...
	/// @param even Values to put in even positions in the return value
	/// @param odd Values to put in odd positions in the return value
	/// @return Interleaved values from both arguments
	FORCEINLINE CONST m128u16 mm_extlo_epu16(m128u16 even, m128u16 odd)
	{
		return mm_castepu16_epi16(mm_extlo_epi16(mm_castepi16_epu16(even), mm_castepi16_epu16(odd)));
	}

	/// @brief PEXTUH : Parallel EXTend Upper from Halfword
	/// 
	/// Use only the upper 4 halfwords of both operands. Interleave them into the return value
	/// such that their relative ordering stays intact and the values from 'even' land in the even
	/// positions and the values from 'odd' land in the odd positions.
	/// 
	/// Bitwise result:
	///		Return value[ 0, 15]	=	even[64, 79]
	///		Return value[16, 31]	=	odd	[64, 79]
	///		Return value[32, 47]	=	even[80, 95]
	///		Return value[48, 63]	=	odd	[80, 95]
	///		...
	/// @param even Values to put in even positions in the return value
	/// @param odd Values to put in odd positions in the return value
	/// @return Interleaved values from both arguments
	FORCEINLINE CONST m128i16 mm_exthi_epi16(m128i16 even, m128i16 odd)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pextuh	%[Result],%[Odd],%[Even]"
			: [Result] "=r" (result.v)
			: [Even] "r" (even.v),
			  [Odd] "r" (odd.v)
		);
#else
		asm(
			"pcpyld	%[OddLo],%[OddHi],%[OddLo]\n\t"
			"pcpyld	%[EvenLo],%[EvenHi],%[EvenLo]\n\t"
			"pextuh	%[ResultLo],%[OddLo],%[EvenLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [OddLo] "r" (odd.lo),
			  [OddHi] "r" (odd.hi),
			  [EvenLo] "r" (even.lo),
			  [EvenHi] "r" (even.hi)
		);
#endif

		return result;
	}

	/// @brief PEXTUH : Parallel EXTend Upper from Halfword
	/// 
	/// Use only the upper 4 halfwords of both operands. Interleave them into the return value
	/// such that their relative ordering stays intact and the values from 'even' land in the even
	/// positions and the values from 'odd' land in the odd positions.
	/// 
	/// Bitwise result:
	///		Return value[ 0, 15]	=	even[64, 79]
	///		Return value[16, 31]	=	odd	[64, 79]
	///		Return value[32, 47]	=	even[80, 95]
	///		Return value[48, 63]	=	odd	[80, 95]
	///		...
	/// @param even Values to put in even positions in the return value
	/// @param odd Values to put in odd positions in the return value
	/// @return Interleaved values from both arguments
	FORCEINLINE CONST m128u16 mm_exthi_epu16(m128u16 even, m128u16 odd)
	{
		return mm_castepu16_epi16(mm_exthi_epi16(mm_castepi16_epu16(even), mm_castepi16_epu16(odd)));
	}

	/// @brief PEXTLW : Parallel EXTend Lower from Word
	/// 
	/// Use only the lower 2 words of both operands. Interleave them into the return value such
	/// that their relative ordering stays intact and the values from 'even' land in the even
	/// positions and the values from 'odd' land in the odd positions.
	/// 
	/// Bitwise result:
	///		Return value[ 0,  31]	=	even[ 0, 31]
	///		Return value[32,  63]	=	odd	[ 0, 31]
	///		Return value[64,  95]	=	even[32, 63]
	///		Return value[96, 127]	=	odd	[32, 63]
	/// @param even Values to put in even positions in the return value
	/// @param odd Values to put in odd positions in the return value
	/// @return Interleaved values from both arguments
	FORCEINLINE CONST m128i32 mm_extlo_epi32(m128i32 even, m128i32 odd)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pextlw	%[Result],%[Odd],%[Even]"
			: [Result] "=r" (result.v)
			: [Even] "r" (even.v),
			  [Odd] "r" (odd.v)
		);
#else
		asm(
			"pcpyld	%[OddLo],%[OddHi],%[OddLo]\n\t"
			"pcpyld	%[EvenLo],%[EvenHi],%[EvenLo]\n\t"
			"pextlw	%[ResultLo],%[OddLo],%[EvenLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [OddLo] "r" (odd.lo),
			  [OddHi] "r" (odd.hi),
			  [EvenLo] "r" (even.lo),
			  [EvenHi] "r" (even.hi)
		);
#endif

		return result;
	}

	/// @brief PEXTLW : Parallel EXTend Lower from Word
	/// 
	/// Use only the lower 2 words of both operands. Interleave them into the return value such
	/// that their relative ordering stays intact and the values from 'even' land in the even
	/// positions and the values from 'odd' land in the odd positions.
	/// 
	/// Bitwise result:
	///		Return value[ 0,  31]	=	even[ 0, 31]
	///		Return value[32,  63]	=	odd	[ 0, 31]
	///		Return value[64,  95]	=	even[32, 63]
	///		Return value[96, 127]	=	odd	[32, 63]
	/// @param even Values to put in even positions in the return value
	/// @param odd Values to put in odd positions in the return value
	/// @return Interleaved values from both arguments
	FORCEINLINE CONST m128u32 mm_extlo_epu32(m128u32 even, m128u32 odd)
	{
		return mm_castepu32_epi32(mm_extlo_epi32(mm_castepi32_epu32(even), mm_castepi32_epu32(odd)));
	}

	/// @brief PEXTUW : Parallel EXTend Upper from Word
	/// 
	/// Use only the upper 2 words of both operands. Interleave them into the return value such
	/// that their relative ordering stays intact and the values from 'even' land in the even
	/// positions and the values from 'odd' land in the odd positions.
	/// 
	/// Bitwise result:
	///		Return value[ 0,  31]	=	even[64,  95]
	///		Return value[32,  63]	=	odd	[64,  95]
	///		Return value[64,  95]	=	even[96, 127]
	///		Return value[96, 127]	=	odd	[96, 127]
	/// @param even Values to put in even positions in the return value
	/// @param odd Values to put in odd positions in the return value
	/// @return Interleaved values from both arguments
	FORCEINLINE CONST m128i32 mm_exthi_epi32(m128i32 even, m128i32 odd)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pextuw	%[Result],%[Odd],%[Even]"
			: [Result] "=r" (result.v)
			: [Even] "r" (even.v),
			  [Odd] "r" (odd.v)
		);
#else
		asm(
			"pcpyld	%[OddLo],%[OddHi],%[OddLo]\n\t"
			"pcpyld	%[EvenLo],%[EvenHi],%[EvenLo]\n\t"
			"pextuw	%[ResultLo],%[OddLo],%[EvenLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [OddLo] "r" (odd.lo),
			  [OddHi] "r" (odd.hi),
			  [EvenLo] "r" (even.lo),
			  [EvenHi] "r" (even.hi)
		);
#endif

		return result;
	}

	/// @brief PEXTUW : Parallel EXTend Upper from Word
	/// 
	/// Use only the upper 2 words of both operands. Interleave them into the return value such
	/// that their relative ordering stays intact and the values from 'even' land in the even
	/// positions and the values from 'odd' land in the odd positions.
	/// 
	/// Bitwise result:
	///		Return value[ 0,  31]	=	even[64,  95]
	///		Return value[32,  63]	=	odd	[64,  95]
	///		Return value[64,  95]	=	even[96, 127]
	///		Return value[96, 127]	=	odd	[96, 127]
	/// @param even Values to put in even positions in the return value
	/// @param odd Values to put in odd positions in the return value
	/// @return Interleaved values from both arguments
	FORCEINLINE CONST m128u32 mm_exthi_epu32(m128u32 even, m128u32 odd)
	{
		return mm_castepu32_epi32(mm_exthi_epi32(mm_castepi32_epu32(even), mm_castepi32_epu32(odd)));
	}

	/// @brief PINTEH : Parallel INTerleave Even Halfword
	/// 
	/// Select only the 4 values in even positions in the input arguments. Interleave the values
	/// from 'even' in the even positions and those from 'odd' in the odd positions of the return
	/// value.
	/// 
	/// Bitwise result:
	///		Return value[ 0, 15]	=	even[ 0, 15]
	///		Return value[16, 31]	=	odd	[ 0, 15]
	///		Return value[32, 47]	=	even[32, 47]
	///		Return value[48, 63]	=	odd	[32, 47]
	///		...
	/// @param even Values to put in the even positions
	/// @param odd Values to put in the odd positions
	/// @return Interleaved values
	FORCEINLINE CONST m128i16 mm_interleaveeven_epi16(m128i16 even, m128i16 odd)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pinteh	%[Result],%[Odd],%[Even]"
			: [Result] "=r" (result.v)
			: [Even] "r" (even.v),
			  [Odd] "r" (odd.v)
		);
#else
		asm(
			"pcpyld	%[OddLo],%[OddHi],%[OddLo]\n\t"
			"pcpyld	%[EvenLo],%[EvenHi],%[EvenLo]\n\t"
			"pinteh	%[ResultLo],%[OddLo],%[EvenLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [OddLo] "r" (odd.lo),
			  [OddHi] "r" (odd.hi),
			  [EvenLo] "r" (even.lo),
			  [EvenHi] "r" (even.hi)
		);
#endif

		return result;
	}

	/// @brief PINTEH : Parallel INTerleave Even Halfword
	/// 
	/// Select only the 4 values in even positions in the input arguments. Interleave the values
	/// from 'even' in the even positions and those from 'odd' in the odd positions of the return
	/// value.
	/// 
	/// Bitwise result:
	///		Return value[ 0, 15]	=	even[ 0, 15]
	///		Return value[16, 31]	=	odd	[ 0, 15]
	///		Return value[32, 47]	=	even[32, 47]
	///		Return value[48, 63]	=	odd	[32, 47]
	///		...
	/// @param even Values to put in the even positions
	/// @param odd Values to put in the odd positions
	/// @return Interleaved values
	FORCEINLINE CONST m128u16 mm_interleaveeven_epu16(m128u16 even, m128u16 odd)
	{
		return mm_castepu16_epi16(mm_interleaveeven_epi16(mm_castepi16_epu16(even), mm_castepi16_epu16(odd)));
	}

	/// @brief PINTH : Parallel INTerleave Halfword
	/// 
	/// Select only the low 4 values from 'even' and the high 4 values from 'odd'. Interleave the
	/// values from 'even' in the even positions and those from 'odd' in the odd positions of the
	/// return value.
	/// 
	/// Bitwise result:
	///		Return value[ 0, 15]	=	even[ 0, 15]
	///		Return value[16, 31]	=	odd	[64, 79]
	///		Return value[32, 47]	=	even[16, 31]
	///		Return value[48, 63]	=	odd	[80, 95]
	///		...
	/// @param even Values to put in the even positions
	/// @param odd Values to put in the odd positions
	/// @return Interleaved values
	FORCEINLINE CONST m128i16 mm_interleavelohi_epi16(m128i16 even, m128i16 odd)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pinth	%[Result],%[Odd],%[Even]"
			: [Result] "=r" (result.v)
			: [Even] "r" (even.v),
			  [Odd] "r" (odd.v)
		);
#else
		asm(
			"pcpyld	%[OddLo],%[OddHi],%[OddLo]\n\t"
			"pcpyld	%[EvenLo],%[EvenHi],%[EvenLo]\n\t"
			"pinth	%[ResultLo],%[OddLo],%[EvenLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [OddLo] "r" (odd.lo),
			  [OddHi] "r" (odd.hi),
			  [EvenLo] "r" (even.lo),
			  [EvenHi] "r" (even.hi)
		);
#endif

		return result;
	}

	/// @brief PINTH : Parallel INTerleave Halfword
	/// 
	/// Select only the low 4 values from 'even' and the high 4 values from 'odd'. Interleave the
	/// values from 'even' in the even positions and those from 'odd' in the odd positions of the
	/// return value.
	/// 
	/// Bitwise result:
	///		Return value[ 0, 15]	=	even[ 0, 15]
	///		Return value[16, 31]	=	odd	[64, 79]
	///		Return value[32, 47]	=	even[16, 31]
	///		Return value[48, 63]	=	odd	[80, 95]
	///		...
	/// @param even Values to put in the even positions
	/// @param odd Values to put in the odd positions
	/// @return Interleaved values
	FORCEINLINE CONST m128u16 mm_interleavelohi_epu16(m128u16 even, m128u16 odd)
	{
		return mm_castepu16_epi16(mm_interleavelohi_epi16(mm_castepi16_epu16(even), mm_castepi16_epu16(odd)));
	}

	/// @brief PPACB : Parallel PACk to Byte
	/// 
	/// Select only the values in even positions of both arguments. Store the values from 'lo' to
	/// the low 8 bytes of the return value and the values from 'hi' to the high 8 bytes.
	/// 
	/// Bitwise result:
	///		Return value[ 0,  7]	=	lo[ 0,  7]
	///		Return value[ 8, 15]	=	lo[16, 23]
	///		...
	///		Return value[64, 71]	=	hi[ 0,  7]
	///		Return value[72, 79]	=	hi[16, 23]
	/// @param lo Values to put in the low 8 bytes of the return value
	/// @param hi Values to put in the high 8 bytes of the return value
	/// @return Packed values of both arguments
	FORCEINLINE CONST m128i8 mm_pack_epi8(m128i8 lo, m128i8 hi)
	{
		m128i8 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"ppacb	%[Result],%[Hi],%[Lo]"
			: [Result] "=r" (result.v)
			: [Lo] "r" (lo.v),
			  [Hi] "r" (hi.v)
		);
#else
		asm(
			"pcpyld	%[LoLo],%[LoHi],%[LoLo]\n\t"
			"pcpyld	%[HiLo],%[HiHi],%[HiLo]\n\t"
			"ppacb	%[ResultLo],%[HiLo],%[LoLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LoLo] "r" (lo.lo),
			  [LoHi] "r" (lo.hi),
			  [HiLo] "r" (hi.lo),
			  [HiHi] "r" (hi.hi)
		);
#endif

		return result;
	}

	/// @brief PPACB : Parallel PACk to Byte
	/// 
	/// Select only the values in even positions of both arguments. Store the values from 'lo' to
	/// the low 8 bytes of the return value and the values from 'hi' to the high 8 bytes.
	/// 
	/// Bitwise result:
	///		Return value[ 0,  7]	=	lo[ 0,  7]
	///		Return value[ 8, 15]	=	lo[16, 23]
	///		...
	///		Return value[64, 71]	=	hi[ 0,  7]
	///		Return value[72, 79]	=	hi[16, 23]
	/// @param lo Values to put in the low 8 bytes of the return value
	/// @param hi Values to put in the high 8 bytes of the return value
	/// @return Packed values of both arguments
	FORCEINLINE CONST m128u8 mm_pack_epu8(m128u8 lo, m128u8 hi)
	{
		return mm_castepu8_epi8(mm_pack_epi8(mm_castepi8_epu8(lo), mm_castepi8_epu8(hi)));
	}

	/// @brief PPACH : Parallel PACk to Halfword
	/// 
	/// Select only the values in even positions of both arguments. Store the values from 'lo' to
	/// the low 4 halfwords of the return value and the values from 'hi' to the high 4 halfwords.
	/// 
	/// Bitwise result:
	///		Return value[ 0, 15]	=	lo[ 0, 15]
	///		Return value[16, 31]	=	lo[32, 47]
	///		...
	///		Return value[64, 79]	=	hi[ 0, 15]
	///		Return value[80, 95]	=	hi[32, 47]
	/// @param lo Values to put in the low 4 halfwords of the return value
	/// @param hi Values to put in the high 4 halfwords of the return value
	/// @return Packed values of both arguments
	FORCEINLINE CONST m128i16 mm_pack_epi16(m128i16 lo, m128i16 hi)
	{
		m128i16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"ppach	%[Result],%[Hi],%[Lo]"
			: [Result] "=r" (result.v)
			: [Lo] "r" (lo.v),
			  [Hi] "r" (hi.v)
		);
#else
		asm(
			"pcpyld	%[LoLo],%[LoHi],%[LoLo]\n\t"
			"pcpyld	%[HiLo],%[HiHi],%[HiLo]\n\t"
			"ppach	%[ResultLo],%[HiLo],%[LoLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LoLo] "r" (lo.lo),
			  [LoHi] "r" (lo.hi),
			  [HiLo] "r" (hi.lo),
			  [HiHi] "r" (hi.hi)
		);
#endif

		return result;
	}

	/// @brief PPACH : Parallel PACk to Halfword
	/// 
	/// Select only the values in even positions of both arguments. Store the values from 'lo' to
	/// the low 4 halfwords of the return value and the values from 'hi' to the high 4 halfwords.
	/// 
	/// Bitwise result:
	///		Return value[ 0, 15]	=	lo[ 0, 15]
	///		Return value[16, 31]	=	lo[32, 47]
	///		...
	///		Return value[64, 79]	=	hi[ 0, 15]
	///		Return value[80, 95]	=	hi[32, 47]
	/// @param lo Values to put in the low 4 halfwords of the return value
	/// @param hi Values to put in the high 4 halfwords of the return value
	/// @return Packed values of both arguments
	FORCEINLINE CONST m128u16 mm_pack_epu16(m128u16 lo, m128u16 hi)
	{
		return mm_castepu16_epi16(mm_pack_epi16(mm_castepi16_epu16(lo), mm_castepi16_epu16(hi)));
	}

	/// @brief PPACW : Parallel PACk to Word
	/// 
	/// Select only the values in even positions of both arguments. Store the values from 'lo' to
	/// the low 2 words of the return value and the values from 'hi' to the high 2 words.
	/// 
	/// Bitwise result:
	///		Return value[ 0,  31]	=	lo[ 0, 31]
	///		Return value[32,  63]	=	lo[64, 95]
	///		Return value[64,  95]	=	hi[ 0, 31]
	///		Return value[96, 127]	=	hi[64, 95]
	/// @param lo Values to put in the low 2 words of the return value
	/// @param hi Values to put in the high 2 words of the return value
	/// @return Packed values of both arguments
	FORCEINLINE CONST m128i32 mm_pack_epi32(m128i32 lo, m128i32 hi)
	{
		m128i32 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"ppacw	%[Result],%[Hi],%[Lo]"
			: [Result] "=r" (result.v)
			: [Lo] "r" (lo.v),
			  [Hi] "r" (hi.v)
		);
#else
		asm(
			"pcpyld	%[LoLo],%[LoHi],%[LoLo]\n\t"
			"pcpyld	%[HiLo],%[HiHi],%[HiLo]\n\t"
			"ppacw	%[ResultLo],%[HiLo],%[LoLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [LoLo] "r" (lo.lo),
			  [LoHi] "r" (lo.hi),
			  [HiLo] "r" (hi.lo),
			  [HiHi] "r" (hi.hi)
		);
#endif

		return result;
	}

	/// @brief PPACW : Parallel PACk to Word
	/// 
	/// Select only the values in even positions of both arguments. Store the values from 'lo' to
	/// the low 2 words of the return value and the values from 'hi' to the high 2 words.
	/// 
	/// Bitwise result:
	///		Return value[ 0,  31]	=	lo[ 0, 31]
	///		Return value[32,  63]	=	lo[64, 95]
	///		Return value[64,  95]	=	hi[ 0, 31]
	///		Return value[96, 127]	=	hi[64, 95]
	/// @param lo Values to put in the low 2 words of the return value
	/// @param hi Values to put in the high 2 words of the return value
	/// @return Packed values of both arguments
	FORCEINLINE CONST m128u32 mm_pack_epu32(m128u32 lo, m128u32 hi)
	{
		return mm_castepu32_epi32(mm_pack_epi32(mm_castepi32_epu32(lo), mm_castepi32_epu32(hi)));
	}

	/// @brief PEXT5 : Parallel EXTend from 5 bits
	/// 
	/// Convert pixel coler data from the 16-bit 1-5-5-5 format to the 32-bit 8-8-8-8 format. Only
	/// the 4 values in even positions in the input argument are converted. Low bits are filled
	/// with '0', so the alpha channel becomes 1 bit from the input concatenated with 7 '0' bits.
	/// @param v 1-5-5-5 values to convert
	/// @return Values converted to 8-8-8-8
	FORCEINLINE CONST m128u32 mm_ext5_epu16(m128u16 v)
	{
		m128u32 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"pext5	%[Result],%[Value]"
			: [Result] "=r" (result.v)
			: [Value] "r" (v.v)
		);
#else
		asm(
			"pcpyld	%[ValueLo],%[ValueHi],%[ValueHi]\n\t"
			"pext5	%[ResultLo],%[ValueLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [ValueLo] "r" (v.lo),
			  [ValueHi] "r" (v.hi)
		);
#endif

		return result;
	}

	/// @brief PPAC5 : Parallel PACk to 5 bits
	/// 
	/// Convert 4 pixel color values in the 8-8-8-8 format to the 1-5-5-5 format and store it in
	/// the even positions of the 8 element return value. The odd positions are filled with '0'.
	/// Color values are truncated, so only the highest bit is used for alpha and only the 5
	/// highest bits are used for the color channels, others are ignored.
	/// @param v 8-8-8-8 values to convert
	/// @return Values converted to 1-5-5-5
	FORCEINLINE CONST m128u16 mm_pack5_epu32(m128u32 v)
	{
		m128u16 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"ppac5	%[Result],%[Value]"
			: [Result] "=r" (result.v)
			: [Value] "r" (v.v)
		);
#else
		asm(
			"pcpyld	%[ValueLo],%[ValueHi],%[ValueHi]\n\t"
			"ppac5	%[ResultLo],%[ValueLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [ValueLo] "r" (v.lo),
			  [ValueHi] "r" (v.hi)
		);
#endif

		return result;
	}

	/// @brief PLZCW : Parallel Leading Zero or one Count Word
	/// 
	/// Uses only the low 64 bits / 2 values from the 4 value argument. For each value, count the
	/// number of leading bits that have the same value minus 1. This means numbers starting with
	/// '0b1110...' and '0b0001...' will both return '2' as there are 3 bits of the same value
	/// beginning at the highest bit and we discard the count of the sign bit.
	/// @param v Numbers to count leading bits of
	/// @return Amount of same leading bits minus 1
	FORCEINLINE CONST m128u32 mm_clb_epi32(m128i32 v)
	{
		m128u32 result = {};

#ifdef PS2INTRIN_UNSAFE
		asm(
			"plzcw	%[Result],%[Value]"
			: [Result] "=r" (result.v)
			: [Value] "r" (v.v)
		);
#else
		asm(
			"pcpyld	%[ValueLo],%[ValueHi],%[ValueLo]\n\t"
			"plzcw	%[ResultLo],%[ValueLo]\n\t"
			"pcpyud	%[ResultHi],%[ResultLo],%[ResultLo]"
			: [ResultLo] "=r" (result.lo),
			  [ResultHi] "=&r" (result.hi)
			: [ValueLo] "r" (v.lo),
			  [ValueHi] "r" (v.hi)
		);
#endif

		return result;
	}

	/// @brief PLZCW : Parallel Leading Zero or one Count Word
	/// 
	/// Uses only the low 64 bits / 2 values from the 4 value argument. For each value, count the
	/// number of leading bits that have the same value minus 1. This means numbers starting with
	/// '0b1110...' and '0b0001...' will both return '2' as there are 3 bits of the same value
	/// beginning at the highest bit and we discard the count of the sign bit.
	/// @param v Numbers to count leading bits of
	/// @return Amount of same leading bits minus 1
	FORCEINLINE CONST m128u32 mm_clb_epu32(m128u32 v)
	{
		return mm_clb_epi32(mm_castepi32_epu32(v));
	}

	/// @brief PLZCW : Parallel Leading Zero or one Count Word
	/// 
	/// Split the 64 bit argument into 2 32-bit values. For each value, count the number of
	/// leading bits that have the same value minus 1. This means numbers starting with
	/// '0b1110...' and '0b0001...' will both return '2' as there are 3 bits of the same value
	/// beginning at the highest bit and we discard the count of the sign bit.
	/// @param v Concatenated numbers to count leading bits of
	/// @return Concatenated amount of same leading bits minus 1; low 32 bits are the value for
	/// the low 32 bits of the input argument and the high 32 bits correspond to the high 32 bits
	/// of the input
	FORCEINLINE CONST uint64_t mm_clb_u64(uint64_t v)
	{
		uint64_t result = {};

		asm(
			"plzcw	%[Result],%[Value]"
			: [Result] "=r" (result)
			: [Value] "r" (v)
		);

		return result;
	}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

/*
* 
* Template wrappers around the function macros.
* 
* It would be great to have a C++ wrapper around the Multimedia and VU0 instructions. That exists
* in the form of 'std::simd' for C++26. The experimental version is already in GCC and works even
* when compiling for PS2 but doesn't use the intrinsics. It would be great if someone could change
* that instead of reimplementing 'std::simd' here.
* 
*/


namespace
{
	/// @brief PREF : PREFetch
	/// 
	/// Prefetch data from memory to the cache.
	/// 
	/// Can set specific hint value. Only 0 is supported as this value, other values are
	/// reserved. Possible values: [0, 32). Prefer using the function 'prefetch' instead.
	/// 
	/// Does nothing on uncached memory locations.
	/// 
	/// Not allowed on memory locations not present in the TLB. Not recently used pages may not
	/// be present in the TLB, reducing effectiveness. Prefetch may not happen when the memory
	/// bus is used.
	/// @tparam Hint The hint value to use
	/// @param ptr The address to prefetch
	template <unsigned Hint>
	FORCEINLINE void prefetch(void* ptr) noexcept
	{
		PREF(ptr, (Hint));
	}

	/// @brief MTSAB : Move byte count To Shift Amount register (Byte)
	/// 
	/// Set a byte shift count in the shift amount register.
	/// 
	/// The values of 'byte_amount' and 'FixedByteAmount' are XOR'ed together. The resulting value
	/// is the amount of bytes 'byte_shift_logical_right' will shift by.
	/// 
	/// Allowable values for 'byte_amount' and 'FixedByteAmount' are [0,15]. Only the lower 4 bits
	/// are used, others are ignored.
	/// 
	/// Use this if you require both the variable and constant values usable by the 'mtsab'
	/// instruction. Otherwise, prefer the no argument overload if you only have a constant value
	/// and non-template 'set_sa_8' if you only have a variable shift amount.
	/// 
	/// This function writes to global state (SA).
	/// @tparam FixedByteAmount The compile time constant byte-amount to set up the shift amount
	/// register for
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param byte_amount The variable byte-amount to set up the shift amount register for
	template <unsigned FixedByteAmount>
	FORCEINLINE void set_sa_8(sa_state_t* state, unsigned byte_amount) noexcept
	{
		MTSAB_BOTH(state, byte_amount, (FixedByteAmount));
	}

	/// @brief MTSAB : Move byte count To Shift Amount register (Byte)
	/// 
	/// Set a byte shift count in the shift amount register.
	/// 
	/// The 'FixedByteAmount' value is the amount of bytes 'byte_shift_logical_right' will shift by.
	/// 
	/// Allowable values for 'FixedByteAmount' are [0,15]. Only the lower 4 bits are used, others
	/// are ignored.
	/// 
	/// Use this function if your shift amount is known at compile time. Using this function does not
	/// use a general purpose register. Otherwise, prefer the overload with argument or non-template
	/// overload.
	/// 
	/// This function writes to global state (SA).
	/// @tparam FixedByteAmount The compile time constant byte-amount to set up the shift amount
	/// register for
	/// @param state Additional state used in safe mode. May not be NULL.
	template <unsigned FixedByteAmount>
	FORCEINLINE void set_sa_8(sa_state_t* state) noexcept
	{
		MTSAB_IMMEDIATE(state, (FixedByteAmount));
	}

	/// @brief MTSAH : Move halfword count To Shift Amount register (Halfword)
	/// 
	/// Set a halfword shift count in the shift amount register.
	/// 
	/// The values of 'halfword_amount' and 'FixedHalfwordAmount' are XOR'ed together. The
	/// resulting value is the amount of halfwords 'byte_shift_logical_right' will shift by.
	/// 
	/// Allowable values for 'halfword_amount' and 'FixedHalfwordAmount' are [0,7]. Only the
	/// lower 3 bits are used, others are ignored.
	/// 
	/// Use this function if you require both the variable and constant values usable by the
	/// 'mtsah' instruction. Otherwise, prefer the no-argument overload if you only have a
	/// constant value and the non-template overload if you only have a variable shift amount.
	/// 
	/// This function writes to global state (SA).
	/// @tparam FixedHalfwordAmount The compile time constant halfword-amount to set up the shift
	/// amount register for
	/// @param state Additional state used in safe mode. May not be NULL.
	/// @param halfword_amount The variable halfword-amount to set up the shift amount register
	/// for
	template <unsigned FixedHalfwordAmount>
	FORCEINLINE void set_sa_16(sa_state_t* state, unsigned halfword_amount) noexcept
	{
		MTSAH_BOTH(state, halfword_amount, (FixedHalfwordAmount));
	}

	/// @brief MTSAH : Move halfword count To Shift Amount register (Halfword)
	/// 
	/// Set a halfword shift count in the shift amount register.
	/// 
	/// The 'FixedHalfwordAmount' value is the amount of halfwords 'byte_shift_logical_right'
	/// will shift by.
	/// 
	/// Allowable values for 'immediate' are [0,7]. Only the lower 3 bits are used, others are
	/// ignored.
	/// 
	/// Use this function if your shift amount is known at compile time. Using this function does
	/// not use a general purpose register. Otherwise, prefer the overload with an argument or
	/// the non-template overload.
	/// 
	/// This function writes to global state (SA).
	/// @tparam FixedHalfwordAmount The compile time constant halfword-amount to set up the shift
	/// amount register for
	/// @param state Additional state used in safe mode. May not be NULL.
	template <unsigned FixedHalfwordAmount>
	FORCEINLINE void set_sa_16(sa_state_t* state) noexcept
	{
		MTSAH_IMMEDIATE(state, (FixedHalfwordAmount));
	}

	/// @brief PSLLH : Parallel Shift Left Logical Halfword
	/// 
	/// Logically left shift 16-bit values. Shifts in '0's into the lower bits.
	/// @tparam ShiftAmount Amount of bits to shift the source value left. Must be in range [0, 15]
	/// @param v Value to shift
	/// @return Shifted values
	template <unsigned ShiftAmount>
	FORCEINLINE CONST m128i16 mm_sll_epi16(m128i16 v) noexcept
	{
		m128i16 result = {};

		PSLLH(result, v, (ShiftAmount));

		return result;
	}

	/// @brief PSLLH : Parallel Shift Left Logical Halfword
	/// 
	/// Logically left shift 16-bit values. Shifts in '0's into the lower bits.
	/// @tparam ShiftAmount Amount of bits to shift the source value left. Must be in range [0, 15]
	/// @param v Values to shift
	/// @return Shifted values
	template <unsigned ShiftAmount>
	FORCEINLINE CONST m128u16 mm_sll_epu16(m128u16 v) noexcept
	{
		return mm_castepu16_epi16(mm_sll_epi16<ShiftAmount>(mm_castepi16_epu16(v)));
	}

	/// @brief PSLLW : Parallel Shift Left Logical Word
	/// 
	/// Logically left shift 32-bit values. Shifts in '0's into the lower bits.
	/// @tparam ShiftAmount Amount of bits to shift the source value left. Must be in range [0, 31]
	/// @param v Values to shift
	/// @return Shifted values
	template <unsigned ShiftAmount>
	FORCEINLINE CONST m128i32 mm_sll_epi32(m128i32 v) noexcept
	{
		m128i32 result = {};

		PSLLW(result, v, (ShiftAmount));

		return result;
	}

	/// @brief PSLLW : Parallel Shift Left Logical Word
	/// 
	/// Logically left shift 32-bit values. Shifts in '0's into the lower bits.
	/// @tparam ShiftAmount Amount of bits to shift the source value left. Must be in range [0, 31]
	/// @param v Values to shift
	/// @return Shifted values
	template <unsigned ShiftAmount>
	FORCEINLINE CONST m128u32 mm_sll_epu32(m128u32 v) noexcept
	{
		return mm_castepu32_epi32(mm_sll_epi32<ShiftAmount>(mm_castepi32_epu32(v)));
	}

	/// @brief PSRAH : Parallel Shift Right Arithmetic Halfword
	/// 
	/// Arithmetically right shift 16-bit values. Shifts in sign bits into the upper bits.
	/// @tparam ShiftAmount Amount of bits to shift the source value right. Must be in range
	/// [0, 15]
	/// @param v Values to shift
	/// @return Shifted values
	template <unsigned ShiftAmount>
	FORCEINLINE CONST m128i16 mm_sra_epi16(m128i16 v) noexcept
	{
		m128i16 result = {};

		PSRAH(result, v, (ShiftAmount));

		return result;
	}

	/// @brief PSRAW : Parallel Shift Right Arithmetic Word
	/// 
	/// Arithmetically right shift 32-bit values. Shifts in sign bits into the upper bits.
	/// @tparam ShiftAmount Amount of bits to shift the source value right. Must be in range
	/// [0, 31]
	/// @param v Values to shift
	/// @return Shifted values
	template <unsigned ShiftAmount>
	FORCEINLINE CONST m128i32 mm_sra_epi32(m128i32 v) noexcept
	{
		m128i32 result = {};

		PSRAW(result, v, (ShiftAmount));

		return result;
	}

	/// @brief PSRLH : Parallel Shift Right Logical Halfword
	/// 
	/// Logically right shift 16-bit values. Shifts in '0's into the upper bits.
	/// @tparam ShiftAmount Amount of bits to shift the source value right. Must be in range
	/// [0, 15]
	/// @param v Values to shift
	/// @return Shifted values
	template <unsigned ShiftAmount>
	FORCEINLINE CONST m128u16 mm_srl_epu16(m128i16 v) noexcept
	{
		m128u16 result = {};

		PSRLH(result, v, (ShiftAmount));

		return result;
	}

	/// @brief PSRLW : Parallel Shift Right Logical Word
	/// 
	/// Logically right shift 32-bit values. Shifts in '0's into the upper bits.
	/// @tparam ShiftAmount Amount of bits to shift the source value right. Must be in range
	/// [0, 31]
	/// @param v Values to shift
	/// @return Shifted values
	template <unsigned ShiftAmount>
	FORCEINLINE CONST m128u32 mm_srl_epu32(m128u32 v) noexcept
	{
		m128u32 result = {};

		PSRLW(result, v, (ShiftAmount));

		return result;
	}
}
#endif

#undef FORCEINLINE
#undef CONST
#undef UNSEQUENCED
#undef PURE
#undef REPRODUCIBLE
#undef ALIGNAS16
#undef DECLAREVECTOR_UNSAFE
#undef DECLAREVECTOR_SAFE
#undef DECLAREVECTOR
#undef MODE_TI
