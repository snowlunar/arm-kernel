#include "P4.h"

void P4() {
  const int FILE = fopen( "/file.txt" );

  char message[ 64 ];
  read( FILE, message, 13 );  
  write( STDIO, message, 13 );

  fclose( FILE );

/*  int m = mqinit(100);*/

/*  int message;*/

/*  msgreceive( m, &message, sizeof( int ) );*/
/*  if (message == 15061996) write( STDIO, "success1", 8 );*/

/*  message = 2;*/
/*  msgsend( m, &message, sizeof( int ) );*/

/*  msgreceive( m, &message, sizeof( int ) );*/
/*  if (message == 3) write( STDIO, "success3", 8 );*/

/*  message = 4;*/
/*  msgsend( m, &message, sizeof( int ) );*/

  cexit();
}

void (*entry_P4)() = &P4;
