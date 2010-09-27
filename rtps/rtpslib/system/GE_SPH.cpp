
#include <GL/glew.h>
#include <math.h>

#include "GE_SPH.h"
#include "../particle/UniformGrid.h"

// GE: need it have access to my datastructure (GE). Will remove 
// eventually. 
//#include "datastructures.h"

namespace rtps {


//----------------------------------------------------------------------
//void GE_SPH::setGEDataStructures(DataStructures* ds)
//{
	//this->ds = ds;
//}
//----------------------------------------------------------------------
GE_SPH::GE_SPH(RTPS *psfr, int n)
{
    //store the particle system framework
    ps = psfr;

    num = n;
	nb_vars = 4;  // for array structure in OpenCL
	nb_el = n;
	printf("num_particles= %d\n", num);

    //*** Initialization, TODO: move out of here to the particle directory
    std::vector<float4> colors(num);
    /*
    std::vector<float4> positions(num);
    std::vector<float4> colors(num);
    std::vector<float4> forces(num);
    std::vector<float4> velocities(num);
    std::vector<float> densities(num);
    */
    positions.resize(num);
    forces.resize(num);
    velocities.resize(num);
    densities.resize(num);


    //for reading back different values from the kernel
    std::vector<float4> error_check(num);
    
    
    //std::fill(positions.begin(), positions.end(),(float4) {0.0f, 0.0f, 0.0f, 1.0f});
    //init sph stuff
    sph_settings.rest_density = 1000;
    sph_settings.simulation_scale = .001;

    sph_settings.particle_mass = (128*1024)/num * .0002;
    printf("particle mass: %f\n", sph_settings.particle_mass);
    sph_settings.particle_rest_distance = .87 * pow(sph_settings.particle_mass / sph_settings.rest_density, 1./3.);
    printf("particle rest distance: %f\n", sph_settings.particle_rest_distance);
   
    sph_settings.smoothing_distance = 2.f * sph_settings.particle_rest_distance;
    sph_settings.boundary_distance = .5f * sph_settings.particle_rest_distance;

    sph_settings.spacing = sph_settings.particle_rest_distance / sph_settings.simulation_scale;
    float particle_radius = sph_settings.spacing;
    printf("particle radius: %f\n", particle_radius);

    //grid = UniformGrid(float3(0,0,0), float3(1024, 1024, 1024), sph_settings.smoothing_distance / sph_settings.simulation_scale);
    grid = UniformGrid(float4(-512,0,-512,1.), float4(512, 1024, 512, 1.), sph_settings.smoothing_distance / sph_settings.simulation_scale);
    grid.make_cube(&positions[0], sph_settings.spacing, num);

/*
typedef struct GE_SPHParams
{
    float4 grid_min;
    float4 grid_max;
    float mass;
    float rest_distance;
    float simulation_scale;
    float boundary_stiffness;
    float boundary_dampening;
    float boundary_distance;
    float EPSILON;
 
} GE_SPHParams;
*/

    cl_params = BufferGE<GE_SPHParams>(ps->cli, 1);
	GE_SPHParams& params = *(cl_params.getHostPtr());
    params.grid_min = grid.getMin();
    params.grid_max = grid.getMax();
    params.mass = sph_settings.particle_mass;
    params.rest_distance = sph_settings.particle_rest_distance;
    params.smoothing_distance = sph_settings.smoothing_distance;
    params.simulation_scale = sph_settings.simulation_scale;
    params.boundary_stiffness = 10000.0f;
    params.boundary_dampening = 256.0f;
    params.boundary_distance = sph_settings.particle_rest_distance * .5f;
    params.EPSILON = .00001f;
    params.PI = 3.14159265f;
    params.K = 1.5f;
 
    //TODO make a helper constructor for buffer to make a cl_mem from a struct
    //std::vector<GE_SPHParams> vparams(0);
    //vparams.push_back(params);
//printf("ps= %d\n", ps);
//printf("ps->cli= %d\n", ps->cli);
//printf("vparams= %d\n", &vparams);
    //cl_params = Buffer<GE_SPHParams>(ps->cli, vparams);
	cl_params.copyToDevice();
//exit(0); // ERROR ON PREVIOUS LINE

    std::fill(colors.begin(), colors.end(),float4(1.0f, 0.0f, 0.0f, 0.0f));
    std::fill(forces.begin(), forces.end(),float4(0.0f, 0.0f, 1.0f, 0.0f));
    std::fill(velocities.begin(), velocities.end(),float4(0.0f, 0.0f, 0.0f, 0.0f));

    std::fill(densities.begin(), densities.end(), 0.0f);
    std::fill(error_check.begin(), error_check.end(), float4(0.0f, 0.0f, 0.0f, 0.0f));

    /*
    for(int i = 0; i < 20; i++)
    {
        printf("position[%d] = %f %f %f\n", positions[i].x, positions[i].y, positions[i].z);
    }
    */

    //*** end Initialization

    // VBO creation, TODO: should be abstracted to another class
    managed = true;
    printf("positions: %d, %d, %d\n", positions.size(), sizeof(float4), positions.size()*sizeof(float4));
    pos_vbo = createVBO(&positions[0], positions.size()*sizeof(float4), GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);
    printf("pos vbo: %d\n", pos_vbo);
    col_vbo = createVBO(&colors[0], colors.size()*sizeof(float4), GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);
    printf("col vbo: %d\n", col_vbo);
    // end VBO creation

    //vbo buffers
    cl_position = Buffer<float4>(ps->cli, pos_vbo);
    cl_color = Buffer<float4>(ps->cli, col_vbo);

    //pure opencl buffers
    cl_force = Buffer<float4>(ps->cli, forces);
    cl_velocity = Buffer<float4>(ps->cli, velocities);

    cl_density = Buffer<float>(ps->cli, densities);

    cl_error_check= Buffer<float4>(ps->cli, error_check);

    printf("create collision wall kernel\n");
    loadCollision_wall();

	setupArrays(); // From GE structures

	int print_freq = 20000;
	int time_offset = 5;

	ts_cl[TI_HASH]   = new GE::Time("hash",      time_offset, print_freq);
	ts_cl[TI_SORT]   = new GE::Time("sort",      time_offset, print_freq);
	ts_cl[TI_BUILD]  = new GE::Time("build",     time_offset, print_freq);
	ts_cl[TI_NEIGH]  = new GE::Time("neigh",     time_offset, print_freq);
	ts_cl[TI_DENS]   = new GE::Time("density",   time_offset, print_freq);
	ts_cl[TI_PRES]   = new GE::Time("pressure",  time_offset, print_freq);
	ts_cl[TI_VISC]   = new GE::Time("viscosity", time_offset, print_freq);
	ts_cl[TI_EULER]  = new GE::Time("euler",     time_offset, print_freq);
	ts_cl[TI_UPDATE] = new GE::Time("update",    time_offset, print_freq);
	//ps->setTimers(ts_cl);

	// copy pos, vel, dens into vars_unsorted()
	// COULD DO THIS ON GPU
	float4* vars = cl_vars_unsorted.getHostPtr();
	for (int i=0; i < num; i++) {
		//vars[i+DENS*num] = densities[i];
		// PROBLEM: density is float, but vars_unsorted is float4
		// HOW TO DEAL WITH THIS WITHOUT DOUBLING MEMORY ACCESS in 
		// buildDataStructures. 

		cl_vars_unsorted(i+DENS*num).x = densities[i];
		cl_vars_unsorted(i+POS*num) = positions[i];
		cl_vars_unsorted(i+VEL*num) = velocities[i];
	}
}

//----------------------------------------------------------------------
GE_SPH::~GE_SPH()
{
    if(pos_vbo && managed)
    {
        glBindBuffer(1, pos_vbo);
        glDeleteBuffers(1, (GLuint*)&pos_vbo);
        pos_vbo = 0;
    }
    if(col_vbo && managed)
    {
        glBindBuffer(1, col_vbo);
        glDeleteBuffers(1, (GLuint*)&col_vbo);
        col_vbo = 0;
    }
}

//----------------------------------------------------------------------
void GE_SPH::update()
{
	static int count=0;

	ts_cl[TI_UPDATE]->start(); // OK

    //call kernels
    //TODO: add timings
#ifdef CPU
	computeOnCPU();
#endif

#ifdef GPU
	computeOnGPU();
#endif
    /*
    std::vector<float4> ftest = cl_force.copyToHost(100);
    for(int i = 0; i < 100; i++)
    {
        if(ftest[i].z != 0.0)
            printf("force: %f %f %f  \n", ftest[i].x, ftest[i].y, ftest[i].z);
    }
    printf("execute!\n");
    */

	ts_cl[TI_UPDATE]->end(); // OK

	count++;
	printf("count= %d\n", count);
	if (count%10 == 0) {
		count = 0;
		GE::Time::printAll();
	}
}
//----------------------------------------------------------------------
void GE_SPH::setupArrays()
{
	// only for my test routines: sort, hash, datastructures
	//printf("setupArrays, nb_el= %d\n", nb_el); exit(0);

	//cl_cells = BufferGE<float4>(ps->cli, nb_el);
	positions[0] = float4(45., -234., 129., 1.); // test sort
	cl_cells = BufferGE<float4>(ps->cli, &positions[0], nb_el);
printf("AFTER cell BufferGE\n");
	// notice the index rotation? 

	for (int i=0; i < nb_el; i++) {
		float aa = rand_float(0.,10.);
		float4* cells = cl_cells.getHostPtr();
		//printf("%d, %f, %f, %f, %f\n", i, cells[i].x, cells[i].y, cells[i].z, cells[i].w);
	}
	//exit(0);
	cl_cells.copyToDevice();

printf("\n\nBEFORE BufferGE<GridParams> check\n"); //**********************
// Need an assign operator (no memory allocation)
	#if 1
	printf("allocate BufferGE<GridParams>\n");
	printf("sizeof(GridParams): %d\n", sizeof(GridParams));
	cl_GridParams = BufferGE<GridParams>(ps->cli, 1); // destroys ...

	GridParams& gp = *(cl_GridParams.getHostPtr());

	gp.grid_min = grid.getMin();
	gp.grid_max = grid.getMax();
	gp.grid_res = grid.getRes();
	gp.grid_size = grid.getSize();
	gp.grid_delta = grid.getDelta();
	gp.numParticles = nb_el;

	gp.grid_inv_delta.x = 1. / gp.grid_delta.x;
	gp.grid_inv_delta.y = 1. / gp.grid_delta.y;
	gp.grid_inv_delta.z = 1. / gp.grid_delta.z;
	gp.grid_inv_delta.w = 1.;
	gp.grid_inv_delta.print("inv delta");

	printf("size: %d\n", sizeof(gp));
	cl_GridParams.copyToDevice();

	printf("delta z= %f\n", gp.grid_delta.z);

	grid_size = (int) (gp.grid_res.x * gp.grid_res.y * gp.grid_res.z);
	printf("grid_size= %d\n", grid_size);
	gp.grid_size.print("grid size (domain dimensions)"); // domain dimensions
	gp.grid_delta.print("grid delta (cell size)"); // cell size
	gp.grid_min.print("grid min");
	gp.grid_max.print("grid max");
	gp.grid_res.print("grid res (nb points)"); // number of points
	gp.grid_delta.print("grid delta");
	gp.grid_inv_delta.print("grid inv delta");
	//exit(0);
	#endif

	cl_unsort = BufferGE<int>(ps->cli, nb_el);
	cl_sort   = BufferGE<int>(ps->cli, nb_el);

	int* iunsort = cl_unsort.getHostPtr();
	int* isort = cl_sort.getHostPtr();

	for (int i=0; i < nb_el; i++) {
		isort[i] = i;
		iunsort[i] = nb_el-i;
		//cl_sort[i] = i;  // DOES NOT WORK, but works with cl_cells
		//cl_unsort[i] = nb_el-i;
	}


	// position POS=0
	// velocity VEL=1
	// Force    FOR=2
	cl_vars_unsorted = BufferGE<float4>(ps->cli, nb_el*nb_vars);
	cl_vars_sorted = BufferGE<float4>(ps->cli, nb_el*nb_vars);
	cl_cell_indices_start = BufferGE<int>(ps->cli, nb_el);
	cl_cell_indices_end   = BufferGE<int>(ps->cli, nb_el);
	cl_sort_indices = BufferGE<int>(ps->cli, nb_el);
	cl_sort_hashes = BufferGE<int>(ps->cli, nb_el);
    printf("about to start writing buffers\n");

	clf_debug = BufferGE<float4>(ps->cli, nb_el);
	cli_debug = BufferGE<int4>(ps->cli, nb_el);

	#if 0
	cl_vars_unsorted.copyToDevice();
	cl_vars_sorted.copyToDevice();
	cl_cell_indices_start.copyToDevice();
	cl_cell_indices_end.copyToDevice();
	cl_sort_indices.copyToDevice();
	cl_sort_hashes.copyToDevice();
	#endif

	int nb_floats = nb_vars*nb_el;
	// WHY NOT cl_float4 (in CL/cl_platform.h)
	float4 f;
	float4 zero;

	zero.x = zero.y = zero.z = 0.0;
	zero.w = 1.;

	for (int i=0; i < nb_floats; i++) {
		f.x = rand_float(0., 1.);
		f.y = rand_float(0., 1.);
		f.z = rand_float(0., 1.);
		f.w = 1.0;
		cl_vars_unsorted[i] = f;
		cl_vars_sorted[i]   = zero;
		//printf("f= %f, %f, %f, %f\n", f.x, f.y, f.z, f.w);
	}

	// SETUP FLUID PARAMETERS
	// cell width is one diameter of particle, which imlies 27 neighbor searches
	#if 1
	cl_FluidParams = BufferGE<FluidParams>(ps->cli, 1);
	FluidParams& fp = *cl_FluidParams.getHostPtr();;
	float radius = gp.grid_delta.x; 
	fp.smoothing_length = radius; // SPH radius
	fp.scale_to_simulation = 1.0; // overall scaling factor
	fp.mass = 1.0; // mass of single particle (MIGHT HAVE TO BE CHANGED)
	fp.friction_coef = 0.1;
	fp.restitution_coef = 0.9;
	fp.damping = 0.9;
	fp.shear = 0.9;
	fp.attraction = 0.9;
	fp.spring = 0.5;
	fp.gravity = -9.8; // -9.8 m/sec^2
	cl_FluidParams.copyToDevice();
	#endif

    printf("done with setup arrays\n");
}
//----------------------------------------------------------------------
void GE_SPH::checkDensity()
{
        //test density
		// Density checks should be in Density.cpp I believe (perhaps not)
        std::vector<float> dens = cl_density.copyToHost(num);
        float dens_sum = 0.0f;
        for(int j = 0; j < num; j++)
        {
            dens_sum += dens[j];
        }
        printf("summed density: %f\n", dens_sum);
        /*
        std::vector<float4> er = cl_error_check.copyToHost(10);
        for(int j = 0; j < 10; j++)
        {
            printf("rrrr[%d]: %f %f %f %f\n", j, er[j].x, er[j].y, er[j].z, er[j].w);
        }
        */
}
//----------------------------------------------------------------------
void GE_SPH::computeOnGPU()
{
    glFinish();
    cl_position.acquire();
    cl_color.acquire();
    
    //printf("execute!\n");
	// More steps per update for visualization purposes
    //for(int i=0; i < 10; i++)

    for(int i=0; i < 1; i++)
    {
		// ***** Create HASH ****
		printf("before hash\n");
		hash();
		printf("after hash\n");
		// SORTING NOT WORKING PROPERLY!! WHY?
		sort();
		//buildDataStructures(); // BUG
		exit(0);

		// ***** DENSITY UPDATE *****
		computeDensity();
		checkDensity();


		// ***** PRESSURE UPDATE *****
        computePressure();
        //k_pressure.execute(num);

		// ***** VISCOSITY UPDATE *****
        //computeViscosity();

		// ***** WALL COLLISIONS *****
        //k_collision_wall.execute(num);

        // ***** EULER UPDATE *****
		computeEuler();
    }

    cl_position.release();
    cl_color.release();
}
//----------------------------------------------------------------------
void GE_SPH::computeOnCPU()
{
    cpuDensity();
    //cpuPressure();
    //cpuEuler();
}
//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------

}
