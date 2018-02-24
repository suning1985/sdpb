//
// Created by Petr Kravchuk on 8/14/17.
// Modified and merged into SDPB by Rajeev Erramilli.
//
#ifdef __SDPB_CUDA__
#define CUDA_API_PER_THREAD_DEFAULT_STREAM
#include <cuda_runtime.h>
#include "cublas_v2.h"
#include <cublasXt.h>
#endif

#include "mpmat.h"
#include <gmpxx.h>
#include <math.h>
#include <cassert>
#include <iostream>
//#include "tests/mpmat_tests.h"
// #include <mkl.h>
#include "../Timers.h"
#include <omp.h>

template <typename T>
inline T ceil_div(T a, T b) {
	return a / b + ( (a % b != 0) & (a > 0) );
}

template <typename T>
inline T min(T a, T b) { return a < b ? a : b ; }

template <typename T>
inline T max (T a,T b) { return a>b ? a : b; }

mpf_class * randomGMPVector(size_t size, int prec) {
	gmp_randclass rr(gmp_randinit_default);
	rr.seed(time(NULL));

	auto tmp = new mpf_class [size];

	for (int i = 0; i < size; i++) {
		tmp[i].set_prec(prec);
		tmp[i] = 10*rr.get_f(prec)-5;
	}

	return tmp;
}

#ifndef __SDPB_CUDA__
void mpmat::realloc(size_t mem_a, size_t mem_b, size_t mem_c){

	if (mem_a > len_a){
		std::cerr << "reallocing a to size " << mem_a/(1<<20) << "\n";
		if (len_a != 0) {
			delete [] a_double_array;
		}
		a_double_array = new mpmat_double[mem_a];
		len_a = mem_a;
	}
	if (mem_b > len_b){
		std::cerr << "reallocing b to size " << mem_b/(1<<20) << "\n";
		if (len_b != 0) {
			delete [] b_double_array;
		}
		b_double_array = new mpmat_double[mem_b];
		len_b = mem_b;
	}
	if (mem_c > len_c){
		std::cerr << "reallocing c to size " << mem_c/(1<<20) << "\n";
		if (len_c != 0) {
			delete [] c_double_array;
		}
		c_double_array = new mpmat_double[mem_c];
		len_c = mem_c;
	}
	int mem_t = max(max(mem_a,mem_b),mem_c);
	if (mem_t > len_t){
		if (len_t != 0) delete [] tmp;
		tmp = new mpmat_double [mem_t];
		len_t = mem_t;
	}
}

#else

void mpmat::realloc(size_t mem_a, size_t mem_b, size_t mem_c){
	if (mem_a > len_a){
		if (len_a != 0) {
			cudaFreeHost(a_double_array);
		}
		cudaMallocHost(&a_double_array,mem_a*sizeof(mpmat_double),cudaHostAllocPortable);
		len_a = mem_a;
	}
	if (mem_b > len_b){
		if (len_b != 0) {
			cudaFreeHost(b_double_array);
		}
		cudaMallocHost(&b_double_array,mem_b*sizeof(mpmat_double),cudaHostAllocPortable);

		len_b = mem_b;
	}
	if (mem_c > len_c){
		if (len_c != 0) {
			cudaFreeHost(c_double_array);
		}
		cudaMallocHost(&c_double_array,mem_c*sizeof(mpmat_double),cudaHostAllocPortable);
		len_c = mem_c;
	}
	size_t mem_t = max(max(mem_a,mem_b),mem_c);
	if (mem_t > len_t){
		if (len_t != 0) delete [] tmp;
		tmp = new mpmat_double [mem_t];
		len_t = mem_t;
	}
}


