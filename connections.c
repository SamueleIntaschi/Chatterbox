/*
 * chatterbox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 * Studente: Samuele Intaschi
 * Matricola: 523864
 * e-mail: sam18195@hotmail.it
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <unistd.h>

#include "config.h"
#include "connections.h"

/**
 *
 * @file  connections.c
 * @author Samuele Intaschi
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore 
 * @brief Contiene l' implementazione delle funzioni che gestiscono la
 *        connessione tra i clients e il server, dichiarate nell' header file
 *        connections.h
 */
 
/**
 * @function openConnection
 * @brief Apre una connessione AF_UNIX verso il server 
 *
 * @param path Path del socket AF_UNIX 
 * @param ntimes numero massimo di tentativi di retry
 * @param secs tempo di attesa tra due retry consecutive
 *
 * @return il descrittore associato alla connessione in caso di successo
 *         -1 in caso di errore
 *
 * Crea un socket AF_UNIX e tenta di connettersi con connect ntimes volte,
 * se ci riesce ritorna il socket creato altrimenti ritorna -1.
 */

int openConnection(char* path, unsigned int ntimes, unsigned int secs){
	int fd;//descrittore di file
	struct sockaddr_un sa;
	strncpy(sa.sun_path, path, UNIX_PATH_MAX);
	sa.sun_family = AF_UNIX;
	int i = 0;
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	while(connect(fd, (struct sockaddr *)&sa, sizeof(sa))==-1 && i<ntimes){
		if (errno == ENOENT) {
			printf("socket %s non esiste\n", sa.sun_path);
			sleep(secs);
		}
		else {
			perror("nella connect");
			return -1;
		}
		i++;
	}
	return fd;
}

//--------- SERVER SIDE ----------

/**
 * @function readHeader
 * @brief Legge l'header del messaggio
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header del messaggio da ricevere
 *
 * @return <=0 se c'e' stato un errore 
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 * 
 * Legge prima il campo op dell' header del messaggio e poi il campo sender,
 * come vengono inviati da sendHeader e li mette nell' header passato come 
 * parametro, infine ritorna 1 se tutto è andato a buon fine o un valore <= 0
 * a seconda dell' errore che c'è stato.
 */
int readHeader(long connfd, message_hdr_t *hdr) {
	int n = 0;
	n = read(connfd, &(hdr->op), sizeof(op_t));
	if (n == 0) return 0;
	if (n < 0) {
		perror("readHeader: nella prima read");
		return -1;
	}
	n = read(connfd, &(hdr->sender), (MAX_NAME_LENGTH + 1));
	if (n == 0) return 0;
	if (n < 0) {
		perror("readHeader: nella seconda read");
		return -1;
	}
	return 1;
}

/**
 * @function readData
 * @brief Legge il body del messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al body del messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 * 
 * Legge il body del messaggio inviato su fd partendo dall' header del body, 
 * cioè i campi receiver (destinatario) e len (lunghezza del messaggio di 
 * testo), così come vengono inviati da sendData oppure da sendRequest.
 * Mette tutto il body del messaggio in quello passato come parametro
 * e ritorna 1 se tutto va bene o un valore <= 0 a seconda dell' errore che
 * c'è stato.
 */
