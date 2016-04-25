#include <pio.h>
#include <pio_internal.h>
/**
 ** @public
 ** @ingroup PIO_openfile
 ** @brief open an existing file using pio
 ** @details  Input parameters are read on comp task 0 and ignored elsewhere.
 ** @param iosysid : A defined pio system descriptor (input)
 ** @param ncidp : A pio file descriptor (output)
 ** @param iotype : A pio output format (input)
 ** @param filename : The filename to open 
 ** @param mode : The netcdf mode for the open operation
 */

int PIOc_openfile(const int iosysid, int *ncidp, int *iotype,
		  const char filename[], const int mode)
{
  int ierr;
  int msg;
  int mpierr;
  size_t len;
  iosystem_desc_t *ios;
  file_desc_t *file;

  ierr = PIO_NOERR;

  msg = PIO_MSG_OPEN_FILE;
  ios = pio_get_iosystem_from_id(iosysid);
  if(ios==NULL){
    printf("bad iosysid %d\n",iosysid);
    return PIO_EBADID;
  }

  file = (file_desc_t *) malloc(sizeof(*file));
  if(file==NULL){
    return PIO_ENOMEM;
  }
  file->iotype = *iotype;
  file->next = NULL;
  file->iosystem = ios;
  file->mode = mode;
  for(int i=0; i<PIO_MAX_VARS;i++){
    file->varlist[i].record = -1;
    file->varlist[i].ndims = -1;
#ifdef _PNETCDF
    file->varlist[i].request = NULL;
    file->varlist[i].nreqs=0;
#endif
    file->varlist[i].fillbuf = NULL;
    file->varlist[i].iobuf = NULL;
  }

  file->buffer.validvars=0;
  file->buffer.vid=NULL;
  file->buffer.data=NULL;
  file->buffer.next=NULL;
  file->buffer.frame=NULL;
  file->buffer.fillvalue=NULL;

  if(ios->async_interface && ! ios->ioproc){
    if(ios->comp_rank==0) 
      mpierr = MPI_Send(&msg, 1,MPI_INT, ios->ioroot, 1, ios->union_comm);
    len = strlen(filename);
    mpierr = MPI_Bcast((void *) filename,len, MPI_CHAR, ios->compmaster, ios->intercomm);
    mpierr = MPI_Bcast(&(file->iotype), 1, MPI_INT,  ios->compmaster, ios->intercomm);
    mpierr = MPI_Bcast(&(file->mode), 1, MPI_INT,  ios->compmaster, ios->intercomm);
  }
  
  if(ios->ioproc){

    switch(file->iotype){
#ifdef _NETCDF
#ifdef _NETCDF4

    case PIO_IOTYPE_NETCDF4P:
#ifdef _MPISERIAL   
      ierr = nc_open(filename, file->mode, &(file->fh));
#else
      file->mode = file->mode |  NC_MPIIO;
      ierr = nc_open_par(filename, file->mode, ios->io_comm,ios->info, &(file->fh));
#endif
      break;

    case PIO_IOTYPE_NETCDF4C:
      file->mode = file->mode | NC_NETCDF4;
      // *** Note the INTENTIONAL FALLTHROUGH ***
#endif

    case PIO_IOTYPE_NETCDF:
      if(ios->io_rank==0){
	ierr = nc_open(filename, file->mode, &(file->fh));
      }
      break;
#endif

#ifdef _PNETCDF
    case PIO_IOTYPE_PNETCDF:
      ierr = ncmpi_open(ios->io_comm, filename, file->mode, ios->info, &(file->fh));

      // This should only be done with a file opened to append
      if(ierr == PIO_NOERR && (file->mode & PIO_WRITE)){
	if(ios->iomaster) printf("%d Setting IO buffer %ld\n",__LINE__,PIO_BUFFER_SIZE_LIMIT);
	ierr = ncmpi_buffer_attach(file->fh, PIO_BUFFER_SIZE_LIMIT );
      }
      break;
#endif

    default:
      ierr = iotype_error(file->iotype,__FILE__,__LINE__);
      break;
    }

    // If we failed to open a file due to an incompatible type of NetCDF, try it 
    // once with just plain old basic NetCDF
#ifdef _NETCDF
    if((ierr == NC_ENOTNC || ierr == NC_EINVAL) && (file->iotype != PIO_IOTYPE_NETCDF)) {
        if(ios->iomaster) printf("PIO2 pio_file.c retry NETCDF\n"); 
	// reset ierr on all tasks
	ierr = PIO_NOERR;
	// reset file markers for NETCDF on all tasks
	file->iotype = PIO_IOTYPE_NETCDF;

	// open netcdf file serially on main task
        if(ios->io_rank==0){
	  ierr = nc_open(filename, file->mode, &(file->fh)); }

    }
#endif
  }

  ierr = check_netcdf(file, ierr, __FILE__,__LINE__);

  if(ierr==PIO_NOERR){
    mpierr = MPI_Bcast(&(file->mode), 1, MPI_INT,  ios->ioroot, ios->union_comm);
    pio_add_to_file_list(file);
    *ncidp = file->fh;
  }
  if(ios->io_rank==0){
    printf("Open file %s %d\n",filename,file->fh); //,file->fh,file->id,ios->io_rank,ierr);
//    if(file->fh==5) print_trace(stdout);
  }
  return ierr;
}

