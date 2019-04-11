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
 * @file
 *
 * Header file for icl_hash routines.
 *
 */
/* $Id$ */
/* $UTK_Copyright: $ */

#ifndef icl_hash_h
#define icl_hash_h

#include <stdio.h>
#include <message.h>
#include <config.h>

#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif

/**
 * @struct icl_entry_t
 * @brief  contiene tutte le informazioni relative a un utente
 *
 * @var fd       descrittore di connessione usato dall' utente
 * @var connesso indica se è connesso (1) o no (0)
 * @var msgs     puntatore alla coda dei messaggi
 * @var nmsgcoda numero di messaggi nella history
 * @var data     nome dell' utente
 * @var next     puntatore all' utente successivo nella casella della
 *               tabella hash
 */
typedef struct icl_entry_s {
    int fd;
    int connesso;
    msgcoda msgs;
    int nmsgcoda;
    void *data;
    struct icl_entry_s* next;
} icl_entry_t;

/**
 * @struct icl_hash_t
 * @brief  rappresenta una casella della tabella hash 
 * 
 * @var nbuckets numero di caselle
 * @var nentries numero di entries presenti nella tabella al momento
 * @var buckets  puntatore al puntatore alla coda delle entry contenute 
 *               nella casella
 * @function hash_function funzione per calcolare l' hash value
 */
typedef struct icl_hash_s {
    int nbuckets;
    int nentries;
    icl_entry_t **buckets;
    unsigned int (*hash_function)(void*);
} icl_hash_t;


/**
 * @function  icl_hash_create
 * @brief     Create a new hash table.
 *
 * @param[in] nbuckets -- number of buckets to create
 *
 * @return pointer to new hash table.
 */
icl_hash_t * icl_hash_create(int nbuckets);

/**
 * @function icl_hash_find
 * @brief    Search for an entry in a hash table.
 *
 * @param ht -- the hash table to be searched
 * @param key -- the key of the item to search for
 *
 * @returns pointer to the data corresponding to the key.
 *   If the key was not found, returns NULL.
 */
void * icl_hash_find(icl_hash_t *, void* );

/**
 * @function icl_hash_insert
 * @brief    Insert an item into the hash table.
 *
 * @param ht -- the hash table
 * @param key -- the key of the new item
 * @param data -- pointer to the new item's data
 *
 * @return pointer to the new item.  Returns NULL on error.
 */
icl_entry_t * icl_hash_insert(icl_hash_t *, int, char *);

/**
 * @function icl_hash_destroy
 * @brief    Free hash table structures (key and data are freed using functions).
 *
 * @param ht -- the hash table to be freed
 * @param free_key -- pointer to function that frees the key
 * @param free_data -- pointer to function that frees the data
 *
 * @return 0 on success, -1 on failure.
 */
int icl_hash_destroy(icl_hash_t * ht);

/**
 * @function icl_hash_delete
 * @brief    Free one hash table entry located by key (key and data are freed using functions).
 *
 * @param ht -- the hash table to be freed
 * @param key -- the key of the new item
 *
 * @return 0 on success, -1 on failure.
 */
int icl_hash_delete( icl_hash_t *ht, void* key);


#define icl_hash_foreach(ht, tmpint, tmpent, kp, dp)    \
    for (tmpint=0;tmpint<ht->nbuckets; tmpint++)        \
        for (tmpent=ht->buckets[tmpint];                                \
             tmpent!=NULL&&((kp=tmpent->key)!=NULL)&&((dp=tmpent->data)!=NULL); \
             tmpent=tmpent->next)


#if defined(c_plusplus) || defined(__cplusplus)
}
#endif

#endif /* icl_hash_h */
