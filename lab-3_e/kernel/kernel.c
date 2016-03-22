#include "kernel.h"

pcb_t pcb[ PROCESS_LIMIT ], *current = NULL;
pcb_t* rq[ PROCESS_LIMIT ]; uint32_t rq_size;

mqueue mq[ MSGCHAN_LIMIT ]; 

ofile_t of[ OFT_LIMIT ]; uint32_t of_size; // open file table
inode_t ai[ AIT_LIMIT ]; uint32_t ai_size; // available inodes table

fs_t fs; // filesystem metadata

int cmp_pcb( const void* p1, const void* p2 ) {
  // Empty entries go at back of list
  if      ( (*((pcb_t**)(p1))) == NULL && (*((pcb_t**)(p2))) != NULL ) return  1;
  else if ( (*((pcb_t**)(p1))) != NULL && (*((pcb_t**)(p2))) == NULL ) return -1;
  else if ( (*((pcb_t**)(p1))) == NULL && (*((pcb_t**)(p2))) == NULL ) return  0;

  // Terminated processes go behind live processes
  if      ( (*((pcb_t**)(p1)))->pst == TERMINATED && (*((pcb_t**)(p2)))->pst != TERMINATED ) return  1;
  else if ( (*((pcb_t**)(p1)))->pst != TERMINATED && (*((pcb_t**)(p2)))->pst == TERMINATED ) return -1;
  else if ( (*((pcb_t**)(p1)))->pst == TERMINATED && (*((pcb_t**)(p2)))->pst == TERMINATED ) return  0;

  // Higher priority goes behind lower priority
  if      ( (*((pcb_t**)(p1)))->prio < (*((pcb_t**)(p2)))->prio ) return  1;
  else if ( (*((pcb_t**)(p1)))->prio > (*((pcb_t**)(p2)))->prio ) return -1;

  // Equal priority live processes ordered by memory addr value
  if      ( p1 > p2 ) return  1;
  else if ( p1 < p2 ) return -1;
  
  return 0;
}

// For standard round robin
void rq_rotate() {
  pcb_t* head = rq[ 0 ];
  int end = head == NULL ? rq_size : rq_size-1;

  for (int i = 0; i < end; i++) {
    rq[ i ] = rq[ i+1 ];
  }  

  rq[ end ] = head;
}

void readyq_add( int p ) {
  if (rq_size < PROCESS_LIMIT) { 
    rq[ rq_size ] = &pcb[ p ];
    rq_size++;
  }
  // TODO: error signal
}

void readyq_rmhead() {
  // Set head to back of queue (for cyclic purposes)
  rq[ 0 ] = rq[ rq_size-1 ];

  // Remove back of queue and decrease size
  rq[ rq_size-1 ] = NULL;
  rq_size--;
} 

void scheduler( ctx_t* ctx ) {
  qsort( rq, PROCESS_LIMIT, sizeof(pcb_t*), cmp_pcb );
  //rq_rotate();

  // There exists a live program in ready queue
  if (rq[ 0 ] != NULL) {
    // Current has not changed
    if (current == rq[ 0 ]) {
      current->prio++; 
    }
    // Current changed
    else {
      current->prio = current->defp; // reset priority
      memcpy( &pcb[ current->pid ].ctx, ctx, sizeof( ctx_t ) );
      memcpy( ctx, &rq[ 0 ]->ctx, sizeof( ctx_t ) );
      current = rq[ 0 ];
    }
  }
}

uint32_t fork( ctx_t* ctx ) {
  pid_t p; // next available pid
  for (p = 0; p < PROCESS_LIMIT; p++) {                          // lowest available pid
    if (pcb[ p ].pst == TERMINATED) {                // process block space available
      pcb[ p ].pid          = p;                     // new id
      pcb[ p ].prt          = current->pid;          // parent id
      memcpy( &pcb[ p ].ctx, ctx, sizeof( ctx_t ) ); // copy current ctx to new block
      pcb[ p ].ctx.gpr[ 0 ] = 0;                     // return value for child process
      //pcb[ p ].ctx.pc       = current->ctx.lr;       // put parent lr into child pc (ENABLES CORRECT RETURNING)
      pcb[ p ].ctx.sp       = ( uint32_t )(  &tos_init + ( p * 0x00001000 ) ); // Prevent stack interference
      pcb[ p ].pst          = EXECUTING;             // process state
      pcb[ p ].defp         = current->defp;
      pcb[ p ].prio         = current->prio;
      readyq_add(p);

      return p; // return value as child pid for parent process 
    }
  }
  return -1;    // error: no available memory
}

