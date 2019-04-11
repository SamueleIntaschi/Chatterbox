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
 * @file  configurazione.c
 * @author Samuele Intaschi
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
 * originale dell'autore 
 * @brief contiene l' implementazione delle funzioni utili ad eseguire il parsing
 *        del file di configurazione
 */
#include "configurazione.h"

/**
 * @function inizializza_conf
 * @brief    inizializza la struttura dati configurazione passata
 *           come parametro
 * 
 * @param conf puntatore alla struttura dati da inizializzare
 *
 * Alloca la memoria necessaria a contenere i nomi e inizializza a zero il resto
 */
void inizializza_conf(configurazione * conf) {
	SYSCALLN(conf->StatFileName, malloc(50*sizeof(char)), "nell' allocazione StatFileName");
	SYSCALLN(conf->UnixPath,malloc(50*sizeof(char)), "nell' allocazione UnixPath");
	SYSCALLN(conf->DirName, malloc(50*sizeof(char)), "nell' allocazione DirName");
	conf->MaxConnections = 0;
	conf->ThreadsInPool = 0;
	conf->MaxMsgSize = 0;
	conf->MaxFileSize = 0;
	conf->MaxHistMsgs = 0;
}

/**
 * @function pulizia_stringa
 * @brief    rimpiazza il carattere ritorno a capo e il carattere speciale
 *           il cui codice ASCII è 9 con 0
 * 
 * @param str la stringa da cui eliminare il ritorno a capo
 * 
 * @return la stringa dopo la sostituzione
 *
 * 
 * Scorre la stringa passata e se alla fine trova i caratteri il cui codice
 * ASCII è 10 o 9 li sostituisce con 0
 */
char * pulizia_stringa(char * str) {
    int dim = strlen(str) + 1;
    int i = 0;
    for (i=0; i<dim; i++) {
    	if (str[i] == 10 || str[i] == 9) {
    		str[i] = 0;
    	    break;
    	}
    }
    return str;
}

/**
 * @function parsing
 * @brief    estrae da filename le informazioni di configurazione e le
 *           mette in conf
 *
 * @param conf     la struttura dati in cui mettere le informazioni prese
 * @param filename il nome del file da cui prendere le informazioni
 *
 * @return 1 per segnalare che ha terminato al chiamante, in caso di errori
 *           termina il programma
 *
 * Apre il file di configurazione passato come argomento, lo scorre riga per
 * riga e se il primo carattere è un # passa direttamente alla riga successiva 
 * perchè si tratta di un commento. 
 * Se invece non lo è, tronca la riga al primo spazio, cioè la prima parola e 
 * controlla se è un campo della struct configurazione, cioè un' informazione
 * importante, se lo è passa alla parola successiva, cioè l' =, e infine a
 * quella ancora successiva che è l' informazione cercata.
 * A questo punto se serve un intero la stringa contenente la terza parola viene
 * convertita altrimenti soltanto copiata. 
 */
