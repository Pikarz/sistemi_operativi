#!/usr/bin/awk -f

function analyzeLine(line,text) {
	analyzing=false;
	if (match(line, REG1) || match(line, REG2)) {
		date_time=substr(line, RSTART, RLENGTH);
		if (match(line, REG1)) line=gensub(REG1, "", "g", line);
		else line=gensub(REG2, "", "g", line);
		if (match(line, text)) {
			analyzing=extractDate(date_time);
		}
	}
	return analyzing;
}

function extractTime(raw_time) { # funzione che estrae il tempo
	split(raw_time,time_array,":");
	hh=time_array[1];
	mm=time_array[2];
	ss=time_array[3];
	time=hh mm ss;
	return time;
}

function extractDate(raw_date) {
	REGDAYS="(Mon|Tue|Wed|Thu|Fri|Sat|Sun)";	// regex dei giorni della settimana
	if(match(raw_date, REG1)) {	// se matcha con la regex che non contiene l'anno
		YY=2022;	// l'anno di base è sempre 2022
		i=1
		if (match($i, REGDAYS)) i++;
		split("Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec",months);	// splitto mese per mese inserendoli nell'array months
		month=gensub(/[a-z]*=/, "", "g", $i); i++;	// rimuovo ciò che c'è prima dell'=
		for (j in months) { 
			if (month==months[j]) { // cerco il numero corretto di mese
				if (length(j)==1) { MM=0j;}	//se j è di una sola cifra, ci metto lo zero davanti
				else { MM=j; }	// altrimenti MM è proprio come j
			}
		}
		nday=$i; i++;	// ora il numero di giorno è contenuto proprio in i
		time=extractTime($i);
	}

	else if(match(raw_date, REG2)) { 	// altrimenti se matcha con la regex2 è del formato che contiene l'anno
		split($1, yearmonthday, "/");	// spezzo sul / 
		YY=gensub(/[a-z]*=/, "", "g", yearmonthday[1]);
		MM=yearmonthday[2];
		nday=yearmonthday[3];
		time=extractTime($2);	// alla fine mi rimane il tempo che quindi lo estraggo
	}
	date=YY MM nday hh mm ss // metto la data tutta insieme e la restituisco
	return date;
	
}

BEGIN {
	REG1="((Mon|Tue|Wed|Thu|Fri|Sat|Sun) )?(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) (([0-2][0-9])|3[0-1] ) ((([0-1][0-9])|2[0-3]):(0[0-9]|[1-5][0-9]):(0[0-9]|[1-5][0-9]))";	// regex che convalida i formati di un tipo (che non iniziano con un anno)
	REG2="([1-2][0-9][0-9][0-9])/((0[0-9])|(1[0-2]))/(([0-2][0-9])|(3[0-1])) ((([0-1][0-9])|2[0-3]):(0[0-9]|[1-5][0-9]):(0[0-9]|[1-5][0-9]))([.]([0-9]*))?" // regex che convalida il formati comprensivi dell'anno
  	while (++i < ARGC) {
    	begin=begin" "ARGV[i]; }
    if (i<=2) { print "Errore: dare almeno 2 file di input" >"/dev/stderr"; exit 10; }
    printf "Eseguito con argomenti%s\n", begin;
} 

{
	while (ARGV[1]==FILENAME && FNR==NR) { 	// FNR è il record number nel file corrente, NR è il numero totale di record number. Voglio agire solo sul primo file, ovvero il file di configurazione, per estrarre date e testo da analizzare nei file di log passati in input
		if(match($0, /from=.*/) && length(from)==0) {	// se esiste from e non è stato analizzato
			from=extractDate($0); 	// estraggo la data
		}
		if(match($0, /to=.*/) && length(to)==0) { // se esiste to e non è stato analizzato
			to=extractDate($0); // estraggo la data
		}
		if(match($0, /text=.*/) && length(text)==0) { // se esiste text e non è stato analizzato
			text=gensub(/text=/, "", "g", $0);	// estraggo solo il testo, rimuovendo 'text='
		}
		next;
	}
	c_date=analyzeLine($0, text);	// terminate di esser analizzate le variabili necessarie, analizzo
	if (c_date!=false) {
		if( (c_date>=from || length(from)==0 ) && (c_date<=to || length(to)==0) ) {	# se c_date è nell'intervallo from e to (se sono definiti)
			print FILENAME": "$0;	# stampo il suo file e la linea che soddisfa la condizione
		}
	}
	
}