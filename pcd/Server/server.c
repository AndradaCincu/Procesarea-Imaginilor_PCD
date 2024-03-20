/******************************
Aceasta este implementarea,
serverului multithreaded care efectuează procesarea imaginilor,
 Acesta are capacitatea de a servi trei tipuri de clienți:
 * Client de încărcare. 
 * Client administrator.
 * Client de control al modului de transformare a imaginii. (Controler client)
 
 * Executabilul (aplicația server) care rezultă,
 din compilarea acestui cod sursă,
  va avea o singură linie de comandă 
care este numărul portului,
 în care serverul ar trebui să asculte pentru incomming.
  
* Pentru a efectua procesarea imaginilor, serverul va utiliza interpretorul Python pentru a rula un script care,
 va efectua sarcina de procesare a imaginii necesară, 
 pe imaginea pe care serverul a primit-o de la client. 
 * Imaginea transformată este trimisă înapoi la client.
 
*******************************/

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

/*Pentru multi threading. */
#include <pthread.h>

/*Mutex care ajută la accesarea în siguranță a secțiunilor critice de cod. */
pthread_mutex_t mux;

/*Calea de acces către scriptul Python care efectuează procesarea imaginilor. */
#define IMG_PROC_SCRIPT "./Processing/img_processor.py"

/*Struct care va conține informațiile necesare pentru thread-ul de gestionare a conexiunii. */
typedef struct THREAD_DATA
{
	int client_sock; // Descriptorul de socket al clientului.

	/*Informații privind adresa socketului 
	  a clientului, care este gestionată.
	*/
	struct sockaddr_in client_addr;
} THREAD_DATA;

/*Definim o structură SERVER_INFO, care va conține setările esențiale ale serverului nostru. 
   * Acesta conține, de asemenea, modul de transformare care este setat pe server, 
de către controlling client.
     
   * conține, de asemenea, informații de autentificare(log info), care sunt trimise către clientul administrator dacă este conectat.  
   
 */
typedef struct SERVER_INFO
{
	char log_data[1024];	 /*String care stochează un comentariu cu privire la ceea ce face serverul în acest moment. */
	int transformation_mode; /*Modul de transformare a imaginii 
	 0 = none (default)
	 1 = grayscale
	 2 = color inversion
	 3 = Arbitrary rotation. 
	 4 = Pyramid Upsampling
	 5 = Pyramid Downsampling
	 6 = Edge detection. 
	 7 = Gaussian Blur. 
*/
	float rotation; // Variabilă pentru stocarea unghiului de rotație în cazul în care modul de transformare este setat la 3.

	/*Numele original al fișierului curent,
     care este procesat de server. Serverul primește acest nume de la clientul responsabil,
   pentru încărcarea imaginilor pe server.  */
	char filename[1024];

	/*Variabila indicator care îl menține în funcțiune, dacă valoarea sa este 1*/
	int run;

	/*Descriptorul de fișier socket al serverului */
	int server_socket_fd;

} SERVER_INFO;

/*Creăm o variabilă structurală SERVER_INFO, la nivel global aici. 
Datele conținute în această instanță struct vor fi protejate cu ajutorul mutexurilor și al locks-urilor,
deoarece vor fi accesate prin intermediul mai multor fire de execuție. 
*/

SERVER_INFO serv_info;

