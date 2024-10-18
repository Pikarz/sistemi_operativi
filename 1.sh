#!/bin/bash

### Funzione che prende due numeri in input, eventualmente float, e li somma.
### Nel programma e' usata solamente per sommare due tempi gia' entrambi normalizzati in secondi, da cui il nome.
sumTimes() {
    sum=$(echo $1 $2 | awk '{ printf ("%.3f", $1+$2) }' );
    echo $sum;
}

### Funzione che prende una stringa di un formato che puo' essere d-hh:mm:ss (caso 1), hh:mm:ss (caso 2), mm:ss.ns (caso 3)
### e lo normalizza in secondi
function timeInSeconds() {
    time_ss=$(echo $1 | awk -F ':' '{ 
                                    dd=0; hh=0; mm=0; ss=0; ns=0;
                                    if ($3>=0) {                               #se il terzo field esiste saranno esclusivamente secondi e mi trovo quindi nel caso 1 o 2 
                                        mm=$2; ss=$3;                   
                                        if ($1 ~ /.*-.*/) {                    # se il primo field contiene un trattino, allora siamo nel caso 1
                                            split($1, day_hour, "-");
                                            dd=day_hour[1]; hh=day_hour[2];
                                        }
                                        else hh=$1;                            # altrimenti siamo nel caso due
                                    }
                                    else {
                                       mm=$1;
                                       split($2, seconds_nanoseconds, ".");
                                       ss=seconds_nanoseconds[1];
                                       ns=seconds_nanoseconds[2];
                                    }
                                    printf("%.3f", (dd*24*60*60) + (hh*60*60) + (mm*60) +ss + (ns/10^length(ns)) );
                                }');
}

### Funzione che estrae i tempi e altre informazioni dalle statistiche su cui e' stata chiamata.
### Queste informazioni vengono associate o sommate tra di loro quando necessario.
function processStats() {
    # variabili che conterranno le varie statistiche.    
    hpeak=0;                     # highest memory peak del file    
    tot_stime=0;                 # tempo systime totale
    tot_utime=0;                 # tempo usertime totale
    tot_etime=0;                 # tempo elapsedtime totale
    n_pi=0;                      # indice che conta i vari pi nel file
    filename=filename_$(echo $file | tr -d "/" | tr -d ".");             # filename del file. Le modifiche sono presenti per permettere di usare tale stringa come variabile "dinamica" in seguito.
    if [[ $flag_h && $(ls -l ./$file | awk '{ if($2>1) print $2 }') ]]; then               # se il flag che indica l'opzione -h e' attivo, allora controllo se il file su cui e' stata chiamata la funzione ha un link count maggiore di 1.
	declare -n nome_file=$filename;                                                    # se si' vedo se nell'array associativo [ismain] relativo al file e' presente il valore "1": se si', si tratta del file "prescelto" tra gli hardlink analizzati, altrimenti questo file sara' saltato
	if ! [[ ${nome_file[ismain]} -eq 1 ]]; then return; fi;
    fi;
    local IFS=$'\n';
    for s in $statistics; do                                            # per ogni statistica appena estratta dal file, quindi del tipo "Execution statistics at pi: system time si, user time ui, elapsed time ti, memory peak mi kB"
	((n_pi++));                                                     # aumento il conteggio di pi relativi al file
	local IFS=,;
	read -ra stats <<<"$s";
      	pi=$(echo ${stats[0]} | egrep -o "at .*: ");  pi=${pi:3:-2};    # viene estratto il pi ### NOTA: forse e' piu' elegante usare awk: DA RIVEDERE ###
	declare -n totals=total$(echo $pi | tr ' ' '_');                # creo una variabile che riferisce al pi specifico. Il cambiamento sulle stringhe e' necessario per far si' che vengano supportati pi con spazi nel loro nome
	### Nelle righe seguenti i vari tempi vengono estratti e suddivisi per tipologia. Vengono normalizzati in secondi, per poi successivamente essere aggiunti al numero totale di secondi del file e numero totale di secondi del rispettivo pi
	stime=$(echo ${stats[0]} | egrep -o $REGEX_TIME_FORMAT); timeInSeconds $stime; tot_stime=$(sumTimes $tot_stime $time_ss);
        totals[1]=$(sumTimes ${totals[1]} $time_ss);
	utime=$(echo ${stats[1]} | egrep -o $REGEX_TIME_FORMAT); timeInSeconds $utime; tot_utime=$(sumTimes $tot_utime $time_ss);
	totals[2]=$(sumTimes ${totals[2]} $time_ss); #((totals[2]+=$time_ss));
  	etime=$(echo ${stats[2]} | egrep -o $REGEX_TIME_FORMAT); timeInSeconds $etime; tot_etime=$(sumTimes $tot_etime $time_ss);
        totals[3]=$(sumTimes ${totals[3]} $time_ss);
	mpeak=$(echo ${stats[3]} | egrep -o $REGEX_MEM_FORMAT);	if [[ "$hpeak" -lt "$mpeak" ]]; then hpeak="$mpeak"; fi;    # i peak di memoria vengono gestiti in maniera diversa dai tempi. Ci interessa il peak piu' alto del rispettivo file...
	if [[ "$hpeak" -lt "$mpeak" ]]; then hpeak="$mpeak"; fi;
	if [[ ! ${totals[4]} || ${totals[4]} -lt $mpeak ]]; then totals[4]=$mpeak; fi;                                      # ... e rispettivo pi
        fnamepi=$(echo $filename$pi | tr ' ' '_');
	declare -Ag $fnamepi="( [systime]=$stime [usertime]=$utime [eltime]=$etime [mem]=$mpeak )";                         # inserisco nell'array associativo che distingue la coppia (nomefile_pi) le varie informazioni appena estratte
	all_pi+="$pi"$'\n';                                                                                                 # il pi corrente viene aggiunto a una variabile che, a termine, li conterra' tutti ( sara' necessaria per generateFile() )
    done;
    declare -n fname=$filename;
    declare -Ag $filename="( [n_pi]=$n_pi [systimetot]=$tot_stime [usertimetot]=$tot_utime [eltimetot]=$tot_etime [memhigh]=$hpeak )";     # a termine dell'analisi delle statistiche del file, i rispettivi tempi totali e highest memory peak vengono salvati nell'array associativo, unico per ogni file
    tbs_files+="$n_pi $mpeak $file"$'\n';                                                                                                  # variabile necessaria al giusto ordinamento dei file ( sara' necessaria in generaFile() )
}

