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
 * @file  mutex.c
 * @author Samuele Intaschi
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore 
 * @brief contiene l' implementazione delle funzioni usate per gestire
 *        la mutua esclusione
 */
 
#include <errno.h>

#include "mutex.h"

/**
 * @function pthread_mutex_lock_m
 * @brief    controlla se la procedura di acquisizione della lock
 *           va a buon fine, altrimenti stampa l' errore
 *
 * @param mutex il mutex su cui effettuare la lock
 */
void pthread_mutex_lock_m (pthread_mutex_t * mtx){
	int err;
    if((err = pthread_mutex_lock(mtx))!=0){
    	errno = err;
        perror("pthread_mutex_lock\n");
    }
}

/**
 * @function pthread_mutex_unlock_m
 * @brief    controlla se la procedura di rilascio della lock va a 
 *           buon fine, altrimenti stampa l' errore
 *
 * @param mutex il mutex su cui effettuare la unlock
 */
void pthread_mutex_unlock_m (pthread_mutex_t * mtx){
	int err;
    if((err = pthread_mutex_unlock(mtx))!=0){
    	errno = err;
    	perror("pthread_mutex_unlock\n");
    }
}

/**
 * @function pthread_mutex_init_m
 * @brief    inizializza un mutex con gli attributi di default, se si verifica 
 *           un errore lo stampa
 * 
 * @param mutex il mutex da inizializzare
 */
void pthread_mutex_init_m (pthread_mutex_t * mutex){
	int err;
	if((err = pthread_mutex_init(mutex, NULL))!=0){
		errno = err;
		perror("pthread_mutex_init\n");
	}
}

/**
 * @function pthread_cond_wait_m
 * @brief    esegue una wait su una variabile di condizione e, se 
 *           ci sono stati errori, li stampa
 *
 * @param cond la variabile di condizione su cui bloccarsi
 * @param mutex il mutex che si possiede al momento della chiamata
 */
void pthread_cond_wait_m (pthread_cond_t * cond, pthread_mutex_t * mutex) {
	int err;
	if ((err = pthread_cond_wait(cond, mutex)) != 0) {
		errno = err;
		perror("wait\n");
	}
}

/**
 * @function pthread_cond_signal_m
 * @brief    esegue una signal su una variabile di condizione e, se ci sono
 *           errori, li stampa
 *
 * @param cond la variabile di condizione su cui altri thread sono bloccati
 */
void pthread_cond_signal_m (pthread_cond_t * cond) {
	int err;
	if ((err = pthread_cond_signal(cond)) != 0) {
		errno = err;
		perror("pthread_cond_signal\n");
	}
}

/**
 * @function pthread_mutex_broadcast_m
 * @brief    esegue una broadcast su una variabile di condizione e, se ci sono
 *           stati errori, li stampa
 * 
 * @param cond la variabile di condizione su cui sono bloccati i thread
 */
void pthread_cond_broadcast_m (pthread_cond_t * cond) {
	int err;
	if ((err = pthread_cond_broadcast(cond)) != 0) {
		errno = err;
		perror("pthread_cond_broadcast\n");
	}
}