/*Funcție care declanșează interpretorul Python pentru a rula scriptul care face procesarea imaginii pentru noi. 
 * Acesta procesează fișierul imagine specificat de "input_path" și salvează imaginea transformată în calea "output_path". 
 * În funcție de valoarea lui "code", se face transformarea corespunzătoare! 
 * Această funcție returnează 0 în caz de succes, iar valorile diferite de zero indică un eșec. 
*/
int transform_image(const char *input_path, const char *output_path, int code, float rotation_angle)
{
	/*Căutați un tabel care să mapeze, codul de transformare a imaginii la codul corect, string argument care trebuie specificat pentru scriptul Python. 
	*/
	const char *script_arguments[8] = {"none", "grayscale", "invert", "rotate", "pyramid_up", "pyramid_down", "edgedetect", "blur"};

	/*Argument selectat. */

	char selected_arg[32];
	memset(selected_arg, 0x00, 32);

	/*Trebuie să tratăm argumentul de rotație într-un mod special, deoarece
	trebuie să introducem și gradele de rotație.  */

	if (code != 3)
	{
		sprintf(selected_arg, "-%s", script_arguments[code]);
	}
	else
	{
		sprintf(selected_arg, "-%s:%2.2f", script_arguments[code], rotation_angle);
	}

	int ret = 0; // Valoarea returnată de la procesul copil.

	/*Folosim fork() și execlp() pentru a porni interpretorul python și pentru a rula scriptul cu argumentele corespunzătoare,
	și să efectuăm procesarea imaginii solicitate.  */

	/*Identificatorul de proces al procesului copil. */
	pid_t p;

	/*Codul de stare pentru waitpid*/
	int status = 0;

	if ((p = fork()) == 0)
	{
		/*Procesul copilului a început. */
		/*înlocuim procesul copil cu interpretorul Python */
		if (execlp("python3", "python3", IMG_PROC_SCRIPT, selected_arg, input_path, output_path, NULL) == -1)
		{
			exit(9);
		}
	}
	else
	{
		/*Procesul părinte așteaptă ca procesul copil să se întoarcă. */
		waitpid(p, &status, 0);
	}

	ret = WEXITSTATUS(status);
	/*Se returnează codul de ieșire al scriptului Python. 
	  * Examinarea acestui cod ne spune dacă transformarea a fost efectuată cu succes sau nu.  
	*/
	return ret;
}

