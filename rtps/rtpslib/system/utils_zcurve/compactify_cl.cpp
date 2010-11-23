#ifndef _COMPACTIFY_CL_
#define _COMPACTIFY_CL_

#include "bank_conflicts.h"

#define T int


//----------------------------------------------------------------------
int sumReduce(__local int* s_data, int nb_elem)
{
	int lid = get_local_id(0);
	int sum;

	barrier(CLK_LOCAL_MEM_FENCE);

// reduce and sum
// typical in GPU computings

	#if 1
	for (int i=(nb_elem>>1); i>0; (i>>=1)) {
    	if (lid < i) {
        	s_data[lid] = s_data[lid] + s_data[i+lid];
			barrier(CLK_LOCAL_MEM_FENCE);
    	}
	}
	#endif
	
	sum = s_data[0];
	return sum;
}
//----------------------------------------------------------------------
void  prescan(
    __global T* g_odata, 
	__global T* g_idata, 
	__local T* temp, 
	int n) 
{
// executed within a single block

	int offset = 1; 
	int lid = get_local_id(0);

	int ai = lid;
	int bi = lid + (n>>1);
	temp[2*lid]   = g_idata[2*lid];
	temp[2*lid+1] = g_idata[2*lid+1];
	
	for (int d = n>>1; d > 0; d >>= 1) // build sum in place up the tree 
	{
		barrier(CLK_LOCAL_MEM_FENCE);
		if (lid < d) {
			int ai = offset*(2*lid+1)-1;
			int bi = offset*(2*lid+2)-1;
			temp[bi] += temp[ai];
		}
		offset <<= 1;
	}

	if (lid == 0) {
		temp[n-1] = 0;
	}

	for (int d=1; d < n; d <<= 1) {
		offset >>= 1;
		barrier(CLK_LOCAL_MEM_FENCE);

		if (lid < d) {
			int ai = offset*(2*lid+1)-1;
			int bi = offset*(2*lid+2)-1;

			T t       = temp[ai];
			temp[ai]  = temp[bi];
			temp[bi] += t;
		}
	}

	barrier(CLK_LOCAL_MEM_FENCE);

	// ERROR!!
	g_odata[2*lid]   = temp[2*lid];
	g_odata[2*lid+1] = temp[2*lid+1];
	barrier(CLK_LOCAL_MEM_FENCE);
}
//----------------------------------------------------------------------
#if 0
__kernel void compactifyArrayKernel(__global int* output)
#else
__kernel void compactifyArrayKernel(
            __global int* output, 
            __global int* input,
			__global int* processorCounts, 
			__global int* processorOffsets, 
			int nb)
#endif
// Compactify an array
// (0,0,1,2,0,7) ==> (2,1,7,0,0,0)  
// order is not important
{

	// count: number of valid elements for each block
	// assume 32 threads per block
	int bid = get_group_id(0);
	int block_size = get_local_size(0);

	int tid = get_global_id(0);
	//if (tid >= nb) return;

	int lid = get_local_id(0);
	int nb_blocks = get_num_groups(0);

	__local int count_loc[512]; // avoid bank conflicts: extra memory
	__local int cnt[1];
	__local int temp[256]; // at least block_size

	// each blocks considers a section of input array
	// number of elements treated per block
	int chunk = nb/nb_blocks;
	if (chunk*nb_blocks != nb) chunk++;

	// for now, chunk*nb_blocks = nb exactly
	//output[bid] = chunk; return;

	// phase 1
	// figure out offsets
	int count = 0;

	// case where chunk = block_size
	int in = input[block_size*bid+lid];
	if (in != 0) count++;

	#if 0
	for (int i=0; i < chunk; i += block_size) {
		int in = input[chunk*bid+i+lid];
		if (in != 0) {
			count++;
		}
	}
	#endif

	//output[lid+bid*block_size] = count; return;

	//return;

	count_loc[lid] = count;
	//if (bid == 0) output[lid] = count;
	//return;

	// HOW DOES IT WORK IF THERE ARE TWO BLOCKS??? ERROR? 
	// Apply scan algorithm for multi-warp block

	barrier(CLK_LOCAL_MEM_FENCE);
	int count_sum;
	count_sum = sumReduce(count_loc, block_size);
	barrier(CLK_LOCAL_MEM_FENCE);

	// int warp_nb = lid >> 5;
	// int count_sum[warp_nb] = sumReduce(count_loc+(warp_nb<<5), 32)
	// int count_offset = scan sum of count_sum

	// total number of valid entires in block bid
	processorCounts[bid] = count_sum; // global access

//	barrier(CLK_LOCAL_MEM_FENCE);

	// brute force prefix sum
	// all blocks (for now) do same calculation
	// only block 0 and thread 0 does the calculation
	// update global memory

	// Must make sure that every block does this operation, but a single thread in each block
	// otherwise, other blocks will retrieve value from global memory BEFORE it is updated, since
	// I cannot synchronize multiple blocks together
	// So now, each block is accessing the same global memory multiple times. VERY INEFFICIENT!!

	return;
}
//----------------------------------------------------------------------

#endif