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
 
/**
 * @file chatty.c
 * @author Samuele Intaschi
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore 
 * @brief File principale del server chatterbox
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <locale.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h> 
#include <sys/mman.h>
#include <sys/time.h>
#include <getopt.h>
#include <fcntl.h>

//miei include
#include "connections.h"
#include "ops.h"
#include "config.h"
#include "icl_hash.h"
#include "message.h"
#include "stats.h"
#include "mutex.h"
#include "history.h"
#include "codafd.h"
#include "configurazione.h"
#include "group_hash.h"

/*----- STRUTTURE DATI -----*/

//struttura dati contenente le statistiche
struct statistics  chattyStats = { 0,0,0,0,0,0,0 };

//tabella hash degli utenti
icl_hash_t * reg_users;

//tabella hash dei gruppi
group_hash_t * reg_groups;

//coda globale dei file descriptor
coda head = NULL;

//struttura dati contenente le informazioni prese dal file di configurazione
configurazione conf;

/*----- VARIABILI DI MUTUA ESCLUSIONE -----*/

//variabili per sincronizzazione
pthread_cond_t condcoda = PTHREAD_COND_INITIALIZER;
//li uso per accedere alla coda globale
pthread_mutex_t mtxcoda = PTHREAD_MUTEX_INITIALIZER;
//lo uso per accedere alle statistiche
pthread_mutex_t mtxstats = PTHREAD_MUTEX_INITIALIZER;
//li uso per accedere alla posizione dell' array utenti
pthread_mutex_t mtx[NMTX] = PTHREAD_MUTEX_INITIALIZER;
//li uso per accedere ai file
pthread_mutex_t mtxfiles = PTHREAD_MUTEX_INITIALIZER;
//mutex per accedere ai gruppi
pthread_mutex_t mtxgroups = PTHREAD_MUTEX_INITIALIZER;

/*----- VARIABILI GLOBALI -----*/

//variabile che indica se è arrivato un segnale di terminazione
volatile sig_atomic_t segnale_arrivato = 0;

//varaibile che indica che l' allarme è arrivato, cioè il timer è scaduto 
volatile sig_atomic_t alrm = 1;

/*----- SPIEGAZIONE UTILIZZO -----*/

/**
 * @function usage
 * @brief    in caso di chiamamata del main con parametri sbagliati, suggerisce
 *           come va fatta la chiamata
 *
 * @param    nome del programma
 */
static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}

/*----- GESTORE DEI SEGNALI -----*/
/**
 * @function signal_handler
 * @brief    gestisce i segnali ricevuti dal server, usata dal thread
 *           gestore dei segnali
 *
 * Si mette in attesa finchè non arrivano alcuni segnali e all' arrivo 
 * esegue l' azione prevista
 */
void * signal_handler(void * arg) {
	int signal;
	int n;
	//la signal mask viene passata come argomento dal main alla creazione
	sigset_t * sigtmp = (sigset_t *)arg;
	sigset_t sigset = *sigtmp;
	//ciclo finchè non arriva un segnale di terminazione
	while (segnale_arrivato == 0) {
		//attendo un segnale
		n = sigwait(&sigset, &signal);
		if (n != 0) {
			errno = n;
			perror("nella sigwait");
			exit(EXIT_FAILURE);
		}
		//se riceve un segnale di terminazione setta la variabile, sveglia 
		//tutti i thread e poi termina
		if ((signal == SIGINT) || (signal == SIGQUIT) || (signal == SIGTERM)) {
    		segnale_arrivato = 1;
    		//sveglio tutti i thread sospesi per farli terminare
    		pthread_cond_broadcast_m(&condcoda);
    	}
    	//in questo caso stampa le statistiche nel file
		if (signal == SIGUSR1) {
		    FILE * file;
		    //apro il file
		    SYSCALLN(file, fopen(conf.StatFileName, "a"), "nella open statistiche");
		    //stampo statistiche nel file
		    printStats(file);
		    //chiudo il file
		    fclose(file);
		}
		//se riceve un segnale di allarme setta la variabile testata dal main
		if (signal == SIGALRM) {
			alrm = 1;	
		}
    }
    pthread_exit(NULL);
}

/*----- FUNZIONI DI UTILITA' -----*/

/**
 * @function calcola_mutex
 * @brief    calcola quale mutex deve prendere un utente a partire dal nome
 *
 * @param name il nome dell' utente di cui devo calcolare quale mutex usare
 *
 * @return il mutex così calcolato
 *
 * Utilizzando la funzione hash della tabella calcola il valore a partire
 * dal nome dell' utente e lo restituisce diviso per quattro, dato che i semafori
 * usati sono un quarto del numero di caselle della tabella hash, cioè un
 * semaforo viene usato per accedere a quattro caselle della tabella
 */
int calcola_mutex(char * name) {
	int val = (* reg_users->hash_function)(name) % reg_users->nbuckets;
	int n = val / 4;
	return n;
}


/**
 * @function user_online_list
 * @brief    crea la lista degli utenti connessi e la spedisce
 *
 * @param fds descrittore di connessione su cui spedire la lista
 *
 * Scorre la tabella hash mettendo in una lista tutti quelli che risultano
 * connessi e poi la invia sul descrittore ricevuto come parametro
 */
void user_online_list(long fds, char * name) {
	icl_entry_t * curr;
	icl_entry_t * user;
	int i = 0;
	int p = 0;
	int len = 0;
	int size = MAX_NAME_LENGTH + 1;
	int size2 = conf.MaxConnections * size * sizeof(char);
	//alloco subito per tutti gli utenti registrabili
	char * lista = (char *)malloc(size2);
	if (lista == NULL) {
		perror("errore malloc");
		exit(EXIT_FAILURE);
	}
	//inizializzazione della lista
	memset(lista, 0, size2);
	for (i=0; i<reg_users->nbuckets; i++) {
		user = reg_users->buckets[i];
		for (curr=user; curr!=NULL; curr=curr->next) {
			if (curr->data != NULL) {
		        if (curr->connesso == 1) {
		        	//copio il nome di un utente nella lista e sposto il puntatore
		        	//avanti di MAX_NAME_LENGTH posizioni per inserire il successivo
		        	strncpy(&lista[p], curr->data, (MAX_NAME_LENGTH + 1));
		        	p = p + MAX_NAME_LENGTH + 1;
		        	//incremento di 1 la lunghezza, cioè il numero di utenti connessi
		        	len++;
				}
			}
		}
	}
	//creo lunghezza vera e propria
	len = len * size;
	//invio lista utenti
	message_data_t msg;
	setData(&msg, name, lista, len);
	sendData(fds, &msg);
	//libero memoria occupata dalla lista
	free(msg.buf);
}


/**
 * @function correggi_filename
 * @brief    aggiunge il percorso predefinito ad un nome di file oggetto
 *           di una richiesta del client
 *
 * @param    filename il nome del file senza percorso
 * @param    filepathname il puntatore all' area che conterrà il percorso 
 *           completo del file, che al momento contiene solo il path relativo
 *           alla cartella dove si trova o troverà il file
 * @param    dimfilename la dimensione del nome del file senza percorso
 */
char * correggi_filename(char * filename, char * filepathname, int dimfilename) {
	char * var;
	char * vartemp;
	//creo una stringa temporanea per non modificare il nome del file originale
	int dim = strlen(filename)*sizeof(char) + 1;
	char * tmp = malloc(dim * sizeof(char));
	if (tmp == NULL) {
		perror("nella malloc");
		exit(EXIT_FAILURE);
	}
	strncpy(tmp, filename, dim);
	//tronco fino al primo /
	var = strtok(tmp, "/");
	if (var != NULL) {
		//scorro fino all' ultima sottostringa dopo / che contiene il nome
		//del file senza il percorso
		while(var != NULL) {
			vartemp = var;
			var = strtok(NULL, "/");
		}
		//concateno il path finale, che adesso contiene solo il path della
		//cartella, prima con uno / e poi con il nome del file formando il path
		//finale che mi serve
		strncat(filepathname, "/", 1);
  		strncat(filepathname, vartemp, dimfilename);
  	}
  	//se sono già al nome cercato evito di scorrere la stringa
  	else {
  		strncat(filepathname, "/", 1);
  		strncat(filepathname, tmp, dimfilename);
  	}
  	//libero memoria occupata dalla la stringa temporanea
  	free(tmp);
  	return filepathname;
}


/*----- PROCEDURA DI GESTIONE RICHIESTE -----*/
/**
 * @function requestreply
 * @brief    gestisce l' operazione contenuta in un messaggio
 *
 * @param fdc il descrittore usato dall' utente
 * @param msg il messaggio contenente operazione da eseguire e dati
 *
 * Riceve un messaggio e, testando il campo op dell' header del messaggio, decide
 * quale richiesta gestire
 */
