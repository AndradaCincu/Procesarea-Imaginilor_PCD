/****************************
  Acesta este client controller 
  Această aplicație client controlează,
  modul de transformare a imaginii de către serverul de procesare a imaginilor. 
****************************/

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
	printf("***** SERVER CONTROLLER CLIENT *****\n");

	/*Cuvânt important de identitate trimis către server */
	char *id = "CONTROLLER";

	/*Buffer pentru stocarea adresei IPv4 a serverului. */
	char ip_address[32];

	/*Buffer pentru stocarea mesajelor de la server*/
	char msg[32];
	memset(msg, 0x00, 32);

	/*Bufferul de comandă care va fi trimis către server.*/
	char buff[16];
	memset(buff, 0x00, 16);

	/* Numărul de port al serverului. */
	int port = 0;

	printf("Please enter IPv4 address of server: ");
	scanf("%s", ip_address);

	printf("Please enter port number of server: ");
	scanf("%d", &port);

	printf("\n\n[*]Attempting to connect to server at: %s on port: %d \n", ip_address, port);

	/*Mai întâi vom crea un descriptor de fișier socket*/

	/*TCP Socket. */
	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (sock_fd == -1)
	{
		printf("\n[!]ERROR: Unable to get a socket file descriptor. \n   Terminating... \n");
		return 1;
	}

	/*structura sockaddr_in pentru adresa serverului... */
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

	/*Îi spunem serverului că suntem clientul CONTROLLER.*/

	if (write(sock_fd, id, strlen(id) + 1) < 0)
	{
		printf("\n[!]ERROR occured in write() \n"); 
		return 4; 
	}
	/*Ne așteptăm să primim "OK" de la server */

	if(read(sock_fd, msg, 32) < 0)
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

	/*Suntem gata să trimitem comenzi către serverul nostru.  */
	printf("\n[*]Server accepted our connection ! \n");

	/*Afișați posibilele setări de transformare a imaginii în servieta */
	printf("***** CONTROL OPTIONS *****\n");

	printf("\t1. Grayscale \n");
	printf("\t2. Color Inversion\n");
	printf("\t3. Arbitrary rotation\n");
	printf("\t4. Pyramid Upsampling\n");

	printf("\t5. Pyramid Downsampling\n");
	printf("\t6. Edge detection. \n");
	printf("\t7. Gaussian Blur. \n"); 

	printf("Option >>> ");

	int op = 0; // Opțiune specificată de utilizator.
	float rotangle = 0.0;

	scanf("%d", &op);

	if (op == 3)
	{
		/*Dacă opțiunea aleasă este rotirea, atunci unghiul de rotație trebuie, de asemenea, citit de la utilizator.  */
		printf("\nPlease input a rotation angle in degrees (counterclockwise): ");
		scanf("%f", &rotangle);

		/*Pregătim comanda care va fi trimisă către server.  */

		sprintf(buff, "%d:%2.2f", op, rotangle);
	}
	else
	{
		sprintf(buff, "%d", op);
	}

	/*În cele din urmă, trimitem buffer-ul de comandă către server. */

	write(sock_fd, buff, 16);

	/*Ne așteptăm să primim un mesaj de SUCCES de la server.  */
	memset(msg, 0x00, 32);
	read(sock_fd, msg, 32);

	if (strcmp(msg, "SUCCESS") == 0)
	{
		printf("\n[*]Server settings have been changed successfully! \n");
	}
	else
	{
		printf("\n[!]ERROR occured in changing server settings! \n   Please try again later... \n");
	}

	printf("\n[*]Thank You very much for using SERVER CONTROLLER CLIENT :) \n");

	/*Se închide descriptorul de fișier socket. */
	close(sock_fd);
	return 0;
}
