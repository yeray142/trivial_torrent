
// Trivial Torrent

// TODO: some includes here

#include "file_io.h"
#include "logger.h"

#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <stdlib.h>

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

struct message_t {
	uint32_t magic_number;
	uint8_t message_code;
	uint64_t block_number;
	uint8_t payload[RAW_MESSAGE_SIZE];
};

// To get rid of some warnings
int clientFunc(char *argv);
int serverFunc(char **argv);


int clientFunc(char *argv) {
	printf("Executing client...\n");
	
	struct torrent_t torrent;
	
	char downloaded_name[2048];
	for (int i = 0; i < (int) strlen((const char *) argv); i++) {
		if (argv[i] != '.')
			downloaded_name[i] = argv[i];
		else{
			downloaded_name[i] = '\0';
			break;
		}
	}
	// printf("Donwloaded file name: %s", downloaded_name);
	
	if(create_torrent_from_metainfo_file((const char *) argv, (struct torrent_t *) &torrent, (const char *) downloaded_name) == -1){
		perror("Error");
		return 1;
	}
	
	int sock;
	struct sockaddr_in servAddr;
	for (int i = 0; i < (int) torrent.peer_count; i++){
		// printf("Adress: %i", torrent.peers[i].peer_address[0]);
		sock = socket(AF_INET, SOCK_STREAM, 0);
		if ( sock == -1 ){
			perror("Socket error");
			return 1;
		}
		
		// printf("Port: %i", ntohs(torrent.peers[i].peer_port));
		servAddr.sin_family = AF_INET;
		servAddr.sin_addr.s_addr = INADDR_ANY;
		servAddr.sin_port = torrent.peers[i].peer_port;
		
		printf("Connecting to peer #%i... \n", i);
		if(connect(sock, (const struct sockaddr *) &servAddr, sizeof(servAddr)) == -1)
			perror("... connection failed");
		else {
			for (uint64_t j = 0; j < torrent.block_count; j++){
				
				uint8_t message[RAW_MESSAGE_SIZE]; 
				printf("	Init serialization...\n");
				message[0] = (uint8_t) ((MAGIC_NUMBER >> 24) & 0xff); 
				message[1] = (uint8_t) ((MAGIC_NUMBER >> 16) & 0xff);	
				message[2] = (uint8_t) ((MAGIC_NUMBER >> 8) & 0xff);
 				message[3] = (uint8_t) (MAGIC_NUMBER & 0xff);	
				message[4] = MSG_REQUEST; 						// Message code!
				message[5] = (uint8_t) ((j >> 56) & 0xff);
				message[6] = (uint8_t) ((j >> 48) & 0xff);
				message[7] = (uint8_t) ((j >> 40) & 0xff);
				message[8] = (uint8_t) ((j >> 32) & 0xff);
				message[9] = (uint8_t) ((j >> 24) & 0xff);
				message[10] = (uint8_t) ((j >> 16) & 0xff);
				message[11] = (uint8_t) ((j >> 8) & 0xff);
				message[12] = (uint8_t) (j & 0xff);				// Block number!
				printf("	End of serialization...\n");

				printf("	Requesting block { magic_number = %08" PRIx32 ", block_number =  %li, message_code = %i}\n", MAGIC_NUMBER, j, MSG_REQUEST);
				if (send(sock, &message, sizeof(message), 0) == -1)
					perror("Send error");

				uint8_t response[RAW_MESSAGE_SIZE+MAX_BLOCK_SIZE]; 
				printf("	Waiting for response...\n");
				if(recv(sock, response, sizeof(response), 0) == -1)
					perror("Receive error");
				else
					printf("	Message received...\n");

				printf("	Init deserialization...\n");
				uint32_t magic = (uint32_t) (response[3] | (response[2] << 8) | (response[1] << 16) | (response[0] << 24));
				uint8_t code = response[4];
				uint64_t nBlock = (uint64_t) (response[12] | (response[11] << 8) | (response[10] << 16) | (response[9] << 24)); // | (response[8] << 32) | (response[7] << 40) | (response[6] << 48) | (response[5] << 56));
				printf("	End of deserialization...\n ");
				printf("	Received message { magic_number = %08" PRIx32 ", block_number = %li, message_code = %i}\n", magic, nBlock, code);       
			}
		}
		
		if(close(sock) == -1){
			perror("Closing error");
			return 1;
		}
	}
	
	printf("Ending clientside...\n");

	(void) argv;
	return 0;
}


int serverFunc(char **argv) {
	printf("Executing server...\n");
	
	(void) argv;
	return 0;
}


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
	if (argv[1][0] == '-' && argv[1][1] == 'l')
		serverFunc(argv);
	else
		clientFunc(argv[1]);

	// The following statements most certainly will need to be deleted at some point...
	(void) argc;
	(void) argv;
	(void) MAGIC_NUMBER;
	(void) MSG_REQUEST;
	(void) MSG_RESPONSE_NA;
	(void) MSG_RESPONSE_OK;

	return 0;
}