void exec( ctx_t* ctx, uint32_t program ) {
  switch (program) {
    case 0x00 : {
      ctx->pc = ( uint32_t )( entry_P0   );
      current->prio = 0;
      break;
    }
    case 0x01 : {
      ctx->pc = ( uint32_t )( entry_P1   );
      current->prio = 0;
      break;
    }
    case 0x02 : {
      ctx->pc = ( uint32_t )( entry_P2   );
      current->prio = 0;
      break;
    }
    case 0x03 : {
      ctx->pc = ( uint32_t )( entry_P3   );
      current->prio = 0;
      break;
    }
    case 0x04 : {
      ctx->pc = ( uint32_t )( entry_P4   );
      current->prio = 0;
      break;
    }
  }
}

void _exit( ctx_t* ctx ) {
  readyq_rmhead();
  current->pst = TERMINATED;

  scheduler( ctx );
  TIMER0->Timer1IntClr = 0x01; // reset timer
}

void kill( pid_t pid, sig_t sig ) {
  if (0 <= pid && pid <= PROCESS_LIMIT) {
    switch (sig) {
      case SIGKILL: { // enforced immediately
        pcb[ pid ].pst = TERMINATED;
        break; 
      }
      case SIGWAIT: { // enforced immediately
        pcb[ pid ].pst = WAITING;
        break;
      }
      case SIGCONT: { // enforced immediately
        if (pcb[ pid ].pst == WAITING) {
          pcb[ pid ].pst = EXECUTING;
        }
        break;
      }
    }
  }
  else if (pid == 9 && sig == SIGKILL) {
    for (pid_t p = 1; p < PROCESS_LIMIT; p++) {
      pcb[ p ].pst = TERMINATED;
    }
  }
}

// ======================
// === MESSAGE QUEUES ===
// ======================

mqd_t mq_open(int name) {
  // Check to see if mqueue already open
  mqd_t j = -1;
  for (mqd_t i = 0; i < MSGCHAN_LIMIT; i++) { // TODO: replace w/ mqueue table
    if (mq[i].msg_qname == name) {
      return i;
    }
    if (j < 0 && mq[i].msg_qname <= 0) { // no lowest qname and mq available
      j = i; // lowest available qname
    }
  }

  // Create new queue
  if (j >= 0) {
    mq[j].msg_qname = name;
  }

  return j; // descriptor > 0 if succeeded; -1 if failed
}

int mq_receive(mqd_t mqd, void* msg_ptr) {
  mq[ mqd ].msg_lrpid = current->pid;  
  
  // space on mqueue & last sender wasn't current receiver
  if (mq[ mqd ].msg_qnum > 0 && mq[ mqd ].msg_lspid != current->pid) { 
    mq[ mqd ].msg_qnum -= 1;
    memcpy( msg_ptr, /*&*/mq[ mqd ].msg_qbuf, sizeof( int ) ); // will pass no. bytes as param

    return sizeof(msg_ptr); //???
  }

  return -1; // return and wait until data is on mqueue
}

int mq_send(mqd_t mqd, void* msg_ptr) {
  mq[ mqd ].msg_lspid = current->pid;

  if (mq[ mqd ].msg_qnum == 0) { // space on mqueue
    mq[ mqd ].msg_qnum++;
    memcpy( mq[ mqd ].msg_qbuf, msg_ptr, sizeof( int ) ); // will pass no. bytes as param
    //mq[ mqd ].msg_qbuf = *(int*)msg_ptr;

    return 0;
  }

  return -1; // return and wait until ready to push data again
}

// ==================
// === FILESYSTEM ===
// ==================

void wipe() {
  // superblock
  fs_t fs;
    fs.fs_sblkno  = 1;
    fs.fs_size    = 2048;
    fs.fs_iblkno  = 2;
    fs.fs_isize   = 64*8;
    fs.fs_dblkno  = 66;
    fs.fs_dsize   = 1982;
    fs.fs_fdbhead = (fs.fs_dsize%64)-1;
    //memset( fs.fs_fdb, 0, 64 * sizeof( daddr32_t ) );
    //memset( fs.fs_fil, 0, 32 * sizeof( uint32_t  ) );

  daddr32_t fdb[ 64 ];

  for (int i = 0; i < 30; i++) {
    if (i == 29) 
      fdb[ 0 ] = fs.fs_sblkno;
    else
      fdb[ 0 ] = i + 1 + fs.fs_dblkno;

    for (int j = 0; j < 63; j++)
      fdb[ j+1 ] = 63*i + j + (fs.fs_dblkno + 30);

    disk_wr( i + fs.fs_dblkno, (uint8_t*)fdb, 64 * sizeof( daddr32_t ) );
  }

  fs.fs_fdb[ 0 ] = fs.fs_dblkno;
  for (int i = 1; i < 62; i++) {
    fs.fs_fdb[ i ] = i + 1986; // 66 + 30 * 64
  }

  disk_wr( fs.fs_sblkno, (uint8_t*)(&fs), sizeof( fs_t ) );
}

