
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
#include <fcntl.h>
#include <poll.h>

// TODO: hey!? what is this?

/**
 * This is the magic number (already stored in network byte order).
 * See https://en.wikipedia.org/wiki/Magic_number_(programming)#In_protocols
 */
static const uint32_t MAGIC_NUMBER = 0xde1c3232; // = htonl(0x32321cde);

static const uint8_t MSG_REQUEST = 0;
static const uint8_t MSG_RESPONSE_OK = 1;
static const uint8_t MSG_RESPONSE_NA = 2;

enum { 	RAW_MESSAGE_SIZE = 13,
		MAX_CLIENTS_PER_SERVER = 1024};

// To get rid of some warnings
int clientFunc(char *argv);
int serverFunc(char *port, char *metainfo_file);
void removeExtension(char* downloaded_name, const char* file_name);
void serialize(uint8_t *buffer, const uint32_t magicNumber, const uint8_t code, const uint64_t bock_number);
void deserialize(uint32_t *magic_number, uint8_t *message_code, uint64_t *block_number, const uint8_t *buffer);
int torrent_creation(struct torrent_t *torrent, const char *metainfo_file);
int listening_socket(const char *port);


/**
 * Removes the extension of a file name string.
 * @param downloaded_name is the string where the file name without the extension will be stored.
 * @param file_name is the file's name.
 * @return void.
 */
void removeExtension(char* downloaded_name, const char* file_name) {
	for (int i = 0; i < (int) strlen((const char *) file_name); i++) {
		if (file_name[i] != '.')
			downloaded_name[i] = file_name[i];
		else {
			downloaded_name[i] = '\0';
			break;
		}
	}
}


/**
 * Serializes all the message data into an uint8_t buffer.
 * @param buffer is the buffer where the data will be stored.
 * @param magicNumber is the MAGIC_NUBER constant.
 * @param code is the message code: MSG_REQUEST...
 * @param block_number is the bock number.
 * @return void.
 */
void serialize(uint8_t *buffer, const uint32_t magicNumber, const uint8_t code, const uint64_t block_number) {
	// log_printf(LOG_INFO, "	Start of serialization...");

	// Serialize MAGIC NUMBER:
	for (int i = 0; i < 4; i++)
		buffer[i] = (uint8_t) ((magicNumber >> (32 - (i+1)*8)) & 0xff);

	// Serialize message code:
	buffer[4] = code;

	// Serialize block number:
	for (int i = 5; i < 13; i++)
		buffer[i] = (uint8_t) ((block_number >> (64 - (i-4)*8)) & 0xff);

	// log_printf(LOG_INFO,"	...end of serialization");
}


/**
 * Deserializes all the buffer's data.
 * @param magic_number is where the magic number will be stored.
 * @param code is where the message code (MST_RESPONSE_OK...) will be stored.
 * @param block_number is where the block_number will be stored.
 * @param buffer is the buffer where all the data is stored.
 * @return void.
 */
void deserialize(uint32_t *magic_number, uint8_t *message_code, uint64_t *block_number, const uint8_t *buffer) {
	// log_printf(LOG_INFO, "	Start of deserialization...");

	// Deserialize MAGIC NUMBER:
	*magic_number = 0;
	for (int i = 0; i < 4; i++) {
		*magic_number <<= 8;
		*magic_number |= (uint32_t) buffer[i];
	}

	// Deserialize message code:
	*message_code = buffer[4];

	// Deserialize block number:
	*block_number = 0;
	for (int i = 0; i < 8; i++) {
		*block_number >>= 8;
		*block_number |= (uint64_t) buffer[i+5];
	}

	// log_printf(LOG_INFO, "	...end of deserialization");
}


/**
 * Creates the torrent from the metainfo file.
 * @param torrent is the torrent struct that will store the data.
 * @param metainfo_file is the metainfo_file's name string.
 * @return -1 if there is an error or 0 if it was executed successfully.
 */
