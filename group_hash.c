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
 * @file  group_hash.c
 * @brief contiene l' implementazione delle funzioni usate per gestire la tabella
 *        hash dei gruppi
 */
 
#include <string.h>
#include <stdlib.h>
#include "group_hash.h"
#include "config.h"


/**
 * @function hash_pjw
 * @brief    data una key, in questo caso il nome del gruppo, calcola in che casella
 *           della tabella hash stanno i suoi dati
 *
 * @param key il nome el gruppo
 * 
 * @return hash_value se tutto va bene, 0 se il nome del gruppo è NULL
 */
static unsigned int hash_pjw(void* key) {
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
 * @function group_hash_create
 * @brief    crea la tabella hash che conterrà i gruppi
 *
 * @param nbuckets numero di caselle con cui creare la tabella
 * 
 * @return il puntatore alla tabella appena creata
 *
 * Alloca lo spazio necessario a contenere una tabella hash con il numero di
 * tabelle passato come argomento e la inizializza
 */
group_hash_t * group_hash_create(int nbuckets) {
	group_hash_t *ht;
    int i;

    ht = (group_hash_t*) malloc(sizeof(group_hash_t));
    if(!ht) return NULL;

    ht->nbuckets = 0;
    ht->buckets = (group_entry**)malloc(nbuckets * sizeof(group_entry*));
    if(!ht->buckets) return NULL;

    ht->nbuckets = nbuckets;
    for(i=0;i<ht->nbuckets;i++)
        ht->buckets[i] = NULL;

    ht->hash_function = hash_pjw;

    return ht;
}

/**
 * @function group_hash_find
 * @brief    cerca un gruppo all' interno della tabella hash
 *
 * @param ht il puntatore alla tabella hash dei gruppi
 * @param key il nome del gruppo da cercare
 *
 * @return il puntatore alla entry contenente il gruppo (e le sue informazioni)
 *         se fa parte della tabella, NULL altrimenti
 *
 * Scorre tutte le entry, cioè i gruppi, facenti parte della tabella e
 * restituisce il puntatore a quello cercato, se esiste, a NULL altrimenti
 */
void * group_hash_find(group_hash_t * ht, void * key) {
	group_entry * curr;
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
 * @function group_hash_insert
 * 
 * @param ht      il puntatore alla tabella hash dei gruppi
 * @param data    il nome del gruppo da inserire 
 * @param creator il nome del creatore del gruppo
 *
 * @return il puntatore alla entry della casella appena creata
 *
 * Calcola in quale casella inserire il gruppo il cui nome è passato come
 * argomento e lo inserisce allocando lo spazio necessario ed inizializzandolo
 */
group_entry * group_hash_insert(group_hash_t * ht, char * data, char * creator) {
	group_entry *curr;
    unsigned int hash_val;

    if(!ht || !data) return (group_entry *)NULL;

    hash_val = (* ht->hash_function)(data) % ht->nbuckets;

    for (curr=ht->buckets[hash_val]; curr != NULL; curr=curr->next) {
        if (strcmp(curr->data, data) == 0) return(NULL); /* key already exists */
    } 
    /* if key was not found */
    curr = (group_entry*)malloc(sizeof(group_entry));
    if(!curr) return (group_entry *)NULL;

    int size = MAX_NAME_LENGTH + 1;
    curr->data = malloc(size*sizeof(char));
    strncpy(curr->data, data, strlen(data) + 1);
    curr->next = ht->buckets[hash_val]; /* add at start */
    curr->admin = malloc(size*sizeof(char));
    curr->n_users = 0;
    curr->u_coda = NULL;
    strncpy(curr->admin, creator, strlen(data) +1);
    curr->u_coda = user_add_group(creator, curr->u_coda);
    ht->buckets[hash_val] = curr;
    ht->ngroups++;

    return curr;
}

/**
 * @function group_hash_destroy
 * @brief    distrugge la tabella hash dei gruppi, liberando tutta la memoria
 *           che occupa
 *
 * @param ht il puntatore alla tabella hash dei gruppi da distruggere
 *
 * @return 0 se tutto va bene, -1 in caso di errore (la tabella passata è null)
 *
 * Scorre tutte le caselle della tabella hash dei gruppi liberando di volta 
 * in volta la memoria che occupano
 */
int group_hash_destroy(group_hash_t * ht) {
	group_entry *curr, *next;
    int i;

    if(!ht) return -1;

    for (i=0; i<ht->nbuckets; i++) {
        curr = ht->buckets[i];
        while (curr!=NULL) {
            next = curr->next;
            curr->u_coda = user_destroy_group(curr->u_coda);
            free(curr->data);
            free(curr->admin);
            free(curr);
            curr = next;
        }
    }

    if(ht->buckets) free(ht->buckets);
    if(ht) free(ht);

    return 0;
}

/**
 * @function group_hash_delete
 * @brief    elimina un gruppo dalla tabella hash
 *
 * @param ht il puntatore alla tabella hash dei gruppi
 * @param key il nome del gruppo da rimuovere
 *
 * @return 0 se tutto va bene, -1 in caso di errore (ht = null oppure gruppo 
 *         non esistente)
 *
 * Scorre le caselle della tabella hash cercando il gruppo il cui nome è passato 
 * come argomento e quando lo trova, se esiste, elimina la entry che lo 
 * rappresenta, liberando la memoria che occupa
 */
int group_hash_delete(group_hash_t *ht, void* name) {
	group_entry *curr, *prev;
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
            curr->u_coda = user_destroy_group(curr->u_coda);
            free(curr->data);
            free(curr->admin);
            free(curr);
            ht->ngroups--;
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    return -1;
}

/**
 * @function remove_user_from_groups
 * @brief    rimuove l' utente da tutti i gruppi di cui fa parte
 *
 * @param ht il puntatore alla tabella hash dei gruppi
 * @param name il nome dell' utente da eliminare da tutti i gruppi 
 *
 * Scorre tutte le entry della tabella hash, i gruppi, e per ognuna di 
 * queste scorre la lista di utenti iscritti.
 * Ogni volta che trova il nome dell' utente da eliminare, lo elimina.
 */
void remove_user_from_groups(group_hash_t *ht, char * name) {
	group_entry *curr;
    int i;

    for (i=0; i<ht->nbuckets; i++) {
        curr = ht->buckets[i];
        while (curr!=NULL) {
        	if (strcmp(name, curr->admin) == 0) {
        		group_hash_delete(ht, curr->data);
        	}
        	else if (user_group_find(name, curr->u_coda) == 1) {
        		curr->u_coda = user_delete_from_group(name, curr->u_coda);
        		curr->n_users--;
        	}
            curr = curr->next;
        }
    }
}

