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
 * @file  codafd.c
 * @author Samuele Intaschi
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore 
 * @brief contiene l' implementazione delle funzioni, definite in codafd.h, che
 *        gestiscono la coda dei descrittori di connessione che sono messi
 *        dal main e prelevati dai thread del pool
 */
#include "codafd.h"

/**
 * @function push
 * @brief    aggiunge un nodo con descrittore di connessione in fondo alla coda
 * 
 * @param hd testa della coda a cui devo aggiungere un nodo
 * @param v  descrittore di connessione da aggiungere alla coda
 *
 * @return la testa della coda con il nodo appeso in fondo
 *
 * Scorre tutta la lista e in fondo inserisce il nodo passato come argomento
 */
coda push(coda hd, long v) {
	coda new = malloc(sizeof(node));
	new->data = v;
	new->next = NULL;
	if (hd == NULL) return new;
	coda curr = hd;
	while (curr->next != NULL) curr = curr->next;
	curr->next = new;
	return (hd);
}

/**
 * @function pop
 * @brief    elimina un nodo con descrittore di connessione dalla testa della coda
 *
 * @param hd testa della coda da cui eliminare un nodo
 *
 * @return la testa della coda senza il nodo che era in testa
 *
 * Preleva il primo nodo della lista, spostando il puntatore alla testa al 
 * successivo
 */
coda pop(coda hd) {
	if (hd == NULL) return NULL;
	if (hd->next == NULL) {
		free(hd);
		return NULL;
	}
	else {
		coda next_next;
		next_next = hd->next;
		free(hd);
		hd = next_next;
		return (hd);
	}
}

/**
 * @function elimina_coda_fd
 * @brief    elimina l' intera coda dei descrittori 
 *
 * @param hd testa della coda da liberare
 *
 * @return un puntatore alla coda liberata, cioè a NULL
 *
 * Scorrendo tutti nodi della lista libera di volta in volta la memoria che 
 * occupano fino a liberare tutta quella occupata dalla lista
 */
coda elimina_coda_fd(coda hd) {
	if (hd == NULL) return NULL;
	if (hd->next == NULL) {
		free(hd);
		return NULL;
	}
	else {
		coda curr = hd;
		coda temp;
		while (curr!=NULL) {
			temp = curr->next;
			free(curr);
			curr = temp;
		}
		return (curr);
	}
}