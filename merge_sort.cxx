/**
 * @file merge_sort.cxx
 * @author Alex H Levi (alex.h.levi.il@gmail.com)
 * @brief Sample merge sort algorithm implementation
 * @version 0.1
 * @date 2021-12-12
 * 
 * @copyright Alex Levi Software Consulting Copyright (c) 2021
 * 
 */
#include <iostream>
#include <cassert>
#include <random>
#include <array>
#include <algorithm>

//#define _MERGE_SORT_PRINT_ 1

using namespace std;

/**
 * @brief Merge sort both halves of the array 
 * 
 * @tparam T1 array type
 * @param parr master array to sort
 * @param begin starting index
 * @param m middle index
 * @param end ending index (one past last)
 */
template<typename T1>
void merge(T1* parr, size_t begin, size_t m, size_t end){
    
    // determinet the left half size of the array
    size_t leftHalf = m - begin;
    // determine the right half size of the array
    size_t rightHalf = end - m;
    
    // let us allocate two buffers to hold left and right halves of the array
    T1* leftBuff = new T1[leftHalf]; // <= left
    T1* rightBuff = new T1[rightHalf]; // <= right

    // copy the first half of the master array into left half buffer
    size_t idxL = 0, idxR = 0, idxM = begin;
    for(; idxL < leftHalf; idxL++, idxM++) {
        leftBuff[idxL] = parr[idxM];
    }
    
    // copy the second half of the master array into right half buffer
    for(; idxR < rightHalf; idxR++) {
        rightBuff[idxR] = parr[m + idxR];
    }
    
    // reset indices: idxL - left; idxR - right; idxM - master array index
    idxL = 0; idxR = 0; idxM = begin;
    
    // sort copy values from both halves
    while (idxL < leftHalf && idxR < rightHalf) { // copy until at least one half is fully copied
        // take a look at the left half if it is greater than the right half
        if (leftBuff[idxL] <= rightBuff[idxR]) {
            parr[idxM] = leftBuff[idxL];
            idxL++;
        } else {
            // copy the right half into the master array
            parr[idxM] = rightBuff[idxR];
            idxR++;
        }
        idxM++; // adjust the master array index
    }

    // left half remainder copy over
    for(; idxL < leftHalf; idxL++, idxM++) {
        parr[idxM] = leftBuff[idxL];
    }

    // right half remainder copy over
    for(; idxR < rightHalf; idxR++, idxM++) {
        parr[idxM] = rightBuff[idxR];
    }

    // clean up left and right buffers
    delete [] leftBuff;
    delete [] rightBuff;

#ifdef _MERGE_SORT_PRINT_    
    for (int i = 0; i < end; i++) {
        cout << "arr[" << i << "]=" << parr[i] << endl;
    }
#endif 
}

/**
 * @brief Divide and conquer merge source \
 *      keep on dividing the array into halves until only one element is left \
 *      on each side
 * @tparam T1 array type
 * @param parr pointer to array to sort
 * @param begin starting offset 
 * @param end ending offset (one past last)
 */
template<typename T1>
void mergeSort(T1* parr, size_t begin, size_t end) {
    // break out recursion
    if (begin >= end) return;
    // if just one element - dont do anything
    int m = begin + (end - begin)/2;
    // if just element left - break out recursion
    if (!(m-begin)) return;
    // merge sort the left half of the array
    mergeSort(parr, begin, m);
    // merge sort the right half of the array
    mergeSort(parr, m, end);
    // merge both parts of the array
    merge(parr, begin, m, end);
}


int main(){

    using T1 = int;
    constexpr size_t data_size = 25;
    random_device rnd_eng;
    uniform_int_distribution<size_t> uniform_dist(0, 100);
    uniform_int_distribution<size_t> rnd_array_size(2, 50);
    array<T1, data_size> data;

    generate(data.begin(), data.end(), [&](){return uniform_dist(rnd_eng);});
 
    for_each(data.begin(), data.end(), [](T1 v){
        cout << v << ",";
    });
    cout << endl;
    mergeSort<T1>(data.data(), 0, data.size());
    for_each(data.begin(), data.end(), [](T1 v){
        cout << v << ",";
    });
    cout << endl;
}