void mpmat::realloc_gpu(size_t mem_a, size_t mem_b, size_t mem_c){
	// std::cerr << "reallocing both " << mem_a << " " << mem_b << " " << mem_c << "\n";
	if (mem_a > len_a){
		if (len_a != 0) {
			cudaFreeHost(a_double_array);
			for (int i = 0; i < gpu_count; ++i){
				cudaSetDevice(i);
				cudaFree(d_a[i]);
			}
		}
		cudaMallocHost(&a_double_array,mem_a*sizeof(mpmat_double),cudaHostAllocPortable);
		for (int i = 0; i < gpu_count; ++i){
			cudaSetDevice(i);
			cudaMalloc(d_a+i,mem_a*sizeof(mpmat_double));
			gpu_len_a[i] = mem_a;
		}
		len_a = mem_a;
	}
	if (mem_b > len_b){
		if (len_b != 0) {
			cudaFreeHost(b_double_array);
			for (int i = 0; i < gpu_count; ++i){
				cudaSetDevice(i);
				cudaFree(d_b[i]);
			}
		}
		cudaMallocHost(&b_double_array,mem_b*sizeof(mpmat_double),cudaHostAllocPortable);
		for (int i = 0; i < gpu_count; ++i){
			cudaSetDevice(i);
			cudaMalloc(d_b+i,mem_b*sizeof(mpmat_double));
			gpu_len_b[i] = mem_b;
		}
		len_b = mem_b;
	}
	if (mem_c > len_c){
		if (len_c != 0) {
			cudaFreeHost(c_double_array);
			for (int i = 0; i < gpu_count; ++i){
				cudaSetDevice(i);
				cudaFree(d_c[i]);
			}
		}
		cudaMallocHost(&c_double_array,mem_c*sizeof(mpmat_double),cudaHostAllocPortable);
		for (int i = 0; i < gpu_count; ++i){
			cudaSetDevice(i);
			cudaMalloc(d_c+i,mem_c*sizeof(mpmat_double));
			gpu_len_c[i] = mem_c;
		}
		len_c = mem_c;
	}
	size_t mem_t = max(max(mem_a,mem_b),mem_c);
	if (mem_t > len_t){
		if (len_t != 0) delete [] tmp;
		tmp = new mpmat_double [mem_t];
		len_t = mem_t;
	}
}

void mpmat::realloc_gpu_only(size_t mem_a, size_t mem_b, size_t mem_c, int device){
	cudaSetDevice(device);
	// std::cerr << "reallocing " << mem_a << " " << mem_b << " " << mem_c << " " << device << "\n";
	// std::cerr << "the existing memory is " << gpu_len_a[device] << " " << gpu_len_b[device] << " " << gpu_len_c[device] << "\n";
	if (mem_a > gpu_len_a[device]){
		// std::cerr << "reallocing a\n";
		if (gpu_len_a[device] != 0) {
			cudaFree(d_a[device]);
		}
		cudaMalloc(d_a+device,mem_a*sizeof(mpmat_double));
		cudaMemset(d_a[device],0,mem_a*sizeof(mpmat_double));
		gpu_len_a[device] = mem_a;
	}
	if (mem_b > gpu_len_b[device]){
		// std::cerr << "reallocing b\n";
		if (gpu_len_b[device] != 0) {
			cudaFree(d_b[device]);
		}
		cudaMalloc(d_b+device,mem_b*sizeof(mpmat_double));
		cudaMemset(d_b[device],0,mem_b*sizeof(mpmat_double));
		gpu_len_b[device] = mem_b;
	}
	if (mem_c > gpu_len_c[device]){
		// std::cerr << "reallocing c\n";
		if (gpu_len_c[device] != 0) {
			cudaFree(d_c[device]);
		}
		cudaMalloc(d_c+device,mem_c*sizeof(mpmat_double));
		cudaMemset(d_c[device],0,mem_c*sizeof(mpmat_double));
		gpu_len_c[device] = mem_c;
	}
}

#endif

