/* ************************************************************************
 * Copyright 2018-2020 Advanced Micro Devices, Inc.
 * ************************************************************************ */

#include "bytes.hpp"
#include "cblas_interface.hpp"
#include "flops.hpp"
#include "near.hpp"
#include "norm.hpp"
#include "rocblas.hpp"
#include "rocblas_init.hpp"
#include "rocblas_math.hpp"
#include "rocblas_random.hpp"
#include "rocblas_test.hpp"
#include "rocblas_vector.hpp"
#include "unit.hpp"
#include "utility.hpp"

template <typename T>
void testing_spr2_bad_arg(const Arguments& arg)
{
    auto rocblas_spr2_fn = arg.fortran ? rocblas_spr2<T, true> : rocblas_spr2<T, false>;

    rocblas_fill         uplo  = rocblas_fill_upper;
    rocblas_int          N     = 100;
    rocblas_int          incx  = 1;
    rocblas_int          incy  = 1;
    T                    alpha = 0.6;
    rocblas_local_handle handle{arg};

    size_t abs_incx = incx >= 0 ? incx : -incx;
    size_t abs_incy = incy >= 0 ? incy : -incy;
    size_t size_A   = size_t(N) * (N + 1) / 2;
    size_t size_x   = size_t(N) * abs_incx;
    size_t size_y   = size_t(N) * abs_incy;

    // allocate memory on device
    device_vector<T> dA_1(size_A);
    device_vector<T> dx(size_x);
    device_vector<T> dy(size_y);
    CHECK_DEVICE_ALLOCATION(dA_1.memcheck());
    CHECK_DEVICE_ALLOCATION(dx.memcheck());
    CHECK_DEVICE_ALLOCATION(dy.memcheck());

    EXPECT_ROCBLAS_STATUS(
        rocblas_spr2_fn(handle, rocblas_fill_full, N, &alpha, dx, incx, dy, incy, dA_1),
        rocblas_status_invalid_value);

    EXPECT_ROCBLAS_STATUS(rocblas_spr2_fn(handle, uplo, N, &alpha, nullptr, incx, dy, incy, dA_1),
                          rocblas_status_invalid_pointer);

    EXPECT_ROCBLAS_STATUS(rocblas_spr2_fn(handle, uplo, N, &alpha, dx, incx, nullptr, incy, dA_1),
                          rocblas_status_invalid_pointer);

    EXPECT_ROCBLAS_STATUS(rocblas_spr2_fn(handle, uplo, N, &alpha, dx, incx, dy, incy, nullptr),
                          rocblas_status_invalid_pointer);

    EXPECT_ROCBLAS_STATUS(rocblas_spr2_fn(nullptr, uplo, N, &alpha, dx, incx, dy, incy, dA_1),
                          rocblas_status_invalid_handle);
}