daddr32_t balloc() { // block allocation
	if (fs.fs_fdbhead > 0) {
		fs.fs_fdbhead--;
		return fs.fs_fdb[ fs.fs_fdbhead+1 ];
	}
	
	// block addr 1 is addr of superblock - indicates no space remaining
	if (fs.fs_fdb[ fs.fs_fdbhead ] != 1) {
		daddr32_t addr = fs.fs_fdb[ fs.fs_fdbhead ];
		disk_rd( addr, (uint8_t*)fs.fs_fdb, 64 * sizeof( daddr32_t ) );

		return addr;
	}

	return -1; // no more available blocks
}

int bfree( daddr32_t a ) { // block free
	if (fs.fs_fdbhead < 63) {
		fs.fs_fdb[ fs.fs_fdbhead ] = a;
		fs.fs_fdbhead++;
		return 0; // success
	}

	disk_wr( a, (uint8_t*)fs.fs_fdb, 64 * sizeof( daddr32_t ) );
	fs.fs_fdb[ 0 ] = a;
	fs.fs_fdbhead  = 0;

	return 1; // sucess
}

inode_t* getInode( inode_t *in, int ino ) {
	// TODO: validation in case ino is invalid

	int q = ino / 8;
	int r = ino % 8;
	
	icommon_t ic[ 8 ];
	disk_rd( fs.fs_iblkno + q, (uint8_t*)ic, 8 * sizeof( icommon_t ) );

	in->i_number = ino;
	in->i_ic		 = ic[ r ];
	
	return in;
}

inode_t* getFreeInode( inode_t *in ) {
	icommon_t ic[ 8 ];
	for (int i = 0; i < fs.fs_isize/8; i++) {
		disk_rd( fs.fs_iblkno + i, (uint8_t*)ic, 8 * sizeof( icommon_t ) );
		for (int j = 0; j < 8; j++) {
			// icommon is unused
			if (ic[ j ].ic_mode == IFZERO) {
				in->i_number = 8*i + j;
				in->i_ic     = ic[ j ];
				// TODO: reset icommon metadata
				return in;
			}
		}
	}

	// no available inodes
	in = NULL;
	return in;
}

void addInode( inode_t* par, uint32_t ino, const char *name ) {
	const int d     = (par->i_ic.ic_size / 32) + 1;  // which directory
	const int q     = d / 16;				     						 // which block 
	const int r		  = d % 16;			       						 // offset in block
	
	dir_t dir[ 16 ];	
	disk_rd( par->i_ic.ic_db[ q ], (uint8_t*)dir, 16 * sizeof( dir_t ) );

	dir[ r ] = (dir_t){ ino, strlen( name ), *name };
	disk_wr( par->i_ic.ic_db[ q ], (uint8_t*)dir, 16 * sizeof( dir_t ) );
}

int name_to_ino( const char *name, const inode_t *in ) {
	const int bytes = in->i_ic.ic_size; // number of bytes
	const int dirs  = bytes / 32;       // number of directories
	const int blks  = dirs / 16;				// number of blocks
	const int r		  = dirs % 16;			  // offset in last block
	
	dir_t dir[ 16 ];

	for (int i = 0; i < blks; i++) {
		disk_rd( in->i_ic.ic_db[ i ], (uint8_t*)dir, 16 * sizeof( dir_t ) );
		for (int j = 0; j < 16; j++) {
			if (strncmp(name, dir[j].d_name, strlen(name)) == 0)
				return dir[j].d_ino;
		}
	}

	disk_rd( in->i_ic.ic_db[ blks ], (uint8_t*)dir, 16 * sizeof( dir_t ) );
	for (int j = 0; j < r; j++) {
		if (strncmp(name, dir[j].d_name, strlen(name)) == 0)
			return dir[j].d_ino;
	}

	/*
	 * TODO: deal with indirect blocks as well
	 */

	return -1; // does not exist in directory
}