int parsing(configurazione * conf, char * filename) {
	FILE * fdconf = 0;
	char ** bufconf = malloc(100*sizeof(char *));
	if (bufconf == NULL) {
		perror("nella malloc");
		exit(EXIT_FAILURE);
	}
	int i = 0; 
	int c = 0;
	//allocazione e inizializzazione buffer che deve contenere le righe del file
	for (i=0; i<100; i++) {
		SYSCALLN(bufconf[i], malloc(82 * sizeof(char)), "nel parsing");
		c = 0;
		for (c=0; c<81; c++) {
			bufconf[i][c] = 0;
		}
	}
	//apertura file
	SYSCALLN(fdconf, fopen(filename, "r"), "nell' apertura del file di configurazione");
	//stringa temporanea
    char * var;
    i = 0;
    //variabile che indica a quale parola sono nella stringa mentre scorro
    int p = 0;
    //array temporaneo usato per le singole parole
    char * sup = malloc(10*sizeof(char));
    while ((!feof(fdconf)) && (i < 40)) {
    	if ((fgets(bufconf[i], 81, fdconf)) == NULL) {
    		break;
    	}
    	//caso commento: si toglie e non si incrementa i
    	if (bufconf[i][0] == '#') {
    		c = 0;
    		for (c=0; c<81; c++) {
    			bufconf[i][c] = 0;
    		}
        }
        //caso in cui la riga contiene informazioni importanti
        else {
        	p = 0;
        	//tronco la riga al primo spazio
        	var = strtok(bufconf[i], " ");
        	var = pulizia_stringa(var);
        	//inserisco in variabile, alla riga 15 trovo il path
        	//in base alla parole che trovo decido a quale variabile assegnarla
        	if (strcmp(var, "UnixPath") == 0/* || i==16-1*/) {
        		//var è divisa in 3 stringhe: nome, =, dato
        		while (var != NULL) {
        			//quando p = 2 sono alla terza parola che è 
        			//l'informazione cercata
        			if (p == 2) strcpy(conf->UnixPath, var);
        			//scorro alla prossima parola
        			var = strtok(NULL, " ");
        			p++;
        			//quando p<2 sono al nome o all' =
        		}
        		//metto la parola fino allo spazio nella variabile giusta 
        		conf->UnixPath = pulizia_stringa(conf->UnixPath);
        		conf->UnixPath = strtok(conf->UnixPath, " ");
        	}
        	//con MaxConnections funziona solo lasciando alcuni spazi alla fine
        	//perchè dopo MaxConnections c' è un tab
        	else if (strcmp(var, "MaxConnections") == 0) {
        		while (var != NULL) {
        			if (p == 2) strcpy(sup, var);
        			var = strtok(NULL, " ");
        			p++;
        		}
        		//trasformo la parola sup, che è un numero in questo caso, in un int
        		conf->MaxConnections = atoi(sup);
        	}
        	else if (strcmp(var, "ThreadsInPool") == 0) {
        		while (var != NULL) {
        			if (p == 2) strcpy(sup, var);
        				var = strtok(NULL, " ");
        				p++;
        		}
        		conf->ThreadsInPool = atoi(sup);
        	}
        	else if (strcmp(var, "MaxMsgSize") == 0) {
        		while (var != NULL) {
        			if (p == 2) strcpy(sup, var);
        			var = strtok(NULL, " ");
        			p++;
        		}
        		conf->MaxMsgSize = atoi(sup);
        	}
        	else if (strcmp(var, "MaxFileSize") == 0) {
        		while (var != NULL) {
        			if (p == 2) strcpy(sup, var);
        			var = strtok(NULL, " ");
        			p++;
        		}
        		conf->MaxFileSize = atoi(sup);
        	}
        	else if (strcmp(var, "MaxHistMsgs") == 0) {
        		while (var != NULL) {
        			if (p == 2) strcpy(sup, var);
        			var = strtok(NULL, " ");
        			p++;
        		}
        		conf->MaxHistMsgs = atoi(sup);
        	}
        	else if (strcmp(var, "DirName") == 0) {
        		while(var != NULL) {
        			if (p == 2) strcpy(conf->DirName, var);
        			var = strtok(NULL, " ");
        			p++;
        		}
        		conf->DirName = pulizia_stringa(conf->DirName);
        		conf->DirName = strtok(conf->DirName, " ");
        	}
        	else if (strcmp(var, "StatFileName") == 0) {
        		p = 0;
        		while(var != NULL) {
        			if (p == 2) strcpy(conf->StatFileName, var);
        			var = strtok(NULL, " ");
        			p++;
        		}
        		conf->StatFileName = pulizia_stringa(conf->StatFileName);
        		conf->StatFileName = strtok(conf->StatFileName, " ");
        	}
        }
        i++;
    }
    //chiusura del file
    if ((fclose(fdconf))==-1) {
    	perror("errore in chiusura file");
    	exit(EXIT_FAILURE);
    }
    //liberazione della memoria
    for (i=0; i<100; i++) {
    	free(bufconf[i]);
    }
    free(bufconf);
    free(sup);
    return 1;
}
