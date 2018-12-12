#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>  
#include <sys/types.h>
#include <arpa/inet.h> 
#include <unistd.h>


typedef struct messaggio messaggio;
struct messaggio {
	int id_messaggio;
	char mittente[32];
	char destinatario[32];
	char oggetto[64];
	char testo[2048];
};

int registerUser(char *username, char *password);
int requestLogin(char *username, char *password);
void sendMessage(int socket_to);
void delMessage(int socket_to);
void getMessagesByUser(int socket_to);
struct messaggio structFromRead(int write_socket);
void writeFromStruct(int write_socket, messaggio msg);
int query_id();

char current_user[32];

int main(int argc, char *argv[])
{
	// Controllo sugli argomenti
	if (argc < 2) {
		printf("Usage: ./server.o port\n");
		exit(EXIT_FAILURE);
	}

	int port = atoi(argv[1]); 

	int listening_socket;
	int connection_socket;
	struct sockaddr_in server_address;
	struct sockaddr_in socket_address;
	int sin_size;

	printf("Server in ascolto sulla porta %d \n", port);

	// Creazione listening socket
	if ( (listening_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
	{
		printf("Errore nella creazione del listening socket \n");
		exit(EXIT_FAILURE);
	}

	//Imposta i byte nel socket a zero e riempi i dati significativi
	memset(&server_address, 0 ,sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(port);

	// Bind il socket address sul socket di ascolto
	if (bind(listening_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0 )
	{
		printf("Errore durante bind \n");
		exit(EXIT_FAILURE);
	}

	// Chiamo la listen()
	if ( listen(listening_socket, 5) < 0)
	{
		printf("Errore durante listen \n");
		exit(EXIT_FAILURE);
	}

	// Ciclo per accettare pi� collegamenti con i client
	while(1)
	{
		sin_size = sizeof(struct sockaddr_in);

		if( (connection_socket = accept(listening_socket, (struct sockaddr *) &socket_address, &sin_size)) < 0 )
		{
			printf("Connessione non accettata \n");
			exit(EXIT_FAILURE);
		}

		printf("Connessione accettata da %s \n", inet_ntoa(socket_address.sin_addr));
		
		// Creo nuovo processo per sessione
		if (!fork())
		{
			close(listening_socket);

			char username[32];
			char password[32];

			int scelta = 0;
			int esito;


			// Ciclo per autenticazione/registrazione
			printf("In attesa di autenticazione \n");
			while (1)
			{
				read(connection_socket, &scelta, sizeof(int));

				printf("Attendo username\n");
				read(connection_socket, username, 32);
				printf("Attendo password\n");
				read(connection_socket, password, 32);
				printf("Username: %s", username);
				printf("\nPassword: %s", password);
				
				if (scelta == 1){
					if ( !requestLogin(username, password) ) {
						esito = 0;
						write(connection_socket, &esito, sizeof(int));
					} else { break;	}
				} else if (scelta == 2) {
					if ( !registerUser(username, password) ) {
						esito = 0;
						write(connection_socket, &esito, sizeof(int));
					} else { break; }
				}
				scelta = 0;

			}
			esito = 1;
			write(connection_socket, &esito, sizeof(int));

			// Copio l'username nella variabile dedicata all'utente connesso 
			memcpy(current_user, username, sizeof(current_user));
			printf ("Utente %s autenticato \n", current_user);

			// Ciclo per menu principale
			scelta = 0;
			while(1) 
			{
				printf("Pronto\n");
				read(connection_socket,	&scelta, sizeof(int));
				printf("Scelta effettuata: %d \n", scelta);

				if (scelta == 1)
				{
					printf("Messaggi ricevuti\n");
					getMessagesByUser(connection_socket);
				}
				else if (scelta == 2)
				{
					printf("Invia messaggio\n");
					sendMessage(connection_socket);
				}
				else if (scelta == 3) {
					printf("Elimina messaggi\n");
					delMessage(connection_socket);
				} if (scelta == 4) {
					printf("Chiusura connessione \n");
					break;
				}
				scelta = 0;
			}
			close(connection_socket);
			exit(EXIT_SUCCESS);
		}
	}

	// Termino la conessione
	if (close (listening_socket) < 0 ) 
	{
		printf("Errore durante close \n");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);

}


// Registrazione utente
int registerUser(char *username, char *password) {
	char toWrite[1024];
	char line[1024];

	// Concateno username e password nel formato utilizzato per il file
	strcpy(toWrite, username);
	strcat(toWrite, "~");
	strcat(toWrite, password);
	strcat(toWrite, " ");

	// Apro il file in lettura ed append
	FILE *list_ptr;
	list_ptr = fopen("user_list.dat", "a+");
	if (list_ptr == NULL) { 
		printf("Impossibile aprire il file di utenti\n");
		return 0;
	}

	// Tramite il separatore ricavo l'username per ogni registrazione 
	// e verifico se il nuovo nome 
	// utente � gia esistente
	while(fscanf(list_ptr, "%s", line) != EOF) {
		char *tokens = NULL;
		// Tokenizzo ogni coppia nome~password dal separatore '~' per estrarne
		// l'username e confrontarlo
		tokens = strtok(line ,"~");

		if( strcmp( tokens, username) == 0) {    
			printf("Utente gi� presente\n");
			return 0;
		}
	}
	printf("#### sto registrando %s ####", toWrite);

	fprintf (list_ptr, "%s", toWrite); 

	printf("Registrazione completata\n");
	return 1;
}

// Login utente 
int requestLogin(char *username, char *password) {
	char compare[1024];
	char line[1024];

	// Concateno username e password nel formato utilizzato per il file
	strcpy(compare, username);
	strcat(compare, "~");
	strcat(compare, password);
	//strcat(compare, " ");


	// Apro il file in sola lettura e ne verifico l'esistenza
	FILE *list_ptr;
	list_ptr = fopen("user_list.dat", "r");
	if ( list_ptr == NULL ) { 
		printf("Nessun utente registrato\n");
		return 0;
	}

	// Verifico se le credenziali sono valide ed eventualmente
	// convalido l'autenticazione
	while(fscanf(list_ptr, "%s", line) != EOF) {

		if( strcmp(line, compare) == 0) {
			printf("Utente trovato\n");
			return 1;
		}
	
	}


	printf("Utente non trovato \n");
	return 0;
}

// Invio messaggio
void sendMessage(int socket_to) { 
	FILE *msg_ptr;
	messaggio msg;
	int esito = 1;

	// Apro il file che contiene i messaggi
	msg_ptr = fopen("messages.dat", "a");
	if (!msg_ptr) { 
		printf("Impossibile aprire messages.dat \n");
		esito = 0;
		write(socket_to, &esito, sizeof(int));
		return;
	}

	// Ottengo il messaggio dal client ed ottengo il primo ID libero
	msg = structFromRead(socket_to);
	if ( (msg.id_messaggio = query_id()) == -1 ) {
		printf("Impossibile ottenere l'ID da inserire. Messaggio annullato.\n");
		esito = 0;
		write(socket_to, &esito, sizeof(int));
		return;
	}

	// Scrivo il messaggio nel file
	printf("Id impostato: %d \n", msg.id_messaggio);
	if (!fwrite (&msg, sizeof(messaggio), 1, msg_ptr) ) {
		esito = 0;
	}

	write(socket_to, &esito, sizeof(int));

	fclose(msg_ptr);
	return;
}

// Elimino messaggio
void delMessage(int socket_to) {
	
	int esito = 1;
	int remove_id;
	// Ottengo tutti i messaggi ricevuti per far scegliere all'utente cosa eliminare
	getMessagesByUser(socket_to);

	read(socket_to, &remove_id, sizeof(int));

	FILE *actual_file_ptr;
	FILE *new_file_ptr;
	messaggio msg;

	// Apro in lettura il file dei messaggi
	actual_file_ptr = fopen("messages.dat", "r");
	if (!actual_file_ptr) {
		printf("Impossibile aprire il file di messaggi \n");
		esito = 0;
		write(socket_to, &esito, sizeof(int));
		return;
	}

	// Creo un file temporaneo in scrittura
	new_file_ptr = fopen("temp.dat", "w");
	if (!new_file_ptr) {
		printf("Impossibile creare il file temporaneo\n");
		esito = 0;
		write(socket_to, &esito, sizeof(int));
		return;
	}

	// Ricopio tutti i messaggi da messages.dat a temp.dat ad eccezione di quello da eliminare
	while(fread(&msg, sizeof(messaggio), 1, actual_file_ptr)) {
		if ( (strcmp(msg.destinatario, current_user) == 0) && (msg.id_messaggio == remove_id) )	{
			printf("Record marcato per eliminazione \n");
		} else {
			fwrite (&msg, sizeof(messaggio), 1, new_file_ptr); 
		}
	}

	fclose(actual_file_ptr);
	fclose(new_file_ptr);

	// Elimino il vecchio file e rinomino temp.dat in messages.dat
	remove("messages.dat");
	rename("temp.dat", "messages.dat");
	
	write(socket_to, &esito, sizeof(int));

	return;

}  


// Ottengo tutti i messaggi ricevuti da un utente
void getMessagesByUser(int socket_to) {  
	messaggio msg;
	FILE *msg_list_ptr;

	int next_message = 0;

	// Apro in lettura il file dei messaggi 
	msg_list_ptr = fopen("messages.dat", "r");
	if ( !msg_list_ptr )  
	{
		printf("Impossibile aprire il file di messaggi \n");
		next_message = 0;
		write(socket_to, &next_message, sizeof(int));
		return;
	}

	// Leggo il file dei messaggi
	while(fread(&msg, sizeof(messaggio), 1, msg_list_ptr))
	{
		// Confronto destinatario del messaggio con l'utente della sessione
		if (strcmp(msg.destinatario, current_user) == 0)
		{
			// Avviso il client di un nuovo messaggio in arrivo e lo invio
			next_message = 1;
			write(socket_to, &next_message, sizeof(int));
			writeFromStruct(socket_to, msg);
		}
	}

	next_message = 0;
	write(socket_to, &next_message, sizeof(int));
	fclose(msg_list_ptr);

	return;
}

/* Funzioni di appoggio */

// Costruisco una struct contenente un messaggio da una sequenza di letture dal client
messaggio structFromRead(int write_socket)
{
	messaggio msg;
	msg.id_messaggio = 0;

	read(write_socket, &msg.id_messaggio, sizeof(int));
	read(write_socket, msg.mittente, 32);
	read(write_socket, msg.destinatario, 32);
	read(write_socket, msg.oggetto, 64);
	read(write_socket, msg.testo, 2048);
	return msg;
}

// Invio una serie di informazioni al client partendo da una struct contenente un messaggio
void writeFromStruct(int write_socket, messaggio msg)
{
	write(write_socket, &msg.id_messaggio, sizeof(int));	
	write(write_socket, msg.mittente, 32);
	write(write_socket, msg.destinatario, 32);
	write(write_socket, msg.oggetto, 64);
	write(write_socket, msg.testo, 2048);
}

// Ottengo il primo ID disponibile per l'utilizzo 
int query_id()
{
	messaggio msg;
	FILE *msg_list_ptr;
	int id;

	// Apro in lettura il file dei messaggi
	msg_list_ptr = fopen("messages.dat", "r");
	if (!msg_list_ptr) {
		printf("Impossibile aprire il file di messaggi \n");
		return -1;
	}

	// Verifico che il file sia vuoto. Per farlo vado all'End of File e controllo la posizione
	fseek(msg_list_ptr, 0, SEEK_END); 
	if (ftell(msg_list_ptr) == 0)
	{
		return 0;
	}

	// Se non � vuoto ritorno l'ultimo id utilizzato incrementato di uno
	fseek(msg_list_ptr, 0, SEEK_SET); 
	while(fread(&msg, sizeof(messaggio), 1, msg_list_ptr))
	{
		id = msg.id_messaggio + 1;
	}

	fclose(msg_list_ptr);

	return id;
}