int path_to_ino( const char *path ) {
	inode_t *in;
	int 		 ino0,  ino = ROOT_DIR;
	char 	  *tok0, *tok;

	for (tok0 = tok = strtok( (char*)path, "/" ); 
			 tok != NULL && ino != -1; 
			 tok = strtok( NULL, "/" ) ) {
		getInode( in, ino ); 					 // first iteration will give us root directory

		ino0 = ino;
		ino  = name_to_ino( tok, in ); // if ino is valid (>-1) then there must exist a corresponding valid inode

		tok0 = tok;                    // tok0 will be name of file upon termination, if path is valid
	} 
	
	if (tok != NULL && ino == -1) {
		return -1; // path doesn't exist
	}
	else if (ino == -1) {
		// create new inode and return corresponding ino
		getFreeInode( in );

		// check there exists available inode for return
		if (in == NULL) // TODO: return -1 instead of NULL
			return -2;

		// add new inode to parent directory (assumes new inode is not a directory)
		inode_t *parent;
		getInode( parent, ino0 );
		addInode( parent, ino, tok0 );

		return in->i_number;
	}

  return ino; 
}

int open( const char *path/*, int oflag, ...*/) {
  // find valid file descriptor  
  int fd;  

  for (fd = 0; fd < FDT_LIMIT; fd++) {
    if (current->fd[ fd ] == NULL)
      return fd;
  }
  if (fd >= FDT_LIMIT) 
		return -1; // table full

	// get ino of file at given path
	int ino = path_to_ino( path );
	if (ino < 0) 
		return -1; // unvalid path or no more inodes
	
	// find valid AIT entry
	inode_t *inode = NULL; // pointer to be used by OFT entry
	int zero = AIT_LIMIT;  // keep record of lowest valid empty table entry

	for (int i = 0; i < AIT_LIMIT; i++) {
		if (ai[ i ].i_ic.ic_mode == IFZERO)
			zero = ai[ i ].i_number < zero ? ai[ i ].i_number : zero;

		if (ai[ i ].i_number == ino) {
			inode = &ai[ i ];
			break;
		}
	}	
	if (inode == NULL) { 			// there does not already exist a table entry with given ino 
		if (zero < AIT_LIMIT) { // there exists valid AIT entry
			getInode( inode, ino );
			ai[ zero ] = *inode; // TODO: check if inode gets cleared by stack!!!
		}
		else
			return -1; // not enough space to add inode to table entry (should this be able to occur???)
	}

	// find valid OFT entry and grab the pointer to it
	ofile_t *ofile = NULL;

	for (int i = 0; i < OFT_LIMIT; i++) {
		if (of[ i ].o_inptr == NULL) { // OFT entry is free for use
			ofile = &of[ i ];
			break;
		}
	}
	if (ofile == NULL) 
		return -1; // this probably can't happen

	// link FDT entry to OFT entry
	current->fd[ fd ] = ofile;

	// link OFT entry to AIT entry
	ofile->o_inptr = inode;	

  return fd;
}

// =====================================
// === INTERRUPTS / SUPERVISOR CALLS ===
// =====================================

void kernel_handler_rst( ctx_t* ctx ) { 
  memset( &pcb[ 0 ], 0, sizeof( pcb_t ) );
  pcb[ 0 ].pid      = 0;
  pcb[ 0 ].prt      = 0;
  pcb[ 0 ].ctx.cpsr = 0x50; // processor switched into USR mode, w/ IRQ interrupts enabled
  pcb[ 0 ].ctx.pc   = ( uint32_t )( entry_init );
  pcb[ 0 ].ctx.sp   = ( uint32_t )(  &tos_init );
  pcb[ 0 ].pst      = EXECUTING;
  pcb[ 0 ].defp = pcb[ 0 ].prio = 10; // Higher value = lower priority; (best is 0)

  current = &pcb[ 0 ]; memcpy( ctx, &current->ctx, sizeof( ctx_t ) );
  rq[0] = current;
  rq_size = 1;

	// superblock defined at block address 1
	disk_rd( 1, (uint8_t*)(&fs), sizeof( fs_t ) );

  TIMER0->Timer1Load     = 0x00100000; // select period = 2^20 ticks ~= 1 sec
  TIMER0->Timer1Ctrl     = 0x00000002; // select 32-bit   timer
  TIMER0->Timer1Ctrl    |= 0x00000040; // select periodic timer
  TIMER0->Timer1Ctrl    |= 0x00000020; // enable          timer interrupt
  TIMER0->Timer1Ctrl    |= 0x00000080; // enable          timer

  GICC0->PMR             = 0x000000F0; // unmask all            interrupts
  GICD0->ISENABLER[ 1 ] |= 0x00000010; // enable timer          interrupt
  GICC0->CTLR            = 0x00000001; // enable GIC interface
  GICD0->CTLR            = 0x00000001; // enable GIC distributor

  irq_enable();

  return;
}