int torrent_creation(struct torrent_t *torrent, const char *metainfo_file) {
	char downloaded_name[strlen(metainfo_file)];
	removeExtension((char *) &downloaded_name, metainfo_file);

	if(create_torrent_from_metainfo_file((const char *) metainfo_file, (struct torrent_t *) torrent, (const char *) &downloaded_name) == -1) {
		perror("Torrent creation from metainfo file failed");
		return -1;
	}
	return 0;
}


/**
 * Creates socket, sets it to non-blocking, binds it and calls listen().
 * @param port is the connection port that the socket has to be bound to.
 * @return -1 if there is an error or the socket's file descriptor if it was executed successfully.
 */
int listening_socket(const char *port) {
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("Socket creation failed");
		return -1;
	}

	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
		perror("Setting socket to non-blocking failed");
		return -1;
	}
	log_printf(LOG_INFO, "	socket ok");

	struct sockaddr_in servAddr;
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = INADDR_ANY;
	servAddr.sin_port = htons((uint16_t)strtol(port, NULL, 10));

	if (bind(sock, (const struct sockaddr *) &servAddr, sizeof(servAddr)) == -1) {
		perror("Socket bound failed");
		return -1;
	}
	log_printf(LOG_INFO, "	bind ok");

	if (listen(sock, 0) == -1) {
		perror("Socket listening failed");
		return -1;
	}
	log_printf(LOG_INFO, "	listen ok");

	return sock;
}


/**
 * Client function.
 */
int clientFunc(char *argv) {
	log_printf(LOG_INFO, "Executing client...");

	struct torrent_t torrent;
	if (torrent_creation(&torrent, argv) == -1)
		return -1;

	log_printf(LOG_INFO, "Total file size: %li", torrent.downloaded_file_size);

	int sock;
	struct sockaddr_in servAddr;
	for (uint64_t i = 0; i < torrent.peer_count; i++){
		sock = socket(AF_INET, SOCK_STREAM, 0);
		if ( sock == -1 ){
			perror("Socket creation failed");
			return -1;
		}

		struct peer_information_t peer = torrent.peers[i];
		uint32_t ip = 0;
		for (int y = 3; y >= 0; y--) {
			ip <<= 8;
			ip |= (uint32_t) peer.peer_address[y];
		}

		servAddr.sin_family = AF_INET;
		servAddr.sin_addr.s_addr = ip;
		servAddr.sin_port = peer.peer_port;

		log_printf(LOG_INFO, "Connecting to peer #%li... ", i);
		if(connect(sock, (const struct sockaddr *) &servAddr, sizeof(servAddr)) == -1)
			perror("... connection failed");
		else {
			for (uint64_t j = 0; j < torrent.block_count; j++){

				uint8_t message[RAW_MESSAGE_SIZE];
				serialize((uint8_t*) &message, MAGIC_NUMBER, MSG_REQUEST, j);

				log_printf(LOG_INFO, "	Requesting block { magic_number = %08" PRIx32 ", block_number =  %li, message_code = %i}", MAGIC_NUMBER, j, MSG_REQUEST);
				if (send(sock, &message, sizeof(message), 0) == -1)
					perror("Message sending failed");
				else {
					log_printf(LOG_INFO, "	Sent message size: %li", sizeof(message));

					log_printf(LOG_INFO, "	Waiting for response...");
					uint8_t response[RAW_MESSAGE_SIZE];
					ssize_t n = recv(sock, &response, sizeof(response), 0);
					if(n == -1)
						perror("Message reception failed");
					else {
						log_printf(LOG_INFO, "	Message received...");
						log_printf(LOG_INFO, "	Received message size: %li", n);

						uint32_t magic; uint8_t code; uint64_t nBlock;
						deserialize(&magic, &code, &nBlock, (const uint8_t*) &response);

						log_printf(LOG_INFO, "	Received message { magic_number = %08" PRIx32 ", block_number = %li, message_code = %i}", magic, nBlock, code);
						if (magic == MAGIC_NUMBER && code == MSG_RESPONSE_OK) {
							log_printf(LOG_INFO, "		Block available...");

							size_t size = MAX_BLOCK_SIZE;
							if (j == (torrent.block_count -1 ))
								size = torrent.downloaded_file_size - (torrent.block_count - 1)*MAX_BLOCK_SIZE;
							uint8_t response_block[size];
							ssize_t n_block = recv(sock, &response_block, sizeof(response_block), MSG_WAITALL);
							if (n_block == -1)
								perror("Block reception failed");
							else {
								// If the server responds with the block, store it to the downloaded file.
								struct block_t  block;
								for (ssize_t k = 0; k < n_block; k++)
									block.data[k] = response_block[k];

								block.size = (size_t) n_block;
								log_printf(LOG_INFO, "		Block data: { block.size = %li; block.data[0] = %i }", block.size, block.data[0]);
								if (store_block(&torrent, j, &block) == -1)
									perror("	Block storing failed...");
							}
						}
						else
							log_printf(LOG_INFO, "		Block not available...");

					}
				}
			}
		}

		if(close(sock) == -1) {
			perror("Socket closing failed");
			return -1;
		}
	}

	log_printf(LOG_INFO, "Ending clientside...");

	return 0;
}


