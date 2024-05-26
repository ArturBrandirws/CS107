#include "vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Initialize the vector
void VectorNew(vector *v, int elemSize, VectorFreeFunction freeFn, int initialAllocation) {
    // Set the size of each element
    v->elemSize = elemSize;
    // Set the logical length (number of elements) to 0
    v->logLength = 0;
    // Set the allocated length to the initial allocation
    v->allocLength = initialAllocation;
    // Set the free function pointer
    v->freeFn = freeFn;
    // Allocate memory for the elements
    v->elements = malloc(initialAllocation * elemSize);
    // Ensure the memory allocation was successful
    assert(v->elements != NULL);
}

// Dispose of the vector
void VectorDispose(vector *v) {
    // If there's a free function, apply it to each element
    if (v->freeFn != NULL) {
        for (int i = 0; i < v->logLength; i++) {
            v->freeFn((char *)v->elements + i * v->elemSize);
        }
    }
    // Free the allocated memory
    free(v->elements);
}

// Return the number of elements in the vector
int VectorLength(const vector *v) {
    // Return the logical length
    return v->logLength;
}

// Get a pointer to the element at the given position
void *VectorNth(const vector *v, int position) {
    // Ensure the position is valid
    assert(position >= 0 && position < v->logLength);
    // Return the pointer to the element at the position
    return (char *)v->elements + position * v->elemSize;
}

// Replace the element at the given position
void VectorReplace(vector *v, const void *elemAddr, int position) {
    // Ensure the position is valid
    assert(position >= 0 && position < v->logLength);
    // Calculate the target position
    void *target = (char *)v->elements + position * v->elemSize;
    // If there's a free function, free the existing element
    if (v->freeFn != NULL) {
        v->freeFn(target);
    }
    // Copy the new element to the target position
    memcpy(target, elemAddr, v->elemSize);
}

// Insert an element at the given position
void VectorInsert(vector *v, const void *elemAddr, int position) {
    // Ensure the position is valid
    assert(position >= 0 && position <= v->logLength);
    // Double the allocated length if necessary
    if (v->logLength == v->allocLength) {
        v->allocLength *= 2;
        // Reallocate memory for the elements
        v->elements = realloc(v->elements, v->allocLength * v->elemSize);
        // Ensure the memory reallocation was successful
        assert(v->elements != NULL);
    }
    // Calculate the target position
    void *target = (char *)v->elements + position * v->elemSize;
    // Move the existing elements to make space for the new element
    memmove((char *)target + v->elemSize, target, (v->logLength - position) * v->elemSize);
    // Copy the new element to the target position
    memcpy(target, elemAddr, v->elemSize);
    // Increase the logical length
    v->logLength++;
}

// Append an element to the end of the vector
void VectorAppend(vector *v, const void *elemAddr) {
    // Double the allocated length if necessary
    if (v->logLength == v->allocLength) {
        v->allocLength *= 2;
        // Reallocate memory for the elements
        v->elements = realloc(v->elements, v->allocLength * v->elemSize);
        // Ensure the memory reallocation was successful
        assert(v->elements != NULL);
    }
    // Calculate the target position
    void *target = (char *)v->elements + v->logLength * v->elemSize;
    // Copy the new element to the target position
    memcpy(target, elemAddr, v->elemSize);
    // Increase the logical length
    v->logLength++;
}

// Delete the element at the given position
void VectorDelete(vector *v, int position) {
    // Ensure the position is valid
    assert(position >= 0 && position < v->logLength);
    // Calculate the target position
    void *target = (char *)v->elements + position * v->elemSize;
    // If there's a free function, free the element at the target position
    if (v->freeFn != NULL) {
        v->freeFn(target);
    }
    // Move the elements to close the gap left by the deleted element
    memmove(target, (char *)target + v->elemSize, (v->logLength - position - 1) * v->elemSize);
    // Decrease the logical length
    v->logLength--;
}

// Sort the elements of the vector
void VectorSort(vector *v, VectorCompareFunction compare) {
    // Use qsort to sort the elements
    qsort(v->elements, v->logLength, v->elemSize, compare);
}

// Apply a function to each element of the vector
void VectorMap(vector *v, VectorMapFunction mapFn, void *auxData) {
    // Apply the function to each element
    for (int i = 0; i < v->logLength; i++) {
        mapFn((char *)v->elements + i * v->elemSize, auxData);
    }
}

// Search for an element in the vector
int VectorSearch(const vector *v, const void *key, VectorCompareFunction searchFn, int startIndex, bool isSorted) {
    // Ensure the start index is valid
    assert(startIndex >= 0 && startIndex <= v->logLength);
    // Calculate the base address for the search
    void *base = (char *)v->elements + startIndex * v->elemSize;
    // Declare a pointer to the result
    void *result;
    // If the vector is sorted, use bsearch
    if (isSorted) {
        result = bsearch(key, base, v->logLength - startIndex, v->elemSize, searchFn);
    } else {
        // If the vector is not sorted, use lfind
        result = lfind(key, base, &(v->logLength), v->elemSize, searchFn);
    }
    // If the result is NULL, return kNotFound
    if (result == NULL) return kNotFound;
    // Return the index of the found element
    return ((char *)result - (char *)v->elements) / v->elemSize;
}

// Define kNotFound as a constant for not found searches
static const int kNotFound = -1;
