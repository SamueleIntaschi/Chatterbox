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
 * @file  history.h
 * @author Samuele Intaschi
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore 
 * @brief Header file per gestione della history di un utente
 */
#define _POSIX_C_SOURCE 200809L
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>

#include "message.h"
#include "icl_hash.h"

/**
 * @function pushm
 * @brief    Inserisce un messaggio in fondo alla history di un utente
 * 
 * @param hd   testa della coda dei messaggi
 * @param hdr  header del messaggio da inserire in coda
 * @param temp puntatore alla parte dati del messaggio da inserire in coda
 *
 * @return il puntatore alla coda con il nuovo messaggio appeso in fondo
 */
msgcoda pushm(msgcoda hd, message_hdr_t hdr, message_data_t * temp);

/**
 * @function popm
 * @brief    Elimina un messaggio dalla testa della coda
 *
 * @param hd puntatore alla testa della coda
 * 
 * @return il puntatore alla coda senza il nodo di testa, la testa è
 *         spostata al nodo successivo
 */
msgcoda popm(msgcoda hd);

/**
 * @function elimina_coda
 * @brief    Elimina tutta la coda dei messaggi 
 * 
 * @param hd puntatore alla testa della coda
 *
 * @return il puntatore alla coda vuota (NULL)
 */
msgcoda elimina_coda(msgcoda hd);