### Filtra i contenuti del file passato in input come primo argomento
function processFile() {
    statistics=$(cat $file | egrep "$REGEX_LINE_FORMAT");                                                                 # Da ogni file vengono estratte tutte le righe consone al formato e passate alla funzione che le processera'
    if [[ $statistics ]]; then processStats; fi;
}

### Scorre i file all'interno della directory passata in input.
### Se il file e' una directory --> la funzione viene chiamata su quella directory ricorsivamente
### altrimenti controllo se il file ha la giusta estensione passata in -e. Se si', su quel file viene chiamata la funzione checkFile
function checkFolder() {
    perm=$(stat -c "%a" $1)     #permessi in ottale. il comando -c vuole un argomento, che specifica formato delle stat. In questo caso "%a" estrae i permessi
    if [[ ! $perm =~ $VALID_PERM ]]; then return; fi; # se e' una directory senza i permessi corretti, appendo alla variabile che poi sara' scritta nel file descriptor 5
    formatted_di=$(echo $1 | tr -d '.' | tr -d '/' );
    declare -n dirname="dir$formatted_di";
    if [[ $dirname == 1 ]]; then return;  fi;   ## check per vedere se la cartella gia' e' stata processata (e.g. passata piu' volte in input)
    dirname=1;
    for file in $(ls -a $1);
    do
	file=$1/$file;
	if [[ -d $file ]]; then checkFolder $file; continue; fi;
        if echo "$file" | grep -q "$REGEX_CHECK_FOLDER" && ! [[ -h $file && $flag_l ]]; then processFile;  fi;
    done
}