/*Funcție care se ocupă de uploader client. Această funcție primește fișierul imagine de la client,
și îl transformă în funcție de setarea curentă a "modului de transformare" al serverului. 
* În cele din urmă, funcția trimite înapoi imaginea transformată către clientul solicitant. 
*/
void handle_uploader_client(const THREAD_DATA *th_data)
{
	int cl_sock_fd = th_data->client_sock;

	/*Variabilă pentru a stoca valoarea de returnare a funcțiilor. */
	int ret = 0;

	/*Okay mesaj */
	char *okay = "OK";

	/*Success mesaj */
	char *success = "SUCCESS";

	/*Error mesaj... */
	char *error = "ERROR";

	/*Trimiteți mesajul de OK către client.  */
	write(cl_sock_fd, okay, strlen(okay) + 1);

	char file_name[1024];
	char file_size[64];

	/*Array pentru a stoca mesajul de răspuns, de la client. */
	char msg[32];
	memset(msg, 0x00, 32);

	/*Pentru a salva fișierul în memoria noastră locală...  */
	char path[128];
	memset(path, 0x00, 128);

	/*Pentru a salva fișierul transformat.*/ char transformed_img_file_path[128];
	memset(transformed_img_file_path, 0x00, 128);

	memset(file_size, 0x00, 64);
	memset(file_name, 0x00, 1024);

	/*Citim numele fișierului care se încarcă. */
	ret = read(cl_sock_fd, file_name, 1024);

	if (ret != -1)
	{
		/*Am primit numele fișierului de la client!  */

		pthread_mutex_lock(&mux);
		printf("\n[*]Uploader Client: Filename: %s\n", file_name);
		pthread_mutex_unlock(&mux);
	}
	else
	{
		return;
	}

	/*Trimitem un mesaj de aprobare înapoi la client.*/
	write(cl_sock_fd, okay, strlen(okay) + 1);

	/*citim dimensiunea fișierului care urmează să fie încărcat de la client. */
	read(cl_sock_fd, file_size, 64);

	/*Trimitem un mesaj de aprobare înapoi la client.  */
	write(cl_sock_fd, okay, strlen(okay) + 1);
	/*Acum începem să citim fișierul de la client.*/
	/*Alocăm un buffer de 4096 bytes. 
         * Citim 4096 bytes într-o singură citire. 
	 * Scriem imediat datele citite în memoria noastră locală, astfel încât să le putem prelucra după ce am terminat de citit.  
	*/
	unsigned char *buff = (unsigned char *)malloc(4096);

	ssize_t n = 0; // Numărul de bytes citiți..

	/*Salvăm fișierul în directorul "Uploads". */
	sprintf(path, "./Uploads/%s", file_name);

	FILE *fp = fopen(path, "wb");

	ssize_t sz = 0;
	/*Convertim dimensiunea șirului de caractere a fișierului într-un număr întreg zecimal.*/

	ssize_t fSize = atol(file_size);

	while (sz < fSize)
	{
		n = read(cl_sock_fd, buff, 4096);
		fwrite(buff, 1, n, fp);
		sz += n;
	}

	/*Închidem dosarul în mod corespunzător. */
	fclose(fp);

	pthread_mutex_lock(&mux);
	printf("\n[*]Successfully received file: %s \n", file_name);

	/*Am resetat logul serverului. */
	memset(serv_info.log_data, 0x00, 1024);

	/*Actualizăm logul serverului. */
	sprintf(serv_info.log_data, "Connection (%s, %d) uploaded '%s'; %d bytes. ", inet_ntoa(th_data->client_addr.sin_addr), ntohs(th_data->client_addr.sin_port), file_name, fSize);

	pthread_mutex_unlock(&mux);

	/*Acum efectuăm procesarea imaginii. */
	sprintf(transformed_img_file_path, "./Transformed/%s", file_name);

	/*Trebuie să obținem informații din secțiunea critică.*/
	pthread_mutex_lock(&mux);
	int code = serv_info.transformation_mode;
	float rotangle = serv_info.rotation;
	pthread_mutex_unlock(&mux);

	/*Efectuarea transformării. */
	ret = transform_image(path, transformed_img_file_path, code, rotangle);

	if (ret == 0)
	{
		/*Dacă transformarea a avut loc fără erori...*/
		/*Trimiterea unui mesaj de succes către client.*/
		write(cl_sock_fd, success, strlen(success) + 1);

		printf("\n[*] '%s' successfully transformed! \n", file_name);
	}
	else
	{
		printf("\n[!]ERROR occured in the transformation process :( \n");

		/*Trimite un mesaj de eroare către client. */
		write(cl_sock_fd, error, strlen(error) + 1);
		return;
	}

	/*Ne așteptăm ca clientul să trimită un mesaj NEWSIZE. */

	read(cl_sock_fd, msg, 32);

	if (strcmp(msg, "NEWSIZE") != 0)
	{
		/*Dacă răspunsul este incorect, se întrerupe comunicarea. */

		pthread_mutex_lock(&mux);
		printf("[!]Client violated communication protocols. \n   Stopped communicating with them. \n");

		pthread_mutex_unlock(&mux);
		return;
	}

	/*Acum obținem dimensiunea fișierului transformat.  */
	struct stat file_status;
	stat(transformed_img_file_path, &file_status);

	memset(file_size, 0x00, 64);
	/*Scriem dimensiunea în memoria buffer sub forma unui șir de caractere zecimale și o trimitem clientului.  */
	sprintf(file_size, "%lld", file_status.st_size);

	/*Se trimite la client*/
	write(cl_sock_fd, file_size, strlen(file_size) + 1);

	/*Acum așteptăm ca clientul să trimită mesajul "TRANSFER", pentru a iniția transferul de fișiere.*/
	memset(msg, 0x00, 32);
	read(cl_sock_fd, msg, 32);

	if (strcmp(msg, "TRANSFER") != 0)
	{
		pthread_mutex_lock(&mux);
		printf("[!]Client not ready for file transfer. \n   Stopped communicating with them. \n");

		pthread_mutex_unlock(&mux);
		return;
	}

	/*Trimiterea fișierului transformat către client*/

	fp = fopen(transformed_img_file_path, "rb");

	while ((n = fread(buff, 1, 4096, fp)) > 0)
	{
		/*Trimitem clientului tot ceea ce am citit din fișierul transformat.*/
		write(cl_sock_fd, buff, n);
	}

	/********************/
	pthread_mutex_lock(&mux);
	printf("\n[*]Successfully sent transformed image, of size %lld bytes. \n", file_status.st_size);

	/*Am resetat logul serverului.*/
	memset(serv_info.log_data, 0x00, 1024);

	/*Actualizăm jurnalul/log-ul serverului. */
	sprintf(serv_info.log_data, "Transformed image of size %lld bytes, sent to connection (%s, %d). ", file_status.st_size, inet_ntoa(th_data->client_addr.sin_addr), ntohs(th_data->client_addr.sin_port));

	pthread_mutex_unlock(&mux);

	/******************/

	/*Se închide fișierul.  */
	fclose(fp);

	/*Dealocam buffer-ul. */
	free(buff);
}

