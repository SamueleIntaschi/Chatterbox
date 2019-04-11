#!/bin/bash

#se i parametri sono 0
if [ $# -eq 0 ]; then
    echo "usa: $(basename $0) fileconf numero_non_negativo"
    exit 1
    
#se c'e' solo un parametro
elif [ $# -eq 1 ]; then
    #se questo è -help
    if [ "$1" = "-help" ]; then
    	echo "usa: $(basename $0) fileconf numero_non_negativo:\n
    	- 0 per elenco dei file nella cartella specificata nel file di configurazione\n
    	- n > 0 per fare un archivio con i file più vecchi di n minuti ed eliminarli "
		exit 1
    else
    	echo "Troppo pochi argomenti"
    	exit 1
    fi

#se ci sono più di due parametri
elif [ $# -gt 2 ]; then
	#se almeno uno è -help
	for i in "$@"; do
		if [ "$i" = "-help" ]; then
			echo "usa: $(basename $0) fileconf numero_non_negativo:\n
    	    - 0 per elenco dei file nella cartella specificata nel file di configurazione\n
    	    - n > 0 per fare un archivio con i file più vecchi di n minuti ed eliminarli "
			exit 1
		fi
	done
	echo "Troppi argomenti"
	exit 1
#se ci sono due parametri
elif [ $# -eq 2 ]; then
	#se il primo parametro è -help
	if [ "$1" = "-help" ]; then
    	echo "usa: $(basename $0) fileconf numero_non_negativo:\n
    	- 0 per elenco dei file nella cartella specificata nel file di configurazione\n
    	- n > 0 per fare un archivio con i file più vecchi di n minuti ed eliminarli "
		exit 1
    #se il secondo parametro è -help
    elif [ "$2" = "-help" ]; then
    	echo "usa: $(basename $0) fileconf numero_non_negativo:\n
    	- 0 per elenco dei file nella cartella specificata nel file di configurazione\n
    	- n > 0 per fare un archivio con i file più vecchi di n minuti ed eliminarli "
		exit 1
    #se il secondo parametro non esiste come file
    elif [ ! -f $1 ]; then
    	echo "Il file $filename non esiste o non e' un file regolare"
    	exit 1
    #se il file esiste ma è vuoto
    elif [ ! -s $1 ]; then
    	echo "Il file $filename e' vuoto"
    	exit 1
    #se il secondo parametro non è un numero e è < 0
    elif [ ! "$2" -ge 0 2>/dev/null ]; then
    	echo "Ricevuto numero non valido"
    	exit 1
    fi
fi

filename=$1
num=$2
tofind=DirName
COLUMN=19 #punto in cui sta il nome cercato


#apro il file da cui devo leggere
exec 3<$filename

while read -u 3 linea; do
	#cerco la parola DirName
	if [ "${linea:0:7}" = "$tofind" ]; then
		#il nome cercato si trova su questa riga a partire 
		#dalla colonna 19
		nomedir=${linea:19}
	fi
done

#apro la cartella
cd $nomedir
#controllo se è vuota
if [ $? != 0 ]; then
	echo "Cartella vuota"
	exit 1
elif [ $num == 0 ]; then 
	ls
	exit 1
else
    for file in $(command find $nomedir/* -mmin $num); do
	#find $nomedir -mmin $num | for file in $nomedir/*; do
		tar -rf chatty.tar $(basename $file)
		if [ $? -ne 0 ]; then
			echo "Errore archiviazione file"
		else 
			if [ -d $file ]; then
				rm -r $file
			else 
				rm $file
			fi
		fi
	done;
	if $(command gzip chatty.tar); then
		echo "Programma terminato"
	else
		echo "Non ci sono file più vecchi di n minuti"
	fi
								
fi
	

