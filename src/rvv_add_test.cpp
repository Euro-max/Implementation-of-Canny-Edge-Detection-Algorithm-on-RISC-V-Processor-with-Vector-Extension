/**
 * @file rvv_add_test.cpp
 * @brief Sandbox test to verify RVV strip-mining and tail case handling.
 */

#include <stdio.h>
#include <stdint.h>
#include <riscv_vector.h>   // RVV intrinsics live here

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

    return 0;
}