template <typename T>
void testing_spr2(const Arguments& arg)
{
    auto rocblas_spr2_fn = arg.fortran ? rocblas_spr2<T, true> : rocblas_spr2<T, false>;

    rocblas_int          N       = arg.N;
    rocblas_int          incx    = arg.incx;
    rocblas_int          incy    = arg.incy;
    T                    h_alpha = arg.get_alpha<T>();
    rocblas_fill         uplo    = char2rocblas_fill(arg.uplo);
    rocblas_local_handle handle{arg};

    // argument check before allocating invalid memory
    if(N < 0 || !incx || !incy)
    {
        EXPECT_ROCBLAS_STATUS(
            rocblas_spr2_fn(handle, uplo, N, nullptr, nullptr, incx, nullptr, incy, nullptr),
            rocblas_status_invalid_size);

        return;
    }

    size_t abs_incx = incx >= 0 ? incx : -incx;
    size_t abs_incy = incy >= 0 ? incy : -incy;
    size_t size_A   = size_t(N) * (N + 1) / 2;
    size_t size_x   = size_t(N) * abs_incx;
    size_t size_y   = size_t(N) * abs_incy;

    // Naming: dK is in GPU (device) memory. hK is in CPU (host) memory
    host_vector<T> hA_1(size_A);
    host_vector<T> hA_2(size_A);
    host_vector<T> hA_gold(size_A);
    host_vector<T> hx(size_x);
    host_vector<T> hy(size_y);
    host_vector<T> halpha(1);
    CHECK_HIP_ERROR(hA_1.memcheck());
    CHECK_HIP_ERROR(hA_2.memcheck());
    CHECK_HIP_ERROR(hA_gold.memcheck());
    CHECK_HIP_ERROR(hx.memcheck());
    CHECK_HIP_ERROR(hy.memcheck());
    CHECK_HIP_ERROR(halpha.memcheck());

    halpha[0] = h_alpha;

    // allocate memory on device
    device_vector<T> dA_1(size_A);
    device_vector<T> dA_2(size_A);
    device_vector<T> dx(size_x);
    device_vector<T> dy(size_y);
    device_vector<T> d_alpha(1);
    CHECK_DEVICE_ALLOCATION(dA_1.memcheck());
    CHECK_DEVICE_ALLOCATION(dA_2.memcheck());
    CHECK_DEVICE_ALLOCATION(dx.memcheck());
    CHECK_DEVICE_ALLOCATION(dy.memcheck());
    CHECK_DEVICE_ALLOCATION(d_alpha.memcheck());

    double gpu_time_used, cpu_time_used;
    double rocblas_error_1;
    double rocblas_error_2;

    // Initial Data on CPU
    rocblas_init(hA_1, true);
    rocblas_init(hx, false);
    rocblas_init(hy, false);

    // copy matrix is easy in STL; hA_gold = hA_1: save a copy in hA_gold which will be output of
    // CPU BLAS
    hA_gold = hA_1;
    hA_2    = hA_1;

    // copy data from CPU to device
    CHECK_HIP_ERROR(dA_1.transfer_from(hA_1));
    CHECK_HIP_ERROR(dA_2.transfer_from(hA_1));
    CHECK_HIP_ERROR(dx.transfer_from(hx));
    CHECK_HIP_ERROR(dy.transfer_from(hy));
    CHECK_HIP_ERROR(d_alpha.transfer_from(halpha));

    if(arg.unit_check || arg.norm_check)
    {
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));
        CHECK_ROCBLAS_ERROR(rocblas_spr2_fn(handle, uplo, N, &h_alpha, dx, incx, dy, incy, dA_1));

        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_device));
        CHECK_ROCBLAS_ERROR(rocblas_spr2_fn(handle, uplo, N, d_alpha, dx, incx, dy, incy, dA_2));

        // CPU BLAS
        cpu_time_used = get_time_us_no_sync();
        cblas_spr2<T>(uplo, N, h_alpha, hx, incx, hy, incy, hA_gold);
        cpu_time_used = get_time_us_no_sync() - cpu_time_used;

        // copy output from device to CPU
        CHECK_HIP_ERROR(hA_1.transfer_from(dA_1));
        CHECK_HIP_ERROR(hA_2.transfer_from(dA_2));

        if(arg.unit_check)
        {
            unit_check_general<T>(1, size_A, 1, hA_gold, hA_1);
            unit_check_general<T>(1, size_A, 1, hA_gold, hA_2);
        }

        if(arg.norm_check)
        {
            rocblas_error_1 = norm_check_general<T>('F', 1, size_A, 1, hA_gold, hA_1);
            rocblas_error_2 = norm_check_general<T>('F', 1, size_A, 1, hA_gold, hA_2);
        }
    }

    if(arg.timing)
    {
        int number_cold_calls = arg.cold_iters;
        int number_hot_calls  = arg.iters;
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));

        for(int iter = 0; iter < number_cold_calls; iter++)
        {
            rocblas_spr2_fn(handle, uplo, N, &h_alpha, dx, incx, dy, incy, dA_1);
        }

        hipStream_t stream;
        CHECK_ROCBLAS_ERROR(rocblas_get_stream(handle, &stream));
        gpu_time_used = get_time_us_sync(stream); // in microseconds

        for(int iter = 0; iter < number_hot_calls; iter++)
        {
            rocblas_spr2_fn(handle, uplo, N, &h_alpha, dx, incx, dy, incy, dA_1);
        }

        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        ArgumentModel<e_uplo, e_N, e_alpha, e_incx, e_incy>{}.log_args<T>(rocblas_cout,
                                                                          arg,
                                                                          gpu_time_used,
                                                                          spr2_gflop_count<T>(N),
                                                                          spr2_gbyte_count<T>(N),
                                                                          cpu_time_used,
                                                                          rocblas_error_1,
                                                                          rocblas_error_2);
    }
}
