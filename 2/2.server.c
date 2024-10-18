#include "es2.h"
#include <regex.h>

#define SERVER_INV_ARGS ""
#define ERRMSG_CREATEFIFO "Unable to create named pipe %s because of %s\n"
#define SERVER_ARGC 3
#define ETYPE_PIPE "pipe"
#define ETYPE_EXECVP "execvp"
#define ETYPE_REGEX "regex"
#define ETYPE_CALLOC "calloc"
#define ENG_LANGUAGE "LANG=C"

/**
 * La funzione createFifo() crea fifo se non sono già esistenti.
 * Se lo sono, essa controlla se i file esistenti sono fifo. Se non lo sono, la funzione interromperà il processo restituendo come exit status 40.
 * */
void createFifo(char *name);

/**
 * La funzione esegue il comando passato in cmd con input 'input'.
 * execute() si serve delle pipe per permettere l'esecuzione del comando con execvp.
 * La funzione inizializza all'interno tre pipe: 
 * in[2], che sarà atta a mantenere l'input con cui sarà eseguito il comando;
 * out[2] che manterrà il risultato, normalmente scritto su stdout, del comando;
 * err[2] che salverà ciò che il comando scriverà su stderr.
 * 
 * La funzione è anche atta alla stampa dei risultati lato server, e scriverà gli stessi risultati sulla named pipe condivisa con il client.
 * I due risultati lato client, essendo distinti, saranno divisi da un carattere speciale non-ASCII definito in es2.h. Un altro carattere speciale indicherà la fine dell'output al client.
 * */
void execute(char **cmd, int sw, char *input);

/**
 * La funzione getInput() prende in input la named pipe da cui deve leggere il client e legge il suo contenuto.
 * La funzione restituisce un char* che corrisponderà all'input appena letto. 
 * */
char *getInput(int sr);

/**
 * parseCommand() è la funzione che viene avviata quando un nuovo client sopraggiunge alla comunicazione con il server.
 * Essa elaborerà il primo input ricevuto dal client il quale conterrà il comando e le opzioni desiderate dal client.
 * La funzione ha come invariante l'utilizzo del protocollo stabilito a priori tra client e server: un carattere speciale (non ASCII) dividerà i vari argomenti.
 * parseCommand() restituisce quindi un array di stringhe pronto per essere elaborato da exevp
 * */
char **parseCommand(char **cmd, char *buffer);

/**
 * La funzione listen() rimane in ascolto di eventuali richieste in entrata da client che hanno effettuato la comunicazione sulle stesse pipe con cui è stato inizializzato il server.
 * La funzione provvede a elaborare il comando ricevuto in input, identificando una nuova connessione client, e poi esegue tutti i comandi inviati dallo stesso client con il comando prestabilito.
 * Un comando verrà identificato grazie a un protocollo stabilito a priori tra client e server: per permettere l'identificazione di un nuovo comando, quindi l'adesione di un nuovo client alla comunicazione, esso sarà dotato di un carattere speciale tra un argomento e l'altro.
 * In funzione del protocollo, verrà elaborata una regex. Se la regex farà match, il comando ricevuto in input si tratta del primo comando di un client e verrà quindi elaborato per poter essere eseguito durante i prossimi input.
 * */
void listen(int sr, int sw, char **cmd, char *file_sr, char *file_sw);

/**
 * La funzione serverClose finalizza la fase di chiusura del server chiudendo le due fifo (in lettura e in scrittura) e liberando la memoria precedentemente allocata per l'esecuzione del server.
 * Questa funzione cancella anche le fifo create dal server.
 * */
void serverClose(char **cmd, int sr, int sw, char *file_sr, char *file_sw);

/**
 * La funzione libera la memoria dell'array di puntatori che verrà passato a execvp in execute(). Essa viene chiamata quando un nuovo client vuole comunicare con il server.
 * */
void freeClientCMD(char **cmd);

int main(int argc, char *argv[]) {
    char **cmd=NULL;
    int sr, sw;
    putenv(ENG_LANGUAGE);
    // controllo se i parametri passati in input sono corretti
    checkArgs(argc, argv[0]);
    // creo le due fifo: una per la comunicazione proveniente dal client, l'altra per la comunicazione verso il client.
    createFifo(argv[1]); createFifo(argv[2]);
    // apro le due fifo
    sw=openFifo(argv[1], O_WRONLY);
    sr=openFifo(argv[2], O_RDONLY); 
    /**
     * Il server si mette in ascolto di eventuali richieste da client che utilizzano sr e sr.
     * La chiusura del server sarà stabilita da una stringa "EXIT" da parte del client
     * */
    listen(sr, sw, cmd, argv[1], argv[2]);
}

