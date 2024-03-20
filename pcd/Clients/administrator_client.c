/****************************
 Acesta este clientul administrator al serverului de procesare a imaginilor cu mai multe fire de execuție. 
 * Acest client poate vizualiza cel mai recent jurnal (activitate) al serverului multi threaded.
 * Acest client poate, de asemenea, să oprească serverul. 
***************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[])
{

	printf("***** ADMINISTRATOR CLIENT *****\n");

	/*Cuvânt important de identitate trimis către server  */
	char *id = "ADMIN";

	/*Buffer pentru stocarea adresei IPv4 a serverului. */
	char ip_address[32];

	/*Buffer pentru a stoca jurnalele primite de la server*/
	char log_data[1024];
	memset(log_data, 0x00, 1024);

	/*Buffer pentru a stoca mesaje mici de la server*/
	char msg[32];
	memset(msg, 0x00, 32);

	/*Bufferul de comandă care va fi trimis către server. */
	char buff[32];
	memset(buff, 0x00, 32);

	/* Numărul de port al serverului.  */
	int port = 0;

	printf("Please enter IPv4 address of server: ");
	scanf("%s", ip_address);

	printf("Please enter port number of server: ");
	scanf("%d", &port);

	printf("\n\n[*]Attempting to connect to server at: %s on port: %d \n", ip_address, port);

	/*Mai întâi vom crea un descriptor de fișier socket */

	/*TCP Socket. */
	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (sock_fd == -1)
	{
		printf("\n[!]ERROR: Unable to get a socket file descriptor. \n   Terminating... \n");
		return 1;
	}

	/*structura sockaddr_in pentru adresa serverului...*/
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0x00, sizeof(struct sockaddr_in));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(ip_address);
	serv_addr.sin_port = htons(port);

	/*Încercăm să ne conectăm la serverul nostru.  */

	if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in)) == -1)
	{
		printf("\n[!]ERROR: Unable to connect() to our server... \n   Terminated!!! \n");
		return 2;
	}

	/*Îi spunem serverului că suntem clientul CONTROLLER. */

	if (write(sock_fd, id, strlen(id) + 1) < 0)
	{
		printf("\n[!]ERROR occured in write() \n");
		return 4;
	}
	/*Ne așteptăm să primim "OK" de la server*/

	if (read(sock_fd, msg, 32) < 0)
	{
		printf("\n[!]ERROR occured in read() \n");
		return 5;
	}

	if (strcmp(msg, "OK") != 0)
	{
		printf("Message from server: %s ", msg);
		printf("\n[!]Server did NOT accept our connection. \n   Terminating... \n");
		return 3;
	}

	/*Suntem gata să trimitem comenzi către serverul nostru.*/
	printf("\n[*]Server accepted our connection ! \n");

	while (1)
	{
		/*Resetarea bufferelor*/
		memset(msg, 0x00, 32);
		memset(buff, 0x00, 32);

		/*Resetează memoria buffer care primește jurnalul.  */
		memset(log_data, 0x00, 1024);
		printf("\n***** ADMIN CONTROLS ****\n");

		printf("\t1. View most recent log message. \n");
		printf("\t2. Stop server. \n");
		printf("\t3. Exit our session! \n");

		printf("Option >>> ");
		int op; // Opțiunea aleasă de utilizator.

		/*Citirea datelor introduse de utilizator.*/
		scanf("%d", &op);

		printf("\n\n");

		if (op == 1)
		{
			/*Trimite comanda LOG către server. */
			sprintf(buff, "LOG");

			if (write(sock_fd, buff, 32) < 0)
			{
				printf("[!]ERROR in write() \n");
				break;
			}

			/*Citim jurnalul primit de la server.  */
			if (read(sock_fd, log_data, 1024) < 0)
			{
				printf("[!]ERROR in write() \n");
				break;
			}

			printf("\n***** LOG RECEIVED ***** \n%s\n", log_data);
		}
		else if (op == 2)
		{
			/*Citim jurnalul primit de la server. */
			sprintf(buff, "STOP");

			if (write(sock_fd, buff, 32) < 0)
			{
				printf("[!]ERROR in write() \n");
				break;
			}

			/*Citim mesajul primit de la server.  */
			if (read(sock_fd, msg, 32) < 0)
			{
				printf("[!]ERROR in write() \n");
				break;
			}

			if (strcmp(msg, "BYE") == 0)
			{
				printf("\n[*]Server SHUTDOWN initiated, successfully...\n");
			}
			else
			{
				printf("\n[!]An ERROR occured in initiating SHUTDOWN !\n");
			}
			break; 
		}
		else if (op == 3)
		{
			/*Trimiteți comanda SESSIONEXIT către server.*/
			sprintf(buff, "SESSIONEXIT");

			if (write(sock_fd, buff, 32) < 0)
			{
				printf("[!]ERROR in write() \n");
				break;
			}
			
			break; 
		}
		else
		{
			printf("\n[!]Please choose a valid option, and try again! \n");
		}
	}

	printf("\n[*]Thank You for using the ADMINISTRATOR CLIENT APPLICATION ! \n");

	/*Se închide socket-ul.*/
	close(sock_fd);
	return 0;
}