int readData(long fd, message_data_t *data) {
	if (fd < 0) return -1;
	message_data_hdr_t hdr;
	hdr.len = 0;
	//leggo il nome del destinatario
	int n = read(fd, &(hdr.receiver), (MAX_NAME_LENGTH + 1));
	if (n < 0) {
		perror("readData: errore in prima lettura dati");
		return -1;
	}
	if (n == 0) return 0;
	//leggo la lunghezza del messaggio
	n = read(fd, &(hdr.len), sizeof(int)); 
	if (n < 0) {
		perror("readData: errore in seconda lettura dati");
		return -1;
	}
	if (n == 0) return 0;
	data->hdr = hdr;
	//leggo il buffer vero e proprio
	int r = 0;
	char * w;
	r = hdr.len;//(data->hdr).len;
	if (r != 0) {
		data->buf = malloc(r * sizeof(char));
		w = data->buf;
		if (data->buf == NULL) {
			perror("readData: nella malloc");
			exit(EXIT_FAILURE);
		}
	}
	//il messaggio è vuoto ma non è un errore
	if (r == 0) return 1;
	//r è la quantità di bytes che vanno letti
	while (r > 0) {//finchè non li ho letti tutti
		n = read(fd, w, r);
		if (n < 0) {
			perror("readData: terza read");
			return -1;
		}
		//connessione chiusa
		else if (n == 0) {
			return 0;
		}
		//decremento quelli restanti da leggere
		r = r - n;
		//incremento quelli letti
		w = w + n;
	}
	return 1;
}

/**
 * @function readMsg
 * @brief Legge l'intero messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 *
 * Legge un intero messaggio da fd nell' ordine in cui ne vengono spedite 
 * le varie parti da sendRequest, e cioè prima parte header con op e sender, 
 * poi parte data con receiver, len e buf.
 * Mette tutte le parti del messaggio nelle rispettive di quello passsato
 * come parametro e ritorna 1 se tutto va a buon fine, un valore <=0 a seconda
 * dell' errore ricevuto.
 */
int readMsg(long fd, message_t *msg){
	if ((fd < 0)) return -1;
	int dim = 0;
	int n = 0;
	dim = sizeof(op_t);
	n = read(fd, &(msg->hdr).op, dim);
	if (n < 0) {
		perror("readMsg: prima read");
		return -1;
	}
	if (n == 0) return 0;
	dim = MAX_NAME_LENGTH + 1;
	n = read(fd, &(msg->hdr).sender, dim);
	if (n < 0) {
		perror("readMsg: seconda read");
		return -1;
	}
	if (n == 0) return 0;
	dim = MAX_NAME_LENGTH + 1;
	n = read(fd, &((msg->data).hdr).receiver, dim);
	if (n < 0) {
		perror("readMsg: terza read");
		return -1;
	}
	if (n == 0) return 0;
	dim = sizeof(unsigned int);
	n = read(fd, &((msg->data).hdr).len , dim);
	if (n < 0) {
		perror("readMsg: quarta read");
		return -1;
	}
	if (n == 0) return 0;
	int r = 0;
	r = msg->data.hdr.len;
	char * w = NULL;
	if (r != 0){
	 	(msg->data).buf = malloc(r * sizeof(char));
		w = (msg->data).buf;
	}	
	else return 1;
	while (r > 0) {
		n = read(fd, w, r);
		if (n < 0) {
			perror("readMsg: quinta read");
			return -1;
		}
		if (n == 0) return 0;
		r = r - n;
		w = (char *)(r + n);
	}
	return 1;
}


//--------- CLIENT SIDE ----------

/**
 * @function sendRequest
 * @brief Invia un messaggio di richiesta al server 
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 * 
 * Invia un intero messaggio su fd.
 * L' ordine in cui le varie parti sono inviate è il seguente: 
 * prima si invia la parte header di cui prima op e poi sender, 
 * poi si invia anche le parte dati di cui prima la parte header
 * (prima receiver e poi len) e poi il buffer che contiene il messaggio
 * di testo.
 * Viene utilizzata anche dal server per inviare messaggi al client quando
 * questo ne fa richiesta.
 * Ritorna -1 se ci sono stati errori.
 */
