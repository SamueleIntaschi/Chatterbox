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
 * @file icl_hash.c
 *
 * Dependency free hash table implementation.
 *
 * This simple hash table implementation should be easy to drop into
 * any other peice of code, it does not depend on anything else :-)
 *
 * @author Jakub Kurzak
 */
/* $Id: icl_hash.c 2838 2011-11-22 04:25:02Z mfaverge $ */
/* $UTK_Copyright: $ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "icl_hash.h"
#include "config.h"
#include "message.h"
#include "history.h"

#include <limits.h>

/**
 * A simple string hash.
 *
 * An adaptation of Peter Weinberger's (PJW) generic hashing
 * algorithm based on Allen Holub's version. Accepts a pointer
 * to a datum to be hashed and returns an unsigned integer.
 * From: Keith Seymour's proxy library code
 *
 * @param[in] key -- the string to be hashed
 *
 * @returns the hash index
 */
unsigned int hash_pjw(void* key) {
    char *datum = (char *)key;
    unsigned int hash_value, i;

    if(!datum) return 0;

    for (hash_value = 0; *datum; ++datum) {
        hash_value = (hash_value << ONE_EIGHTH) + *datum;
        if ((i = hash_value & HIGH_BITS) != 0)
            hash_value = (hash_value ^ (i >> THREE_QUARTERS)) & ~HIGH_BITS;
    }
    return (hash_value);
}


/**
 * @function  icl_hash_create
 * @brief     Create a new hash table.
 *
 * @param[in] nbuckets -- number of buckets to create
 *
 * @returns pointer to new hash table.
 */
icl_hash_t * icl_hash_create(int nbuckets) {
    icl_hash_t *ht;
    int i;

    ht = (icl_hash_t*) malloc(sizeof(icl_hash_t));
    if(!ht) return NULL;

    ht->nentries = 0;
    ht->buckets = (icl_entry_t**)malloc(nbuckets * sizeof(icl_entry_t*));
    if(!ht->buckets) return NULL;

    ht->nbuckets = nbuckets;
    for(i=0;i<ht->nbuckets;i++)
        ht->buckets[i] = NULL;

    ht->hash_function = hash_pjw;

    return ht;
}

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
void * icl_hash_find(icl_hash_t *ht, void* key) {
    icl_entry_t * curr;
    unsigned int hash_val;

    if(!ht || !key) return NULL;

    hash_val = (* ht->hash_function)(key) % ht->nbuckets;

    for (curr=ht->buckets[hash_val]; curr != NULL; curr=curr->next) {
        if ( strcmp(curr->data, key) == 0) {
        	return(curr);
        }
    }
    return NULL;
}

/**
 * @function icl_hash_insert
 * @brief    Insert an item into the hash table.
 *
 * @param ht -- the hash table
 * @param key -- the key of the new item
 * @param data -- pointer to the new item's data
 *
 * @returns pointer to the new item.  Returns NULL on error.
 */
icl_entry_t * icl_hash_insert(icl_hash_t *ht, int fd, char*data) {
    icl_entry_t *curr;
    unsigned int hash_val;

    if(!ht || !data) return (icl_entry_t *)NULL;

    hash_val = (* ht->hash_function)(data) % ht->nbuckets;

    for (curr=ht->buckets[hash_val]; curr != NULL; curr=curr->next) {
        if (strcmp(curr->data, data) == 0) return(NULL); /* key already exists */
    } 
    /* if key was not found */
    curr = (icl_entry_t*)malloc(sizeof(icl_entry_t));
    if(!curr) return (icl_entry_t *)NULL;

    int size = MAX_NAME_LENGTH + 1;
    curr->data = malloc(size*sizeof(char));
    strncpy(curr->data, data, strlen(data) + 1);
    curr->fd = fd;
    curr->connesso = 1;
    curr->nmsgcoda = 0;
    curr->msgs = NULL;
    curr->next = ht->buckets[hash_val]; /* add at start */

    ht->buckets[hash_val] = curr;
    ht->nentries++;

    return curr;
}

/**
 * @function icl_hash_delete
 * @brief    Free one hash table entry located by key (key and data are freed using functions).
 *
 * @param ht -- the hash table to be freed
 * @param key -- the key of the new item
 * @param free_key -- pointer to function that frees the key
 * @param free_data -- pointer to function that frees the data
 *
 * @returns 0 on success, -1 on failure.
 */
int icl_hash_delete(icl_hash_t *ht, void * name) {
    icl_entry_t *curr, *prev;
    unsigned int hash_val;

    if(!ht || !name) return -1;
    hash_val = (* ht->hash_function)(name) % ht->nbuckets;

    prev = NULL;
    for (curr=ht->buckets[hash_val]; curr != NULL; )  {
        if (strcmp(curr->data, name) == 0) {
            if (prev == NULL) {
                ht->buckets[hash_val] = curr->next;
            } else {
                prev->next = curr->next;
            }
            curr->msgs = elimina_coda(curr->msgs);
            free(curr->data);
            free(curr);
            ht->nentries++;
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    return -1;
}

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
int icl_hash_destroy(icl_hash_t *ht) {
    icl_entry_t *curr, *next;
    int i;

    if(!ht) return -1;

    for (i=0; i<ht->nbuckets; i++) {
        curr = ht->buckets[i];
        while (curr!=NULL) {
            next = curr->next;
            curr->msgs = elimina_coda(curr->msgs);
            free(curr->data);
            free(curr);
            curr = next;
        }
    }

    if(ht->buckets) free(ht->buckets);
    if(ht) free(ht);

    return 0;
}

	
