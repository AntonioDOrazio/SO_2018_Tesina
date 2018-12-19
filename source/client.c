#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>  
#include <sys/types.h>
#include <arpa/inet.h> 
#include <unistd.h>
#include <signal.h>

typedef struct messaggio messaggio;
struct messaggio {
	char id_messaggio[4];
	char mittente[32];
	char destinatario[32];
	char oggetto[64];
	char testo[2048];
};

int registrazione(); 
int eseguiLogin();
void messaggiRicevuti();
void inviaMessaggio();
void eliminaMessaggio();
struct messaggio structFromRead(int write_socket);

void sigpipeHandler();

int conn_socket; 
char active_user[32];

int main(int argc, char *argv[])
{
	if (argc < 3) {
		printf("Usage: ./client.o ipaddress port\n");
		exit(-1);
	}

	int port_number = atoi(argv[2]);
	char ip_address[16];
	memcpy(ip_address, argv[1], sizeof(ip_address));

	struct sockaddr_in server_address; // Socket address string

	conn_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (conn_socket < 0) {
		printf("Socket non aperto\n");
		exit(EXIT_FAILURE);
	}


	// Imposto il socket address
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(port_number);

	// Imposto indirizzo ip
	if (inet_aton(ip_address, &server_address.sin_addr) <= 0 )
	{
		printf("Ip non valido\n");
		exit(EXIT_FAILURE);
	}

	// Connect al server remoto
	if (connect(conn_socket, (struct sockaddr *) &server_address, sizeof(server_address) ) < 0 )
	{
		printf ("Errore durante connect \n");
		exit (EXIT_FAILURE);
	}





	struct sigaction action;
	sigset_t set;
	sigfillset(&set);
	action.sa_sigaction = sigpipeHandler;     // handler è la funzione a cui va il controllo
	action.sa_mask = set;
	action.sa_flags = 0;

	sigaction(SIGPIPE, &action, NULL);










	printf("############ SCAMBIO DI MESSAGGI - Client ############\n\n");
	int auth = 0;
	char scelta[2];
	strcpy(scelta, "0");

	// Ciclo per autenticazione/registrazione
	while (!auth) {
		printf("Accedi: 1\nRegistrati: 2\n>> ");

		scanf("%s", scelta);

		printf("La scelta è %s", scelta);

		if ((strcmp(scelta, "1") != 0) && (strcmp(scelta, "2") != 0)) 
			continue;

		write(conn_socket, scelta, 1);
		printf("ho scritto - %s -", scelta);


		if ( strcmp(scelta, "1") == 0 ) {
			auth = eseguiLogin();
		} else if ( strcmp(scelta, "2") == 0 ) {
			auth = registrazione();
	 	}
	}

	// Menu principale
	strcpy(scelta, "0");
	while ( strcmp(scelta, "4") != 0 )
	{
		strcpy(scelta, "0");
		printf("\nPrego selezionare un'operazione\n");
		printf("1 - Messaggi ricevuti\n");
		printf("2 - Invia un messaggio\n");
		printf("3 - Elimina dei messaggi ricevuti\n");
		printf("4 - Esci dall'applicazione\n");
		printf("\n>> ");
		scanf("%s", scelta);

		if ((strcmp(scelta, "1") != 0) && (strcmp(scelta, "2") != 0) && (strcmp(scelta, "3") != 0) && (strcmp(scelta, "4") != 0))
			continue;

		write(conn_socket, scelta, 1);

		
		if (strcmp(scelta, "1") == 0)
		{
			messaggiRicevuti();
		}
		else if (strcmp(scelta, "2") == 0)
		{
			inviaMessaggio();
		}
		else if (strcmp(scelta, "3") == 0)
		{
			eliminaMessaggio();
		} 

	}

	return 0;
}

// Registrazione per nuovo utente
int registrazione() 
{ 
	char username[32];
	char password[32];
	char esito[2];
	strcpy(esito, "0");

	// Invio al server username e password
	printf("Inserire username: ");
	scanf("%s", username);
	write(conn_socket, username, 32);
	printf("Inserire password: ");
	scanf("%s", password);
	write(conn_socket, password, 32);

	read(conn_socket, esito, 1); 
	printf("l'esito - %s -", esito);

	if (strcmp(esito, "1") == 0) {
		printf("Registrazione completata\n ");			
		printf("\nBenvenuto %s\n", username);
		memcpy(active_user, username, 32);
		return 1;
	} else {
		printf("Impossibile completare la registrazione\n");
		return 0;
	}

}

// Login utente esistente
int eseguiLogin()
{
	char username[32];
	char password[32];
	char esito[2];
	strcpy(esito, "0");
	
	// Invio al server username e password
	printf("Inserire username: ");
	scanf ("%s", username);
	write(conn_socket, username, 32);
	printf("Inserire password: ");
	scanf ("%s", password);
	write(conn_socket, password, 32);

	// Richiedo l'esito
	read(conn_socket, esito, 1);

	printf("ESITO: %s", esito);

	if (strcmp(esito, "1") == 0){
		printf("\nBenvenuto %s\n", username);
		// Autenticazione effettuata
		memcpy(active_user, username, 32);
		return 1;
	}
	else {
		printf("Impossiile eseguire il login \n");
		return 0;
	}
}

// Visualizzo tutti i messaggi ricevuti da un utente
void messaggiRicevuti()
{
	char next_message[2];
	strcpy(next_message, "1");
	messaggio msg;
	read(conn_socket, next_message, 1);

	while( strcmp(next_message, "0") != 0 ) {
		read(conn_socket, &msg, sizeof(msg));
		printf("\n## MESSAGGIO ##\n");
		printf("ID\n   %s\n", msg.id_messaggio);
		printf("Destinatario:\n   %s\n",msg.destinatario);
		printf("Oggetto:\n   %s\n", msg.oggetto);
		printf("Testo:\n   %s\n", msg.testo);
		read(conn_socket, next_message, 1);

	}
	
}

// Invio un nuovo messaggio al destinatario scelto
void inviaMessaggio() 
{
	char esito[2];
	strcpy(esito, "0");

	messaggio msg;

	strcpy(msg.id_messaggio, "0");
	printf("Inserisci il destinatario \n");
	scanf(" %49[^\n]", msg.destinatario);
	printf("Inserisci oggetto \n");
	scanf(" %49[^\n]", msg.oggetto);
	printf("Inserisci testo \n");
	scanf(" %49[^\n]", msg.testo);

	memcpy(msg.mittente, active_user, 32);

	write(conn_socket, &msg, sizeof(msg));

	read(conn_socket, esito, 1);

	if ( strcmp(esito, "1") == 0 ){
		printf("Messaggio inviato correttamente\n");
	}
	else {
		printf("Impossibile inviare il messaggio\n");
	}
	return;
}

// Elimino un messaggio ricevuto dato l'ID inserito
void eliminaMessaggio()
{
	char id_msg[4];
	char esito[2];

	strcpy(id_msg, "-1");
	strcpy(esito, "0");

	messaggiRicevuti();

	printf("Quale messaggio vuoi eliminare? Inserisci l'ID \n>> ");

	scanf("%s", id_msg);
	write(conn_socket, id_msg, 4);
	
	read(conn_socket, esito, 1);
	
	if ( strcmp(esito, "1") == 0 ) {
		printf ("Messaggio eliminato \n");
	} else {
		printf ("Impossibile eliminare il messaggio\n");
	}
}

/* Funzioni di appoggio */

void sigpipeHandler() {
	printf("\nErrore: server disconnesso. Il programma verra chiuso\n");
	exit(EXIT_FAILURE);
}