int sendRequest(long fd, message_t *msg) {
	//non invio niente se il messaggio è NULL
	if ((msg == NULL) || (fd < 0)) return -1;
	//intanto invio l' header che contiene le info di op e sender
	int dim = 0;
	dim = sizeof(op_t);
	//invio dell' operazione
	int n = 0;
	n = write(fd, &(msg->hdr).op, dim);
	if (n < 0) {
		perror("sendRequest: nella prima write");
		return -1;
	}
	//invio il mittente del messaggio
	dim = MAX_NAME_LENGTH + 1;
	n = write(fd, &(msg->hdr).sender, dim);
	if (n < 0) {
		perror("sendRequest: nella seconda write");
		return -1;
	}
	dim = MAX_NAME_LENGTH + 1;
	//invio del destinatario
	n = write(fd, &((msg->data).hdr).receiver, dim);
	if (n < 0) {
		perror("sendRequest: nella terza write");
		return -1;
	}
	dim = sizeof(unsigned int);
    n = write(fd, &((msg->data).hdr).len, dim);
    if (n < 0) {
    	perror("sendRequest: nella quarta write");
    	return -1;
    }
	//printf("sendRequest: %s invia messaggio -> %s\n", msg->hdr.sender, (msg->data).buf);
	int scrittura = ((msg->data).hdr).len;
	char * str = (msg->data).buf;
	int s = 0; 
	int w = 0;
	//se non ho niente da scrivere non scrivo altrimenti bloccherei
    if(scrittura == 0 || (msg->data).buf == NULL) return 0;
    //scrivo fino ad essere sicuro di aver scritto tutto
    while(scrittura > 0 && str != NULL){
    	w = write(fd, str, scrittura);
    	if (w < 0) {
    		perror("sendRequest: nella quinta write");
    		return -1;
    	}
        scrittura = scrittura - w;
        str = str + w;
        s = s + w;
    }
	return 0;
}

/**
 * @function sendData
 * @brief Invia il body del messaggio al server
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 * 
 * Invia la parte dati di un messaggio su fd nel seguente ordine:
 * prima invia la parte header di cui prima len e poi receiver,
 * poi invia il buffer contenente il messaggio testuale.
 * E' utilizzata anche dal server quando deve inviare ad esempio la
 * lista utenti oppure un file richiesto con GETFILE_OP.
 * Ritorna -1 se ci sono stati errori.
 */
int sendData(long fd, message_data_t * msg) {
	if ((msg == NULL) || (fd < 0)) {
		printf("errore sendData\n");
		return -1;
	}
	int n = 0;
	n = write(fd, &((msg->hdr).receiver), (MAX_NAME_LENGTH + 1));
	if (n < 0) {
		perror("sendData: nella prima write");
		return -1;
	}
	n = write(fd, &((msg->hdr).len), sizeof(int));
	if (n < 0) {
		perror("sendData: nella seconda write");
		return -1;
	}
	int da_scrivere = (msg->hdr).len;
	char * str = msg->buf;
	int w = 0;
	//se non ho niente da scrivere non scrivo altrimenti bloccherei
    if(da_scrivere == 0 || msg->buf == NULL) return 0;
    //scrivo fino ad essere sicuro di aver scritto tutto
    while(da_scrivere > 0 && str != NULL){
    	w = write(fd, str, da_scrivere);
    	if (n < 0) {
    		perror("sendData: nella quinta write");
    		return -1;
    	}
    	//decremento quelli da scrivere
        da_scrivere = da_scrivere - w;
        //incremento quelli scritti
        str = str + w;
    }
	return 0;
}

/**
 * @function sendHeader
 * @brief Invia l' header del messaggio
 * 
 * @param fd     descrittore della connessione
 * @param msg    puntatore all' header del messaggio da inviare
 *
 * @return <=0 se c' e' stato un errore
 *
 * Invia su fd la parte header di un messaggio partendo dal campo op e 
 * proseguendo con il campo sender. Ritorna -1 se c'è un errore
 * oppure 1 se tutto va a buon fine.
 */
int sendHeader(long fd, message_hdr_t * hdr) {
	int n = 0;
	n = write(fd, &(hdr->op), sizeof(op_t));
	if (n < 0) {
		perror("sendHeader: nella prima write\n");
		return -1;
	}
	n = write(fd, &(hdr->sender), (MAX_NAME_LENGTH + 1));
	if (n < 0) {
		perror("sendHeader; nella seconda write");
		return -1;
	}
	else return 1;
}