void kernel_handler_irq( ctx_t* ctx ) {
  uint32_t id = GICC0->IAR; //read  the interrupt identifier so we know the source

  // handle the interrupt, then clear (or reset) the source.
  if( id == GIC_SOURCE_TIMER0 ) {
    scheduler( ctx );
    TIMER0->Timer1IntClr = 0x01;
  }

  GICC0->EOIR = id; // write the interrupt identifier to signal we're done

  return;
}

void kernel_handler_svc( ctx_t* ctx, uint32_t id, int a1, int a2 ) { 
  switch( id ) {
    case 0x00 : { // yield()
      scheduler( ctx );
      TIMER0->Timer1IntClr = 0x01; // reset timer
      break;
    }
    case 0x01 : { // write( fd, x, n )
      int   fd = ( int   )( ctx->gpr[ 0 ] );  
      char*  x = ( char* )( ctx->gpr[ 1 ] );  
      int    n = ( int   )( ctx->gpr[ 2 ] ); 

      for( int i = 0; i < n; i++ ) {
        PL011_putc( UART0, *x++ );
      }
      
      ctx->gpr[ 0 ] = n;
      break;
    }
    case 0x02 : { // read( fd, x, n ) - non-silent
      int   fd = ( int   )( ctx->gpr[ 0 ] );  
      char*  x = ( char* )( ctx->gpr[ 1 ] );  
      int    n = ( int   )( ctx->gpr[ 2 ] );

      for( int i = 0; i < n; i++ ) {
        x[i] = PL011_getc( UART0 );
        if (x[i] == 13) { // ASCII carriage return
          PL011_putc( UART0, '\n' );
          break; 
        }
        PL011_putc( UART0, x[i] );
      }

      ctx->gpr[ 0 ] = n; // unnecessary?
      break;
    }
    case 0x03 : { // fork
      ctx->gpr[ 0 ] = fork( ctx );
      break;
    }
    case 0x04 : { // exit
      _exit( ctx );
      break;
    }
    case 0x05 : { // exec
      exec( ctx, a1 );
      break;
    }
    case 0x06 : { // kill
      kill( a1, a2 );
      break;
    }
    case 0x07 : { // raise
      kill( current->pid, a1 );
      scheduler( ctx ); // for now - raise immediately invokes scheduler
      break;
    }
    case 0x08 : { // mqueue open
      ctx->gpr[ 0 ] = mq_open( a1 );
      break;
    }
    case 0x09 : { // channel send
      if (0 <= a1 && a1 < MSGCHAN_LIMIT ) { // within bounds of legal message queue addr.
        ctx->gpr[ 0 ] = mq_send( a1, (void*)a2 );
        if (ctx->gpr[ 0 ] == -1) {
          ctx->gpr[ 1 ] = a1;
          ctx->gpr[ 2 ] = a2;
        }
      } 
      break;
    }
    case 0x0a : { // channel receive
      if (0 <= a1 && a1 < MSGCHAN_LIMIT ) { // within bounds of legal message queue addr.
        ctx->gpr[ 0 ] = mq_receive( a1, (void*)a2 );
        if (ctx->gpr[ 0 ] == -1) {
          ctx->gpr[ 1 ] = a1;
          ctx->gpr[ 2 ] = a2;
        }
      }
      break;
    }
    case 0x0b: {
      ///*       block addr     data pointer             byte length   */
      //disk_wr( ctx->gpr[ 0 ], (uint8_t*)ctx->gpr[ 1 ], ctx->gpr[ 2 ] );
      wipe();
      break;
    }
    case 0x0c: {
      /*       block addr     data pointer             byte length   */
      disk_rd( ctx->gpr[ 0 ], (uint8_t*)ctx->gpr[ 1 ], ctx->gpr[ 2 ] );
      break;
    }
    default   : { // unknown
      break;
    }
  }

  return;
}