void handle_controller_client(const THREAD_DATA *th_data)
{
	/*Copiem descriptorul de fișier al socket-ului clientului  */
	int cl_sock_fd = th_data->client_sock;

	/*Buffer pentru a stoca răspunsul clientului. */
	char buff[16];
	memset(buff, 0x00, 16);

	/*Variabilă pentru a stoca valoarea de returnare a funcțiilor. */
	int ret = 0;

	/*Okay mesaj*/
	char *okay = "OK";

	/*Success mesaj! */
	char *success = "SUCCESS";

	/*Error mesaj... */
	char *error = "ERROR";

	/*trimitem mesajul de aprobare către client.  */
	write(cl_sock_fd, okay, strlen(okay) + 1);

	/*În cele din urmă primim comanda de control de la client.  */
	ret = read(cl_sock_fd, buff, 16);
	/*Primul caracter al răspunsului lor constă în codul de transformare (cifră) ca un caracter. Deoarece este un caracter, îl transformăm într-o cifră reală (int)
	*/

	int code = (int)buff[0] - (int)'0';

	/*Verificăm dacă codul este valid, altfel oprim comunicarea și trimitem un mesaj de eroare. */

	if ((code == 0) || (code > 7))
	{
		write(cl_sock_fd, error, strlen(error) + 1);
		return;
	}

	float rotangle = 0.0;
	/*Variabilă pentru a stoca unghiul de rotație pentru modul de rotație. */

	if (code == 3)
	{
		/*Dacă este detectat modul de rotație, trebuie să analizăm și unghiul de rotație. */
		/*În răspuns, clientul plasează acest unghi după un ":"  */

		rotangle = atof(strstr(buff, ":") + 1);
	}

	/*Acum vom modifica setările serverului.
         Deoarece vom accesa secțiunea critică de cod, folosim blocaje mutex pentru a o proteja. 
	 */
	pthread_mutex_lock(&mux);
	serv_info.transformation_mode = code;
	serv_info.rotation = rotangle;

	pthread_mutex_unlock(&mux);

	/*Acum trimitem mesajul de succes,
        pentru a anunța clientul că editarea a fost efectuată cu succes. */

	write(cl_sock_fd, success, strlen(success) + 1);

	pthread_mutex_lock(&mux);
	/*We reset the server log. */
	memset(serv_info.log_data, 0x00, 1024);

	/*Actualizăm jurnalul serverului. */
	sprintf(serv_info.log_data, "Connection (%s, %d) modified server settings. ", inet_ntoa(th_data->client_addr.sin_addr), ntohs(th_data->client_addr.sin_port));

	pthread_mutex_unlock(&mux);
}

void handle_admin_client(const THREAD_DATA *th_data)
{
	/*Copiem descriptorul de fișier al socket-ului clientului. */
	int cl_sock_fd = th_data->client_sock;

	/*Buffer pentru stocarea răspunsului de la clientul administrator. */
	char msg[32];
	memset(msg, 0x00, 32);

	char *okay = "OK";

	/*Trimitem un mesaj de aprobare clientului. */
	write(cl_sock_fd, okay, strlen(okay) + 1);

	/*Continuăm să interogăm diferitele comenzi de la clientul de administrare.*/
	while (1)
	{
		memset(msg, 0x00, 32);

		read(cl_sock_fd, msg, 32);

		if (strcmp(msg, "STOP") == 0)
		{
			/*Clientul administrator dorește ca serverul să nu mai funcționeze.*/
			/*Trimitem un mesaj "The server has stopped" către clientul administrator. */
			write(cl_sock_fd, (void *)"The server has stopped", 4);

			pthread_mutex_lock(&mux);
			printf("\n[*]Administrator client requested shutting down of server!\n   Thank You for using the server! \n");

			/*Încheiem bucla de funcționare a serverului.  */
			serv_info.run = 0;

			/*Se închide socket-ul serverului.*/
			shutdown(serv_info.server_socket_fd, SHUT_RD);
			pthread_mutex_unlock(&mux);

			break;
		}
		else if (strcmp(msg, "LOG") == 0)
		{
			/*Clientul administrator dorește să primească jurnalul actual al serverului.  */

			/*Scriem jurnalul actual pe socket-ul clientului. */
			char log[1024];

			pthread_mutex_lock(&mux);

			memcpy(log, serv_info.log_data, 1024);
			pthread_mutex_unlock(&mux);

			write(cl_sock_fd, log, 1024);
		}
		else if (strcmp(msg, "SESSIONEXIT") == 0)
		{
			/*Se încheie această sesiune. */
			return;
		}
	}
}

