/* ************************************************************************
 * Copyright 2018-2020 Advanced Micro Devices, Inc.
 * ************************************************************************ */

#ifndef _TESTING_UTILITY_H_
#define _TESTING_UTILITY_H_

#include "logging.h"
#include "rocblas.h"
#include "rocblas_test.hpp"
#include "utility.h"
#include <cstdio>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

/*!\file
 * \brief provide common utilities
 */

// We use rocblas_cout and rocblas_cerr
// This must come after the header #includes above, to avoid poisoning system headers.
// This is only enabled for rocblas-test and rocblas-bench.
#if defined(GOOGLE_TEST) || defined(ROCBLAS_BENCH)
#undef stdout
#undef stderr
// All stdio and std::ostream functions related to stdout and stderr are poisoned, as are functions
// which can create buffer overflows
#pragma GCC poison cout cerr clog stdout stderr gets puts putchar fprintf printf sprintf vfprintf \
    vprintf vsprintf
#endif

/* ============================================================================================ */
/*! \brief  local handle which is automatically created and destroyed  */
class rocblas_local_handle
{
    rocblas_handle handle;

public:
    rocblas_local_handle()
    {
        rocblas_create_handle(&handle);
    }
    ~rocblas_local_handle()
    {
        rocblas_destroy_handle(handle);
    }

    // Allow rocblas_local_handle to be used anywhere rocblas_handle is expected
    operator rocblas_handle&()
    {
        return handle;
    }
    operator const rocblas_handle&() const
    {
        return handle;
    }
};

/* ============================================================================================ */
/*  device query and print out their ID and name */
rocblas_int query_device_property();

/*  set current device to device_id */
void set_device(rocblas_int device_id);

/* ============================================================================================ */
/*  timing: HIP only provides very limited timers function clock() and not general;
            rocblas sync CPU and device and use more accurate CPU timer*/

/*! \brief  CPU Timer(in microsecond): synchronize with the default device and return wall time */
double get_time_us(void);

/*! \brief  CPU Timer(in microsecond): synchronize with given queue/stream and return wall time */
double get_time_us_sync(hipStream_t stream);

/* ============================================================================================ */
// Return path of this executable
std::string rocblas_exepath();

class rocblas_print_helper
{
public:
    /************************************************************************************
     * Print values
     ************************************************************************************/
    // Default output
    template <typename T>
    static void print_value(std::ostream& os, const T& x)
    {
        os << x;
    }

    // Floating-point output
    static void print_value(std::ostream& os, double x)
    {
        if(std::isnan(x))
            os << ".nan";
        else if(std::isinf(x))
            os << (x < 0 ? "-.inf" : ".inf");
        else
        {
            char s[32];
            snprintf(s, sizeof(s) - 2, "%.17g", x);

            // If no decimal point or exponent, append .0
            char* end = s + strcspn(s, ".eE");
            if(!*end)
                strcpy(end, ".0");
            os << s;
        }
    }

    // Complex output
    template <typename T>
    static void print_value(std::ostream& os, const rocblas_complex_num<T>& x)
    {
        os << "'(";
        print_value(os, std::real(x));
        os << ",";
        print_value(os, std::imag(x));
        os << ")'";
    }
};

/* ============================================================================================ */
/*! \brief  Debugging purpose, print out CPU and GPU result matrix, not valid in complex number  */
template <typename T>
inline void rocblas_print_matrix(
    std::vector<T> CPU_result, std::vector<T> GPU_result, size_t m, size_t n, size_t lda)
{
    for(size_t i = 0; i < m; i++)
        for(size_t j = 0; j < n; j++)
        {
            rocblas_cout << "matrix  col " << i << ", row " << j
                         << ", CPU result=" << CPU_result[j + i * lda]
                         << ", GPU result=" << GPU_result[j + i * lda] << "\n";
        }
}

template <typename T>
void rocblas_print_matrix(const char* name, T* A, rocblas_int m, rocblas_int n, rocblas_int lda)
{
    printf("---------- %s ----------\n", name);
    for(int i = 0; i < m; i++)
    {
        for(int j = 0; j < n; j++)
        {
            rocblas_print_helper::print_value(rocblas_cout, A[i + j * lda]);
            printf(" ");
        }
        printf("\n");
    }
}

#endif
