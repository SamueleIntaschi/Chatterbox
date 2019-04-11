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
 * @file config.h
 * @author Samuele Intaschi
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore 
 * @brief File contenente alcune define con valori massimi utilizzabili
 */

#if !defined(CONFIG_H_)
#define CONFIG_H_

#include <limits.h>

#define MAX_NAME_LENGTH                  32

/* aggiungere altre define qui */

/**
 * @def macro usate da icl_hash
 */
#define BITS_IN_int     ( sizeof(int) * CHAR_BIT )
#define THREE_QUARTERS  ((int) ((BITS_IN_int * 3) / 4))
#define ONE_EIGHTH      ((int) (BITS_IN_int / 8))
#define HIGH_BITS       ( ~((unsigned int)(~0) >> ONE_EIGHTH ))

/**
 * @def NMTX   numero massimo di mutex utilizzabili
 * @def NTAB   numero di caselle della tabella hash degli utenti
 * @def NTIMES numero tentativi di connessione
 * @def SECS   secondi di attesa tra due tentativi
 * @def NTABG  numero di caselle tabella hash dei gruppi
 */
#define NMTX                             64
#define NTAB                             256
#define NTIMES                           10
#define SECS                             5
#define NTABG                            24

/**
 * @def SYSCALL  esegue una system call controllando se il risultato è -1, 
 *               in quel caso esce stampando errno
 * @def SYSCALLN esegue una system call controllando se il risultato è un 
 *               puntatore a NULL, in quel caso esce stampando errno
 */
#define SYSCALL(r,c,e) if((r=c) == -1) { perror(e);exit(errno); }
#define SYSCALLN(r,c,e) if((r=c) == NULL) { perror(e);exit(EXIT_FAILURE); }

// to avoid warnings like "ISO C forbids an empty translation unit"x
typedef int make_iso_compilers_happy;

#endif /* CONFIG_H_ */