### Funzione che genera effettivamente il contenuto che sara' l'output del programma.
### Vengono ordinati tutti i file e tutti i pi come richiesto dalla traccia.
### Sulla base di questo ordine, vengono estratte le informazioni precedentemente calcolate e stampate dove desidera l'utente 
function generateFile() {
    local IFS=$'\n';
    sorted_pi=$(echo "$all_pi" | LC_ALL=C sort -u);                  # LC_ALL=C e' per permettere la portabilita'. Ho notato che la macchina virtuale ordinava effettuava l' ordinamento in maniera diversa dalle soluzioni
    firstline="Filename,";                                           
    sortfiles=$(LC_ALL=C sort -k1nr,1 -k2n,2 -k3,3 <<<$tbs_files);   # LC_ALL=C e' per permettere la portabilita'. Ho notato che la macchina virtuale ordinava effettuava l' ordinamento in maniera diversa dalle soluzioni
    fl_flag=1;                                                       # flag che indica che ci troviamo alla prima riga del file
    for file in $sortfiles; do {
	fname=filename_$(echo $file | awk '{print $3}');
	fileline="${fname:9}",;
	fname_array=$(echo $fname | tr -d '.' | tr -d '/');          
	declare -n fname=$(echo $fname | tr -d '.' | tr -d '/');
	declare -n fname_a=$fname_array;                             # in queste righe uso ancora la mia convenzione per permettermi di usare i nomi dei file come array associativi dinamicamente
	for pi in $sorted_pi; do
	    if [[ $fl_flag ]]; then firstline+="$pi"systime,"$pi"usertime,"$pi"eltime,"$pi"mem,; fi;     # se siamo nella prima riga, scrivo le informazioni relative all'intestazione
	    fnamepi=$(echo $fname_array$pi | tr ' ' '_');
	    declare -n array_fnamepi=$fnamepi;                                                           
	    fileline+=${array_fnamepi[systime]},${array_fnamepi[usertime]},${array_fnamepi[eltime]},${array_fnamepi[mem]},;    # ancora mie convenzioni per permettermi di usare array associativi
	done;
	if [[ fl_flag -eq 1 ]]; then
	    firstline+="systimetot,usertimetot,eltimetot,memhigh";             # termino l'intestazione se ci troviamo alla prima riga
	    echo $firstline>>$o_param;
	    fl_flag=0;                                                         # ora la prima riga e' terminata, quindi abbasso il flag.
	fi;
	# nelle seguenti variabili inserisco i vari tempi totali del file. La somma +0.0 e' stata inserita perché awk mi convertiva automaticamente numeri troppo grandi con notazione scientifica.
	sys=$(echo ${fname_a[systimetot]} | awk '{ printf ("%.3e" , $1+0.0) }' | tr ',' '.');   
	user=$(echo ${fname_a[usertimetot]} | awk '{ printf ("%.3e" , $1+0.0) }' | tr ',' '.');
	elapsed=$(echo ${fname_a[eltimetot]} | awk '{ printf ("%.3e" , $1+0.0) }' | tr ',' '.');
	fileline+=$sys,$user,$elapsed,${fname_a[memhigh]};
	echo $fileline>>$o_param;                                              # a termine della scrittura della riga, la appendo al file o stdout (dove desidera l'utente)
    }
    done;
    lastline="TOTALS,";                                                        # inizio analisi dell'ultima riga
    for pi in $sorted_pi; do
	declare -n totals=total$(echo $pi | tr ' ' '_');
	t1=$(echo ${totals[1]} | awk '{ printf ("%.3e", $1+0.0) }' | tr ',' '.');
        t2=$(echo ${totals[2]} | awk '{ printf ("%.3e", $1+0.0) }' | tr ',' '.');
	t3=$(echo ${totals[3]} | awk '{ printf ("%.3e", $1+0.0) }' | tr ',' '.');   # ancora mie convenzioni per uso di array associativi. La somma +0.0 e' stata inserita perché awk mi convertiva automaticamente numeri troppo grandi con notazione scientifica.
        lastline+=$t1,$t2,$t3,${totals[4]},;
    done;
    echo ${lastline:0:-1}>>$o_param;             # appendo finalmente l'ultima riga 
}


# La funzione, chiamata quando sono presenti hard links, trova tutti gli hard links presenti in una directory tramite il loro inode number.
# I vari file vengono ordinati, e a quello con ordine lessicografico minore viene settato il flag [ismain] a "1".
# Questo permettera' di distinguere i gli hard link "non main", quindi da saltare ai fini dell' analisi del file ( in processStats() )
## funzione necessaria per l' opzione -h.
function checkInodes() {
    prev_inode=-1;                   # mia convenzione per indicare che non sono ancora stati analizzati inode
    for dir in $dirs; do
      all_hl+=$(find $dir -type f -regex ".*.$e_param" -ls | awk '{ if($4>1) print $1, $11 }')$'\n';     # vengono estratti tutti gli hardlink delle varie directory passate in input
    done;
    all_hl=$(echo "$all_hl" | sort -t$'\n' -k1n,1 -k2); # la lista degli hardlink viene ordinata
    local IFS=$'\n';
    for inode_file in $all_hl; do                                  # per ogni coppia (inode, filename)
	inode=$(echo $inode_file | awk '{ print $1}');             # 'scoppio' la coppia
	filename=filename_$(echo $inode_file | awk '{ print $2}' | tr -d '.' | tr -d '/' );
        if [[ ! $prev_inode -eq $inode ]]; then                      # se il l'inode number precedente e' diverso da quello su cui sto iterando, significa che quello dove avviene l' iterazione e' quello con ordine lessicografico maggiore, e quindi il main
	    declare -Ag $filename="( [ismain]=1 )";
	    prev_inode=$inode;
	fi;
    done;
}

### MAIN ###

# inizializzazioni di varie variabili, costanti e flag necessarie per il programma
e_param=log;
o_param="/dev/stdout";
STD_ERRMSG="Uso: ../1.sh [-h] [-l] [-e string] [-r regex] [-o file] [dirs]";
OPT_ERRMSG="Non e' possibile dare contemporaneamente -l e -h";