static void requestreply(long fdc, message_t * msg) {
	//nome del mittente
	char * sname = msg->hdr.sender;
	//nome del ricevitore
	char * rname = msg->data.hdr.receiver;
	//variabili per risultati temporanei 
	group_entry * group;
	op_t op = msg->hdr.op;
	message_t message;
	//user che fa la richiesta
	icl_entry_t * user;
	//user a cui spedirò qualcosa
	icl_entry_t * ruser;
	//variabili usate per decidere quale semaforo usare
	int mutex = 0;
	int mutex2 = 0;
	//indice per i cicli
	int i = 0;
	//variabile usata per trovare l' fd del ricevente
	int f = 0;
	
//--------------------------------------------------------------------------
	
	if (op == REGISTER_OP) {
		
		//calcolo quale semaforo usare
		mutex = calcola_mutex(sname);
        pthread_mutex_lock_m(&mtx[mutex]);
        //inserisco l' user nella tabella hash
        if (icl_hash_insert(reg_users, fdc, sname) != NULL) {
	        //setto l'header con opok
	        setHeader(&(msg->hdr), OP_OK, "server");
	        //invio l' header tutto intero
	        sendHeader(fdc, &(msg->hdr));
	        //invio lista utenti
	        user_online_list(fdc, sname);
	        //esco dalla mutua esclusione
        	pthread_mutex_unlock_m(&mtx[mutex]);
        	
	        //in mutua esclusione modifico le statistiche
	        pthread_mutex_lock_m(&mtxstats);
	        chattyStats.nusers++;
	        chattyStats.nonline++;
	        pthread_mutex_unlock_m(&mtxstats);
	    }
	    //se è già presente invio errore
        else {
            //gestione del fallimento della registrazione
            //creo messaggio con operazione fallita e lo invio al mittente
            setHeader(&(msg->hdr), OP_NICK_ALREADY, "server");
	        sendHeader(fdc, &(msg->hdr));
            pthread_mutex_unlock_m(&mtx[mutex]);
            
            //aggiorno statistiche
            pthread_mutex_lock_m(&mtxstats);
  			chattyStats.nerrors++;
  			pthread_mutex_unlock_m(&mtxstats);
	    }
	}
	
//------------------------------------------------------------------------------	

	else if (op == CONNECT_OP) {
		
		//calcolo quale semaforo usare
		mutex = calcola_mutex(sname);
		pthread_mutex_lock_m(&mtx[mutex]);
		//controllo se è registrato
		user = icl_hash_find(reg_users, sname);
		//se l' user è registrato
		if (user != NULL) {
			//se l' user non è connesso
			if (user->connesso != 1){
				//connetto l' user
				user->connesso = 1;
				user->fd = fdc;
				//invio ack
				setHeader(&msg->hdr, OP_OK, "server");
				sendHeader(fdc, &msg->hdr);
				//invio lista utenti
				user_online_list(fdc, sname);
				pthread_mutex_unlock_m(&mtx[mutex]);
				
				//aggiornamento statistiche
				pthread_mutex_lock_m(&mtxstats);
				chattyStats.nonline++;
				pthread_mutex_unlock_m(&mtxstats);
			}
			//utente già connesso
			else {
				//aggiorno fd perchè poi la comunicazione avviene su quello nuovo
				user->fd = fdc;
				//invio ack
				setHeader(&msg->hdr, OP_OK, "server");
				sendHeader(fdc, &msg->hdr);
				//invio lista utenti
				user_online_list(fdc, sname);
				pthread_mutex_unlock_m(&mtx[mutex]);
				//non aggiorno statistiche perchè l' utente figurava già connesso
			}
		}
		//se non è registrato invio errore
		else {
			//invio ack
			setHeader(&msg->hdr, OP_NICK_UNKNOWN, "server");
			sendHeader(fdc, &msg->hdr);
			pthread_mutex_unlock_m(&mtx[mutex]);
			
			//aggiornamento statistiche
			pthread_mutex_lock_m(&mtxstats);
  			chattyStats.nerrors++;
  			pthread_mutex_unlock_m(&mtxstats);
		}
	}
	
//------------------------------------------------------------------------------	

  	else if (op == USRLIST_OP) {
  		
  		//calcolo quale semaforo usare
  		mutex = calcola_mutex(sname);
		pthread_mutex_lock_m(&mtx[mutex]);
		//controllo se è registrato
		user = icl_hash_find(reg_users, sname);
		if (user != NULL) {
			//controllo se è connesso (deve esserlo visto che il client
			//richiede che la prima opzione sia la connessione)
			if (user->connesso == 1) {
				//invio ack e lista utenti
				setHeader(&msg->hdr, OP_OK, "server");
				sendHeader(fdc, &msg->hdr);
				user_online_list(fdc, sname);
				pthread_mutex_unlock_m(&mtx[mutex]);
			}
			else {
				//invio ack
				setHeader(&msg->hdr, OP_FAIL, "server");
				sendHeader(fdc, &msg->hdr);
				pthread_mutex_unlock_m(&mtx[mutex]);
				
				//aggiornamento statistiche
				pthread_mutex_lock_m(&mtxstats);
				chattyStats.nerrors++;
				pthread_mutex_unlock_m(&mtxstats);
			}
		}
		//se non è registrato invio errore
		else {
			//invio ack
			setHeader(&msg->hdr, OP_NICK_UNKNOWN, "server");
			sendHeader(fdc, &msg->hdr);
			pthread_mutex_unlock_m(&mtx[mutex]);
			
			//aggiornamento statistiche
			pthread_mutex_lock_m(&mtxstats);
  			chattyStats.nerrors++;
  			pthread_mutex_unlock_m(&mtxstats);
		}
	}
  
//------------------------------------------------------------------------------

  	else if (op == DISCONNECT_OP) {
  		
  		mutex =  calcola_mutex(sname);
  		pthread_mutex_lock_m(&mtx[mutex]);
  		//controllo se è registrato
  		user = icl_hash_find(reg_users, sname);
  		//se l' user non esiste 
  		if (user == NULL) {
  			//invio ack
  			setHeader(&message.hdr, OP_NICK_UNKNOWN, "server");
  			sendHeader(fdc, &message.hdr);
  			pthread_mutex_unlock_m(&mtx[mutex]);
  			
  			//aggiornamento statistiche
  			pthread_mutex_lock_m(&mtxstats);
  			chattyStats.nerrors++;
  			pthread_mutex_unlock_m(&mtxstats);
  		}
  		//se l' user esiste
  		else {
  			//se non è connesso invio errore
  			if (user->connesso == 0) {
  				//invio ack
  				setHeader(&message.hdr, OP_FAIL, "server");
  				sendHeader(fdc, &message.hdr);
  				pthread_mutex_unlock_m(&mtx[mutex]);
  				
  				//aggiornamento statistiche
  				pthread_mutex_lock_m(&mtxstats);
  				chattyStats.nerrors++;
  				pthread_mutex_unlock_m(&mtxstats);
  			}
  			//se l' user è connesso lo disconnetto
  			else {  	  	  	  
  				user->connesso = 0;
  				user->fd = -1;
  				//invio ack
  				setHeader(&message.hdr, OP_OK, sname);
  				sendHeader(fdc, &message.hdr);
  				pthread_mutex_unlock_m(&mtx[mutex]);
  				
  				//aggiornamento statistiche
  				pthread_mutex_lock_m(&mtxstats);
  				chattyStats.nonline--;
  				pthread_mutex_unlock_m(&mtxstats);
  			}
  		}
  	}
  	
//------------------------------------------------------------------------------  	

  	else if (op == UNREGISTER_OP) {
  		
  		//calcolo quale semaforo usare
  		mutex = calcola_mutex(sname);
  		pthread_mutex_lock_m(&mtx[mutex]);
  		//controllo se è registrato l' user che chiede di deregistrare
  		user = icl_hash_find(reg_users, sname);
  		//se non lo è invio errore
  		if (user == NULL) {
  			//invio ack
  			setHeader(&msg->hdr, OP_NICK_UNKNOWN, "server");
  			sendHeader(fdc, &msg->hdr);
  			pthread_mutex_unlock_m(&mtx[mutex]);  
			
  			//aggiornamento statistiche
  			pthread_mutex_lock_m(&mtxstats);
  			chattyStats.nerrors++;
  			pthread_mutex_unlock_m(&mtxstats);
  		}
  		else {
  			pthread_mutex_unlock_m(&mtx[mutex]);
  			//calcolo quale semaforo usare per l' utente da deregistrare
  			//che può essere anche un gruppo
  			mutex = calcola_mutex(rname);
  			pthread_mutex_lock_m(&mtx[mutex]);
  			//controllo se l' utente da deregistrare è registrato
  			ruser = icl_hash_find(reg_users, rname);
  			if (ruser != NULL) {
                //se è un utente controllo se è lo stesso che fa la richiesta
                if (strcmp(sname, rname) == 0) {
                	//lo disconnetto
                	user->connesso = 0;
                	user->fd = -1;
                	//libero la memoria che occupava nella tabella hash
                	icl_hash_delete(reg_users, rname);
                	pthread_mutex_unlock_m(&mtx[mutex]);
                	pthread_mutex_lock_m(&mtxgroups);
                	//lo elimino da tutti i gruppi
                	remove_user_from_groups(reg_groups, rname);
                	pthread_mutex_unlock(&mtxgroups);
                	//invio ack
                	setHeader(&msg->hdr, OP_OK, "server");
                	sendHeader(fdc, &msg->hdr);
  				
                	//aggiornamento statistiche
                	pthread_mutex_lock_m(&mtxstats);
                	chattyStats.nusers--;
                	chattyStats.nonline--;
                	pthread_mutex_unlock_m(&mtxstats);
                }
                //se non lo è invio errore
                else {
                	//invio errore
                	setHeader(&msg->hdr, OP_FAIL, "server");
                	sendHeader(fdc, &msg->hdr);
                	pthread_mutex_unlock_m(&mtx[mutex]);
                	
                	//aggiornamento statistiche
  					pthread_mutex_lock_m(&mtxstats);
  					chattyStats.nerrors++;
  					pthread_mutex_unlock_m(&mtxstats);
  				}
  			}
  			//se non era un utente registrato potrebbe essere un gruppo
  			else {
  				pthread_mutex_unlock_m(&mtx[mutex]);
  				//controllo se è un gruppo a dover essere deregistrato
  				pthread_mutex_lock_m(&mtxgroups);
  				group = group_hash_find(reg_groups, rname);
  				if (group != NULL) {
  					//controllo se l' utente che lo richiede è il creatore (admin)
  					//e in quel caso lo elimino
  					if (strcmp(group->admin, sname)) {
  						//libero la memoria che occupava nella tabella hash
  						group_hash_delete(reg_groups, rname);
  						//setto l'header con opok
  						setHeader(&(msg->hdr), OP_OK, "server");
  						//invio l' header tutto intero
  						sendHeader(fdc, &(msg->hdr));
  						pthread_mutex_unlock_m(&mtxgroups);
  					}
  					//se non è nemmeno un gruppo invio l' errore
  					else {
  						//invio ack
  						setHeader(&(msg->hdr), OP_FAIL, "server");
  						sendHeader(fdc, &(msg->hdr));
  						pthread_mutex_unlock_m(&mtxgroups);
  						
  						//aggiornamento statistiche
  						pthread_mutex_lock_m(&mtxstats);
  						chattyStats.nerrors++;
  						pthread_mutex_unlock_m(&mtxstats);
  					}
  				}
  				//se l' utente che ha richiesto una deregistrazione non
  				//è registrato invio un errore
  				else {
  					//invio ack
  					setHeader(&(msg->hdr), OP_FAIL, "server");
  					sendHeader(fdc, &(msg->hdr));
  					pthread_mutex_unlock_m(&mtxgroups);
  				
  					//aggiornamento statistiche
  					pthread_mutex_lock_m(&mtxstats);
  					chattyStats.nerrors++;
  					pthread_mutex_unlock_m(&mtxstats);
  				}
  			}
  		}
  	}
  	
//------------------------------------------------------------------------------  	

  	else if (op == POSTTXT_OP) {
  		
  		//se il messaggio è troppo lungo invio subito un messaggio
  		//di operazione fallita
  		if (msg->data.hdr.len > conf.MaxMsgSize) {
  			//invio ack
  			setHeader(&msg->hdr, OP_MSG_TOOLONG, "server");
  			sendHeader(fdc, &msg->hdr);
  			
  			//aggiornamento statistiche
  			pthread_mutex_lock_m(&mtxstats);
  			chattyStats.nerrors++;
  			pthread_mutex_unlock_m(&mtxstats);
  			
  			free(msg->data.buf);
  			msg->data.hdr.len = 0;
  		}
  		else {
  			//variabile per sapere alla fine se è stato inviato o no e 
  			//aggiornare di conseguenza le statistiche
  			int inviato = 0;
  			//calcolo semaforo da usare per il mittente
  			mutex = calcola_mutex(sname);
  			pthread_mutex_lock_m(&mtx[mutex]);
  			//controllo se l' user è registrato
  			user = icl_hash_find(reg_users, sname);
  			if (user != NULL) {  				
  				//controllo subito se esiste il destinatario in modo da 
  				//inviare subito l' ack al mittente
  				mutex2 = calcola_mutex(rname);
  				ruser = icl_hash_find(reg_users, rname);
  				if (ruser !=NULL) {
  					//invio subito ack al mittente
  					setHeader(&message.hdr, OP_OK, "server");
  					sendHeader(fdc, &message.hdr);
  					//ho finito di lavorare con il mittente
  					pthread_mutex_unlock(&mtx[mutex]);
  					//a questo punto prendo il semaforo del destinatario e 
  					//lavoro con lui
  					pthread_mutex_lock_m(&mtx[mutex2]);
  					msg->hdr.op = TXT_MESSAGE;
  					//se il destinatario è connesso gli invio subito
  					//il messaggio
  					if (ruser->connesso == 1){
  						f = ruser->fd;
  						//se nel frattempo ha chiuso la connessione lo disconnetto
  						if (sendRequest(f, msg) == -1) {
  							ruser->connesso = 0;
  							ruser->fd = -1;
  							//le statistiche vengono aggiornate quando si chiude
  							//la connessione
  						}
  						//altrimenti segno che è stato inviato il messaggio
  						else inviato = 1;
  					}
  					//lo metto anche nella history del destinatario a meno
  					//che non sia piena
  					if (ruser->nmsgcoda < conf.MaxHistMsgs) {
  						ruser->msgs = pushm(ruser->msgs, (msg->hdr), &(msg->data));
  						ruser->nmsgcoda++;
  					}
  					//a questo punto posso uscire dalla mutua esclusione 
  					//e eliminare il messaggio dato che ne è presente
  					//una copia nella history
  					pthread_mutex_unlock(&mtx[mutex2]);
 					
  					//aggiornamento statistiche
  					pthread_mutex_lock_m(&mtxstats);
  					if (inviato == 1) chattyStats.ndelivered++;
  					else chattyStats.nnotdelivered++;
  					pthread_mutex_unlock_m(&mtxstats);
  					
  					//eliminazione messaggio
  					free(msg->data.buf);
  					msg->data.hdr.len = 0;
  				}
  				//se il destinatario non era registrato potrebbe essere un gruppo
  				else {
  					pthread_mutex_unlock_m(&mtx[mutex]);
  					pthread_mutex_lock_m(&mtxgroups);
  					//controllo se il destinatario è un gruppo
  					group = group_hash_find(reg_groups, rname);
  					if (group != NULL) {
  						//controllo se il mittente è iscritto al gruppo
  						if (user_group_find(sname, group->u_coda) == 1) {
  							//se lo è invio ack al mittente
  							setHeader(&message.hdr, OP_OK, "server");
  							sendHeader(fdc, &message.hdr);
  							//invio a tutti gli utenti del gruppo
  							msg->hdr.op = TXT_MESSAGE;
  							//tengo il conto di quanti messaggi invio e quanti no
  							//per aggiornare alla fine le statistiche
  							int inviati = 0;
  							int non_inviati = 0;
  							//variabile usata per scorrere gli utenti di un gruppo
  							usernode * utente = group->u_coda;
  							//scorro e invio a tutti il messaggio
  							while (utente != NULL) {
  								char * nome = utente->username;
  								//calcolo semaforo per l' utente
  								mutex = calcola_mutex(nome);
  			                    pthread_mutex_lock_m(&mtx[mutex]);
  								user = icl_hash_find(reg_users, nome);
  								//se è registrato invio e/o metto nella history
  								if (user != NULL) {
  										non_inviati++;
  										if (user->connesso == 1) {
  											f = user->fd;
  											//se nel frattempo ha chiuso la connessione
  											//lo disconnetto
  											if (sendRequest(f, msg) == -1) {
  												user->connesso = 0;
  												user->fd = -1;
  												//le statistiche vengono aggiornate quando si chiude
  												//la connessione
  											}
  											//aggiorno i contatori
  											inviati++;
  											non_inviati--;
  										}
  										//lo metto nella history se non è piena
  										if (user->nmsgcoda < conf.MaxHistMsgs) {
  											user->msgs = pushm(user->msgs, (msg->hdr), &(msg->data));
  											user->nmsgcoda++;
  										}
  								}
  								pthread_mutex_unlock_m(&mtx[mutex]);
  								utente = utente->next;
  							}
  							pthread_mutex_unlock_m(&mtxgroups);
  							
  							//aggiornamento statistiche
  							pthread_mutex_lock_m(&mtxstats);
  							chattyStats.ndelivered = chattyStats.ndelivered + inviati;
  							chattyStats.nnotdelivered = chattyStats.nnotdelivered + non_inviati;
  							pthread_mutex_unlock_m(&mtxstats);
  							
  							free(msg->data.buf);
  							msg->data.hdr.len = 0;
  						}
  						//se il mittente non è iscritto al gruppo invio errore
  						else {
  							pthread_mutex_unlock_m(&mtxgroups);
  							//invio ack
  							setHeader(&message.hdr, OP_NICK_UNKNOWN, "server");
  							sendHeader(fdc, &message.hdr);
  				
  							//aggiornamento statistiche
  							pthread_mutex_lock_m(&mtxstats);
  							chattyStats.nerrors++;
  							pthread_mutex_unlock_m(&mtxstats);
  							
  							free(msg->data.buf);
  							msg->data.hdr.len = 0;
  						}
  					}
  					//se il destinatario non è neanche un gruppo invio errore
  					else {
  						pthread_mutex_unlock_m(&mtxgroups);
  						//invio ack
  						setHeader(&message.hdr, OP_NICK_UNKNOWN, "server");
  						sendHeader(fdc, &message.hdr);
  				
  						//aggiornamento statistiche
  						pthread_mutex_lock_m(&mtxstats);
  						chattyStats.nerrors++;
  						pthread_mutex_unlock_m(&mtxstats);
  					
  						free(msg->data.buf);
  						msg->data.hdr.len = 0;
  					}
  				}
  			}
  			//se il mittente non è registrato invio messaggio di operazione fallita
  			else {
  				//invio ack
  				setHeader(&message.hdr, OP_NICK_UNKNOWN, "server");
  				sendHeader(fdc, &message.hdr);
  				pthread_mutex_unlock_m(&mtx[mutex]);
  				
  				//aggiornamento statistiche
  				pthread_mutex_lock_m(&mtxstats);
  				chattyStats.nerrors++;
  				pthread_mutex_unlock_m(&mtxstats);
 				
  				free(msg->data.buf);
  				msg->data.hdr.len = 0;
  			}
  		}
 	}
 	
//------------------------------------------------------------------------------  	

  	else if (op == POSTTXTALL_OP) {
  		mutex = calcola_mutex(sname);
  		pthread_mutex_lock_m(&mtx[mutex]);
  		//controllo che il mittente esista
  		user = icl_hash_find(reg_users, sname);
  		//se l' utente non è iscritto invio l' errore
  		if (user == NULL) {
  			setHeader(&message.hdr, OP_NICK_UNKNOWN, "server");
  			sendHeader(fdc, &message.hdr);
  			pthread_mutex_unlock_m(&mtx[mutex]);
  			
  			//aggiornamento statistiche
  			pthread_mutex_lock_m(&mtxstats);
  			chattyStats.nerrors++;
  			pthread_mutex_unlock_m(&mtxstats);
  			
  			free(msg->data.buf);
  			msg->data.hdr.len = 0;
  		}
  		//se il messaggio è troppo lungo invio l' errore
  		else if (msg->data.hdr.len > conf.MaxMsgSize) {
  			setHeader(&msg->hdr, OP_MSG_TOOLONG, "server");
  			sendHeader(fdc, &msg->hdr);
  			pthread_mutex_unlock_m(&mtx[mutex]);
  			
  			//aggiornamento statistiche
  			pthread_mutex_lock_m(&mtxstats);
  			chattyStats.nerrors++;
  			pthread_mutex_unlock_m(&mtxstats);
  			
  			free(msg->data.buf);
  			msg->data.hdr.len = 0;
  		}
  		//se non ci sono errori
  		else {
  			//invio ack
  			setHeader(&message.hdr, OP_OK, "server");
  			sendHeader(fdc, &message.hdr);
  			pthread_mutex_unlock_m(&mtx[mutex]);
  			msg->hdr.op = TXT_MESSAGE;
  			//variabile usata per scorrere tutti gli utenti della tabella
  			icl_entry_t * curr;
  			//variabili usate per tenere il conto di quanti ne invio e di
  			//quanti no per aggiornare alla fine le statistiche evitando 
  			//di entrare in mutua esclusione per aggiornarle ad ogni iterazione
  			int inviati = 0;
  			int non_inviati = 0;
  			//scorro tutta la tabella hash prendendo di volta in volta la chiave
  			for (i=0; i<reg_users->nbuckets; i++) {
  				//scorro tutti gli utenti in una entry della tabella
  				for (curr=reg_users->buckets[i]; curr!=NULL; curr=curr->next) {
  					//se sono diversi dal nome dall' user che invia
  					if (strcmp(curr->data, user->data) != 0) {
  						//aumento subito il numero di non inviati
  						non_inviati++;
  						//di volta in volta accedo prendendo la giusta variabile
  						//di mutua esclusione
  						pthread_mutex_lock_m(&mtx[i/4]);
  						//se è connesso gli invio direttamente il messaggio
  						if ((curr->connesso == 1) && (curr->fd != -1)) {
  							f = curr->fd;
  							//nel frattempo potrebbe aver chiuso la connessione
  							//e in quel caso lo disconnetto
  							if (sendRequest(f, msg) == -1) {
  								curr->connesso = 0;
  								curr->fd = -1;
  								//le statistiche vengono aggiornate quando si chiude
  							    //la connessione
  							}
  							else {
  								//aggiorno il conto di quanti ne invio e di quanti no
  								inviati++;
  								non_inviati--;
  							}
  						}
  						//in ogni caso lo metto nella history se non è già piena
  						if (curr->nmsgcoda < conf.MaxHistMsgs) {	
  							curr->msgs = pushm(curr->msgs, (msg->hdr), &(msg->data));
  							curr->nmsgcoda++;
  						}
  						pthread_mutex_unlock_m(&mtx[i/4]);
  					}
  				}
  			}
  			//aggiorno le statistiche
  			pthread_mutex_lock_m(&mtxstats);
  			chattyStats.ndelivered = chattyStats.ndelivered + inviati;
  			chattyStats.nnotdelivered = chattyStats.nnotdelivered + non_inviati;
  			pthread_mutex_unlock_m(&mtxstats);
  			
  			//alla fine, dopo averlo inviato e messo nella history di tutti 
  			//posso liberare la memoria visto che tutti ne hanno una copia
  			free(msg->data.buf);
  			msg->data.hdr.len = 0;
  		}
  	}
  	
//------------------------------------------------------------------------------  	
  
  	else if (op == POSTFILE_OP) {
  		
  		//usata per testare valore di ritorno
  		int n = 0;
  		//usata per ricordarsi la dimensione del file
  		int filesize = 0;
  		pthread_mutex_lock_m(&mtxfiles);
  		mutex = calcola_mutex(sname);
  	  	pthread_mutex_lock_m(&mtx[mutex]);
  	  	//cerco l' user nella tabella hash
  	  	user = icl_hash_find(reg_users, sname);
  	  	//controllo se è registrato
  	  	if (user != NULL) {
  	  		//ricevo il nome del file
  	  		n = readData(fdc, &message.data);
  	  		if (n <= 0) {
  	  			perror("reply data");
  	  		}
  	  		filesize = message.data.hdr.len;
  	  		//se il file è vuoto invio errore
  	  		if (filesize <= 0) {
  	  			setHeader(&message.hdr, OP_FAIL, "server");
  	  			sendHeader(fdc, &message.hdr);
  	  			pthread_mutex_unlock_m(&mtx[mutex]);
  			
  	  			//aggiornamento statistiche
  	  			pthread_mutex_lock_m(&mtxstats);
  	  			chattyStats.nerrors++;
  	  			pthread_mutex_unlock_m(&mtxstats);
  			
  	  			//libero la memoria
  	  			msg->data.hdr.len = 0;
  	  			free(msg->data.buf);
  	  		}
  	  		//trasformo la dimensione in bytes e controllo che non sia troppo grande
  	  		else if (filesize > (conf.MaxFileSize * 1024)) {
  	  			//invio errore
  	  			setHeader(&message.hdr, OP_MSG_TOOLONG, "server");
  	  			sendHeader(fdc, &message.hdr);
  	  			pthread_mutex_unlock_m(&mtx[mutex]);
  			
  	  			//aggiornamento statistiche
  	  			pthread_mutex_lock_m(&mtxstats);
  	  			chattyStats.nerrors++;
  	  			pthread_mutex_unlock_m(&mtxstats);
  			
  	  			msg->data.hdr.len = 0;
  	  			free(msg->data.buf);
  	  		}
  	  		//se le dimensioni del messaggio sono regolari
  	  		else {
  	  	  		//devo preparare il path del file con la cartella DirName
  	  	  		int dimdirname = strlen(conf.DirName) * sizeof(char) + 1;
  	  	  		int dimfilename = strlen(msg->data.buf) * sizeof(char) + 1;
  	  	 		char * directory = malloc(dimdirname);
  	  	 		if (directory == NULL) {
  	  	  			perror("nella malloc");
  	  	  			exit(EXIT_FAILURE);
  	  	  		}
  	  	  		strncpy(directory, conf.DirName, dimdirname);
  	  	  		char * filename = malloc(dimfilename);
  	  	  		if (filename == NULL) {
  	  	  			perror("nella malloc");
  	  	  			exit(EXIT_FAILURE);
  	  	  		}
  	  	 		strncpy(filename, msg->data.buf, dimfilename);
  	  	  		//il formato finale contenuto qui sarà: directory/filename
  	  	  		char * filepathname = malloc(dimdirname + dimfilename);
  	  	  		if (filepathname == NULL) {
  	  	  			perror("nella malloc");
  	  	  			exit(EXIT_FAILURE);
  	  	  		}
  	  	  		//inizializzo la stringa che conterrà il path finale
  	  	 		memset(filepathname, 0, (dimdirname + dimfilename));
  	  	 		//aggiungo la cartella al percorso del file, qui ancora vuoto
  	  	 		strncat(filepathname, directory, dimdirname);
  	  	 		//con la procedura aggiungo anche il nome del file ottenendo il 
  	  	 		//path finale 
  	    		filepathname = correggi_filename(filename, filepathname, dimfilename);
  	  	  		//a questo punto posso mettere realmente il file in memoria
  	  	  		int filedesc = 0;
  	  	  		//apro il file in cui andrò a scrivere in lettura e scrittura nella cartella
  	            //temporanea usata dal server (DirName), creandolo se non esiste
  	  	  		SYSCALL(filedesc, open(filepathname, O_RDWR|O_CREAT, 0777), "nella open file da ricevere");
  	  	  		//descrittore del file che devo mettere in memoria
  	  	  		int filetosend = 0;
  	  	  		//apro il file inviato dal client per metterne il contenuto nel 
  	  	  		//file aperto prima, nella cartella DirName
  	  	  		SYSCALL(filetosend, open(filename, O_RDONLY), "nella open file da inviare"); 
  	  	  		int r = 0;
  	  	  		char buffer[filesize];
  	  	  		//leggo e scrivo finchè non ho letto e scritto tutto il file in quello nuovo
  	  	  		while ((r = read(filetosend, buffer, filesize)) > 0) {
  	  	  			SYSCALL(n, write(filedesc, buffer, r), "nella write file");
  	  	  		}
  	  	  		//chiudo i descrittori di file usati
  	  	  		close(filetosend);
  	  	  		close(filedesc);
  	  	  		pthread_mutex_unlock_m(&mtx[mutex]);
  	  	  		//scrivo nel messaggio che è un file
  	  	  		msg->hdr.op = FILE_MESSAGE;
  	  	  		//controllo se il destinatario esiste nella tabella
  	  	  		mutex2 = calcola_mutex(rname);
  	  	  		pthread_mutex_lock(&mtx[mutex2]);
  	  	  		ruser = icl_hash_find(reg_users, rname);
  	  	  		if (ruser != NULL) {
  	  	  			//invio l' ack al mittente
  	  	  			setHeader(&message.hdr, OP_OK, "server");
  	  	  			sendHeader(fdc, &message.hdr);
  	  	  			//se il destinatario è connesso invio subito e dopo salvo nella history
  	  	  			if ((ruser->connesso == 1) && (ruser->fd != -1)) {
  	  	  				f = ruser->fd;
  	  	  				//se nel frattempo ha chiuso la connessione lo disconnetto
  	  	  				if (sendRequest(f, msg) == -1) {
  	  	  					ruser->connesso = 0;
  	  	  					ruser->fd = -1;
  	  	  					//le statistiche vengono aggiornate quando si chiude
  							//la connessione
  	  	  				}
  	  	  			}
  	  	  			//se posso metterlo in history lo faccio altrimenti viene scartato
  	  	  			if (ruser->nmsgcoda < conf.MaxHistMsgs) {
  	  	  				ruser->msgs = pushm(ruser->msgs, (msg->hdr), &(msg->data));
  	  	  				ruser->nmsgcoda++;
  	  	  			}
  	  	  			pthread_mutex_unlock_m(&mtx[mutex2]);

  	  	  			//aggiornamento statistiche
  	  	  			pthread_mutex_lock_m(&mtxstats);
  	  	  			chattyStats.nfilenotdelivered++;
  	  	  			pthread_mutex_unlock_m(&mtxstats);
  	  	  			
  	  	  			//libero la memoria
  	  	  			free(msg->data.buf);
  	  	  			msg->data.hdr.len = 0;
  	  	  		}
  	  	  		//se il destinatario non è un utente iscritto controllo se è un gruppo
  	  	  		else {
  	  	  			pthread_mutex_unlock_m(&mtx[mutex2]);
  	  	  			pthread_mutex_lock_m(&mtxgroups);
  	  	  			group = group_hash_find(reg_groups, rname);
  	  	  			if (group != NULL) {
  	  	  				//controllo se il mittente è iscritto al gruppo
  	  	  				if (user_group_find(sname, group->u_coda) == 1) {
  	  	  					//invio ack al mittente
  	  	  					setHeader(&message.hdr, OP_OK, "server");
  							sendHeader(fdc, &message.hdr);
  							//variabile usata per tenere il conto dei file non consegnati
  							int non_cons = 0;
  							//variabile usata per scorrere gli utenti del gruppo
  							usernode * utente = group->u_coda;
  							while (utente != NULL) {
  								char * nome = utente->username;
  								mutex = calcola_mutex(nome);
  			                    pthread_mutex_lock_m(&mtx[mutex]);
  								user = icl_hash_find(reg_users, nome);
  								if (user != NULL) {
  										non_cons++;
  										//se l' utente risulta connesso gli invio il messaggio
  										if (user->connesso == 1) {
  											f = user->fd;
  											//se nel frattempo ha chiuso la connessione lo disconnetto
  											if (sendRequest(f, msg) == -1) {
  												user->connesso = 0;
  												user->fd = -1;
  												//le statistiche vengono aggiornate quando si chiude
  							                    //la connessione
  											}
  										}
  										//se non è piena lo metto anche nella history
  										if (user->nmsgcoda < conf.MaxHistMsgs) {
  											user->msgs = pushm(user->msgs, (msg->hdr), &(msg->data));
  											user->nmsgcoda++;
  										}
  								}
  								pthread_mutex_unlock_m(&mtx[mutex]);
  								//passo all' utente successivo
  								utente = utente->next;
  							}
  							pthread_mutex_unlock_m(&mtxgroups);
  							
  							//aggiornamento statistiche
  							pthread_mutex_lock_m(&mtxstats);
  							chattyStats.nfilenotdelivered =chattyStats.nfilenotdelivered + non_cons;
  							pthread_mutex_unlock_m(&mtxstats);
  							
  							msg->data.hdr.len = 0;
  							free(msg->data.buf);
  	  	  				}
  	  	  			}
  	  	  			//se non è un utente e nemmeno un gruppo invio errore
  	  	  			else {
  	  	  				pthread_mutex_unlock_m(&mtxgroups);
						setHeader(&message.hdr, OP_NICK_UNKNOWN, "server");
						sendHeader(fdc, &message.hdr);
  	  	  			
						//aggiornamento statistiche
						pthread_mutex_lock_m(&mtxstats);
						chattyStats.nerrors++;
						pthread_mutex_unlock_m(&mtxstats);
  	  	  		
						msg->data.hdr.len = 0;
						free(msg->data.buf);
					}
  	  	  		}
  	  	  		//libero memoria allocata durante questa operazione
  	  	  		free(directory);
  	  	  		free(filename);
  	  	  		free(filepathname);
  	  	  	}
  	  	}
  	  	//se il mittente non esiste nella tabella hash, cioè non è registrato,
  	  	//invio l' errore corrispondente
  	  	else {
  	  		setHeader(&message.hdr, OP_NICK_UNKNOWN, "server");
  	  	  	sendHeader(fdc, &message.hdr);
  	  	  	pthread_mutex_unlock(&mtx[mutex]);
  	  	 		
  	  	  	//aggiornamento statistiche
  	   		pthread_mutex_lock_m(&mtxstats);
  	  		chattyStats.nerrors++;
   	  		pthread_mutex_unlock_m(&mtxstats);
    	  	
  	  	  	msg->data.hdr.len = 0;
  	  	  	free(msg->data.buf);
  	  	}
  	    pthread_mutex_unlock_m(&mtxfiles);
  	  	free(message.data.buf);
  	  	message.data.hdr.len = 0;
  	}
  	
//------------------------------------------------------------------------------  	
  
  	else if (op == GETFILE_OP) {
  		
  		//descrittore del file
  		int filedesc = 0;
  		//dimensioni dei nomi di cartella e file
  		int dimdirname = strlen(conf.DirName) * sizeof(char) + 1;
  		int dimfilename = strlen(msg->data.buf) * sizeof(char) + 1;
  		//nome della cartella
  		char * directory = malloc(dimdirname);
  		if (directory == NULL) {
  	  	    perror("nella malloc");
  	  	    exit(EXIT_FAILURE);
  	  	}
  		strncpy(directory, conf.DirName, dimdirname);
  		//nome del file
  		char * filename = msg->data.buf;
  		//nome completo con percorso del file
  		char * filepathname = malloc(dimdirname + dimfilename);
  		if (filepathname == NULL) {
  	  	  	perror("nella malloc");
  	  	  	exit(EXIT_FAILURE);
  	  	}
  	  	//inizializzazione
  		memset(filepathname, 0, (dimdirname + dimfilename));
  		//inserisco il nome della cartella nel percorso finale
  		strncat(filepathname, directory, dimdirname);
  		//aggiungo anche il nome del file creando quello completo
  		filepathname = correggi_filename(filename, filepathname, dimfilename);
  		//controllo che esista il file nella cartella
  		pthread_mutex_lock_m(&mtxfiles);
  		//controllo se l' utente è registrato
  		mutex = calcola_mutex(sname);
  		pthread_mutex_lock_m(&mtx[mutex]);
  		user = icl_hash_find(reg_users, sname);
  		if (user != NULL) {
  			//controllo se il file esiste
  			struct stat st;
  			//se il file non esiste invio l' errore
  			if (stat(filepathname, &st) == -1) {
  				setHeader(&message.hdr, OP_NO_SUCH_FILE, "server");
  				sendHeader(fdc, &message.hdr);
  				pthread_mutex_unlock(&mtx[mutex]);
  				
  				//aggiornamento statistiche
  				pthread_mutex_lock_m(&mtxstats);
  				chattyStats.nerrors++;
  				pthread_mutex_unlock_m(&mtxstats);
  				
  				free(message.data.buf);
  				message.data.hdr.len = 0;
  			}
  			//se il file esiste
  			else {
  				//invio ack
  				setHeader(&message.hdr, OP_OK, "server");
  				sendHeader(fdc, &message.hdr);
  				//apro file da leggere (nella cartella DirName)
  				SYSCALL(filedesc, open(filepathname, O_RDONLY), "nella open getfile");
				  
				//TODO: In MaxFileSize è specificata la dimensione in kilobyte, quindi qui il buffer deve contenere 
				//      MaxFileSize * 1024 bytes e non solo MaxFileSize, e di conseguenza la read va fatta su questo numero 
				//      di bytes

  				char buffer[conf.MaxFileSize];
  				//inizializzo il buffer
  				memset(buffer, 0, conf.MaxFileSize);
  				int r = 0;
  				//leggo il file e lo metto in buffer che poi invio all' utente
  				while((r = read(filedesc, buffer, conf.MaxFileSize)) > 0);
  				close(filedesc);
  				//invio il corpo del messaggio del messaggio
  				setData(&(message.data), msg->data.hdr.receiver, buffer, conf.MaxFileSize);
  				sendData(fdc, &(message.data));
  				pthread_mutex_unlock_m(&mtx[mutex]);
  				msg->data.hdr.len = 0;
  				free(msg->data.buf);
			
  				//aggiornamento statistiche
  				pthread_mutex_lock_m(&mtxstats);
  				chattyStats.nfiledelivered++;
  				chattyStats.nfilenotdelivered--;
  				pthread_mutex_unlock_m(&mtxstats);
			}
  		}
  		//se l' utente non è iscritto invio errore
  		else {
  			setHeader(&message.hdr, OP_NICK_UNKNOWN, "server");
  			sendHeader(fdc, &message.hdr);
  			pthread_mutex_unlock_m(&mtx[mutex]);
  			
  			//aggiornamento statistiche
  			pthread_mutex_lock_m(&mtxstats);
  			chattyStats.nerrors++;
  			pthread_mutex_unlock_m(&mtxstats);
  			
  			free(msg->data.buf);
  			msg->data.hdr.len = 0;
  		}
  	  	pthread_mutex_unlock_m(&mtxfiles);
  	  	//libero lo spazio occupato dai nomi
  		free(directory);
  		free(filepathname);
  	}
  	
//------------------------------------------------------------------------------  	
	
  	else if (op ==  GETPREVMSGS_OP) {
  		
  		//controllo se l' utente è registrato
  		mutex = calcola_mutex(sname);
  		pthread_mutex_lock_m(&mtx[mutex]);
  		user = icl_hash_find(reg_users, sname);
  		//se l' user non esiste
  		if (user == NULL) {
  			//invio ack
  			setHeader(&message.hdr, OP_NICK_UNKNOWN, "server");
  			sendHeader(fdc, &message.hdr);
  			pthread_mutex_unlock_m(&mtx[mutex]);
  			
  			//aggiornamento statistiche
  			pthread_mutex_lock_m(&mtxstats);
  			chattyStats.nerrors++;
  			pthread_mutex_unlock_m(&mtxstats);
  		}
  		//consegnare a un utente la lista dei messaggi che gli sono stati inviati
  		//che si trova nella sua entry della tabella hash dopo averli sistemati
  		//come li vuole il client
  		else {
  			//invio l' ack al mittente
  			setHeader(&message.hdr, OP_OK, "server");
  			sendHeader(fdc, &message.hdr);
  			//invio un messaggio che contiene il numero di messaggi che sono in 
  			//coda e lo invio sotto forma di corpo del messaggio al mittente
  			int nmsg = 0;
  			//variabile usata per scorrere i messaggi
  			msgcoda curr = user->msgs;
  			//conto i messaggi nella history
  			while (curr != NULL) {
  				nmsg++;
  				curr = curr->next;
  			}
  			//converto questo numero nel formato size_t
  			size_t nms = 0;
  			nms = (size_t) nmsg; 
  			//invio il numero di messaggi nella history
  			setData(&(message.data), sname, (const char *)&nms, (int)sizeof(size_t*));
  			sendData(fdc, &(message.data));
  			//variabile usata per tenere il conto di quanti messaggi sono inviati
  			//e alla fine aggiornare di conseguenza le statistiche
  			int msg_inviati = 0;
  			while (user->msgs != NULL) {
  				//prendo il messaggio in testa alla coda
   				curr = user->msgs;
  				if (curr != NULL) {
  					//passo il puntatore al messaggio che è contenuto nel 
  					//nodo curr
  					//se nel frattempo chiude la connessione lo disconnetto
  					if (sendRequest(fdc, &(curr->data)) == -1) {
  						user->connesso = 0;
  						user->fd = -1;
  						//le statistiche vengono aggiornate quando si chiude
  						//la connessione
  					}
  					if (curr->data.hdr.op == TXT_MESSAGE) msg_inviati++;
  					//tolgo il messaggio dalla history
  					user->msgs = popm(user->msgs);
  					user->nmsgcoda--;
  				}
  			}
  			pthread_mutex_unlock_m(&mtx[mutex]);
  			
  			//aggiornamento statistiche
  			pthread_mutex_lock_m(&mtxstats);
  			chattyStats.ndelivered = chattyStats.ndelivered + msg_inviati;
  			chattyStats.nnotdelivered = chattyStats.nnotdelivered - msg_inviati;
  			pthread_mutex_unlock_m(&mtxstats);
  		}
  	}
  	
//------------------------------------------------------------------------------  	
  	
  	else if (op == CREATEGROUP_OP) {
  		
  		//controllo che esista l' utente
  		mutex = calcola_mutex(sname);
		pthread_mutex_lock_m(&mtx[mutex]);
		user = icl_hash_find(reg_users, sname);
		pthread_mutex_unlock_m(&mtx[mutex]);
  		if (user != NULL) {
  			pthread_mutex_lock_m(&mtxgroups);
  			//creo il gruppo
  			if (group_hash_insert(reg_groups, rname, sname) != NULL) {
  				//invio ack se il gruppo viene creato
  				setHeader(&(msg->hdr), OP_OK, "server");
  				sendHeader(fdc, &(msg->hdr));
  				pthread_mutex_unlock_m(&mtxgroups);
  			}
  			else {
  				//invio errore se il gruppo esisteva già
  				setHeader(&(msg->hdr), OP_NICK_ALREADY, "server");
  				sendHeader(fdc, &(msg->hdr));
  				pthread_mutex_unlock_m(&mtxgroups);
  				
  				//aggiornamento statistiche
  				pthread_mutex_lock_m(&mtxstats);
  				chattyStats.nerrors++;
  				pthread_mutex_unlock_m(&mtxstats);
  			}
  		}
  		//se il mittente non è registrato invio errore
  		else {
  			setHeader(&message.hdr, OP_NICK_UNKNOWN, "server");
  			sendHeader(fdc, &message.hdr);
  			pthread_mutex_unlock_m(&mtx[mutex]);
  			
  			//aggiornamento statistiche
  			pthread_mutex_lock_m(&mtxstats);
  			chattyStats.nerrors++;
  			pthread_mutex_unlock_m(&mtxstats);
  			
  			free(msg->data.buf);
  			msg->data.hdr.len = 0;
  		}
  	}
  	
//------------------------------------------------------------------------------

	else if (op == ADDGROUP_OP) {
		
  		mutex = calcola_mutex(sname);
		pthread_mutex_lock_m(&mtx[mutex]);
		//controllo che l' utente esista
		user = icl_hash_find(reg_users, sname);
		pthread_mutex_unlock_m(&mtx[mutex]);
		if (user != NULL) {
			pthread_mutex_lock_m(&mtxgroups);
			//controllo che il gruppo esista
			group = group_hash_find(reg_groups, rname);
			if (group != NULL) {
				group->n_users++;
				//aggiungo l' utente agli iscritti al gruppo
				group->u_coda = user_add_group(sname, group->u_coda);
				//invio ack
   				setHeader(&(msg->hdr), OP_OK, "server");
  				sendHeader(fdc, &(msg->hdr));
				pthread_mutex_unlock_m(&mtxgroups);
			}
			//se non esiste il gruppo invio errore
			else {
				setHeader(&(msg->hdr), OP_FAIL, "server");
  				sendHeader(fdc, &(msg->hdr));
  				pthread_mutex_unlock_m(&mtxgroups);
  				
  				//aggiornamento statistiche
  				pthread_mutex_lock_m(&mtxstats);
  				chattyStats.nerrors++;
  				pthread_mutex_unlock_m(&mtxstats);
			}
		}
		//se il mittente non è registrato invio l' errore
		else {
			setHeader(&message.hdr, OP_NICK_UNKNOWN, "server");
  			sendHeader(fdc, &message.hdr);
  			pthread_mutex_unlock_m(&mtx[mutex]);
  			
  			//aggiornamento statistiche
  			pthread_mutex_lock_m(&mtxstats);
  			chattyStats.nerrors++;
  			pthread_mutex_unlock_m(&mtxstats);
  			
  			free(msg->data.buf);
  			msg->data.hdr.len = 0;
  		}
	}
	
//------------------------------------------------------------------------------

	else if (op == DELGROUP_OP) {
		
  		mutex = calcola_mutex(sname);
		pthread_mutex_lock_m(&mtx[mutex]);
		//controllo che l' utente esista
		user = icl_hash_find(reg_users, sname);
		pthread_mutex_unlock_m(&mtx[mutex]);
		if (user != NULL) {
			pthread_mutex_lock_m(&mtxgroups);
			//controllo che il gruppo esista
			group = group_hash_find(reg_groups, rname);
			if (group != NULL) {
				//controllo cheil mittente faccia parte del gruppo
				if (user_group_find(sname, group->u_coda) == 1) {
					//lo elimino dagli iscritti al gruppo
					group->u_coda = user_delete_from_group(sname, group->u_coda);
					//invio ack
					setHeader(&(msg->hdr), OP_OK, "server");
					sendHeader(fdc, &(msg->hdr));
					pthread_mutex_unlock_m(&mtxgroups);
				}
				//se l' utente non era iscritto al gruppo invio errore 
				else {
					setHeader(&(msg->hdr), OP_FAIL, "server");
					sendHeader(fdc, &(msg->hdr));
					pthread_mutex_unlock_m(&mtxgroups);
  				
					//aggiornamento statistiche
					pthread_mutex_lock_m(&mtxstats);
					chattyStats.nerrors++;
					pthread_mutex_unlock_m(&mtxstats);
					
					free(msg->data.buf);
					msg->data.hdr.len = 0;
				}
			}
			//se il gruppo non esiste invio errore
			else {
				setHeader(&(msg->hdr), OP_NICK_UNKNOWN, "server");
  				sendHeader(fdc, &(msg->hdr));
  				pthread_mutex_unlock_m(&mtxgroups);
  				
  				//aggiornamento statistiche
  				pthread_mutex_lock_m(&mtxstats);
  				chattyStats.nerrors++;
  				pthread_mutex_unlock_m(&mtxstats);
  				
  				free(msg->data.buf);
  				msg->data.hdr.len = 0;
			}
		}
		//se il mittente non è registrato invio errore
		else {
			setHeader(&message.hdr, OP_NICK_UNKNOWN, "server");
  			sendHeader(fdc, &message.hdr);
  			pthread_mutex_unlock_m(&mtx[mutex]);

  			//aggiornamento statistiche
  			pthread_mutex_lock_m(&mtxstats);
  			chattyStats.nerrors++;
  			pthread_mutex_unlock_m(&mtxstats);
  			
  			free(msg->data.buf);
  			msg->data.hdr.len = 0;
  		}
	}
  	
//------------------------------------------------------------------------------  	
    
    else printf("Operazione richiesta non supportata\n");
    
}

