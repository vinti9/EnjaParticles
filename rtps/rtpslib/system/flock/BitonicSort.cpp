#include "FLOCK.h"

namespace rtps
{

void FLOCK::loadBitonicSort()
{

    printf("about to instantiate sorting\n");
    
    bitonic = Bitonic<int>( ps->cli,    
                            &cl_sort_output_hashes,
                            &cl_sort_output_indices,
                            &cl_sort_hashes,
                            &cl_sort_indices);
    
    
}

void FLOCK::bitonic_sort()
{
    try
    {
		int dir = 1; 		// dir: direction
		//int batch = num;
		int arrayLength = max_num;
        int batch = max_num / arrayLength;

        //printf("about to try sorting\n");
        bitonic.Sort(batch, arrayLength, dir);
    
	} catch (cl::Error er) {
        printf("ERROR(bitonic sort): %s(%s)\n", er.what(), oclErrorString(er.err()));
		exit(0);
	}

    ps->cli->queue.finish();

    /*
    int nbc = 10;
    std::vector<int> sh = cl_sort_hashes.copyToHost(nbc);
    std::vector<int> eci = cl_cell_indices_end.copyToHost(nbc);

    for(int i = 0; i < nbc; i++)
    {
        printf("before[%d] %d eci: %d\n; ", i, sh[i], eci[i]);
    }
    printf("\n");
    */


	scopy(num, cl_sort_output_hashes.getDevicePtr(), 
	             cl_sort_hashes.getDevicePtr());
	scopy(num, cl_sort_output_indices.getDevicePtr(), 
	             cl_sort_indices.getDevicePtr());

    /*
    ps->cli->queue.finish();

    sh = cl_sort_hashes.copyToHost(nbc);
    eci = cl_cell_indices_end.copyToHost(nbc);

    for(int i = 0; i < nbc; i++)
    {
        printf("after[%d] %d eci: %d\n; ", i, sh[i], eci[i]);
    }
    printf("\n");
    */



}

}