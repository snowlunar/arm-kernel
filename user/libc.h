#ifndef __LIBC_H
#define __LIBC_H

#include <stddef.h>
#include <stdint.h>

#include "terms.h"

// cooperatively yield control of processor, i.e., invoke the scheduler
void yield();

// POSIX fork : page 882 and exec : page 772?
int cfork();

int cexec( char *path ); // DEPRECATED

// exit
void cexit();

// POSIX kill
void ckill( int pid, sig_t sig );

// POSIX raise
void craise( sig_t sig );

// POSIXish
int mqinit( int name ); // need to add mqd to processes list of open mqueues
int mqunlink( int mqd );
void msgsend( int mqd, const void* buf, size_t size );
void msgreceive( int mqd, const void* buf, size_t size );

// write n bytes from x to the file descriptor fd
int write( int fd, void* x, size_t n );
int read( int fd, void* x, size_t n );

// filesystem functions
void disk_wipe();
int fopen( const char *path, int ofile );
int fclose( const int fd );
int fseek( const int fd, uint32_t offset, const int whence );
int funlink( const char *name );

int ftell( const int fd );

// ===========================
// === DIRECTORY FUNCTIONS ===
// ===========================

void pwd();
void ls();
int mkdir( const char *name );
int cd( const char *path );
int mv( const char *src, const char *dest );
int cp( const char *src, const char *dest );

// =========================
// === HELPFUL FUNCTIONS ===
// =========================

char* int2str( int value, char *str, int base );
int str2int( char *str, int n, int base );
void write_int( int fd, char *buf, int x );

#endif
