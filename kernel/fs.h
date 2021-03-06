#ifndef __FS_H
#define __FS_H

#define BLOCK_SIZE 512                       // number of bytes in a block
#define ROOT_DIR 0                           // inode number of root directory

#define FDT_LIMIT 16                         // limit on number of file descriptor table entries (per process)
#define OFT_LIMIT FDT_LIMIT * PROCESS_LIMIT  // limit on number of open file table entries       (global)
#define AIT_LIMIT OFT_LIMIT                  // limit on number of active inode table entries    (global)

#define NDADDR 11                            // number of direct blocks per icommon
#define NIADDR 3                             // number of indirect blocks per icommon
#define MAXNAMLEN 25                         // max number of characters in "inode name"

typedef uint32_t daddr32_t; // 32-bit disk block address

typedef struct {
  uint32_t fs_sblkno;   // block address of superblock
  uint32_t fs_size;     // number of blocks on disk
  daddr32_t fs_iblkno;  // block address of first inode
  uint32_t fs_isize;    // number of inodes on disk (8 inodes per "block")
  daddr32_t fs_dblkno;  // block address of first data block
  uint32_t fs_dsize;    // number of data blocks on disk
  
  uint32_t  fs_fdbhead; // head of list of free data blocks

  daddr32_t fs_fdb[ 64 ]; // list of free data blocks (fs_fdb[ 0 ] points to block of more free addresses, etc.)
  uint32_t  fs_fil[ 32 ]; // list of free inodes (CURRENTLY UNUSED)

  uint8_t __pad__[100]; // usused space
} fs_t; // superblock (defines the filesystem) - 412 < 512 bytes

typedef enum {
  IFZERO = 0, // inode usused
  IFLNK  = 1, // symbolic link
  IFDIR  = 2, // directory
  IFREG  = 3, // regular data file
} mode_t; // icommon (on-core inode) mode / type

typedef struct {
  uint32_t ic_mode;          // 0:       icommon type
  uint32_t ic_size;          // 4:       size of icommon in bytes (ignores fragments)
  daddr32_t ic_db[ NDADDR ]; // 8  - 48: direct   blocks
  daddr32_t ic_ib[ NIADDR ]; // 52 - 60: indirect blocks
} icommon_t; // on-core inode (64 bytes)

typedef struct {
  uint32_t d_ino;
  uint16_t d_namlen;
  char d_name[ MAXNAMLEN+1 ]; // +1 for trailing null character
} dir_t; // directory - simplified to be small, static length (32 bytes) rather than varlength

typedef struct {
  uint32_t  i_number;
  uint32_t  i_links; // how many OFT entries link to this inode
  icommon_t i_ic;
} inode_t; // in-core inode

typedef struct {
  inode_t *o_inptr;
  uint32_t o_head; // r/w head position
} ofile_t; // open file

// === BLOCK ALLOCATION FUNCTIONS ===

daddr32_t balloc();
int bfree( daddr32_t a );

// === SUPERBLOCK FUNCTIONS ===

void wipe();

// === DATA BLOCK FUNCTIONS ===

int getDataBlockAddr( const inode_t *inode, uint32_t byte );
int allocateDataBlocks( inode_t *inode, uint32_t n );
int freeDataBlocks( inode_t *inode );
int getDataBlock( uint8_t *block, const inode_t *inode, uint32_t byte );

// === INODE FUNCTIONS ===

inode_t *copyInode( inode_t *copy, inode_t *inode );
inode_t *readInode( inode_t *in, int ino );
inode_t *writeInode( inode_t *inode );
inode_t *getFreeInode( inode_t *in );
dir_t *getLastDir( dir_t *d, const inode_t *inode );
int removeable( inode_t *parent, dir_t *child );
int remove( dir_t *child, inode_t *parent, const char *name );
int name_to_ino( const char *name, const inode_t *in );
int path_to_ino( char *path, const int dir );
int path_to_ino2(  char *path, const int dir ); // also deals with creating new files

// === DIRECTORIES FUNCTIONS ===

void addInodeToDirectory( inode_t* par, uint32_t ino, const char *name );

// === POSIX FUNCTIONS ===

int open( char *path, int oflag);
int close( const int fd );
int fwrite( const int fd, const uint8_t *data, const int n );
int fread( const int fd, uint8_t *data, const int n );
int lseek( const int fd, uint32_t offset, const int whence );
int unlink(char *name);

// === EXTRA FUNCTIONS ===

int tell( const int fd );

#endif