/**
 * @function     disconnect_user
 * @brief        Disconnette l' utente che usava questo descrittore
 *
 * @param fds    il descrittore della connessione usato dall' utente
 * 
 * @return -1 in caso di errore, 0 se tutto va bene
 *
 * Scorre la tabella hash fino a trovare l' utente che usa il descrittore ricevuto
 * come parametro e lo disconnette
 */
int disconnect_user(long fds) {
	int i = 0;
	icl_entry_t * curr;
	icl_entry_t * user;
	//si scorre nelle caselle della tabella hash
	for (i=0; i<reg_users->nbuckets; i++) {
		user = reg_users->buckets[i];
		pthread_mutex_lock_m(&mtx[i/4]);
		//si scorre nella lista di utenti nella stessa casella
		for (curr=user; curr!=NULL; curr=curr->next) {
			if (curr->fd == fds) {
				//disconnetto
				curr->connesso = 0;
				curr->fd = -1;
				pthread_mutex_unlock_m(&mtx[i/4]);
				
				//aggiornamento statistiche
				pthread_mutex_lock_m(&mtxstats);
				chattyStats.nonline--;
				pthread_mutex_unlock_m(&mtxstats);
				
				return 0;
			}
		}
		pthread_mutex_unlock_m(&mtx[i/4]);
	}
	return -1;
}


