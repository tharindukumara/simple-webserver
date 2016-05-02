#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>


#define SERVER_STRING "Server: tharindu/0.1.0\r\n"

void accept_request(int);
void bad_request(int);
void cat(int, FILE *, char *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char *, char *);
void not_found(int);
void serve_file(int, const char *, char *);
int startup(u_short *);
void unimplemented(int);
void execute_php(char* , int);
void unsupported_media(int);
int get_file_size(int);


typedef struct {
	char *ext;
	char *mediatype;
} extn;

//Possible media types
extn extensions[] ={
	{"gif", "image/gif" },
	{"txt", "text/plain" },
	{"jpg", "image/jpg" },
	{"jpeg","image/jpeg"},
	{"png", "image/png" },
	{"ico", "image/ico" },
	{"zip", "image/zip" },
	{"gz",  "image/gz"  },
	{"tar", "image/tar" },
	{"htm", "text/html" },
	{"html","text/html" },
	{"php", "text/html" },
	{"pdf","application/pdf"},
	{"zip","application/octet-stream"},
	{"rar","application/octet-stream"},
	{0,0} 
};



void execute_php(char* script_path, int fd) {
	
	dup2(fd, STDOUT_FILENO);
	char script[500];
	strcpy(script, "SCRIPT_FILENAME=");
	strcat(script, script_path);
	putenv("GATEWAY_INTERFACE=CGI/1.1");
	putenv(script);
	putenv("QUERY_STRING=");
	putenv("REQUEST_METHOD=GET");
	putenv("REDIRECT_STATUS=true");
	putenv("SERVER_PROTOCOL=HTTP/1.1");
	putenv("REMOTE_HOST=127.0.0.1");
	execl("/usr/bin/php", "/usr/bin/php", "-q", script_path, (char *) NULL);
	
}