void mpmat::mpmatMultiplyGMPBaseCase(mpf_class & dest,
	const mpf_class  a,
	const mpf_class  b) {

	int mpmat_limb = MPMAT_DOUBLE_MANT_IMPLICIT / 2;
	int mpmat_size_a = ceil_div( (a.get_mpf_t()->_mp_prec + 1) * mp_bits_per_limb, mpmat_limb );
	int mpmat_size_b = ceil_div( (b.get_mpf_t()->_mp_prec + 1) * mp_bits_per_limb, mpmat_limb );

	while ( 2 * mpmat_limb + ceil(log2(fmin(mpmat_size_a, mpmat_size_b))) > MPMAT_DOUBLE_MANT_IMPLICIT ) {
		mpmat_limb = ( MPMAT_DOUBLE_MANT_IMPLICIT - ceil(log2(fmin(mpmat_size_a, mpmat_size_b))) ) / 2;
		mpmat_size_a = ceil_div( (a.get_mpf_t()->_mp_prec + 1) * mp_bits_per_limb, mpmat_limb );
		mpmat_size_b = ceil_div( (b.get_mpf_t()->_mp_prec + 1) * mp_bits_per_limb, mpmat_limb );
	}

		// Add some limbs for more reliable comparison with GMP. In principle we only know min(...) limbs of the result.
	int mpmat_size_c = min(mpmat_size_a, mpmat_size_b)+ceil_div(MPMAT_DOUBLE_MANT_IMPLICIT, mpmat_limb);

	assert(mpmat_limb > 0);

	mpmat_double * a_double = new mpmat_double[mpmat_size_a];
	mpmat_double * b_double = new mpmat_double[mpmat_size_b];
	mpmat_double * c_double = new mpmat_double[mpmat_size_c];

	int a_mp_exp = a.get_mpf_t() -> _mp_exp * mp_bits_per_limb;
	int b_mp_exp = b.get_mpf_t() -> _mp_exp * mp_bits_per_limb;

	mpmatConvertGMPToDouble(a,a_double,mpmat_size_a, mpmat_limb, a_mp_exp);
	mpmatConvertGMPToDouble(b,b_double,mpmat_size_b, mpmat_limb, b_mp_exp);

	for(int i = 0; i< mpmat_size_c; i++) {
		c_double[i]=0;
		for(int k = 0; k<=i; k++) {
			if (i-k < mpmat_size_a && k < mpmat_size_b) {
				c_double[i] += a_double[i - k] * b_double[k];
			}
		}
	}

	mpmatConvertDoubleToGMP(dest, c_double, mpmat_size_c,mpmat_limb, a_mp_exp+b_mp_exp-mpmat_limb);

	delete [] a_double;
	delete [] b_double;
	delete [] c_double;
}


void mpmat::gemm_reduced(
	const CBLAS_ORDER Layout,
	const CBLAS_TRANSPOSE transa,
	const CBLAS_TRANSPOSE transb,
	const int m,
	const int n,
	const int k,
				//const mpf_class alpha,
	const mpf_class * a,
				//const int lda,
	const mpf_class * b,
				//const int ldb,
				//const mpf_class beta,
	mpf_class * c
				//const int ldc
	) {
	std::cerr << "starting a mult\n";
	timers["mpmat_gemm_reduced.complete"].start();

	int mpmat_limb = ( MPMAT_DOUBLE_MANT_IMPLICIT - ceil(log2(k)) )/ 2;
	int mpmat_size_a = ceil_div( abs(a[0].get_mpf_t()->_mp_prec+1) * mp_bits_per_limb, mpmat_limb );
	int mpmat_size_b = ceil_div( abs(b[0].get_mpf_t()->_mp_prec+1) * mp_bits_per_limb, mpmat_limb );


	while ( 2 * mpmat_limb + ceil(log2(k*min(mpmat_size_a, mpmat_size_b))) > MPMAT_DOUBLE_MANT_IMPLICIT ) {
		mpmat_limb = ( MPMAT_DOUBLE_MANT_IMPLICIT - ceil(log2(k*min(mpmat_size_a, mpmat_size_b))) ) / 2;
		mpmat_size_a = ceil_div( abs(a[0].get_mpf_t()->_mp_prec+1) * mp_bits_per_limb, mpmat_limb );
		mpmat_size_b = ceil_div( abs(b[0].get_mpf_t()->_mp_prec+1) * mp_bits_per_limb, mpmat_limb );
	}

	int mpmat_size_c = min(mpmat_size_a, mpmat_size_b);

	int mem_a = mpmat_size_a * m * k;
	int mem_b = mpmat_size_b * n * k;
	int mem_c = (4*mpmat_size_c + 2) * m * n;

	realloc(mem_a,mem_b,mem_c);

	memset(c_double_array, 0, mem_c * sizeof(mpmat_double));

	int expa, expb;

	std::flush(std::cout);
	mpmatConvertGMPToDoubleVector(
		a,
		m * k,
		a_double_array,
		mpmat_size_a,
		mpmat_limb,
		expa,
		tmp
		);

	mpmatConvertGMPToDoubleVector(
		b,
		n * k,
		b_double_array,
		mpmat_size_b,
		mpmat_limb,
		expb,
		tmp
		);

	timers["mpmat_gemm_reduced.multiplication"].start();


// #pragma omp parallel for
//     for (int i = 0; i < mpmat_size_c; i++) {
//         for (int j = 0; j <= i; j++) {
//             cblas_dgemm(
//                     Layout,
//                     transa,
//                     transb,
//                     m,
//                     n,
//                     k,
//                     1,
//                     a_double_array+k*m*j,
//                     Layout == CblasRowMajor ? k : m,
//                     b_double_array+(i-j)*k*n,
//                     Layout == CblasRowMajor ? n : k,
//                     1,
//                     c_double_array+i*m*n,
//                     Layout == CblasRowMajor ? n : m
//             );

//         }
//     }
	karatsuba(mpmat_size_c, Layout, transa, transb, m, n, k);

	timers["mpmat_gemm_reduced.multiplication"].stop();

	mpmatConvertDoubleToGMPVector(
		c,
		m*n,
		c_double_array,
		mpmat_size_c,
		mpmat_limb,
		expa+expb-mpmat_limb,
		tmp
		);


	timers["mpmat_gemm_reduced.complete"].stop();

	std::cerr << "ending a mult\n";
}

