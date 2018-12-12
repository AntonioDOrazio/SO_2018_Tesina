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

int registrazione(); 
int eseguiLogin();
void messaggiRicevuti();
void inviaMessaggio();
void eliminaMessaggio();
struct messaggio structFromRead(int write_socket);


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

	printf("############ SCAMBIO DI MESSAGGI - Client ############\n\n");
	int auth = 0;
	int scelta = 0;

	// Ciclo per autenticazione/registrazione
	while (!auth) {
		printf("Accedi: 1\nRegistrati: 2\n>> ");

		scanf("%d", &scelta);

		if ( (scelta != 1 ) && (scelta != 2) )
			continue;

		write(conn_socket, &scelta, sizeof(int));

		if (scelta == 1) {
			auth = eseguiLogin();
		} else if (scelta == 2) {
			auth = registrazione();
	 	}
	}

	// Menu principale
	scelta = 0;
	while (scelta != 4)
	{
		printf("\nPrego selezionare un'operazione\n");
		printf("1 - Messaggi ricevuti\n");
		printf("2 - Invia un messaggio\n");
		printf("3 - Elimina dei messaggi ricevuti\n");
		printf("4 - Esci dall'applicazione\n");
		printf("\n>> ");
		scanf("%d", &scelta);

		if (scelta > 4) 
			continue;

		write(conn_socket, &scelta, sizeof(int));

		if (scelta == 1)
		{
			messaggiRicevuti();
		}
		else if (scelta == 2)
		{
			inviaMessaggio();
		}
		else if (scelta == 3) 
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
	int esito = 0;

	// Invio al server username e password
	printf("Inserire username: ");
	scanf("%s", username);
	write(conn_socket, username, 32);
	printf("Inserire password: ");
	scanf("%s", password);
	write(conn_socket, password, 32);

	read(conn_socket, &esito, sizeof(int)); 

	if (esito) {
		printf("Registrazione completata\n ");			
		printf("\nBenvenuto %s\n", username);
		memcpy(active_user, username, 32);
	} else {
		printf("Impossibile completare la registrazione\n");
	}
	return esito;

}

// Login utente esistente
int eseguiLogin()
{
	char username[32];
	char password[32];
	int esito = 0;
	
	// Invio al server username e password
	printf("Inserire username: ");
	scanf ("%s", username);
	write(conn_socket, username, 32);
	printf("Inserire password: ");
	scanf ("%s", password);
	write(conn_socket, password, 32);


	// Richiedo l'esito

	read(conn_socket, &esito, sizeof(int));

	if (esito){
		printf("\nBenvenuto %s\n", username);
		// Autenticazione effettuata
		memcpy(active_user, username, 32);
	}
	else {
		printf("Impossiile eseguire il login \n");
	}
	return esito;
}

// Visualizzo tutti i messaggi ricevuti da un utente
void messaggiRicevuti()
{
	int next_message = 1;
	messaggio msg;
	read(conn_socket, &next_message, sizeof(int));
	while(next_message != 0){
		msg = structFromRead(conn_socket);
		printf("\n## MESSAGGIO ##\n");
		printf("ID\n   %d\n", msg.id_messaggio);
		printf("Destinatario:\n   %s\n",msg.destinatario);
		printf("Oggetto:\n   %s\n", msg.oggetto);
		printf("Testo:\n   %s\n", msg.testo);
		read(conn_socket, &next_message, sizeof(int));

	}
	
}

// Invio un nuovo messaggio al destinatario scelto
void inviaMessaggio() 
{
	int esito = 0;
	messaggio msg;

	msg.id_messaggio = 0;
	printf("Inserisci il destinatario \n");
	scanf(" %49[^\n]", msg.destinatario);
	printf("Inserisci oggetto \n");
	scanf(" %49[^\n]", msg.oggetto);
	printf("Inserisci testo \n");
	scanf(" %49[^\n]", msg.testo);

	write(conn_socket, &msg.id_messaggio, sizeof(int));
	write(conn_socket, active_user, 32);
	write(conn_socket, msg.destinatario, 32);
	write(conn_socket, msg.oggetto, 64);
	write(conn_socket, msg.testo, 2048);

	read(conn_socket, &esito, sizeof(int));

	if (esito){
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
	int id_msg = -1;
	int esito = 0;
	messaggiRicevuti();

	printf("Quale messaggio vuoi eliminare? Inserisci l'ID \n>> ");

	scanf("%d", &id_msg);
	write(conn_socket, &id_msg, sizeof(int));
	
	read(conn_socket, &esito, sizeof(int));
	
	if (esito) {
		printf ("Messaggio eliminato \n");
	} else {
		printf ("Impossibile eliminare il messaggio\n");
	}
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