/*----- PROCEDURA UTILIZZATA DAI THREAD -----*/
/**
 * @function procedura_thread
 * @brief    è la procedura eseguita dai thread del pool
 *
 * @param arg è il nome della pipe con cui comunicare con il main
 *
 * Il thread preleva un descrittore dalla coda se non è vuota altrimenti si mette
 * in attesa che il main lo risvegli quando ne mette uno.
 * Una volta preso e rimosso dalla coda legge un messaggio su questo e se la lettura
 * va a buon fine chiama la procedura di gestione della richiesta, mentre se 
 * fallisce o non legge niente disconnette l' utente e chiude il descrittore.
 * Quando arriva un segnale di terminazione interrompe il ciclo, chiude la connessione
 * in pipe e quella sul descrittore su cui sta ascoltando e termina restituendo NULL.
 */
static void * procedura_thread(void * arg) {
	char * pipe = (char *)arg;
   	long fdc = 0; 
   	int n = 0;
	message_t msg;
	//descrittore pipe 
	int pfd = 0;
	//apertura pipe in scrittura per comunicare che ho finito
	SYSCALL(pfd, open(pipe, O_WRONLY, O_NONBLOCK), "nella open pipe");
	while (segnale_arrivato == 0) {
		//prendo la chiave per la coda
		pthread_mutex_lock_m(&mtxcoda);
		//aspetto che ci siano fd in coda
		if (head == NULL) {
	    	pthread_cond_wait_m(&condcoda, &mtxcoda);
	    	if (segnale_arrivato == 1) {
	    		pthread_mutex_unlock_m(&mtxcoda);
	    		break;
	    	}
	    }
	    //se ci sono fd in coda
	    if (head != NULL) {
	    	//prendo un descrittore dalla coda e lo tolgo
	    	fdc = head->data;
	    	head = pop(head);
	    	pthread_mutex_unlock_m(&mtxcoda);
	    	n = readMsg(fdc, &msg);
	    	//se il client ha finito di comunicare o ha chiuso la connessione 
	    	//disconnetto l' utente che usava questo descrittore,
	    	//aggiornando nella procedura anche le statistiche
	    	if ((n == 0) || (n == -1)) {
                disconnect_user(fdc);
                close(fdc);
            }
	    	//se il client deve ancora comunicare qualcosa lo comunico in pipe
	    	//al main che ascolterà le richieste successive settandolo di
	    	//nuovo nella maschera usata dalla select
	    	else if (n == 1) {
	    		//invio l'operazione alla gestione operazioni
	    		requestreply(fdc, &msg);
	    		//scrivo nella pipe che ho finito di lavorarci ma il client
	    		//non ha finito di comunicare e quindi va rimesso in coda
	    		SYSCALL(n, write(pfd, &fdc, sizeof(int)), "nella write");
	    	}
	    }
	    else pthread_mutex_unlock_m(&mtxcoda);
	}
	//chiudo le connessioni
	close(fdc);
	close(pfd);
	pthread_exit(NULL);
}