void accept_request(int client)
{
	char buf[1024];
	int numchars;
	char method[255];
	char url[255];
	char path[512];
	size_t i, j;
	struct stat st;
	int php = 0;      /* becomes true if server decides this is a CGI
					* program */
	char *query_string = NULL;

	numchars = get_line(client, buf, sizeof(buf));
	i = 0; j = 0;
	while (!isspace(buf[j]) && (i < sizeof(method) - 1))
	{
		method[i] = buf[j];
		i++; j++;
	}
	method[i] = '\0';

	if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
	{
		unimplemented(client);
		return;
	}

	if (strcasecmp(method, "POST") == 0)
		php = 1;

	i = 0;
	while (isspace(buf[j]) && (j < sizeof(buf)))
		j++;
		
	while (!isspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
	{
		url[i] = buf[j];
		i++; j++;
	}
	url[i] = '\0';
	printf("%s\n", buf);
	if (strcasecmp(method, "GET") == 0)
	{
		query_string = url;
		while ((*query_string != '?') && (*query_string != '\0'))
		query_string++;
		if (*query_string == '?')
		{
			php = 1;
			*query_string = '\0';
			query_string++;
		}
	}

	sprintf(path, "htdocs%s", url);
	if (path[strlen(path) - 1] == '/')
		strcat(path, "index.html");
	
	
	if (stat(path, &st) == -1) {
		while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
			numchars = get_line(client, buf, sizeof(buf));
		not_found(client);
	}
	else
	{
		printf("Path: %s\n", path);
		printf("URL: %s\n", url);
		
		char* type = strchr(path, '.');
		int i;
		
		for (i = 0; extensions[i].ext != NULL; i++) {
			if (strcmp(type + 1, extensions[i].ext) == 0) {
				if (strcmp(extensions[i].ext, "php") == 0) {
					php = 1;
				}
				break;
			}
			
		}
		
		if (!php){
			printf("%s\n", "Normal file to process");
			serve_file(client, path, extensions[i].mediatype);
		}
		else{
			printf("%s\n", "PHP file to process");
			serve_file(client, path, extensions[i].mediatype);
			execute_php(path, client);
		}
	}

	close(client);
}

void cat(int client, FILE *resource, char *type)
{
	
	if(strcmp(type, "text/html") == 0){
		char buf[1024];

		fgets(buf, sizeof(buf), resource);
		while (!feof(resource))
		{
			send(client, buf, strlen(buf), 0);
			fgets(buf, sizeof(buf), resource);
		}
		
	}else{
		printf("%s\n", type);
		char ch;
		unsigned long fileLen;
		unsigned char *buffer;

		fseek(resource, 0, SEEK_END);
		fileLen = ftell(resource);
		fseek(resource, 0, SEEK_SET);
		buffer = (char *)malloc(fileLen);
		fread(buffer,fileLen,sizeof(unsigned char),resource);

		write(client, buffer, fileLen);
	}
	
}

int get_line(int sock, char *buf, int size)
{
	int i = 0;
	char c = '\0';
	int n;

	while ((i < size - 1) && (c != '\n'))
	{
		n = recv(sock, &c, 1, 0);
		if (n > 0)
		{
			if (c == '\r')
			{
				n = recv(sock, &c, 1, MSG_PEEK);
				
				if ((n > 0) && (c == '\n'))
					recv(sock, &c, 1, 0);
			else
				c = '\n';
			}
			buf[i] = c;
			i++;
		}
		else
		c = '\n';
	}
	buf[i] = '\0';

	return(i);
}

void headers(int client, const char *filename, char *type)
{
	char buf[1024];
	(void)filename;  /* could use filename to determine file type */

	strcpy(buf, "HTTP/1.0 200 OK\r\n");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, SERVER_STRING);
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: %s\r\n", type);
	send(client, buf, strlen(buf), 0);
	strcpy(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
}


void serve_file(int client, const char *filename, char *type)
{
	FILE *resource = NULL;
	int numchars = 1;
	char buf[1024];

	buf[0] = 'A'; buf[1] = '\0';
	while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
		numchars = get_line(client, buf, sizeof(buf));

	resource = fopen(filename, "r");
	if (resource == NULL)
		not_found(client);
	else
	{
		headers(client, filename, type);
		cat(client, resource, type);
	}
	fclose(resource);
}

void not_found(int client)
{
	char buf[1024];

	sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, SERVER_STRING);
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<html><title>Not Found</title>\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<body><h1>File Not Found!!!</h1><p>The server could not fulfill\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "your request because the resource specified\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "is unavailable or nonexistent.\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</body></html>\r\n");
	send(client, buf, strlen(buf), 0);
}


void unimplemented(int client)
{
	char buf[1024];

	sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, SERVER_STRING);
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<html><head><title>Method Not Implemented\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</title></head>\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<body><p>HTTP request method not supported.\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</body></html>\r\n");
	send(client, buf, strlen(buf), 0);
}

void error_die(const char *sc)
{
	perror(sc);
	exit(1);
}

int startup(u_short *port)
{
	int httpd = 0;
	
	struct sockaddr_in name;

	httpd = socket(PF_INET, SOCK_STREAM, 0);
	if (httpd == -1)
		error_die("socket");
		
	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;
	name.sin_port = htons(*port);
	name.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
		error_die("bind");

	if (listen(httpd, 5) < 0)
	error_die("listen");
	
	return(httpd);
}

int main(void)
{
	int server_sock = -1;
	u_short port = 2222;
	int pid = 0;
	int client_sock = -1;
	struct sockaddr_in client_name;
	int client_name_len = sizeof(client_name);
	pthread_t newthread;

	server_sock = startup(&port);
	printf("webserver running on port %d\n", port);

	while (1)
	{
		client_sock = accept(server_sock,
						   (struct sockaddr *)&client_name,
						   &client_name_len);
		if (client_sock == -1)
			error_die("accept");
		
		pid = fork();
		if (pid < 0)
			error("ERROR on fork");
		if (pid == 0) {
			close(server_sock);
			accept_request(client_sock);
			exit(0);
		} else
			close(client_sock);
		
	}

	close(server_sock);

	return(0);
}
