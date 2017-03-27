#include "Socket.h"

Socket::Socket( int port )
  : port( port ), clientFd( NULL_FD ), serverFd( NULL_FD ) {
}

Socket::~Socket( ) {
  if ( clientFd != NULL_FD )
    close( clientFd );
  if ( serverFd != NULL_FD )
    close( serverFd );
}

int Socket::getClientSocket( char ipName[] ) {
  return getClientSocket( ipName, 0, false );
}

int Socket::getClientSocket( char ipName[], int sndbufsize, bool nodelay ) {

  // Get the host entry corresponding to ipName
  struct hostent* host = gethostbyname( ipName );
  if( host == NULL ) {
    perror ( "Cannot find hostname." );
    return NULL_FD;
  }
      
  // Fill in the structure "sendSockAddr" with the address of the server.
  sockaddr_in sendSockAddr;
  bzero( (char*)&sendSockAddr, sizeof( sendSockAddr ) );
  sendSockAddr.sin_family      = AF_INET; // Address Family Internet
  sendSockAddr.sin_addr.s_addr =
    inet_addr( inet_ntoa( *(struct in_addr*)*host->h_addr_list ) );
  sendSockAddr.sin_port        = htons( port );

  // Open a TCP socket (an Internet strem socket).
  if( ( clientFd = socket( AF_INET, SOCK_STREAM, 0 ) ) < 0 ) {
    perror( "Cannot open a client TCP socket." );
    return NULL_FD;
  }

  // Set the rcvbuf option
  if ( sndbufsize > 0 ) {
    cout << sndbufsize << endl;
    if ( setsockopt( clientFd, SOL_SOCKET, SO_SNDBUF,
		     ( char * )&sndbufsize, sizeof( sndbufsize ) ) < 0 ) {
      perror( "setsockopt SO_SNDBUF failed" );
      return NULL_FD;
      }
  }
  
  // Set the nodelay option
  if ( nodelay == true ) {
    const int on = 1;
    if ( setsockopt( clientFd, IPPROTO_TCP, TCP_NODELAY,
		     ( char * )&on, sizeof( int ) ) < 0 ) {
      perror( "setsockopt TCP_NODELAY failed" );
      return NULL_FD;
    }
  }

  // debug: check sock_sndbuf = 16384
  /* 
  int val;
  socklen_t len = sizeof( val );
  getsockopt( clientFd, SOL_SOCKET, SO_SNDBUF, &val, &len );
  cout << "sock_sndbuf = " << val << endl;
  */
    
  // Connect to the server.
  while ( connect( clientFd, (sockaddr*)&sendSockAddr, 
		   sizeof( sendSockAddr ) ) < 0 );

  // Connected
  return clientFd;
}

int Socket::getServerSocket( ) {
  return getServerSocket( 0, false );
}

int Socket::getServerSocket( int rcvbufsize, bool nodelay ) {
  if ( serverFd == NULL_FD ) { // Server not ready
    sockaddr_in acceptSockAddr;

    // Open a TCP socket (an internet stream socket).
    if( ( serverFd = socket( AF_INET, SOCK_STREAM, 0 ) ) < 0 ) {
      perror( "Cannot open a server TCP socket." );
      return NULL_FD;
    }

    // Set the resuseaddr option
    const int on = 1;
    if ( setsockopt( serverFd, SOL_SOCKET, SO_REUSEADDR,
		     ( char * )&on, sizeof( on ) ) < 0 ) {
      perror( "setsockopt SO_REUSEADDR failed" );
      return NULL_FD;
    }

    // Set the rcvbuf option
    if ( rcvbufsize > 0 ) {
      if ( setsockopt( serverFd, SOL_SOCKET, SO_RCVBUF,
		       ( char * )&rcvbufsize, sizeof( rcvbufsize ) ) < 0 ) {
	perror( "setsockopt SO_RCVBUF failed" );
	return NULL_FD;
      }
    }

    // Set the nodelay option
    if ( nodelay == true ) {
      const int on = 1;
      if ( setsockopt( serverFd, IPPROTO_TCP, TCP_NODELAY,
		       ( char * )&on, sizeof( on ) ) < 0 ) {
	perror( "setsockopt TCP_NODELAY failed" );
	return NULL_FD;
      }
    }
    
    // Bind our local address so that the client can send to us
    bzero( (char*)&acceptSockAddr, sizeof( acceptSockAddr ) );
    acceptSockAddr.sin_family      = AF_INET; // Address Family Internet
    acceptSockAddr.sin_addr.s_addr = htonl( INADDR_ANY );
    acceptSockAddr.sin_port        = htons( port );
    
    if( bind( serverFd, (sockaddr*)&acceptSockAddr,
	      sizeof( acceptSockAddr ) ) < 0 ) {
      perror( "Cannot bind the local address to the server socket." );
      return NULL_FD;
    }
    
    listen( serverFd, 5 );
  }
  
  // Read to accept new requests
  int newFd = NULL_FD;
  sockaddr_in newSockAddr;
  socklen_t newSockAddrSize = sizeof( newSockAddr );
  
  if( ( newFd =
	accept( serverFd, (sockaddr*)&newSockAddr, &newSockAddrSize ) ) < 0 ) {
    perror( "Cannot accept from another host." );
    return NULL_FD;
  }
  return newFd;
}