/*Funcție care gestionează o conexiune primită.
 Această funcție va fi rutina care va funcționa concomitent cu... 
*/
void *handle_connection(void *data)
{
	/*Transformăm pointerul în pointerul struct THREAD_DATA corespunzător. */
	THREAD_DATA *th_data = (THREAD_DATA *)data;

	/*Folosim pthread_mutex_lock și pthread_mutex_unlock pentru a obține și a elibera blocaje, astfel încât să putem accesa în siguranță secțiunile critice ale codului nostru. 
	*/

	pthread_mutex_lock(&mux);
	printf("\n[*]Connection accepted from client at IP: %s on port: %d\n", inet_ntoa(th_data->client_addr.sin_addr), ntohs(th_data->client_addr.sin_port));

	/*Am resetat jurnalul serverului. */
	memset(serv_info.log_data, 0x00, 1024);

	/*Actualizăm jurnalul serverului. */
	sprintf(serv_info.log_data, "Connection (%s, %d) accepted. ", inet_ntoa(th_data->client_addr.sin_addr), ntohs(th_data->client_addr.sin_port));

	pthread_mutex_unlock(&mux);

	int ret = 0; // Variabilă suport pentru a stoca valoarea de returnare a funcțiilor.

	/*Stocăm identitatea trimisă de client către server în aceast array*/
	char identity[32];
	memset(identity, 0x00, 32);

	/*Citim ceea ce ne spune clientul despre identitatea sa. */
	ret = read(th_data->client_sock, identity, 32);

	if (ret == -1)
	{
		/*S-a produs o eroare la citire... 
		Prin urmare, nu putem servi această conexiune. */
		pthread_mutex_lock(&mux);
		printf("\n[!]Error occured in reading identity string. \n");

		/*Înregistrăm eroarea */
		memset(serv_info.log_data, 0x00, 1024);
		sprintf(serv_info.log_data, "Error communicating with: (%s, %d)", inet_ntoa(th_data->client_addr.sin_addr), ntohs(th_data->client_addr.sin_port));

		pthread_mutex_unlock(&mux);

		/*Eliberăm structura de date a firului deoarece a fost alocată inițial în heap
	 */
		free(th_data);
		return NULL;
	}

	/*Identificăm tipul de client cu care ne conectăm. 
	 * Acest lucru este important, deoarece numai atunci vom putea să îi servim corect.  
	*/

	if (strcmp(identity, "UPLOADER") == 0)
	{
		pthread_mutex_lock(&mux);
		printf("\n[*]Uploader client connected. \n");
		/*Am resetat jurnalul serverului.*/
		memset(serv_info.log_data, 0x00, 1024);

		/*Actualizăm jurnalul serverului. */
		sprintf(serv_info.log_data, "Connection (%s, %d) is UPLOADER client. ", inet_ntoa(th_data->client_addr.sin_addr), ntohs(th_data->client_addr.sin_port));
		pthread_mutex_unlock(&mux);

		/*Se gestionează uploader client */
		handle_uploader_client(th_data);
	}
	else if (strcmp(identity, "CONTROLLER") == 0)
	{
		pthread_mutex_lock(&mux);
		printf("\n[*]Controller client detected. \n");
		/*Am resetat jurnalul serverului.*/
		memset(serv_info.log_data, 0x00, 1024);

		/*Actualizăm jurnalul serverului. */
		sprintf(serv_info.log_data, "Connection (%s, %d) is CONTROLLER client. ", inet_ntoa(th_data->client_addr.sin_addr), ntohs(th_data->client_addr.sin_port));
		pthread_mutex_unlock(&mux);

		/*Se gestionează controller client în modul specificat de acesta. */ handle_controller_client(th_data);
	}
	else if (strcmp(identity, "ADMIN") == 0)
	{
		pthread_mutex_lock(&mux);
		printf("\n[*]Administrator client detected. \n");
		/*Am resetat jurnalul serverului. */
		memset(serv_info.log_data, 0x00, 1024);

		/*Actualizăm jurnalul serverului. */
		sprintf(serv_info.log_data, "Connection (%s, %d) is ADMIN client. ", inet_ntoa(th_data->client_addr.sin_addr), ntohs(th_data->client_addr.sin_port));

		pthread_mutex_unlock(&mux);

		/*Se gestionează administrator client în modul specificat de acesta.*/
		handle_admin_client(th_data);
	}
	else
	{
		pthread_mutex_lock(&mux);
		printf("\n[!]Unsupported client detected. \n   Closing them... \n");
		/*Am resetat jurnalul serverului. */
		memset(serv_info.log_data, 0x00, 1024);

		/*Actualizăm jurnalul serverului. */
		sprintf(serv_info.log_data, "Connection (%s, %d) is UNSUPPORTED ! ", inet_ntoa(th_data->client_addr.sin_addr), ntohs(th_data->client_addr.sin_port));
		pthread_mutex_unlock(&mux);
	}

	/*Se inchide socket*/
	close(th_data->client_sock);

	pthread_mutex_lock(&mux);
	/*Am resetat jurnalul serverului. */
	memset(serv_info.log_data, 0x00, 1024);

	/*Actualizăm jurnalul serverului.  */
	sprintf(serv_info.log_data, "Connection (%s, %d) is disconnected. ", inet_ntoa(th_data->client_addr.sin_addr), ntohs(th_data->client_addr.sin_port));

	pthread_mutex_unlock(&mux);

	/*Eliberăm structura de date a firului deoarece,
	a fost alocată inițial în heap
	 */
	free(th_data);
	return NULL;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~
       Main Entry Point
~~~~~~~~~~~~~~~~~~~~~~~~~~*/
int main(int argc, char *argv[])
{
	printf("***** IMAGE PROCESSING MULTI THREADED SERVER *****\n");

	if (argc < 2)
	{
		/*În cazul în care utilizatorul care pornește aplicația server nu furnizează un număr de port, se informează utilizatorul să încerce din nou să furnizeze numărul de port. */
		printf("Usage: %s [PORT NUMBER]\n\n[!]Please provide port number... \n", argv[0]);
		return 1;
	}

	/*înregistrăm numărul de port furnizat. */
	int port_num = atoi(argv[1]);

	/*Variabilă pentru a stoca numărul de fire de execuție generate. */
	int num = 0;

	/*Array care stochează ID-urile firelor de execuție ale firelor de execuție generate. */
	pthread_t *tids;

	// Alocăm memorie pentru a stoca inițial zece astfel de ID-uri de fire de execuție
	tids = (pthread_t *)malloc(sizeof(pthread_t) * 10);

	// Deoarece am alocat pentru zece astfel de ID-uri de fire de execuție, am setat num_alocat la 10

	int num_allocated = 10;

	/*Înainte de a configura efectiv serverul,
	trebuie să ne inițializăm globalul,
	'serv_info' struct*/
	memset(serv_info.log_data, 0x00, 1024);
	memset(serv_info.filename, 0x00, 1024);
	/*Am setat modul de transformare la valoarea implicită (0), care corespunde la nicio transformare. */
	serv_info.transformation_mode = 0;
	/*Se pornește bucla de servicii a serverului */
	serv_info.run = 1;

	/*Acum vom configura serverul. */

	/* Adresa serverului */
	struct sockaddr_in server_addr;

	/*Inițializăm corect structura la toți octeții zero. */
	memset(&server_addr, 0, sizeof(struct sockaddr_in));

	/*Creăm socket-ul serverului*/
	/*Aici creăm un socket TCP. */
	int serv_sock_fd = socket(AF_INET, SOCK_STREAM, 0);

	/* Verificăm dacă am primit un socket valid*/
	if (serv_sock_fd == -1)
	{
		printf("\n[!]ERROR: Unable to create socket! \n");
		return 2;
	}

	/*Copiem valoarea descriptorului de fișier socket al serverului în variabila echivalentă din structura globală. 
	 * Acest lucru ne va ajuta în gestionarea asincronă în viitor.
	*/
	serv_info.server_socket_fd = serv_sock_fd;

	/*Am configurat informații importante în structura sockaddr_in a serverului. */

	server_addr.sin_family = AF_INET;

	/*Folosim htons() și htonl(), pentru a ne asigura că, indiferent de ceea ce se întâmplă cu
 endianitatea(ordinea sau secvența de octeți ai unui cuvânt de date digitale din memoria computerului.) sistemului  nostru, octeții, sunt în ordinea octeților din rețea. */
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port_num);

	/*Atribuim o adresă socket-ului nostru folosind apelul de sistem bind().*/
	if (bind(serv_sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
	{
		printf("\n[!]ERROR: Unable to assign an address to our socket. \n");
		return 3;
	}

	/*Acum apelăm listen() pentru a configura socket-ul nostru pentru a primi conexiuni primite.*/

	if (listen(serv_sock_fd, 5) == -1)
	{
		printf("\n[!]ERROR: Unable to set up socket for receiving incomming connections... \n");
		return 4;
	}

	/*Se inițializează mutexul, cu atribute implicite. Utilizăm mutexul pentru siguranța firelor. */
	pthread_mutex_init(&mux, NULL);

	/*Intrăm în bucla noastră infinită de serviciu. 
	* În această buclă, serverul nostru va accepta noi conexiuni în creștere,
	și va genera fire pentru a se ocupa de fiecare client nou. 
	*/

	printf("\n[*]Server set up and waiting for connections! \n");

	while (serv_info.run == 1)
	{
		/*Alocăm memorie pentru structura THREAD_DATA, care va conține informații,
		necesare pentru ca noul nostru fir să funcționeze. */
		THREAD_DATA *th_data = (THREAD_DATA *)malloc(sizeof(THREAD_DATA));

		/*În cazul în care există o conexiune de intrare,
		acceptăm conexiunea pentru a obține descriptorul de fișier al acesteia. */

		socklen_t size = sizeof(struct sockaddr_in);
		th_data->client_sock = accept(serv_sock_fd, (struct sockaddr *)&th_data->client_addr, &size);

		/*Trebuie să verificăm dacă accept() a reușit */
		if (th_data->client_sock == -1)
		{
			/* accept() a eșuat. */
			printf("\n[!]ERROR: accept() failed. \n   Skipping... \n");
			free(th_data);
			continue;
		}

		if (num == num_allocated)
		{
			// Trebuie să alocăm mai mult
			tids = (pthread_t *)realloc(tids, num_allocated + 1);

			// Actualizarea numărului de ID-uri de fire alocate
			num_allocated += 1;
		}

		if (pthread_create(&tids[num], NULL, handle_connection, (void *)th_data) != 0)
		{
			/*Am întâmpinat o eroare la generarea firului de discuție. 
			 Așa că vom raporta utilizatorului... 
			*/

			printf("\n[!]ERROR occured in spawning thread... \n   Skipping connection request... \n");
			/*Se închide socket-ul clientului.*/
			close(th_data->client_sock);

			/*Memorie liberă. */
			free(th_data);

			continue;
		}
	}

	/*UNim thread-urile create*/

	for (int i = 0; i < num; i++)
	{
		pthread_join(tids[i], NULL);
	}

	/*Eliberam array-ul */
	free(tids);
	/*Distrugeți mutexul deoarece am ajuns la sfârșitul aplicației noastre de server.*/
	pthread_mutex_destroy(&mux);
	return 0;
}
