generateFamily() {
    if [[ $1 != "" ]]; then {
	threads=$(ps h -T q $1 -o lwp | awk 'BEGIN {ORS=" "}{print}' | xargs );    # estraggo i threads del processo
	if [[ -n $threads ]]; then running=1; fi;  # se almeno uno è in esecuzione allora il processo è ancora in esecuzione
	child=$(ps h --ppid $1 -o pid |  awk 'BEGIN {ORS=" "}{print}' | xargs );   # genero tutti i figli del processo, ovvero quelli col parent pid pari al pid
	forest+="$1"\("$threads"\)": ${child}"$'\n'; # aggiungo alla variabile finale la famiglia di processi del processo
	local IFS=' ';
	for c in $child; do {
	    generateFamily "$c";   # eseguo ricorsivamente per ogni figlio 
	}
	done;
    };
    fi;
}


function generateForest() {
   local IFS='_';
   for p in $pids; do
       generateFamily $p;   # per ogni p in pids genero la sua famiglia di processi
   done;
   if [[ $running -ne 1 ]]; then echo "Tutti i processi sono terminati"; exit $nc; fi;
   forest=$(echo "$forest" | sort -n -t'(' -k1 ); # ordino in ordine crescente di pid
   echo ${forest:1}; # stampo la famiglia finale di processi
}


function analyzeText() {
  local IFS=$'\n';
  cmds=( $(awk -F'|' '{ print $1; }' $F) ); # vengono estratti tutti i comandi, ovvero qualsiasi valore prima della pipe
  redirs=( $(awk -F'|' '{ print $2; }' $F) );   # vengono estratte tutte le redirezioni
  i=0
  for cmd in ${cmds[@]}; do {   # per ogni cmd
      cmd_and_redirs="$cmd ";
      cmd_and_redirs+=$(echo ${redirs[$i]} | awk -F'|' '{ print $1, $2 }' | tr -d '()' | tr ',' '>' | xargs);
      cmd_and_redirs+=' &'  # creo la variabile che sarà data in pasto ad eval che consiste di: comando, tutte le redirezioni necessarie formattate a dovere, & per mandarla in bg
      eval "$cmd_and_redirs";
      pids+=$!"_";  # mantengo in una variabile tutti i pids dei processi lanciati
      ((i++))
  }
  done;
  echo ${pids::-1}>&3;  # viene eliminato l'ultimo carattere (un underscore di troppo)
  while [[ ! -f "./done.txt" ]]; do sleep $sleeping_time; done; # dormo finché non trovo il file richiesto
  echo 'File done.txt trovato'>&4;
  generateForest; 
}

### MAIN

sleeping_time=$1;
F=$2;
ERRMSG="Uso: 2.sh sampling commandsfile" 
if [[ -z $sleeping_time || -z $F ]]; then echo $ERRMSG>&2; exit 30; fi;
analyzeText;
exit 0;