/**
 ** @public
 ** @ingroup PIO_createfile
 ** @brief open a new file using pio
 ** @details  Input parameters are read on comp task 0 and ignored elsewhere.
 ** @param iosysid : A defined pio system descriptor (input)
 ** @param ncidp : A pio file descriptor (output)
 ** @param iotype : A pio output format (input)
 ** @param filename : The filename to open 
 ** @param mode : The netcdf mode for the open operation
 */

int PIOc_createfile(const int iosysid, int *ncidp,  int *iotype,
		 const char filename[], const int mode)
{
  int ierr;
  int msg;
  int mpierr;
  
  size_t len;
  iosystem_desc_t *ios;
  file_desc_t *file;


  ierr = PIO_NOERR;

  ios = pio_get_iosystem_from_id(iosysid);
  file = (file_desc_t *) malloc(sizeof(file_desc_t));
  file->next = NULL;
  file->iosystem = ios;
  file->iotype = *iotype;

  file->buffer.validvars=0;
  file->buffer.data=NULL;
  file->buffer.next=NULL;
  file->buffer.vid=NULL;
  file->buffer.ioid=-1;
  file->buffer.frame=NULL;
  file->buffer.fillvalue=NULL;

  for(int i=0; i<PIO_MAX_VARS;i++){
    file->varlist[i].record = -1;
    file->varlist[i].ndims = -1;
#ifdef _PNETCDF
    file->varlist[i].request = NULL;
    file->varlist[i].nreqs=0;
#endif
    file->varlist[i].fillbuf = NULL;
    file->varlist[i].iobuf = NULL;
  }

  msg = PIO_MSG_CREATE_FILE;
  file->mode = mode;

  int my_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
  printf("%d PIOc_createfile\n", my_rank);

  if(ios->async_interface && ! ios->ioproc){
    if(ios->comp_rank==0) 
      mpierr = MPI_Send(&msg, 1, MPI_INT, ios->ioroot, 1, ios->union_comm);
    len = strlen(filename);
    printf("%d bcasting len = %d filename = %s iotype = %d mode = %d ios->compmaster = %d\n",
	   my_rank, len, filename, file->iotype, file->mode, ios->compmaster);
    if (!ios->compmaster)
	ios->compmaster = MPI_PROC_NULL;
    mpierr = MPI_Bcast(&len, 1, MPI_INT,  ios->compmaster, ios->intercomm);
    printf("%d bcasting len = %d filename = %s iotype = %d mode = %d\n",
	   my_rank, len, filename, file->iotype, file->mode);
    mpierr = MPI_Bcast((void *) filename, len + 1, MPI_CHAR, ios->compmaster, ios->intercomm);
    mpierr = MPI_Bcast(&(file->iotype), 1, MPI_INT,  ios->compmaster, ios->intercomm);
    mpierr = MPI_Bcast(&file->mode, 1, MPI_INT,  ios->compmaster, ios->intercomm);
    printf("%d bcasting complete\n", my_rank);
  }
  

  if(ios->ioproc){
    switch(file->iotype){
#ifdef _NETCDF
#ifdef _NETCDF4
    case PIO_IOTYPE_NETCDF4P:
      //         The 64 bit options are not compatable with hdf5 format files
      //      printf("%d %d %d %d %d \n",__LINE__,file->mode,PIO_64BIT_DATA, PIO_64BIT_OFFSET, NC_MPIIO);
      file->mode = file->mode |  NC_MPIIO | NC_NETCDF4;
      //printf("%s %d %d %d\n",__FILE__,__LINE__,file->mode, NC_MPIIO| NC_NETCDF4);
      ierr = nc_create_par(filename, file->mode, ios->io_comm, ios->info, &(file->fh));
      break;
    case PIO_IOTYPE_NETCDF4C:
      file->mode = file->mode | NC_NETCDF4;
#endif
    case PIO_IOTYPE_NETCDF:
      if(ios->io_rank==0){
	ierr = nc_create(filename, file->mode, &(file->fh));
      }
      break;
#endif
#ifdef _PNETCDF
    case PIO_IOTYPE_PNETCDF:
	printf("%d about to call ncmpi_create info = %d\n", my_rank, ios->info);
      ierr = ncmpi_create(ios->io_comm, filename, file->mode, ios->info, &(file->fh));
      if(ierr == PIO_NOERR){
	if(ios->io_rank==0){
	  printf("%d Setting IO buffer size on all iotasks to %ld\n",ios->io_rank,PIO_BUFFER_SIZE_LIMIT);
	}
	int oldfill;
	ierr = ncmpi_buffer_attach(file->fh, PIO_BUFFER_SIZE_LIMIT );
	//	ierr = ncmpi_set_fill(file->fh, NC_FILL, &oldfill);
	printf("%d file->fh = %d\n", my_rank, file->fh);
      }
      break;
#endif
    default:
      ierr = iotype_error(file->iotype,__FILE__,__LINE__);
    }
  }

  ierr = check_netcdf(file, ierr, __FILE__,__LINE__);

  if(ierr == PIO_NOERR){
    mpierr = MPI_Bcast(&(file->mode), 1, MPI_INT,  ios->ioroot, ios->union_comm);
    file->mode = file->mode | PIO_WRITE;  // This flag is implied by netcdf create functions but we need to know if its set
    mpierr = MPI_Bcast(&(file->fh), 1, MPI_INT,  ios->ioroot, ios->union_comm);
    *ncidp = file->fh;
    pio_add_to_file_list(file);
  }
  if(ios->io_rank==0){
    printf("Create file %s %d\n",filename,file->fh); //,file->fh,file->id,ios->io_rank,ierr);
//    if(file->fh==5) print_trace(stdout);
  }
  return ierr;
}