/*----- MAIN -----*/

int main(int argc, char *argv[]) {
	
	/*----- CONTROLLO DEI PARAMETRI PASSATI -----*/
	
	//nome del file di configurazione
	char * filename = malloc(50*sizeof(char));
	if (filename == NULL) {
		perror("nella malloc");
		exit(EXIT_FAILURE);
	}
	//se sono presenti meno di 3 argomenti stampo metodo di chiamata
	if (argc < 3) {
		usage("chatty\n");
		return -1;
	}
	else {
		//prendo il nome del file di configurazione
		strcpy(filename, argv[2]);
	}
	//controllo che il file di configurazione esista, altrimenti stampo
	//il giusto metodo di chiamata
	struct stat st = {0};
	if (stat(filename, &st) == -1) {
		printf("file di configurazione non trovato\n");
		usage("chatty\n");
		return -1;
	}
	//variabili usate per valori di ritorno o errori system calls
	int err = 0;
	int retval = 0;
	
	/*----- GESTIONE SEGNALI -----*/
	
	//ignoro SIGPIPE
    struct sigaction s;
    //inizializzazione struttura
    memset(&s, 0, sizeof(s));
    s.sa_handler = SIG_IGN;
    SYSCALL(err, sigaction(SIGPIPE, &s, NULL), "nell' ignorare sigpipe");
    //gestione dei segnali
    sigset_t sigset;
    pthread_t signh;
    //azzero la maschera sets
    SYSCALL(err, sigemptyset(&sigset), "nella sigemptyset");
    //setto a 1 quelli che voglio ascoltare
    SYSCALL(err, sigaddset(&sigset, SIGUSR1), "nella sigaddset 1");
    SYSCALL(err, sigaddset(&sigset, SIGINT), "nella sigaddset 2");
    SYSCALL(err, sigaddset(&sigset, SIGTERM), "nella sigaddset 3");
    SYSCALL(err, sigaddset(&sigset, SIGQUIT), "nella sigaddset 4");
    SYSCALL(err, sigaddset(&sigset, SIGALRM), "nella sigaddset 5");
    //cambio la maschera con quella appena definita
    SYSCALL(err, pthread_sigmask(SIG_SETMASK,&sigset,NULL),"nella pthread_sigmask");
    //genero thread gestore dei segnali passandogli la maschera
    if (pthread_create(&signh, NULL, &signal_handler, (void *)&sigset) != 0) {
    	perror("nella creazione del gestore segnali");
    	exit(EXIT_FAILURE);
    }
    
    /*----- PARSING -----*/
    
    //inizializzo struttura contenente informazioni di configurazione
    inizializza_conf(&conf);
    //attendo che sia finito il parsing delle informazioni
	while (parsing(&conf, filename) != 1) {
		sleep(1);
    }
    //creazione cartelle e file che conterrà le statistiche se non esistono
    if (stat(conf.DirName, &st) == -1) {
    	mkdir(conf.DirName, 0700);
    }
    if (stat(conf.StatFileName, &st) == -1) {
    	SYSCALL(err, open(conf.StatFileName, O_WRONLY|O_CREAT, 0777), "nella open statistiche");
    	close(err);
    }
    //libero subito la memoria occupata dal nome del file di configurazione
    free(filename);
    
    /*----- CREAZIONE TABELLE HASH E INIZIALIZZAZIONE SEMAFORI-----*/
    
    //creazione tabella hash degli utenti
 	reg_users = icl_hash_create(NTAB);
	//creazione tabella hash dei gruppi
	reg_groups = group_hash_create(NTABG);
	//inizializzazione array di semafori
	int i = 0;
	for (i=0; i<NMTX; i++) {
		pthread_mutex_init_m(&mtx[i]);
	}
	
	/*----- GENERAZIONE PIPE -----*/
	
	//descrittore di connessione pipe
	int pfd = 0;
	//Creo il path per la comunicazione con pipe
	int dimdirname = strlen(conf.DirName)*sizeof(char) + 1;
	int dimpipe = strlen("serverpipe")*sizeof(char) + 2;
	char * pathpipe = malloc(dimpipe + dimdirname);
	if (pathpipe == NULL) {
		perror("nella malloc");
		exit(EXIT_FAILURE);
	}
	//inizializzazione nome della pipe
	memset(pathpipe, 0, (dimpipe + dimdirname));
	//creo percorso della pipe con nome nella cartella DirName
	strncat(pathpipe, conf.DirName, dimpipe);
	strncat(pathpipe, "/serverpipe", dimpipe);
	//generazione di una pipe in cui tutti i thread possono comunicare quando
	//hanno finito di lavorare con un file descriptor
	if ((mkfifo(pathpipe, 0664) == -1) && errno != EEXIST) {
		perror("nella mkfifo");
		exit(EXIT_FAILURE);
	}
	//diritti di scrittura per il gruppo
	SYSCALL(retval, chmod(pathpipe, 0664), "nella chmod");
	//apertura in lettura e scrittura in modo che rimanga sempre aperta
	SYSCALL(pfd, open(pathpipe, O_RDWR, O_NONBLOCK), "nella open pipe thread");
	//l e libero usati per read su pipe
	int l = 0;
	//descrittore di connessione da settare di nuovo per la select dopo la 
	//comunicazione in pipe
	int libero = 0;
	
	/*----- CREAZIONE POOL DI THREAD -----*/
	
	pthread_t * tid = malloc((conf.ThreadsInPool - 1)*sizeof(pthread_t));
	if (tid == NULL) {
		perror("errore allocazione pool di thread");
		exit(EXIT_FAILURE);
	}
	//creo i thread passandogli il nome della pipe che poi dovranno usare
	for (i=0; i<(conf.ThreadsInPool - 1); i++) {
		//ne genero uno in meno perchè uno è il gestore dei segnali
		if ((err = pthread_create(&tid[i], NULL, &procedura_thread, (void *)pathpipe)) != 0) {
			errno = err;
			perror("nella generazione del pool di thread");
			exit(EXIT_FAILURE);
		}
	}
	
	/*----- PREPARAZIONE CONNESSIONE -----*/    

	//descrittore che metterò in coda per essere preso dai threads
    long fdc = 0;
    //indirizzo del server
	struct sockaddr_un sa;
	//descrittore connessione del server
	int fd = 0;
	//massimo descrittore di connessione
	int fdnum = 0;
	//insieme aggiornato usato dalla select
	fd_set set;
	//insieme da cui leggere con la select
	fd_set rdset;
	//inizializzazione indirizzo
	memset(&sa,'0',sizeof(sa));
	strncpy(sa.sun_path, conf.UnixPath, UNIX_PATH_MAX);
	sa.sun_family = AF_UNIX;
	//preparazione socket
	SYSCALL(fd, socket(AF_UNIX, SOCK_STREAM, 0), "nella socket");
	SYSCALL(retval, bind(fd, (struct sockaddr *)&sa, sizeof(sa)), "nella bind");
	SYSCALL(retval, listen(fd, conf.MaxConnections), "nella listen");
	//mantengo il massimo descrittore attivo in fdnum
	if (fd > fdnum) fdnum = fd;
	//azzero le maschere
	FD_ZERO(&set);
	FD_ZERO(&rdset);
	//setto a 1 il bit relativo al fd del server nella maschera
	FD_SET(fd, &set);
	FD_SET(pfd, &set);
	//la select ascolterà su fd e pfd (inizio)
	//i-esimo descrittore di connessione
	long fdi = 0;
	
	/*----- PREPARAZIONE SELECT -----*/
	
	//struttura con intervallo per select
	struct timeval time;
    time.tv_usec = 100;
    time.tv_sec = 0;
	//setto un timer per la select in modo che solo ogni mezzo secondo
	//controlli se qualche thread ha comunicato in pipe
	struct itimerval new;
	new.it_interval.tv_usec = 500;
	new.it_interval.tv_sec = 0;
	new.it_value.tv_usec = 500;
	new.it_value.tv_sec = 0;
	SYSCALL(err, setitimer(ITIMER_REAL, &new, NULL), "nel settaggio del timer");
	
	/*----- CICLO DI ESECUZIONE -----*/
	
	//ciclo finchè il gestore dei thread non comunica che è arrivato 
	//un segnale di terminazione
	while (segnale_arrivato == 0) {
		//aggiorno il rdset perchè cambia nelle esecuzioni
		rdset = set;
		if (select((fdnum + 1), &rdset, NULL, NULL, &time) != -1) {
			for (fdi = 0; fdi <= fdnum; fdi++) {
                //si guarda per ogni fdi settato a 1 nell' rdset se c' è 
                //qualcosa da leggere, cioè se una read non si bloccherebbe
                if (FD_ISSET(fdi, &rdset)) {
					//se è quello del server si fa l' accept
					if ((fdi == fd) && (fdi != pfd)) {
						//in questo caso il socket è settato a 1
						//ed è proprio quello che cercavo
						fdc = accept(fd, NULL, 0);
						if (fdc == -1) {
							perror("errore sull' accept");
							exit(EXIT_FAILURE);
						}
						//quando ho accettato setto a 1 il bit pronto
						FD_SET(fdc, &set);
						//infine aggiorno fdnum se serve
						if (fdc > fdnum) {
							fdnum = fdc;
						}
					}
					//se non è quello del server allora è uno su cui è già stata 
					//fatta l' accept e quindi pronto per essere messo in coda
					else if (fdi != pfd) {
						//metto a 0 il bit di pronto in modo che non se lo prenda
						//nessun altro thread finchè non comunico nella pipe che
						//ho finito di lavorare all' attuale richiesta
						FD_CLR(fdi, &set);
						pthread_mutex_lock_m(&mtxcoda);
						head = push(head, fdi);
						//sveglio in ogni caso tanto se la coda è piena non ci
						//sono problemi
						pthread_cond_signal_m(&condcoda);
						pthread_mutex_unlock_m(&mtxcoda);
					}
					//se sto lavorando con l' fd della pipe
					//(devo risettarlo solo se deve ancora scrivere qualcosa, 
					//controllo già fatto dal thread che ha inviato il messaggio
					//in pipe)
					else if ((fdi == pfd) && (alrm == 1)) {
						SYSCALL(l, read(pfd, &libero, sizeof(int)), "nella pipe read");
						//devo rimetterlo nel set della select perchè non ha finito
						//di comunicare richieste
					    FD_SET(libero, &set);
					    //azzero di nuovo alrm
					    alrm = 0;
					}
				}
			}
		}
		//se la select fallisce chiudo stampando l' errore
		else {
			perror("errore sulla select");
			exit(EXIT_FAILURE);
		}
	}
	
    /*----- TERMINAZIONE PROGRAMMA -----*/
	
    //attendo che tutti i thread terminino
	pthread_join(signh, NULL);
	for (i=0; i < (conf.ThreadsInPool - 1); i++){
		pthread_join(tid[i], NULL);
	}
	//libero memoria occupata dall' array contenente i thread id
	free(tid);
	//libero memoria occupata dalla tabella hash dei gruppi
	err = group_hash_destroy(reg_groups);
	if (err != 0) {
		printf("errore distruzione tabella hash gruppi\n");
		exit(EXIT_FAILURE);
	}
	//libero memoria occupata dalla tabella hash degli utenti
	err = icl_hash_destroy(reg_users);
	if (err != 0) {
		printf("errore distruzione tabella hash utenti\n");
		exit(EXIT_FAILURE);
	}
	//chiudo le connessioni
    close(pfd);
	close(fd);
	close(fdc);
	//libero memoria occupata dalle informazioni di configurazione
	free(conf.UnixPath);
	free(conf.DirName);
	free(conf.StatFileName);
	//libero memoria occupata dalla coda dei descrittori delle connessioni
	head = elimina_coda_fd(head);
	//rimuovo pipe
	remove(pathpipe);
	//libero memoria occupata dal nome della pipe
	free(pathpipe);
	printf("Server terminato correttamente\n");
    return 0;
}