while getopts ":e:o:hl" args; do      # vengono analizzate le varie opzioni passate dall' utente
        case "${args}" in
        e) e_param=$OPTARG;       # estensione
	   ;;
	o) o_param=$OPTARG;      # nome di file di output
	   ;;
        h) flag_h=1;                 # flag che indica che e' stata inserita l'opzione -h
	   ;;
	l) flag_l=1;                 # analogo a sopra ma con -l
	   ;;
	:) ## In caso di mancati argomenti nelle opzioni uso i default (gia' definiti sopra)
	   ;;
	*) flag_invalid_opt=1;       # per quando l' utente inserisce un'opzione non esistente
	   ;;
    esac
done

### Inizializzazione di altre costanti 
REGEX_CHECK_FOLDER=".*\.$e_param";
REGEX_TIME_FORMAT="([1-9][0-9]*-)?(2[0-3]|[0-1][0-9])?:?[0-5][0-9]:[0-5][0-9]\.?[1-9]?[0-9]*";
REGEX_MEM_FORMAT="[1-9][0-9]*";
REGEX_LINE_FORMAT="Execution statistics at .*: system time $REGEX_TIME_FORMAT, user time $REGEX_TIME_FORMAT, elapsed time $REGEX_TIME_FORMAT, memory peak $REGEX_MEM_FORMAT kB";
VALID_PERM="[0-7](3|7)(3|7)"; 
exit=0; # variabile atta a finalizzare l' exit status del programma
#if [[ $flag_h ]]; then checkInodes; fi;   # se il flag dell' opzione -h e' attivo, eseguo la funzione che gestira' i vari inodes tra tutte le directory.
for dir in "${@:OPTIND}"; do
    Dir=$(echo $dir | egrep -o ".*[0-9a-zA-Z]");
    if [[ !(-e "$Dir") ]]; then fd3msg+="L'argomento $Dir non esiste\n"; ((exit++)); continue; fi; # appendo alla variabile che poi sara' scritta nel file descriptor 3
    if [[ !(-d "$Dir") ]]; then fd4msg+="L'argomento $Dir non e' una directory\n"; ((exit++)); continue; fi; # appendo alla variabile che poi sara' scritta nel file descriptor 4
    if [[ -h "$Dir" ]]; then Dir=$(readlink -f $Dir); continue; fi;   # se la directory è un link simbolico, prendo il path della directory originale
    perm=$(stat -c "%a" $Dir)     #permessi in ottale. il comando -c vuole un argomento, che specifica formato delle stat. In questo caso "%a" estrae i permessi
    if [[ ! $perm =~ $VALID_PERM ]]; then fd5msg+="I permessi $perm dell'argomento $Dir non sono quelli richiesti\n"; ((exit++)); continue; fi; # se e' una directory senza i permessi corretti, appendo alla variabile che poi sara' scritta nel file descriptor 5
    dirs+=$Dir$'\n';
done;

if [[ -z $Dir ]]; then # se non vengono passate directory, il programma viene eseguito sulla directory .
    Dir=.;
    if [[ $flag_h -eq 1 ]]; then checkInodes; fi;
    checkFolder $Dir;
fi;

echo Eseguito con opzioni $*; # stampo in stdout il messaggio che indica come l'utente ha eseguito il programma
exit_octal=$(printf '%o\n' $exit);
if [[ "flag_invalid_opt" -eq 1 ]]; then echo $STD_ERRMSG>&2; echo $exit_octal>&2; exit 10; fi;
if [[ "$flag_l" && "$flag_h" ]]; then echo $OPT_ERRMSG>&2; echo $STD_ERRMSG>&2; exit=10; exit_octal=$(printf '%o\n' $exit); echo $exit_octal>&2; exit 10; fi; # echo 12 rappresenta l'exit status 10 in ottale.
if [[ $flag_h -eq 1 ]]; then checkInodes; fi;   # se il flag dell' opzione -h e' attivo, eseguo la funzione che gestira' i vari inodes tra tutte le directory.
for dir in $dirs; do
    checkFolder $dir;
done;
0
generateFile;
# Nelle righe seguenti scrivo nei vari file descriptor le stringhe ordinate come richiesto dalla traccia.
if [[ $fd3msg ]]; then echo "$(echo -e $fd3msg | LC_ALL=C sort -ru -t $'\n')">&3; fi;
if [[ $fd4msg ]]; then echo "$(echo -e $fd4msg | LC_ALL=C sort -ru -t$'\n')">&4; fi;
if [[ $fd5msg ]]; then echo "$(echo -e $fd5msg | LC_ALL=C sort -ru -t $'\n')">&5; fi;
echo $exit_octal >&2; # in stderr scrivo il numero di directory saltate per le varie ragioni, cioe' l'exit status, in ottale
exit $exit; # exit status