/**
 ** @ingroup PIO_closefile
 ** @brief close a file previously opened with PIO
 ** @param ncid: the file pointer 
 */
int PIOc_closefile(int ncid)
{
  int ierr;
  int msg;
  int mpierr;
  iosystem_desc_t *ios;
  file_desc_t *file;

  ierr = PIO_NOERR;

  int my_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
  printf("%d PIOc_closefile ncid = %d PIO_MSG_CLOSE_FILE = %d\n", my_rank, ncid,
	 PIO_MSG_CLOSE_FILE);

  file = pio_get_file_from_id(ncid);
  if(file == NULL)
    return PIO_EBADID;
  ios = file->iosystem;
  msg = PIO_MSG_CLOSE_FILE;
  if((file->mode & PIO_WRITE)){
    PIOc_sync(ncid);
  }

  /* If async is in use and this is a comp tasks, then the compmaster
   * sends a msg to the pio_msg_handler running on the IO master and
   * waiting for a message. Then broadcast the ncid over the intercomm
   * to the IO tasks. */
  printf("%d PIOc_closefile ios->ioproc = %d\n", my_rank, ios->ioproc);
  if(ios->async_interface && !ios->ioproc){
      if(ios->comp_rank==0) {
	  printf("%d PIOc_closefile about to call MPI_Send msg = %d\n", my_rank, msg);
	  mpierr = MPI_Send(&msg, 1, MPI_INT, ios->ioroot, 1, ios->union_comm);
      }
    mpierr = MPI_Bcast(&(file->fh), 1, MPI_INT, ios->compmaster, ios->intercomm);
  }

  if(ios->ioproc){
    switch(file->iotype){
#ifdef _NETCDF
#ifdef _NETCDF4
    case PIO_IOTYPE_NETCDF4P:
      ierr = nc_close(file->fh);
      break;
    case PIO_IOTYPE_NETCDF4C:
#endif
    case PIO_IOTYPE_NETCDF:
      if(ios->io_rank==0){
	ierr = nc_close(file->fh);
      }
      break;
#endif
#ifdef _PNETCDF
    case PIO_IOTYPE_PNETCDF:
      if((file->mode & PIO_WRITE)){
	ierr = ncmpi_buffer_detach(file->fh);
      }
      ierr = ncmpi_close(file->fh);
      break;
#endif
    default:
      ierr = iotype_error(file->iotype,__FILE__,__LINE__);
    }
  }
  if(ios->io_rank==0){
    printf("Close file %d \n",file->fh);
//    if(file->fh==5) print_trace(stdout);
  }

  ierr = check_netcdf(file, ierr, __FILE__,__LINE__);

  int iret =  pio_delete_file_from_list(ncid);


  return ierr;
}

