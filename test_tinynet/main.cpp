#define TN_IMPLEMENTATION
#include "../tinynet.h"

#define TT_IMPLEMENTATION
#include "../tinytime.h"

#if TN_PLATFORM != TN_WINDOWS

	// meh. nothing convenient enough to actually implement this on non-Windows
	#define GetAsyncKeyState( ... ) 0

	#include <time.h>
	
	int Sleep( uint32_t milliseconds )
	{
		struct timespec spec;
		spec.tv_sec = 0;
		spec.tv_nsec = milliseconds * 1000000;
		return nanosleep( &spec, 0 );
	}

#endif

struct PacketA
{
	int a;
	int b;
	float c;
	double d;
};

enum PacketTypes
{
	PT_PACKET_NONE,
	PT_PACKETA,

	PT_COUNT,
};

PacketA packet;

float dt = 1.0f / 60.0f;
tnContext* ctx;
tnAddress server_address;
tnSocket server_socket;
tnAddress client_address;
tnSocket client_socket;
tnTransport server;
tnTransport client;

#include "tests.h"

void PeakCheck( tnTransport* transport )
{
	PacketA p;
	tnAddress from;
	int packet_type;
	uint32_t words[ TN_MTU_WORDCOUNT ];

	int bytes = tnPeak_internal( transport, &from, words );
	if ( bytes )
	{
		int header_was_ok = tnReadPacketHeader( transport, words, bytes, &packet_type, 0 );
		CHECK( header_was_ok );

		if ( header_was_ok )
		{
			int serialize_was_ok = tnGetPacketData_internal( transport, words, &p, packet_type );
			CHECK( serialize_was_ok );
			CHECK( Check( packet, p ) );
		}
	}
}

int main( void )
{
	ctx = tnInit( PT_COUNT );

	server_address = tnMakeAddress( "[::1]:1500" );
	client_address = tnMakeAddress( "[::1]:1501" );
	server_socket = tnMakeSocket( server_address, 1024 * 1024, 1 );
	client_socket = tnMakeSocket( client_address, 1024 * 1024, 1 );

	tnRegister( ctx, PT_PACKETA, WritePacketA, ReadPacketA, MeasureWritePacketA, sizeof( PacketA ) );
	tnMakeTransport( &server, ctx, server_socket, client_address, "server" );
	tnMakeTransport( &client, ctx, client_socket, server_address, "client" );

	tnSpawnWorkerThread( &server );
	//tnSpawnWorkerThread( &client );

	//DoTests( );

#if 0
	tnNetSimDef sim;
	sim.latency = 400;
	sim.jitter = 0;
	sim.drop = 0;
	sim.corruption = 0;
	sim.duplicates = 0;
	sim.duplicates_min = 0;
	sim.duplicates_max = 0;
	sim.pool_size = 1024;
	tnAddNetSim( ctx, &sim );
#endif

	packet.a = 5;
	packet.b = 10;
	packet.c = 0.12f;
	packet.d = 102.0912932f;

	while ( 1 )
	{
		if ( GetAsyncKeyState( VK_ESCAPE ) )
			break;
		
		for ( int i = 0; i < 1; ++i )
		{
			tnReliable( &server, PT_PACKETA, &packet );
			tnReliable( &client, PT_PACKETA, &packet );
		}
		tnSend( &server, 0, 0 );
		tnSend( &client, 0, 0 );

		char buffer[ TN_PACKET_DATA_MAX_SIZE ];
		int type = ~0;
		tnAddress from;
		while ( tnGetPacket( &server, &from, &type, buffer ) )
			;
		while ( tnGetPacket( &client, &from, &type, buffer ) )
			;

		PacketA p;
		while ( tnGetReliable( &server, &type, &p ) )
			CHECK( Check( packet, p ) );
		while ( tnGetReliable( &client, &type, &p ) )
			CHECK( Check( packet, p ) );

		// RTT is bugged??
		// golly gee wtf

		float dt = ttTime( );
		tnFlushNetSim( ctx );
		printf( "dt: %f (milliseconds), rtt: %d, ping: %d\n", dt * 1000.0f, server.round_trip_time_millis, 0 );

		tnSleep( 16 );
	}

	tnFreeTransport( &server );
	tnFreeTransport( &client );
	tnShutdown( ctx );

	return 0;
}
