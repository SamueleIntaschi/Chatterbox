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
#include "history.h"
/**
 * @file  history.c
 * @author Samuele Intaschi
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore 
 * @brief Contiene l' implementazione delle funzioni per aggiungere o eliminare
 *        messaggi dalla history di un utente, implementata come una coda
 */

/**
 * @function pushm
 * @brief    Inserisce un messaggio in fondo alla history di un utente
 * 
 * @param hd   testa della coda dei messaggi
 * @param hdr  header del messaggio da inserire in coda
 * @param temp puntatore alla parte dati del messaggio da inserire in coda
 *
 * @return il puntatore alla coda con il nuovo messaggio appeso in fondo
 *
 * 
 */
msgcoda pushm(msgcoda hd, message_hdr_t hdr, message_data_t * tmp) {
	msgcoda new = malloc(sizeof(msgnode));
	(new->data).data.buf = malloc(tmp->hdr.len * sizeof(char));
	strncpy((new->data).data.buf, tmp->buf, tmp->hdr.len);
	(new->data).data.hdr = tmp->hdr;
	(new->data).hdr = hdr;
	new->next = NULL;
	if (hd == NULL) return new;
	msgcoda curr = hd;
	while (curr->next != NULL) curr = curr->next;
	curr->next = new;
	return (hd);
}

/**
 * @function popm
 * @brief    Elimina un messaggio dalla testa della coda
 *
 * @param hd puntatore alla testa della coda
 * 
 * @return il puntatore alla coda senza il nodo di testa, la testa è
 *         spostata al nodo successivo
 * 
 *
 */
msgcoda popm(msgcoda hd){
	msgcoda next_next;
	if (hd == NULL) return NULL;
	if (hd->next == NULL){
		free((hd->data).data.buf);
		free(hd);
		return NULL;
	}
	next_next = hd->next;
	free((hd->data).data.buf);
	free(hd);
	hd = next_next;
	return (hd);
}

/**
 * @function elimina_coda
 * @brief    Elimina tutta la coda dei messaggi 
 * 
 * @param hd puntatore alla testa della coda
 *
 * @return il puntatore alla coda vuota (NULL)
 * 
 *
 */
msgcoda elimina_coda(msgcoda hd) {
	if (hd == NULL) return NULL;
	if (hd->next == NULL) {
		if (hd->data.data.hdr.len != -1) {
			free(hd->data.data.buf);
			hd->data.data.hdr.len = -1;
		}
		free(hd);
		return NULL;
	}
	msgcoda curr = hd;
	msgcoda nxt;
	while (curr != NULL) {
		nxt = curr->next;
		if (curr->data.data.hdr.len != -1) {
			free(curr->data.data.buf);
			curr->data.data.hdr.len = -1;
		}
		free(curr);
		curr = nxt;
	}
	return (hd);
}

