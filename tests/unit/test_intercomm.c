/**
 * @file 
 * Tests for PIOc_Intercomm
 *
 */
#include <pio.h>
#ifdef TIMING
#include <gptl.h>
#endif

/** The number of possible output netCDF output flavors available to
 * the ParallelIO library. */
#define NUM_NETCDF_FLAVORS 4

/** The number of dimensions in the example data. In this test, we
 * are using three-dimensional data. */
#define NDIM 3

/** The length of our sample data along each dimension. */
/**@{*/
#define X_DIM_LEN 400
#define Y_DIM_LEN 400
/**@}*/

/** The number of timesteps of data to write. */
#define NUM_TIMESTEPS 6

/** The name of the variable in the netCDF output file. */
#define VAR_NAME "foo"

/** The meaning of life, the universe, and everything. */
#define START_DATA_VAL 42

/** Error code for when things go wrong. */
#define ERR_AWFUL 1111

/** Values for some netcdf-4 settings. */
/**@{*/
#define VAR_CACHE_SIZE (1024 * 1024)
#define VAR_CACHE_NELEMS 10
#define VAR_CACHE_PREEMPTION 0.5
/**@}*/


/** Handle MPI errors. This should only be used with MPI library
 * function calls. */
#define MPIERR(e) do {                                                  \
	MPI_Error_string(e, err_buffer, &resultlen);			\
	fprintf(stderr, "MPI error, line %d, file %s: %s\n", __LINE__, __FILE__, err_buffer); \
	MPI_Finalize();							\
	return ERR_AWFUL;							\
    } while (0) 

/** Handle non-MPI errors by finalizing the MPI library and exiting
 * with an exit code. */
#define ERR(e) do {				\
        fprintf(stderr, "Error %d in %s, line %d\n", e, __FILE__, __LINE__); \
	MPI_Finalize();				\
	return e;				\
    } while (0) 

/** Global err buffer for MPI. When there is an MPI error, this buffer
 * is used to store the error message that is associated with the MPI
 * error. */
char err_buffer[MPI_MAX_ERROR_STRING];

/** This is the length of the most recent MPI error message, stored
 * int the global error string. */
int resultlen;

/** The dimension names. */
char dim_name[NDIM][NC_MAX_NAME + 1] = {"timestep", "x", "y"};

/** Length of the dimensions in the sample data. */
int dim_len[NDIM] = {NC_UNLIMITED, X_DIM_LEN, Y_DIM_LEN};

/** Length of chunksizes to use in netCDF-4 files. */
size_t chunksize[NDIM] = {2, X_DIM_LEN/2, Y_DIM_LEN/2};

/** Run Tests for NetCDF-4 Functions.
 *
 * @param argc argument count
 * @param argv array of arguments
 */
