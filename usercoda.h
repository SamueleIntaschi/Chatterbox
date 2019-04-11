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
 * @file  usercoda.h
 * @author Samuele Intaschi
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore 
 * @brief Header file per la gestione della lista degli utenti iscritti a
 *        un gruppo
 */
 
#include <stdio.h>

/**
 * @struct nodo utente
 * @brief  nodo rappresentante un utente iscritto al gruppo
 *
 * @var username nome dell' utente
 * @var next puntatore al nodo successivo della lista
 */
typedef struct user_node_struct {
	void * username;
	struct user_node_struct * next;
} usernode;

typedef usernode * usercoda;

/**
 * @function find user in group
 * @brief restituisce 1 se l' utente fa parte della coda, 0 altrimenti
 *
 * @param name nome dell' utente da cercare nella coda
 * @param coda in cui cercare l' utente
 */
int user_group_find(char * name, usercoda coda);


/**
 * @function add user at group
 * @brief aggiunge un utente a una coda di utenti
 *
 * @param name nome dell' utente da inserire nella coda
 * @param coda coda in cui inserire l' utente
 */
usercoda user_add_group(char * name, usercoda coda);

/**
 * @function remove user from group
 * @brief rimuove un utente da una coda di utenti
 *
 * @param name nome dell' utente da rimuovere dalla coda
 * @param coda coda da cui rimuovere l' utente
 */      
usercoda user_delete_from_group(char * name, usercoda coda);

/**
 * @function destroy group queue
 * @brief rimuove la coda di utenti iscritti a un gruppo
 *
 * @param coda coda da rimuovere
 */
usercoda user_destroy_group(usercoda coda);