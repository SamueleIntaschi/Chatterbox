/*
 * chatterbox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 * Si dichiara che il programma è, in ogni sua parte, opera originale di:
 * Nome: Samuele Intaschi
 * Matricola: 523864
 * e-mail: sam18195@hotmail.it
 */
/**
 * @file  group_hash.h
 * @brief header del file per quanto riguarda la gestione della tabella hash
 *        contenente i gruppi iscritti 
 */
 
#include <stdio.h>
#include <usercoda.h>
 
/**
 * @struct group_entry
 * @brief  contiene tutte le informazioni relative a un gruppo
 *
 * @var data     nome del gruppo
 * @var admin    utente che ha creato il gruppo
 * @var u_coda   coda degi nomi degli utenti attualmente iscritti al gruppo
 * @var n_users  numero di users attualmente iscritti al gruppo
 * @var next     puntatore al gruppo successivo nella tabella hash
 */
typedef struct group_hash_entry {
	char * data;
	char * admin;
	usercoda u_coda;
	int n_users;
	struct group_hash_entry* next;
} group_entry;

/**
 * @struct group_hash_table
 * @brief  rappresenta la tabella hash dei gruppi
 * 
 * @var ngroups   numero di gruppi contenuti, cioè iscritti
 * @var nbuckets  numero di caselle
 * @var buckets   puntatore al puntatore alla coda delle entry contenute 
 *                nella casella
 * @function hash_function funzione per calcolare l' hash value
 */
typedef struct group_hash_table {
	int ngroups;
	int nbuckets;
    group_entry **buckets;
    unsigned int (*hash_function)(void *);
} group_hash_t;

/**
 * @function group_hash_create
 * @brief    crea la tabella hash che conterrà i gruppi
 *
 * @param nbuckets numero di caselle con cui creare la tabella
 * 
 * @return il puntatore alla tabella appena creata
 */
group_hash_t * group_hash_create(int nbuckets);

/**
 * @function group_hash_find
 * @brief    cerca un gruppo all' interno della tabella hash
 *
 * @param ht il puntatore alla tabella hash dei gruppi
 * @param key il nome del gruppo da cercare
 *
 * @return il puntatore alla entry contenente il gruppo (e le sue informazioni)
 *         se fa parte della tabella, NULL altrimenti
 */
void * group_hash_find(group_hash_t * ht, void * key);

/**
 * @function group_hash_insert
 * 
 * @param ht      il puntatore alla tabella hash dei gruppi
 * @param data    il nome del gruppo da inserire 
 * @param creator il nome del creatore del gruppo
 *
 * @return il puntatore alla entry della casella appena creata
 */
group_entry * group_hash_insert(group_hash_t * ht, char * data, char * creator);

/**
 * @function group_hash_destroy
 * @brief    distrugge la tabella hash dei gruppi, liberando tutta la memoria
 *           che occupa
 *
 * @param ht il puntatore alla tabella hash dei gruppi da distruggere
 *
 * @return 0 se tutto va bene, -1 in caso di errore (la tabella passata è null)
 */
int group_hash_destroy(group_hash_t * ht);

/**
 * @function group_hash_delete
 * @brief    elimina un gruppo dalla tabella hash
 *
 * @param ht il puntatore alla tabella hash dei gruppi
 * @param key il nome del gruppo da rimuovere
 *
 * @return 0 se tutto va bene, -1 in caso di errore (ht = null oppure gruppo 
 *         non esistente)
 */
int group_hash_delete(group_hash_t *ht, void* key);

/**
 * @function remove_user_from_groups
 * @brief    rimuove l' utente da tutti i gruppi di cui fa parte
 *
 * @param ht il puntatore alla tabella hash dei gruppi
 * @param name il nome dell' utente da eliminare da tutti i gruppi 
 */
void remove_user_from_groups(group_hash_t *ht, char * name);