void listen(int sr, int sw, char **cmd, char *file_sr, char *file_sw) {
    char *input;
    int iscmd;
    regex_t regex;
    while(1){
        input=getInput(sr);
        if (strlen(input)==0) { free(input); continue; }
        if ( (iscmd=regcomp(&regex, PROTOCOL_REGEX, 0))!=0 ) manageError(ETYPE_REGEX, errno);
        iscmd=regexec(&regex, input, 0, NULL, 0);
        regfree(&regex); 
        if (iscmd==0) {
            /**
            *  se l'input contiene i caratteri speciali (non-ASCII) utilizzati come protocollo dal client e dal server per la gestione di nuovi comandi,
            * allora significa che la stringa appena letta contiene un nuovo comando e quindi viene gestito.
            * */
            cmd=parseCommand(cmd, input);
            free(input);
            continue;
        }
        // se il nuovo input equivale al comando stabilito da client/server atto a terminare la comunicazione, viene avviata la procedura di chiusura del server.
        if(strcmp(input, EXITMSG)==0) { free(input); serverClose(cmd, sr, sw, file_sr, file_sw); }
        execute(cmd, sw, input);
    }
}

void createFifo(char *name) {
    struct stat fstat;
    if (stat(name, &fstat)) {
        if ( mkfifo(name, S_IRUSR | S_IWUSR )) {
            fprintf(stderr, ERRMSG_CREATEFIFO, name, strerror(errno));
            exit(40);
        }
    }
    else isPipe(name, &fstat);
}

int openFifo(char *name, int flag) {
    int fd;
    fd = open(name, flag);
    return fd;
}

void checkArgs(int argc, char *prog_name) {
    if (argc != SERVER_ARGC) {
        fprintf(stderr, INVALID_ARGUMENTS, prog_name, SERVER_INV_ARGS);
        exit(10);
    }
}


char *getInput(int fd) {
    char c_read, *buffer=malloc(sizeof(char));
    int rread, char_i=0;
    // Viene letto un carattere alla volta per evitare di esser vincolati da una lunghezza massima di un buffer.
    while( rread=read(fd, &c_read, sizeof(char))>0 ) {
        if (c_read==NEWLINE) { 
            buffer=realloc(buffer, char_i+2);
            if(buffer==NULL) manageError(ETYPE_REALLOC, errno);
            buffer[char_i]=NEWLINE;
            char_i++;
            break; 
        }
        buffer=realloc(buffer, char_i+1);
        if(buffer==NULL) manageError(ETYPE_REALLOC, errno);
        buffer[char_i]=c_read;
        char_i++;
    }
    buffer[char_i]=0;
    return buffer;
}

char **parseCommand(char **cmd, char *parsing_cmd) {
    char *end, *tofree, *tok;
    int j=0;
    freeClientCMD(cmd);
    tofree=end=strdup(parsing_cmd);
    while ( (tok = strsep(&end, ARG_SEPARATOR)) != NULL ) {
        if (strlen(tok)<2) continue;
        cmd=realloc(cmd, (j+3)*sizeof(char*));
        cmd[j]=strdup(tok);
        j++;
    }
    free(tofree);
    cmd[j]=0;
    return cmd;
}

void execute(char **cmd, int sw, char *input) {
    int in[2], out[2], err[2], pid;

    // vengono aperte le pipe atte alla comunicazione con il processo figlio, il quale eseguirà il comando richiesto dal client
    if (pipe(in) < 0) manageError(ETYPE_PIPE, errno);
    if (pipe(err) < 0) manageError(ETYPE_PIPE, errno);   
    if (pipe(out) < 0) manageError(ETYPE_PIPE, errno);

    write(in[1], input, strlen(input));  // viene preparato l'input del comando per il figlio
    free(input);
    if( (pid=fork() == 0)) {    // processo figlio
        close(in[1]); close(out[0]); close(err[0]); // vengono chiuse le pipe che non saranno utilizzate
        dup2(in[0], STDIN_FILENO); dup2(out[1], STDOUT_FILENO); dup2(err[1], STDERR_FILENO); // vengono reindirizzati stdin, stdout e stderr
        execvp(cmd[0], cmd);
        manageError(ETYPE_EXECVP, errno);
    }
    else {          // processo padre
        char c; int r; 
        close(in[1]); close(in[0]); close(out[1]); close(err[1]);  // vengono chiuse le pipe che non saranno utilizzate
        
        /**
         * nelle righe seguenti viene controllato l'output di exec del processo figlio, in cui viene letto
         * un carattere alla volta per evitare di esser vincolati da una dimensione massima di buffer
         * */
        while (r=read(out[0], &c, sizeof(char))>0) { printf("%c", c); fflush(stdout); write(sw, &c, sizeof(char)); }   
        close(out[0]); 
        write(sw, STDOUT_END, strlen(STDOUT_END));

        while (r=read(err[0], &c, sizeof(char))>0) { printf("%c", c); fflush(stdout); write(sw, &c, sizeof(char)); }
        close(err[0]);
        write(sw, END_OUT, BYTES_SPECIAL_CHARS);
    }
}

void serverClose(char **cmd, int sr, int sw, char *file_sr, char *file_sw) {
    freeClientCMD(cmd);
    free(cmd);
    close(sw); close(sr);
    remove(file_sr); remove(file_sw);
    exit(0);
}

void freeClientCMD(char **cmd) {
    if (cmd==NULL) return;
    int i=0;
    while(cmd[i]!=NULL) {
        free(cmd[i]);
        i++;
    }
}