#ifdef __SDPB_CUDA__
void mpmat::gemm_reduced_gpu(
	const CBLAS_ORDER Layout,
	const CBLAS_TRANSPOSE transa,
	const CBLAS_TRANSPOSE transb,
	const int m,
	const int n,
	const int k,
				//const mpf_class alpha,
	const mpf_class * a,
				//const int lda,
	const mpf_class * b,
				//const int ldb,
				//const mpf_class beta,
	mpf_class * c
				//const int ldc
	) {

	timers["mpmat_gemm_reduced.complete"].resume();

	timers["mpmat_gemm_reduced.precalculations"].resume();

	int mpmat_limb = ( MPMAT_DOUBLE_MANT_IMPLICIT - ceil(log2(k)) )/ 2;
	int mpmat_size_a = ceil_div( abs(a[0].get_mpf_t()->_mp_prec+1) * mp_bits_per_limb, mpmat_limb );
	int mpmat_size_b = ceil_div( abs(b[0].get_mpf_t()->_mp_prec+1) * mp_bits_per_limb, mpmat_limb );


	while ( 2 * mpmat_limb + ceil(log2(k*min(mpmat_size_a, mpmat_size_b))) > MPMAT_DOUBLE_MANT_IMPLICIT ) {
		mpmat_limb = ( MPMAT_DOUBLE_MANT_IMPLICIT - ceil(log2(k*min(mpmat_size_a, mpmat_size_b))) ) / 2;
		mpmat_size_a = ceil_div( abs(a[0].get_mpf_t()->_mp_prec+1) * mp_bits_per_limb, mpmat_limb );
		mpmat_size_b = ceil_div( abs(b[0].get_mpf_t()->_mp_prec+1) * mp_bits_per_limb, mpmat_limb );
	}
	int mpmat_size_c = max(mpmat_size_a, mpmat_size_b);


	int mem_a = mpmat_size_a * m * k;
	int mem_b = mpmat_size_b * n * k;
	int mem_c = mpmat_size_c * m * n;

	realloc(mem_a,mem_b,mem_c);

	memset(c_double_array, 0, mem_c * sizeof(mpmat_double));
	cudaMemset(d_c, 0, mem_c * sizeof(mpmat_double));

	timers["mpmat_gemm_reduced.precalculations"].stop();

	timers["mpmat_gemm_reduced.GMPtoDouble"].resume();

	int expa, expb;

	std::flush(std::cout);
	mpmatConvertGMPToDoubleVector(
		a,
		m * k,
		a_double_array,
		mpmat_size_a,
		mpmat_limb,
		expa,
		tmp
		);

	std::flush(std::cout);
	mpmatConvertGMPToDoubleVector(
		b,
		n * k,
		b_double_array,
		mpmat_size_b,
		mpmat_limb,
		expb,
		tmp
		);

	timers["mpmat_gemm_reduced.GMPtoDouble"].stop();

	timers["mpmat_gemm_reduced.gpu_copy_forward"].resume();

	double alpha = 1.0, beta = 1.0;

		#pragma omp parallel for 
	for (int i = 0; i < gpu_count; ++i){
		cudaSetDevice(i);
		cudaMemcpyAsync(d_a[i],a_double_array,mem_a*sizeof(mpmat_double),cudaMemcpyHostToDevice);
		cudaMemcpyAsync(d_b[i],b_double_array,mem_b*sizeof(mpmat_double),cudaMemcpyHostToDevice);
	}
	cudaThreadSynchronize();

	timers["mpmat_gemm_reduced.gpu_copy_forward"].stop();

	timers["mpmat_gemm_reduced.multiplication"].resume();


#pragma omp parallel for schedule(dynamic)
	for (int i = 0; i < mpmat_size_c; i++) {
		int gpu_id = i * gpu_count / mpmat_size_c;
		cudaSetDevice(gpu_id);
		for (int j = 0; j <= i; j++) {
			cublasDgemm(handles[gpu_id],
				(Layout == CblasRowMajor) != (transa == CblasTrans) ? CUBLAS_OP_N : CUBLAS_OP_T,
				(Layout == CblasRowMajor) != (transb == CblasTrans) ? CUBLAS_OP_N : CUBLAS_OP_T,
				m,
				n,
				k,
				&alpha,
				d_a[gpu_id]+k*m*j,
				(Layout == CblasRowMajor) != (transa == CblasTrans) ? m : k,
				d_b[gpu_id]+(i-j)*k*n,
				(Layout == CblasRowMajor) != (transb == CblasTrans) ? k : n,
				&beta,
				d_c[gpu_id]+i*m*n,
				Layout == CblasRowMajor ? m : n
				);
		}
		cudaMemcpyAsync(c_double_array+i*m*n,d_c[gpu_id]+i*m*n,m*n*sizeof(mpmat_double),cudaMemcpyDeviceToHost);
	}
	cudaThreadSynchronize();
	timers["mpmat_gemm_reduced.multiplication"].stop();

	timers["mpmat_gemm_reduced.gpu_copy_back"].resume();

		//gpu copy back is done in parallel asynchronously with the multiplication

	timers["mpmat_gemm_reduced.gpu_copy_back"].stop();

	timers["mpmat_gemm_reduced.DoubletoGMP"].resume();

	mpmatConvertDoubleToGMPVector(
		c,
		m*n,
		c_double_array,
		mpmat_size_c,
		mpmat_limb,
		expa+expb-mpmat_limb,
		tmp
		);

	timers["mpmat_gemm_reduced.DoubletoGMP"].stop();


	timers["mpmat_gemm_reduced.complete"].stop();
}

