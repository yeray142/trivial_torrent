
// Trivial Torrent

// TODO: some includes here

#include "file_io.h"
#include "logger.h"

#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>

// TODO: hey!? what is this?

/**
 * This is the magic number (already stored in network byte order).
 * See https://en.wikipedia.org/wiki/Magic_number_(programming)#In_protocols
 */
static const uint32_t MAGIC_NUMBER = 0xde1c3232; // = htonl(0x32321cde);

static const uint8_t MSG_REQUEST = 0;
static const uint8_t MSG_RESPONSE_OK = 1;
static const uint8_t MSG_RESPONSE_NA = 2;

enum { RAW_MESSAGE_SIZE = 13 };


/**
 * Main function.
 */
int main(int argc, char **argv) {

	set_log_level(LOG_DEBUG);
	log_printf(LOG_INFO, "Trivial Torrent (build %s %s) by %s", __DATE__, __TIME__, "Y. CORDERO and A. VARGAS");
	//printf("File name: %s", argv[1]);
	
	// ==========================================================================
	// Parse command line
	// ==========================================================================

	// TODO: some magical lines of code here that call other functions and do various stuff.
	// Check if client or server using argc and argv.
	

	struct torrent_t torrent;
	char downloaded_name[2048];
	for (int i = 0; i < (int) strlen((const char *) argv[1]); i++) {
		if (argv[1][i] != '.')
			downloaded_name[i] = argv[1][i];
		else{
			downloaded_name[i] = '\0';
			break;
		}
	}
	// printf("Donwloaded file name: %s", downloaded_name);
	int n = create_torrent_from_metainfo_file((const char *) argv[1], (struct torrent_t *) &torrent, (const char *) downloaded_name);
	if(n == -1){
		perror("error");
		return 1;
	}
	
	int sock;
	struct sockaddr_in clientAddr;
	int retConnect;
	for (int i = 0; i < torrent.peer_count; i++){
		sock = socket(AF_INET,SOCK_STREAM,0);
		if ( sock == -1 ){
			perror("Socket error");
			return 1;
		}
		clientAddr.sin_family = AF_INET;
		clientAddr.sin_addr.s_addr = htons(torrent.peers[i].peer_address);
		clientAddr.sin_port = htons(torrent.peers[i].peer_port);
		retConnect =connect(sock, &clientAddr, sizeof(clientAddr));
		if(retConnect == -1){
			perror("Connection error");
			return 1;
		}
		//torrent.peers[i].peer_port
		
		
		
		
		if(close(sock) == -1){
			perror("Closing error");
			return 1;
		}
	}
	

	// The following statements most certainly will need to be deleted at some point...
	(void) argc;
	(void) argv;
	(void) MAGIC_NUMBER;
	(void) MSG_REQUEST;
	(void) MSG_RESPONSE_NA;
	(void) MSG_RESPONSE_OK;

	return 0;
}
