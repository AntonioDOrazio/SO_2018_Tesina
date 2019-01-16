#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>  
#include <sys/types.h>
#include <arpa/inet.h> 
#include <unistd.h>
#include <sys/sem.h>

typedef struct messaggio messaggio;
struct messaggio {
	char id_messaggio[4];
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
int query_id();

char current_user[32];

int id_sem;
int chiave_sem = 1234;
struct sembuf sem_lock;
struct sembuf sem_unlock;

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
	if ((listening_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Errore nella creazione del listening socket \n");
		exit(EXIT_FAILURE);
	}

	//Imposta i byte nel socket a zero e riempi i dati significativi
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(port);

	// Bind il socket address sul socket di ascolto
	if (bind(listening_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
	{
		printf("Errore durante bind \n");
		exit(EXIT_FAILURE);
	}

	// Chiamo la listen()
	if (listen(listening_socket, 5) < 0)
	{
		printf("Errore durante listen \n");
		exit(EXIT_FAILURE);
	}

	// Creo il semaforo per gestire gli accessi ai file
	id_sem = semget(chiave_sem, 1, IPC_CREAT | 0666);
	if (id_sem == -1) {
		printf("Errore nella creazione del semaforo \n");
		exit(EXIT_FAILURE);
	}
	semctl(id_sem, 0, SETVAL, 1); // Inizializzo il semaforo a valore 1

	sem_lock.sem_num = 0;
	sem_lock.sem_op = -1;
	sem_lock.sem_flg = SEM_UNDO;

	sem_unlock.sem_num = 0;
	sem_unlock.sem_op = 1;
	sem_unlock.sem_flg = SEM_UNDO;

	// Ciclo per accettare piu collegamenti con i client
	while (1)
	{
		sin_size = sizeof(struct sockaddr_in);

		if ((connection_socket = accept(listening_socket, (struct sockaddr *) &socket_address, &sin_size)) < 0)
		{
			printf("Connessione non accettata \n");
			exit(EXIT_FAILURE);
		}

		printf("Connessione accettata da %s \n", inet_ntoa(socket_address.sin_addr));

		// Creo nuovo processo per sessione
		if (!fork())
		{
			close(listening_socket);

			int res;

			// Imposto il timeout per una lettura
			struct timeval tv;
			tv.tv_sec = 300; // 5 Minuti
			tv.tv_usec = 0;
			setsockopt(connection_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

			char username[32];
			char password[32];

			char scelta[2];
			strcpy(scelta, "0");
			char esito[2];
			strcpy(esito, "0");

			// Ciclo per autenticazione/registrazione
			printf("In attesa di autenticazione \n");
			while (1)
			{
				res = read(connection_socket, scelta, 1);
				if (res == -1 || res == 0)
				{
					printf("Disconnected from client \n");
					exit(EXIT_FAILURE);
				}

				printf("Attendo username\n");
				res = read(connection_socket, username, 32);
				if (res == -1 || res == 0)
				{
					printf("Disconnected from client \n");
					exit(EXIT_FAILURE);
				}
				printf("Username: %s\n", username);

				printf("Attendo password\n");
				res = read(connection_socket, password, 32);
				if (res == -1 || res == 0)
				{
					printf("Disconnected from client \n");
					exit(EXIT_FAILURE);
				}
				printf("Password: %s\n", password);

				if (strcmp(scelta, "1") == 0) {
					if (!requestLogin(username, password)) {

						strcpy(esito, "0");
						write(connection_socket, esito, 1);
					}
					else { break; }
				}
				else if (strcmp(scelta, "2") == 0) {
					if (!registerUser(username, password)) {
						strcpy(esito, "0");
						write(connection_socket, esito, 1);
					}
					else { break; }
				}
				strcpy(scelta, "0");

			}
			strcpy(esito, "1");
			write(connection_socket, esito, 1);

			// Copio l'username nella variabile dedicata all'utente connesso 
			memcpy(current_user, username, sizeof(current_user));
			printf("Utente %s autenticato \n", current_user);

			// Ciclo per menu principale
			strcpy(scelta, "0");
			while (1)
			{
				printf("Pronto\n");
				res = read(connection_socket, scelta, 1);
				if (res == -1 || res == 0)
				{
					printf("Disconnected from client \n");
					exit(EXIT_FAILURE);
				}
				printf("Scelta effettuata: %s \n", scelta);

				if (strcmp(scelta, "1") == 0)
				{
					printf("Messaggi ricevuti\n");
					getMessagesByUser(connection_socket);
				}
				else if (strcmp(scelta, "2") == 0)
				{
					printf("Invia messaggio\n");
					sendMessage(connection_socket);
				}
				else if (strcmp(scelta, "3") == 0) {
					printf("Elimina messaggi\n");
					delMessage(connection_socket);
				}
				else if (strcmp(scelta, "4") == 0) {
					printf("Chiusura connessione \n");
					break;
				}
				strcpy(scelta, "0");
			}
			close(connection_socket);
			exit(EXIT_SUCCESS);
		}
	}

	// Termino la conessione
	if (close(listening_socket) < 0)
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


	printf("registerUser -> Attendo il semaforo \n");
	semop(id_sem, &sem_lock, 1);

	// Apro il file in lettura ed append
	FILE *list_ptr;
	list_ptr = fopen("user_list.dat", "a+");
	if (list_ptr == NULL) {
		printf("Impossibile aprire il file di utenti\n");
		printf("registerUser -> Rilascio il semaforo\n");
		semop(id_sem, &sem_unlock, 1);
		return 0;
	}

	// Tramite il separatore ricavo l'username per ogni registrazione 
	// e verifico se il nuovo nome 
	// utente e' gia esistente
	while (fscanf(list_ptr, "%s", line) != EOF) {
		char *tokens = NULL;
		// Tokenizzo ogni coppia nome~password dal separatore '~' per estrarne
		// l'username e confrontarlo
		tokens = strtok(line, "~");

		if (strcmp(tokens, username) == 0) {
			printf("Utente gia presente\n");
			fclose(list_ptr);
			printf("registerUser -> Rilascio il semaforo\n");
			semop(id_sem, &sem_unlock, 1);
			return 0;
		}
	}
	printf("#### sto registrando %s ####\n", toWrite);

	fprintf(list_ptr, "%s", toWrite);

	fclose(list_ptr);

	printf("registerUser -> Rilascio il semaforo\n");
	semop(id_sem, &sem_unlock, 1);

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

	printf("requestLogin -> Attendo il semaforo\n");
	semop(id_sem, &sem_lock, 1);

	// Apro il file in sola lettura e ne verifico l'esistenza
	FILE *list_ptr;
	list_ptr = fopen("user_list.dat", "r");
	if (list_ptr == NULL) {
		printf("Nessun utente registrato\n");
		printf("requestLogin -> Rilascio il semaforo\n");
		semop(id_sem, &sem_unlock, 1);
		return 0;
	}

	// Verifico se le credenziali sono valide ed eventualmente
	// convalido l'autenticazione
	while (fscanf(list_ptr, "%s", line) != EOF) {

		if (strcmp(line, compare) == 0) {
			printf("Utente trovato\n");
			fclose(list_ptr);
			printf("requestLogin -> Rilascio il semaforo\n");
			semop(id_sem, &sem_unlock, 1);
			return 1;
		}

	}
	fclose(list_ptr);

	printf("requestLogin -> Rilascio il semaforo\n");
	semop(id_sem, &sem_unlock, 1);

	printf("Utente non trovato \n");
	return 0;
}

// Invio messaggio
void sendMessage(int socket_to) {
	int res;
	int next_id = -1;
	FILE *msg_ptr;
	messaggio msg;
	char esito[2];
	strcpy(esito, "1");

	printf("sendMessage -> Attendo il semaforo\n");
	semop(id_sem, &sem_lock, 1);

	// Apro il file che contiene i messaggi
	msg_ptr = fopen("messages.dat", "a");
	if (!msg_ptr) {
		printf("Impossibile aprire messages.dat \n");
		strcpy(esito, "0");
		printf("sendMessage -> Rilascio il semaforo \n");
		semop(id_sem, &sem_unlock, 1);
		write(socket_to, esito, 1);
		return;
	}

	// Ottengo il messaggio dal client ed ottengo il primo ID libero
	res = read(socket_to, &msg, sizeof(msg));
	if (res == -1 || res == 0)
	{
		printf("Disconnected from client \n");
		semop(id_sem, &sem_unlock, 1);
		exit(EXIT_FAILURE);
	}
	if ((next_id = query_id()) == -1) {
		printf("Impossibile ottenere l'ID da inserire. Messaggio annullato.\n");
		strcpy(esito, "0");
		printf("sendMessage -> Rilascio il semaforo \n");
		semop(id_sem, &sem_unlock, 1);
		write(socket_to, esito, 1);

		return;
	}

	// Converto l'id ottenuto in stringa e lo inserisco nella struct
	snprintf(msg.id_messaggio, 4, "%d", next_id);

	// Scrivo il messaggio nel file
	printf("Id impostato: %s \n", msg.id_messaggio);
	if (!fwrite(&msg, sizeof(messaggio), 1, msg_ptr)) {
		strcpy(esito, "0");
	}

	printf("sendMessage -> Rilascio il semaforo \n");
	semop(id_sem, &sem_unlock, 1);

	fclose(msg_ptr);
	write(socket_to, esito, 1);

	return;
}

// Elimino messaggio
void delMessage(int socket_to) {
	int res;
	char esito[2];
	strcpy(esito, "1");

	char remove_id[4];
	// Ottengo tutti i messaggi ricevuti per far scegliere all'utente cosa eliminare
	getMessagesByUser(socket_to);

	res = read(socket_to, remove_id, 4);
	if (res == -1 || res == 0)
	{
		printf("Disconnected from client \n");
		exit(EXIT_FAILURE);
	}
	FILE *actual_file_ptr;
	FILE *new_file_ptr;
	messaggio msg;

	printf("delMessage -> Attendo il semaforo\n");
	semop(id_sem, &sem_lock, 1);

	// Apro in lettura il file dei messaggi
	actual_file_ptr = fopen("messages.dat", "r");
	if (!actual_file_ptr) {
		printf("Impossibile aprire il file di messaggi \n");
		strcpy(esito, "0");
		printf("delMessage -> Rilascio il semaforo \n");
		semop(id_sem, &sem_unlock, 1);
		write(socket_to, esito, 1);
		return;
	}

	// Creo un file temporaneo in scrittura
	new_file_ptr = fopen("temp.dat", "w");
	if (!new_file_ptr) {
		printf("Impossibile creare il file temporaneo\n");
		strcpy(esito, "0");
		printf("delMessage -> Rilascio il semaforo \n");
		semop(id_sem, &sem_unlock, 1);
		write(socket_to, esito, 1);
		return;
	}

	// Ricopio tutti i messaggi da messages.dat a temp.dat ad eccezione di quello da eliminare
	while (fread(&msg, sizeof(messaggio), 1, actual_file_ptr)) {
		if ((strcmp(msg.destinatario, current_user) == 0) && (strcmp(msg.id_messaggio, remove_id) == 0)) {
			printf("Record marcato per eliminazione \n");
		}
		else {
			fwrite(&msg, sizeof(messaggio), 1, new_file_ptr);
		}
	}

	fclose(actual_file_ptr);
	fclose(new_file_ptr);

	// Elimino il vecchio file e rinomino temp.dat in messages.dat
	remove("messages.dat");
	rename("temp.dat", "messages.dat");

	printf("delMessage -> Rilascio il semaforo \n");
	semop(id_sem, &sem_unlock, 1);

	write(socket_to, esito, 1);

	return;
}

// Ottengo tutti i messaggi ricevuti da un utente
void getMessagesByUser(int socket_to) {
	messaggio msg;
	FILE *msg_list_ptr;

	char next_message[2];
	strcpy(next_message, "0");

	printf("getMessagesByUser -> Attendo il semaforo \n");
	semop(id_sem, &sem_lock, 1);

	// Apro in lettura il file dei messaggi 
	msg_list_ptr = fopen("messages.dat", "r");
	if (!msg_list_ptr)
	{
		printf("Impossibile aprire il file di messaggi \n");
		strcpy(next_message, "0");
		printf("getMessagesByUser -> Rilascio il semaforo \n");
		semop(id_sem, &sem_unlock, 1);
		write(socket_to, next_message, 1);
		return;
	}

	// Leggo il file dei messaggi
	while (fread(&msg, sizeof(messaggio), 1, msg_list_ptr))
	{
		// Confronto destinatario del messaggio con l'utente della sessione
		if (strcmp(msg.destinatario, current_user) == 0)
		{
			// Avviso il client di un nuovo messaggio in arrivo e lo invio
			strcpy(next_message, "1");
			write(socket_to, next_message, 1);
			write(socket_to, &msg, sizeof(msg));
		}
	}

	strcpy(next_message, "0");
	write(socket_to, next_message, 1);
	fclose(msg_list_ptr);

	printf("getMessagesByUser -> Rilascio il semaforo \n");
	semop(id_sem, &sem_unlock, 1);

	return;
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

	// Se non e' vuoto ritorno l'ultimo id utilizzato incrementato di uno
	fseek(msg_list_ptr, 0, SEEK_SET);
	while (fread(&msg, sizeof(messaggio), 1, msg_list_ptr))
	{
		id = atoi(msg.id_messaggio) + 1;
	}

	fclose(msg_list_ptr);

	return id;
}