void mpmat::syrk_reduced_gpu(
	const CBLAS_ORDER Layout,
	const CBLAS_TRANSPOSE transa,
	const int m,
	const int k,
				//const mpf_class alpha,
	const mpf_class * a,
				//const int lda,
	mpf_class * c
				//const int ldc
	) {
	timers["mpmat_syrk_reduced.complete"].resume();

	timers["mpmat_syrk_reduced.precalculations"].resume();

	int mpmat_limb = ( MPMAT_DOUBLE_MANT_IMPLICIT - ceil(log2(k)) )/ 2;
	int mpmat_size_a = ceil_div( abs(a[0].get_mpf_t()->_mp_prec+1) * mp_bits_per_limb, mpmat_limb );


	while ( 2 * mpmat_limb + ceil(log2(k*mpmat_size_a)) > MPMAT_DOUBLE_MANT_IMPLICIT ) {
		mpmat_limb = ( MPMAT_DOUBLE_MANT_IMPLICIT - ceil(log2(k*mpmat_size_a)) ) / 2;
		mpmat_size_a = ceil_div( abs(a[0].get_mpf_t()->_mp_prec+1) * mp_bits_per_limb, mpmat_limb );
	}
		//mpmat_limb--;
		//mpmat_size_a = ceil_div( abs(a[0].get_mpf_t()->_mp_prec+1) * mp_bits_per_limb, mpmat_limb );
		//std::cout << "mpmat_limb is " << mpmat_limb << "\n";
	int mpmat_size_c = mpmat_size_a;

	int mem_a = pow(2,ceil(log2(mpmat_size_a))) * m * k;
	int mem_c = (6*pow(2,ceil(log2(mpmat_size_c))) + 2)* m * m;

	realloc(mem_a,0,mem_c);

	memset(c_double_array, 0, mem_c * sizeof(mpmat_double));
	double * c2_double_array = new double[mem_c];
#pragma omp parallel for
	for (int i = 0; i < gpu_count; ++i){
		cudaSetDevice(i);
		cudaMemset(d_c[i], 0, mem_c * sizeof(mpmat_double));
	}

	timers["mpmat_syrk_reduced.precalculations"].stop();

	timers["mpmat_syrk_reduced.GMPtoDouble"].resume();

	int expa;
	std::flush(std::cout);

	mpmatConvertGMPToDoubleVector(
		a,
		m * k,
		a_double_array,
		mpmat_size_a,
		mpmat_limb,
		expa,
		tmp
		);

	timers["mpmat_syrk_reduced.GMPtoDouble"].stop();

	timers["mpmat_syrk_reduced.gpu_copy_forward"].resume();

	double alpha = 1.0, beta = 1.0;

#pragma omp parallel for 
	for (int i = 0; i < gpu_count; ++i){
		cudaSetDevice(i);
		cudaMemcpyAsync(d_a[i],a_double_array,mem_a*sizeof(mpmat_double),cudaMemcpyHostToDevice);
	}
	cudaThreadSynchronize();

	timers["mpmat_syrk_reduced.gpu_copy_forward"].stop();

	timers["mpmat_syrk_reduced.multiplication"].resume();
	std::flush(std::cout);


// #pragma omp parallel for schedule(dynamic)
//     for (int i = 0; i < mpmat_size_c; i++) {
//       int gpu_id = i * gpu_count / mpmat_size_c;
// 	cudaSetDevice(gpu_id);
//       for (int j = 0; j < i / 2 + i % 2; j++) {
	
// 	  cublasDgemm(handles[gpu_id],
// 		      (Layout == CblasRowMajor) != (transa == CblasTrans) ? CUBLAS_OP_N : CUBLAS_OP_T,
// 		      (Layout == CblasRowMajor) != (transa == CblasTrans) ? CUBLAS_OP_T : CUBLAS_OP_N,
// 		       m,
// 		       m,
// 		       k,
// 		       &alpha,
// 		       d_a[gpu_id]+k*m*j,
// 		       (Layout == CblasRowMajor) != (transa == CblasTrans) ? m : k,
// 		       d_a[gpu_id]+(i-j)*k*m,
// 		       (Layout == CblasRowMajor) != (transa == CblasTrans) ? m : k,
// 		       &beta,
// 		       d_c[gpu_id]+i*m*m,
// 		       m
// 		       );
//         }
// 	cublasDcopy(handles[gpu_id],m*m,d_c[gpu_id]+i*m*m,1,d_b[gpu_id]+i*m*m,1);
//         cublasDgeam(handle,CUBLAS_OP_T,CUBLAS_OP_N,m,m,
// 		    &alpha,d_b[gpu_id]+i*m*m,m,
// 		    &beta,d_c[gpu_id]+i*m*m,m,
// 		    d_c[gpu_id]+i*m*m,m);
// 	// if significance of result is even, calculate the symmetric part
// 	if ( i % 2 == 0)
// 	  cublasDsyrk(handles[gpu_id],CUBLAS_FILL_MODE_UPPER,(Layout == CblasRowMajor) != (transa == CblasTrans) ? CUBLAS_OP_N : CUBLAS_OP_T,
// 		    m,k,
// 		    &alpha, d_a[gpu_id]+k*m*(i/2), (Layout == CblasRowMajor) != (transa == CblasTrans) ? m : k,
// 		    &beta, d_c[gpu_id]+m*m*i, m);
// 	 cudaMemcpyAsync(c2_double_array+i*m*m,d_c[gpu_id]+i*m*m,m*m*sizeof(mpmat_double),cudaMemcpyDeviceToHost);
//     }
//     cudaThreadSynchronize();
	//memset(c_double_array, 0, mem_c * sizeof(mpmat_double));
	karatsuba(pow(2,ceil(log2(mpmat_size_c))), Layout, transa,
		m, k, true);
		

	

	// if (!compareSymmMatrices(c_double_array,c2_double_array,m,mpmat_size_c)){//(!std::equal(c2_double_array,c2_double_array+n*n*l,c_double_array)){
	//     std::cerr << "c_double_array is incorrect\n";
	//     //result = false;
	//     for (int i = 0; i < m*m*(3*mpmat_size_c - (int)log2(mpmat_size_c) - 1); ++i)
	//       std::cout << c_double_array[i] << (i % (m*m) == m*m-1 ? "|" : (i % m == m-1 ? "," : " "));
	//     std::cout << "\n";
	//     for (int i = 0; i < m*m*mpmat_size_c; ++i)
	//       std::cout << c2_double_array[i] << (i % (m*m) == m*m-1 ? "|" : (i % m == m-1 ? "," : " "));
	//     std::cout << "\n";
	//     //print_mpmat_double_array(c_double_array,m*n*(int)pow(3,ceil(log2(l))));
	//     //print_mpmat_double_array(c2_double_array,m*n*l);
 //  	}
 //  	else std::cerr << "all good";
	timers["mpmat_syrk_reduced.multiplication"].stop();

	timers["mpmat_syrk_reduced.gpu_copy_back"].resume();
	//cudaMemcpy(c_double_array,d_c[0],mem_c*sizeof(mpmat_double),cudaMemcpyDeviceToHost);
		// gpu copy back is actually happening asynchronously with the multiplications
	timers["mpmat_syrk_reduced.gpu_copy_back"].stop();

	timers["mpmat_syrk_reduced.DoubletoGMP"].resume();

	mpmatConvertDoubleToGMPSymm(
		c,
		m,
		c_double_array,
		mpmat_size_c,
		mpmat_limb,
		expa+expa-mpmat_limb,
		tmp
		);

	timers["mpmat_syrk_reduced.DoubletoGMP"].stop();


	timers["mpmat_syrk_reduced.complete"].stop();
	delete [] c2_double_array;
}