int
main(int argc, char **argv)
{
    int verbose = 1;
    
    /** Zero-based rank of processor. */
    int my_rank;

    /** Number of processors involved in current execution. */
    int ntasks;

    /** Specifies the flavor of netCDF output format. */
    int iotype;

    /** Different output flavors. */
    int format[NUM_NETCDF_FLAVORS] = {PIO_IOTYPE_PNETCDF, 
				      PIO_IOTYPE_NETCDF,
				      PIO_IOTYPE_NETCDF4C,
				      PIO_IOTYPE_NETCDF4P};

    /** Names for the output files. */
    char filename[NUM_NETCDF_FLAVORS][NC_MAX_NAME + 1] = {"test_nc4_pnetcdf.nc",
							  "test_nc4_classic.nc",
							  "test_nc4_serial4.nc",
							  "test_nc4_parallel4.nc"};
	
    /** Number of processors that will do IO. In this test we
     * will do IO from all processors. */
    int niotasks;

    /** Stride in the mpi rank between io tasks. Always 1 in this
     * test. */
    int ioproc_stride = 1;

    /** Number of the aggregator? Always 0 in this test. */
    int numAggregator = 0;

    /** Zero based rank of first processor to be used for I/O. */
    int ioproc_start = 0;

    /** The dimension IDs. */
    int dimids[NDIM];

    /** Array index per processing unit. */
    PIO_Offset elements_per_pe;

    /** The ID for the parallel I/O system. */
    int iosysid;

    /** The ncid of the netCDF file. */
    int ncid = 0;

    /** The ID of the netCDF varable. */
    int varid;

    /** Storage of netCDF-4 files (contiguous vs. chunked). */
    int storage;

    /** Chunksizes set in the file. */
    size_t my_chunksize[NDIM];
    
    /** The shuffle filter setting in the netCDF-4 test file. */
    int shuffle;
    
    /** Non-zero if deflate set for the variable in the netCDF-4 test file. */
    int deflate;

    /** The deflate level set for the variable in the netCDF-4 test file. */
    int deflate_level;

    /** Non-zero if fletcher32 filter is used for variable. */
    int fletcher32;

    /** Endianness of variable. */
    int endianness;

    /* Size of the file chunk cache. */
    size_t chunk_cache_size;

    /* Number of elements in file cache. */
    size_t nelems;

    /* File cache preemption. */
    float preemption;

    /* Size of the var chunk cache. */
    size_t var_cache_size;

    /* Number of elements in var cache. */
    size_t var_cache_nelems;

    /* Var cache preemption. */    
    float var_cache_preemption;
    
    /** The I/O description ID. */
    int ioid;

    /** A buffer for sample data. */
    float *buffer;

    /** A buffer for reading data back from the file. */
    int *read_buffer;

    /** The decomposition mapping. */
    PIO_Offset *compdof;

    /** Return code. */
    int ret;

    /** Index for loops. */
    int fmt, d, d1, i;
    
#ifdef TIMING    
    /* Initialize the GPTL timing library. */
    if ((ret = GPTLinitialize ()))
	return ret;
#endif    
    
    /* Initialize MPI. */
    if ((ret = MPI_Init(&argc, &argv)))
	MPIERR(ret);

    /* Learn my rank and the total number of processors. */
    if ((ret = MPI_Comm_rank(MPI_COMM_WORLD, &my_rank)))
	MPIERR(ret);
    if ((ret = MPI_Comm_size(MPI_COMM_WORLD, &ntasks)))
	MPIERR(ret);

    /* Check that a valid number of processors was specified. */
    if (!(ntasks == 1 || ntasks == 2 || ntasks == 4 ||
	  ntasks == 8 || ntasks == 16))
	fprintf(stderr, "Number of processors must be 1, 2, 4, 8, or 16!\n");
    if (verbose)
	printf("%d: ParallelIO Library test_intercomm running on %d processors.\n",
	       my_rank, ntasks);

    /* keep things simple - 1 iotask per MPI process */    
    niotasks = ntasks;

    /* For example, if I have 4 processors, and I want to have 2 of them be computational, */
    /* and 2 of them be IO: component count is 1  */
    /* peer_comm = MPI_COMM_WORLD */
    /* comp_comms is an array of comms of size 1 with a comm defined just over tasks (0,1) */
    /* io_comm is a comm over tasks (2,3) */

    /* Initialize the PIO IO system. This specifies how many and which
     * processors are involved in I/O. */
#define COMPONENT_COUNT 1
    MPI_Comm comp_comms;
    MPI_Comm io_comm;
    MPI_Group io_group;
    MPI_Group comp_group;

    /* Tasks 0 and 1 will be computational. Tasks 2 and 3 will be I/O
     * tasks. */

    // Get the group of processes in MPI_COMM_WORLD
    MPI_Group world_group;
    MPI_Comm_group(MPI_COMM_WORLD, &world_group);

    if (my_rank == 0 || my_rank == 1)
    {
	/* We will define comp_comm. The io_comm will get null. */
	io_comm = MPI_COMM_NULL;
	int n = 2;
	const int ranks[2] = {0, 1};

	/* Construct a group with ranks 0, 1 in world_group. */
	MPI_Group_incl(world_group, n, ranks, &comp_group);
	MPI_Comm_create_group(MPI_COMM_WORLD, comp_group, 0, &comp_comms);
	if (verbose)
	    printf("%d: included in comp_group.\n", my_rank);
    }
    else
    {
	/* We will define io_comm. The comp_comms array will get nulls. */
	comp_comms = MPI_COMM_NULL;
	int n = 2;
	const int ranks[2] = {2, 3};

	/* Construct a group with ranks 2, 3 in world_group. */
	MPI_Group_incl(world_group, n, ranks, &io_group);
	MPI_Comm_create_group(MPI_COMM_WORLD, io_group, 0, &io_comm);
	if (verbose)
	    printf("%d: included in io_group.\n", my_rank);
    }
    
    if ((ret = PIOc_Init_Intercomm(COMPONENT_COUNT, MPI_COMM_WORLD, &comp_comms,
				   io_comm, &iosysid)))
	ERR(ret);
    if (verbose)
	printf("rank: %d init intercomm returned %d iosysid = %d\n", my_rank, ret,
	       iosysid);

    /* Free local MPI resources. */
    if (verbose)
	printf("rank: %d Freeing local MPI resources...\n", my_rank);
    MPI_Group_free(&world_group);
    if (my_rank == 0 || my_rank == 1)
    {
	MPI_Group_free(&comp_group);
	MPI_Comm_free(&comp_comms);
    }
    else
    {
	MPI_Group_free(&io_group);
	MPI_Comm_free(&io_comm);
    }
    
    /* Finalize the IO system. */
    if (verbose)
	printf("rank: %d Freeing PIO resources...\n", my_rank);
    if ((ret = PIOc_finalize(iosysid)))
    	ERR(ret);

    /* Finalize the MPI library. */
    MPI_Finalize();

#ifdef TIMING    
    /* Finalize the GPTL timing library. */
    if ((ret = GPTLfinalize ()))
	return ret;
#endif    

    return 0;
}
