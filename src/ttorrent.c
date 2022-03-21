
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
		
		printf("Connecting to PORT: %i \n", ntohs(torrent.peers[i].peer_port));
		if(connect(sock, (const struct sockaddr *) &servAddr, sizeof(servAddr)) == -1)
			perror("Connection error");
		else {
			printf("Successfully connected to the server...\n");
			
			// For each incorrect block in the downloaded file.
			//
			// i. Send a request to the server peer.
			// ii. If the server responds with the block, store it to the downloaded file.
			// iii. Otherwise, if the server signals the unavailablity of the block, do nothing.
			//
			
			struct message_t message;
			message.magic_number = ntohl(MAGIC_NUMBER);
			message.message_code = MSG_REQUEST;
			
			struct message_t response;
			for (uint64_t j = 0; j < torrent.block_count; j++){
				// printf("%li\n", message.block_number);
				message.block_number = j;
				if(send(sock, &message, sizeof(message), MSG_OOB) == -1)
					perror("Send error");
				else
					printf("Successfully sent: %i %i %ld \n", ntohl(message.magic_number), message.message_code, message.block_number);
				
				if(recv(sock, &response, sizeof(response), MSG_PEEK) == -1)
					perror("Receive error");
				else
					printf("Message received: %i", response.message_code);
			}
		}
		
		if(close(sock) == -1){
			perror("Closing error");
			return 1;
		}
	}
	
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