/**
 ** @ingroup PIO_deletefile
 ** @brief Delete a file 
 ** @param iosysid : a pio system handle
 ** @param filename : a filename 
 */
int PIOc_deletefile(const int iosysid, const char filename[])
{
  int ierr;
  int msg;
  int mpierr;
  int chkerr;
  iosystem_desc_t *ios;

  ierr = PIO_NOERR;
  ios = pio_get_iosystem_from_id(iosysid);

  if(ios == NULL)
    return PIO_EBADID;

  msg = 0;

  if(ios->async_interface && ! ios->ioproc){
    if(ios->comp_rank==0) 
      mpierr = MPI_Send(&msg, 1,MPI_INT, ios->ioroot, 1, ios->union_comm);
    //    mpierr = MPI_Bcast(iosysid,1, MPI_INT, ios->compmaster, ios->intercomm);
  }
  // The barriers are needed to assure that no task is trying to operate on the file while it is being deleted.
  if(ios->ioproc){
    MPI_Barrier(ios->io_comm);
#ifdef _NETCDF
    if(ios->io_rank==0)
      ierr = nc_delete(filename);
#else
#ifdef _PNETCDF
    ierr = ncmpi_delete(filename, ios->info);
#endif
#endif
    MPI_Barrier(ios->io_comm);
  }
  //   Special case - always broadcast the return from the  
  MPI_Bcast(&ierr, 1, MPI_INT, ios->ioroot, ios->my_comm);
  


  return ierr;
}

///
/// PIO interface to nc_sync
///
/// This routine is called collectively by all tasks in the communicator ios.union_comm.  
/// 
/// Refer to the <A HREF="http://www.unidata.ucar.edu/software/netcdf/docs/modules.html" target="_blank"> netcdf </A> documentation. 
///
/** 
* @name    PIOc_sync
*/
int PIOc_sync (int ncid) 
{
  int ierr;
  int msg;
  int mpierr;
  iosystem_desc_t *ios;
  file_desc_t *file;
  wmulti_buffer *wmb, *twmb;

  int my_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
  printf("%d PIOc_sync ncid = %d\n", my_rank, ncid);

  ierr = PIO_NOERR;

  file = pio_get_file_from_id(ncid);
  if(file == NULL)
    return PIO_EBADID;
  ios = file->iosystem;
  msg = PIO_MSG_SYNC;

  if(ios->async_interface && ! ios->ioproc){
    if(!ios->comp_rank) 
      mpierr = MPI_Send(&msg, 1,MPI_INT, ios->ioroot, 1, ios->union_comm);
    mpierr = MPI_Bcast(&(file->fh),1, MPI_INT, ios->compmaster, ios->intercomm);
    printf("%d PIOc_sync comp task Bcast file->fh = %d\n", my_rank, file->fh);
  }

  if((file->mode & PIO_WRITE)){
    //  cn_buffer_report( *ios, true);
    wmb = &(file->buffer); 
    while(wmb != NULL){
      //    printf("%s %d %d %d\n",__FILE__,__LINE__,wmb->ioid, wmb->validvars);
      if(wmb->validvars>0){
	flush_buffer(ncid, wmb, true);
      }
      twmb = wmb;
      wmb = wmb->next;
      if(twmb == &(file->buffer)){
	twmb->ioid=-1;
	twmb->next=NULL;
      }else{
	brel(twmb);
      }
    }
    flush_output_buffer(file, true, 0);
    printf("%d PIOc_sync flushed internal output buffer.\n", my_rank, file->fh);

    if(ios->ioproc){
      switch(file->iotype){
#ifdef _NETCDF
#ifdef _NETCDF4
      case PIO_IOTYPE_NETCDF4P:
	ierr = nc_sync(file->fh);;
	break;
      case PIO_IOTYPE_NETCDF4C:
#endif
      case PIO_IOTYPE_NETCDF:
	if(ios->io_rank==0){
	  ierr = nc_sync(file->fh);;
	}
	break;
#endif
#ifdef _PNETCDF
      case PIO_IOTYPE_PNETCDF:
	printf("%d PIOc_sync calling ncmpi_sync file->fh = %d\n", my_rank, file->fh);
	ierr = ncmpi_sync(file->fh);;
	break;
#endif
      default:
	ierr = iotype_error(file->iotype,__FILE__,__LINE__);
      }
    }

    ierr = check_netcdf(file, ierr, __FILE__,__LINE__);
  }
  return ierr;
}