#endif

void mpmat::syrk_reduced(
	const CBLAS_ORDER Layout,
	const CBLAS_TRANSPOSE transa,
	const int m,
	const int k,
				//const mpf_class alpha,
	const mpf_class * a,
				//const int lda,
	mpf_class * c
				//const int ldc
	) {
	//std::cerr << m << " " << k << "\n";

	timers["mpmat_syrk_reduced.complete"].resume();

	timers["mpmat_syrk_reduced.precalculations"].resume();

	int mpmat_limb = ( MPMAT_DOUBLE_MANT_IMPLICIT - ceil(log2(k)) )/ 2;
	int mpmat_size_a = ceil_div( abs(a[0].get_mpf_t()->_mp_prec+1) * mp_bits_per_limb, mpmat_limb );


	while ( 2 * mpmat_limb + ceil(log2(k*mpmat_size_a)) > MPMAT_DOUBLE_MANT_IMPLICIT ) {
		mpmat_limb = ( MPMAT_DOUBLE_MANT_IMPLICIT - ceil(log2(k*mpmat_size_a)) ) / 2;
		mpmat_size_a = ceil_div( abs(a[0].get_mpf_t()->_mp_prec+1) * mp_bits_per_limb, mpmat_limb );
	}
		//mpmat_limb--;
		//mpmat_size_a = ceil_div( abs(a[0].get_mpf_t()->_mp_prec+1) * mp_bits_per_limb, mpmat_limb );
		//std::cerr << "mpmat_limb is " << mpmat_limb << "\n";

	int mpmat_size_c = mpmat_size_a;
	int mem_a = pow(2,ceil(log2(mpmat_size_a))) * m * k;
	int mem_c = (6*pow(2,ceil(log2(mpmat_size_c))) + 2)* m * m;

	realloc(mem_a,max(mem_a,mem_c),mem_c);
	double * c2_double_array = new double[mem_c];
	memset(c_double_array, 0, mem_c * sizeof(mpmat_double));

	timers["mpmat_syrk_reduced.precalculations"].stop();

	timers["mpmat_syrk_reduced.GMPtoDouble"].resume();

	int expa;

	mpmatConvertGMPToDoubleVector(
		a,
		m * k,
		a_double_array,
		mpmat_size_a,
		mpmat_limb,
		expa,
		tmp
		);

	timers["mpmat_syrk_reduced.GMPtoDouble"].stop();

	double alpha = 1.0, beta = 1.0;

	timers["mpmat_syrk_reduced.multiplication"].resume();

// #pragma omp parallel for schedule(dynamic)
//     for (int i = 0; i < mpmat_size_c; i++) {
//       for (int j = 0; j < i / 2 + i % 2; j++) {
// 	cblas_dgemm(CblasColMajor,(Layout == CblasRowMajor) != (transa == CblasTrans) ? CblasNoTrans : CblasTrans,
// 		      (Layout == CblasRowMajor) != (transa == CblasTrans) ? CblasTrans: CblasNoTrans,
// 		    m,
// 		    m,
// 		    k,
// 		    alpha,
// 		    a_double_array+k*m*j,
// 		    (Layout == CblasRowMajor) != (transa == CblasTrans) ? m : k,
// 		    a_double_array+(i-j)*k*m,
// 		    (Layout == CblasRowMajor) != (transa == CblasTrans) ? m : k,
// 		    beta,
// 		    c2_double_array+i*m*m,
// 		    m);
//         }
//       mkl_domatcopy('c','t',
// 		    m,m,1,
// 		    c2_double_array+i*m*m,
// 		    m,
// 		    b_double_array+i*m*m,
// 		    m
// 		    );
//       cblas_daxpy(m*m,1.0,
// 		  b_double_array+i*m*m,1,
// 		  c2_double_array+i*m*m,1);
//       // if significance of result is even, calculate the symmetric part
// 	if ( i % 2 == 0)
// 	  cblas_dsyrk(CblasColMajor,CblasUpper,(Layout == CblasRowMajor) != (transa == CblasTrans) ? CblasNoTrans : CblasTrans,
// 		      m,k,alpha,a_double_array+k*m*(i/2),(Layout == CblasRowMajor) != (transa == CblasTrans) ? m : k,
// 		      beta,c2_double_array+i*m*m,m);
//     }
// 	std::cerr << "multing";
	karatsuba(pow(2,ceil(log2(mpmat_size_c))), Layout, transa,
		m, k);

	// if (!compareSymmMatrices(c_double_array,c2_double_array,m,mpmat_size_c)){//(!std::equal(c2_double_array,c2_double_array+n*n*l,c_double_array)){
	//     std::cerr << "c_double_array is incorrect\n";
	//     //result = false;
	//     for (int i = 0; i < m*m*(3*mpmat_size_c - (int)log2(mpmat_size_c) - 1); ++i)
	//       std::cout << c_double_array[i] << (i % (m*m) == m*m-1 ? "|" : (i % m == m-1 ? "," : " "));
	//     std::cout << "\n";
	//     for (int i = 0; i < m*m*mpmat_size_c; ++i)
	//       std::cout << c2_double_array[i] << (i % (m*m) == m*m-1 ? "|" : (i % m == m-1 ? "," : " "));
	//     std::cout << "\n";
	//     //print_mpmat_double_array(c_double_array,m*n*(int)pow(3,ceil(log2(l))));
	//     //print_mpmat_double_array(c2_double_array,m*n*l);
 //  	}
  	//else std::cerr << "all good";
	timers["mpmat_syrk_reduced.multiplication"].stop();

	timers["mpmat_syrk_reduced.DoubletoGMP"].resume();

	mpmatConvertDoubleToGMPSymm(
		c,
		m,
		c_double_array,
		mpmat_size_c,
		mpmat_limb,
		expa+expa-mpmat_limb,
		tmp
		);

	timers["mpmat_syrk_reduced.DoubletoGMP"].stop();


	timers["mpmat_syrk_reduced.complete"].stop();
	delete [] c2_double_array;
}
