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
 * @file  codafd.h
 * @author Samuele Intaschi
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore 
 * @brief contiene la struttura che implementa la coda dei descrittori di
 *        connessioni ascoltate dal main e su cui i thread lavoreranno
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * @struct node
 * @brief  contiene il descrittore della connessione che è in coda
 *
 * @var data descrittore della connessione
 * @var next puntatore al nodo successivo
 */
typedef struct node_struct {
	long data;
	struct node_struct * next;
} node;

typedef node * coda;

/**
 * @function push
 * @brief    aggiunge un nodo con descrittore di connessione in fondo alla coda
 * 
 * @param hd testa della coda a cui devo aggiungere un nodo
 * @param v  descrittore di connessione da aggiungere alla coda
 *
 * @return la testa della coda con il nodo appeso in fondo
 */
coda push(coda hd, long v);

/**
 * @function pop
 * @brief    elimina un nodo con descrittore di connessione dalla testa della coda
 *
 * @param hd testa della coda da cui eliminare un nodo
 *
 * @return la testa della coda senza il nodo che era in testa
 */
coda pop(coda hd);

/**
 * @function elimina_coda_fd
 * @brief    elimina l' intera coda dei descrittori 
 *
 * @param hd testa della coda da liberare
 *
 * @return un puntatore alla coda liberata, cioè a NULL
 */
coda elimina_coda_fd(coda hd);
