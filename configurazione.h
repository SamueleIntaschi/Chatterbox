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
 * @file  configurazione.h
 * @author Samuele Intaschi
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore 
 * @brief contiene la struttura dati che conterrà le informazioni di
 *        configurazione e le dichiarazioni delle funzioni usate per estrarle
 *        dal file di configurazione
 */
#define _POSIX_C_SOURCE 200809L
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>

#include "config.h"
 
/**
 * @struct configurazione
 * @brief  contiene le informazioni che saranno estratte dal file di
 *         configurazione
 * 
 * @var MaxConnections numero massimo di connessioni pendenti
 * @var ThreadsInPool  numero di thread nel pool
 * @var MaxMsgSize     dimensione massima di un messaggio testuale (numero di
 *                     caratteri)
 * @var MaxFileSize    dimensione massima di un file accettato dal server
 *                     (in kilobytes)
 * @var MaxHistMsgs    numero massimo di messaggi che il server ricorda per
 *                     ogni client
 * @var UnixPath       path utilizzato per la creazione del socket AF_UNIX
 * @var DirName        directory dove memorizzare i files da inviare agli utenti 
 * @var StatFileName   file nel quale verranno scritte le statistiche del server
 */
typedef struct conf_struct {
	int MaxConnections;
	int ThreadsInPool;
	int MaxMsgSize;
	int MaxFileSize;
	int MaxHistMsgs;
	char * UnixPath;
	char * DirName;
	char * StatFileName;
} configurazione;

/**
 * @function inizializza_conf
 * @brief    inizializza la struttura dati configurazione passata
 *           come parametro
 * 
 * @param conf puntatore alla struttura dati da inizializzare
 */
void inizializza_conf(configurazione * conf);

/**
 * @function pulizia_stringa
 * @brief    rimpiazza il carattere ritorno a capo con 0
 * 
 * @param str la stringa da cui eliminare il ritorno a capo
 * 
 * @return la stringa dopo la sostituzione
 */
char * pulizia_stringa(char * str);

/**
 * @function parsing
 * @brief    estrae da filename le informazioni di configurazione e le
 *           mette in conf
 *
 * @param conf     la struttura dati in cui mettere le informazioni prese
 * @param filename il nome del file da cui prendere le informazioni
 *
 * @return 1 per segnalare che ha terminato al chiamante, in caso di errori
 *           termina il programma
 */
int parsing(configurazione * conf, char * filename);
