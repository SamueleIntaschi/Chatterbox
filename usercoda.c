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
 * @file  usercoda.c
 * @author Samuele Intaschi
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore 
 * @brief contiene le funzioni per gestire la lista degli utenti iscritti 
 *        a un gruppo 
 */
 
#include <string.h>
#include <stdlib.h>
#include "usercoda.h"
#include "config.h"

/**
 * @function find user in group
 * @brief restituisce 1 se l' utente fa parte della coda, 0 altrimenti
 *
 * @param name nome dell' utente da cercare nella coda
 * @param coda in cui cercare l' utente
 *
 * Scorre la lista e quando trova l' utente cercato ritorna 1,
 * se arriva alla fine della lista senza trovarlo ritorna 0
 */
int user_group_find(char * name, usercoda coda) {
	usernode * curr = coda;
	while(curr != NULL) {
		if (strcmp(name, curr->username) == 0) {
			return 1;
		}
		curr = curr->next;
	}
	return 0;
}

/**
 * @function add user at group
 * @brief aggiunge un utente a una coda di utenti
 *
 * @param name nome dell' utente da inserire nella coda
 * @param coda coda in cui inserire l' utente
 *
 * Aggiunge un nodo, cioè un utente, in testa ad una lista di utenti
 */
usercoda user_add_group(char * name, usercoda coda) {
	usernode * new = (usernode *)malloc(sizeof(usernode));
	int size = MAX_NAME_LENGTH + 1;
	new->username = malloc(size*sizeof(char));
	strncpy(new->username, name, strlen(name)+1);
	new->next = coda;
	coda = new;
	return coda;
}

/**
 * @function remove user from group
 * @brief rimuove un utente da una coda di utenti
 *
 * @param name nome dell' utente da rimuovere dalla coda
 * @param coda coda da cui rimuovere l' utente
 *
 * Cerca un nodo nella lista scorrendola e quando lo trova libera 
 * la memoria occupata da questo e scorre i puntatori
 */    
usercoda user_delete_from_group(char * name, usercoda coda) {
	usernode * next_next = coda->next;
	if (strcmp(coda->username, name) == 0) {
		free(coda->username);
		free(coda);
		return next_next;
	}	
	usernode * curr = coda;
	usernode * prev = coda;
	while(curr != NULL) {
		next_next = curr->next;
		if (strcmp(name, curr->username)) {
			free(curr->username);
			free(curr);
			prev->next = next_next;
			return coda;
		}
		prev = curr;
		curr = next_next;
	}
	return NULL;
}

/**
 * @function destroy group queue
 * @brief rimuove la coda di utenti iscritti a un gruppo
 *
 * @param coda coda da rimuovere
 *
 * Distrugge tutta lista liberando la memoria occupata da ogni nodo
 */
usercoda user_destroy_group(usercoda coda) {
	if (coda == NULL) return NULL;
	if (coda->next == NULL) {
		free(coda->username);
		free(coda);
        return NULL;
	}
	usercoda curr = coda;
	usercoda nxt;
	while (curr != NULL) {
		nxt = curr->next;
		free(curr->username);
		free(curr);
		curr = nxt;
	}
	return (coda);
}