/**
 * Server function.
 */
int serverFunc(char *port, char *metainfo_file) {
	log_printf(LOG_INFO, "Executing server...");

	// 1. Create the torrent from the metainfo_file:
	struct torrent_t torrent;
	if (torrent_creation(&torrent, metainfo_file) == -1)
		return -1;

	log_printf(LOG_INFO, "Total file size: %li", torrent.downloaded_file_size);

	// 2. Get a listening socket:
	int sock = listening_socket(port);
	if (sock == -1)
		return -1;

	// 3. Initialisation of structures:
	struct pollfd fds[MAX_CLIENTS_PER_SERVER + 1];
	
	nfds_t nfds = 0;	// fds counter
	fds[nfds].fd = sock;
	fds[nfds].events = POLLIN;
	nfds++;

	for(nfds_t i = nfds; i <= MAX_CLIENTS_PER_SERVER; i++) {
    	fds[i].fd = -1;		// File descriptor
    	fds[i].events = 0;	// Events to monitor
    	fds[i].revents = 0;	// Events in place
    }

	// 4. Polling loop:
	while (1) {
		log_printf(LOG_INFO, "	polling...");

		if (poll(fds, nfds, -1) == -1) { // Polling failed
			perror(" Polling failed");
			return -1;
		}
		else {	// Polling succeed
			log_printf(LOG_INFO, " 	...poll has returned with events...");

			// Loop through all file descriptors in fds:
			for (nfds_t i = 0; i < nfds; i++) {
				if (fds[i].fd == -1)	// If the file descriptor is -1, skip it.
					continue;

				if (fds[i].fd == sock && (fds[i].revents & POLLIN)) {	// File descriptor is the listening socket with POLLIN event.
					// Accept a new incoming connection:
					log_printf(LOG_INFO, "		processing pollfd with index %i (fd =  %i, .events = %i, .revents = %i)", i, fds[i].fd, fds[i].events, fds[i].revents);
					log_printf(LOG_INFO, "		New connection incoming");

					struct sockaddr clientAddr;
					socklen_t size = sizeof(clientAddr);

					int s1 = accept(sock, &clientAddr, &size);
					if (s1 == -1) {
						perror("			accept failed");
						continue;	// Skip this iteration.
					}
					else {
						log_printf(LOG_INFO, "			accept ok");

						int option_value = 13;
						if (setsockopt(s1, IPPROTO_TCP, SO_RCVLOWAT, &option_value, sizeof(option_value)) == -1) { // Set sockopt to 13.
							perror("			setsockopt failed");
							close(s1);
							continue;
						}
						else
							log_printf(LOG_INFO, "			setsockopt[SO_RCVLOWAT=13] successful");

						if (fcntl(s1, F_SETFL, O_NONBLOCK) == -1) {	// Set socket to non-blocking.
							perror("			fcntl failed");
							close(s1);
							continue;
						}
						else
							log_printf(LOG_INFO, "			fcntl[O_NONBLOCK] successful");

						// Append socket to fds:
						if (nfds <= MAX_CLIENTS_PER_SERVER) {
							fds[nfds].fd = s1;
							fds[nfds].events = POLLIN;
							nfds++;
						}
						else
							close(s1);
						continue; // Skip iteration.
					}
				}
				else if (fds[i].fd == sock)
					continue;

				// We can ensure it is an ordinary file descriptor:
				log_printf(LOG_INFO, "		processing pollfd with index %i (fd =  %i, .events = %i, .revents = %i)", i, fds[i].fd, fds[i].events, fds[i].revents);
				if (fds[i].revents & POLLIN) { // Attend POLLIN events.
					log_printf(LOG_INFO, "		POLLIN event");
					uint8_t message[RAW_MESSAGE_SIZE];

					if (recv(fds[i].fd, &message, sizeof(message), 0) == -1) { // Client has closed connection.
						log_printf(LOG_INFO, "			client closed connection");

						close(fds[i].fd);
								
						fds[i].fd = 0;
						nfds--;
						continue; // Skip iteration.
					}
					
					// We can ensure there is a message:
					log_printf(LOG_INFO, "			received message from client");

					uint32_t magic; uint8_t code; uint64_t nBlock;
					deserialize(&magic, &code, &nBlock, (const uint8_t*) &message);

					log_printf(LOG_INFO, "			Request is { magic_number = %08" PRIx32 ", block_number = %li, message_code = %i}", magic, nBlock, code);

					if (torrent.block_map[nBlock] == 1) {
						// Block available:
						struct block_t block;
						if (load_block(&torrent, nBlock, &block) == -1) {
							perror("		Block storing failed...");
							continue;
						}

						uint8_t responseWithBlock[MAX_BLOCK_SIZE + RAW_MESSAGE_SIZE];
						serialize((uint8_t*) &responseWithBlock, MAGIC_NUMBER, MSG_RESPONSE_OK, nBlock);
						for (ssize_t k = 0; k < MAX_BLOCK_SIZE; k++)
							responseWithBlock[k + RAW_MESSAGE_SIZE] = block.data[k];

						if (send(fds[i].fd, &responseWithBlock, sizeof(responseWithBlock), 0) == -1) {
							perror("			Message sending failed");
							continue;
						}

						log_printf(LOG_INFO, "			Response will be { magic_number = %08" PRIx32 ", block_number = %li, message_code = %i}", MAGIC_NUMBER, nBlock, MSG_RESPONSE_OK);
						log_printf(LOG_INFO, "			(we will handle this later; marking this fd for POLLOUT");
						// fds[i].events |= POLLOUT;
					}
					else {
						// Block not available:
						uint8_t response[RAW_MESSAGE_SIZE];
						serialize((uint8_t*) &response, MAGIC_NUMBER, MSG_RESPONSE_NA, nBlock);

						if (send(fds[i].fd, &response, sizeof(response), 0) == -1)
							perror("			Message sending failed");
						else
							log_printf(LOG_INFO, "			Response has been { magic_number = %08" PRIx32 ", block_number = %li, message_code = %i}", MAGIC_NUMBER, nBlock, MSG_RESPONSE_NA);
					}
				}
				else if (fds[i].revents & POLLOUT) { // Attend POLLOUT events.
					log_printf(LOG_INFO, "		POLLOUT event");
				}
			}
		}
	}

	(void) metainfo_file;
	(void) port;

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
		serverFunc(argv[2], argv[3]);
	else
		clientFunc(argv[1]);

	// The following statements most certainly will need to be deleted at some point...
	(void) argc;
	(void) MAGIC_NUMBER;
	(void) MSG_REQUEST;
	(void) MSG_RESPONSE_NA;
	(void) MSG_RESPONSE_OK;

	return 0;
}
