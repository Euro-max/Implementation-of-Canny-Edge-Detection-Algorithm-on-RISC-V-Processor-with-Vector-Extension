/**
 * @file rvv_add_test.c
 * @brief Introduction to RISC-V Vector (RVV) intrinsics using a scalar addition.
 * * This file contains a foundational example of RVV programming. It demonstrates 
 * the "strip-mining" technique, where an array of arbitrary length is processed 
 * in hardware-defined vector chunks using the `vsetvl` instruction. It handles 
 * edge cases automatically, such as arrays whose length is not a clean multiple 
 * of the hardware vector register length (VLEN).
 */

#include <stdio.h>
#include <stdint.h>
#include <riscv_vector.h>   // RVV intrinsics live here

/**
 * @brief Adds a scalar value to every element in an array using RVV instructions.
 * * This function utilizes hardware vectorization to process multiple array elements 
 * simultaneously. It dynamically queries the CPU for the maximum number of elements 
 * it can process per iteration (Vector Length, or 'vl') and steps through the 
 * array until all elements, including the remainder (tail), are processed.
 * * @param array Pointer to the start of the 32-bit integer array.
 * @param value The scalar value to add to every element.
 * @param n     The total number of elements in the array.
 */
// This function adds a scalar value to every element in an array
// using RVV vector instructions
void vector_add_scalar(int32_t* array, int32_t value, int n) {

    int i = 0;                          // current position in array

    while (i < n) {                     // keep going until all elements done

        // Step A: Ask CPU "how many elements can you handle right now?"
        // e32 = each element is 32 bits
        // m1  = use LMUL=1 (one vector register)
        // n-i = how many elements we still have left
        int vl = __riscv_vsetvl_e32m1(n - i);

        // Step B: Load vl elements from array starting at position i
        // vle32 = vector load, 32-bit elements
        // v_i32m1 = returns a vector of int32, LMUL=1
        vint32m1_t vec = __riscv_vle32_v_i32m1(array + i, vl);

        // Step C: Add the scalar value to every element in the vector
        // vadd_vx = vector + scalar (x means scalar)
        vint32m1_t result = __riscv_vadd_vx_i32m1(vec, value, vl);

        // Step D: Store the result back into the array
        // vse32 = vector store, 32-bit elements
        __riscv_vse32_v_i32m1(array + i, result, vl);

        // Step E: Move forward by however many elements we just processed
        i += vl;
    }
}

/**
 * @brief Main execution entry point.
 * * Initializes a 10-element test array to verify the RVV strip-mining loop. 
 * Purposefully uses an array size of 10 to ensure the vector length (vl) correctly 
 * handles fractional/tail iterations on hardware where VLEN is typically a power of 2.
 * * @return 0 on successful execution.
 */
int main() {

    // Create an array of 10 numbers
    // (10 is NOT a multiple of 8 — this tests the tail case!)
    int32_t arr[10] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};

    printf("Before: ");
    for (int i = 0; i < 10; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    // Add 5 to every element using RVV
    vector_add_scalar(arr, 5, 10);

    printf("After:  ");
    for (int i = 0; i < 10; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    // Verify correctness
    int correct = 1;
    int32_t expected[10] = {15, 25, 35, 45, 55, 65, 75, 85, 95, 105};
    for (int i = 0; i < 10; i++) {
        if (arr[i] != expected[i]) {
            correct = 0;
            printf("WRONG at index %d: got %d, expected %d\n",
                   i, arr[i], expected[i]);
        }
    }

    if (correct) {
        printf("ALL CORRECT! RVV is working!\n");
    }

    return 0;
}