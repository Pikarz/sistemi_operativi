#include "es2.h"
#include <ctype.h>

#define CLIENT_INV_prot_args " file [opts]"
#define ERRMSG_OPENFIFO "Unable to open named pipe %s because of %s"
#define CLIENT_ARGC 4

/**
 * Funzione che prende l'input letto da riga di comando.
 * */
char *getInput();

/**
 * La funzione executeProtocol() scrive il comando che vuole eseguire il client con un protocollo stabilito a priori con il server.
 * Il protocollo consiste in un prefisso e un suffisso definito in es2.h
 * executeProtocol() esegue quindi la scrittura sulla pipe condivisa con il server della stringa che conterrà il comando comprensiva di protocollo.
 * */
void executeProtocol(int cw, int argc, char *argv[]);
/**
 * La funzione è atta alla comunicazione verso il server tramite le named pipe condivise.
 * La comunicazione consiste nell'inviare la stringa di cui verrà poi elaborato l'output in funzione del comando stabilito durante la creazione del processo client
 * comunicate() è atta anche alla stampa prima dello stdout e poi dello stderr del comando eseguito dal server
 *  */
void comunicate(int cr, int cw);

int main(int argc, char *argv[]) {
    int cr, cw;
    // controllo se i parametri passati in input sono corretti
    checkArgs(argc, argv[0]);
    // vengono controllate se le fifo sono valide ed eventualmente verranno aperte
    cr=openFifo(argv[1], O_RDONLY); 
    cw=openFifo(argv[2], O_WRONLY);
    // viene eseguita la funzione che inizializza il protocollo da inviare al server per permettergli la corretta lettura del comando desiderato dal client
    executeProtocol(cw, argc, argv);
    // viene inizializzata la comunicazione verso il server
    comunicate(cr, cw);
}

void comunicate(int cr, int cw) {
    int len_input, r, i=0, flag=0;
    char *input=NULL, read_char, supp[BYTES_SPECIAL_CHARS+1] = { 0 };
    FILE *fd;
    while (1) {
        input=getInput();
        len_input=strlen(input);
        write(cw, input, len_input);
        if (strcmp(input, EXITMSG)==0) { free(input); exit(0); }
        free(input);
        fd=stdout;
        while((r=read(cr, &read_char, sizeof(char)))>0) {
            if (isascii(read_char)) { 
                fprintf(fd, "%c", read_char); fflush(fd); 
                flag=0;
            }
            else { 
                /**
                 * Se il carattere non è ascii viene analizzato completamente. Quando terminata la fase di analisi del carattere, viene controllato se corrisponde
                 * a uno dei caratteri speciali usati come flag di inizio stderr e di terminazione output e verrà quindi eseguita la parte di codice corrispettiva.
                 * */
                if (flag==0) { i=0; flag=1; }
                supp[i]=read_char; 
                supp[i+1]=0;
                if (i==BYTES_SPECIAL_CHARS-1) {
                    if (strncmp(supp, STDOUT_END, BYTES_SPECIAL_CHARS)==0) { fd=stderr; flag=0; memset(supp, 0, BYTES_SPECIAL_CHARS+1); }
                    if (strncmp(supp, END_OUT, BYTES_SPECIAL_CHARS)==0) break;
                }
                i++;
            }          
        }
    }
}

int openFifo(char *name, int flag) {
    struct stat fstat;
    int fd;
    if (stat(name, &fstat)) {
        fprintf(stderr, ERRMSG_OPENFIFO, name, strerror(errno));
        exit(80);
    }
    isPipe(name, &fstat);
    fd = open(name, flag);
    return fd;
}


void checkArgs(int argc, char *prog_name) {
    if (argc < CLIENT_ARGC) {
        fprintf(stderr, INVALID_ARGUMENTS, prog_name, CLIENT_INV_prot_args);
        exit(20);
    }
}


void executeProtocol(int cw, int argc, char *argv[]) {
    int size;
    char *prot_args=malloc(sizeof(char));
    prot_args[0]=0;
    for (int i=CLIENT_ARGC-1; i<argc; i++) {
        size=(strlen(prot_args) + strlen(argv[i]) + strlen(ARG_SEPARATOR) + 1)*sizeof(char);
        prot_args=realloc(prot_args, size);
        strcat(prot_args, argv[i]); strcat(prot_args, ARG_SEPARATOR);   // tra un argomento e l'altro viene aggiunto il carattere speciale separatore (da protocollo)
        prot_args[size-1]=0;
    }
    size=(strlen(prot_args) +2)*sizeof(char);
    prot_args=realloc(prot_args, size);
    prot_args[size-2]=NEWLINE;
    prot_args[size-1]=0;
    write(cw, prot_args, strlen(prot_args));
    free(prot_args);
}


char *getInput() {
    int i=0, r;
    char c=0, *input=NULL;
    while( c!=NEWLINE ) {   // da traccia ogni argomento termina con il carattere newline '\n'
        if (r=read(STDIN_FILENO, &c, sizeof(char))<=0) exit(0);
        input=realloc(input, i+2);
        if (input==NULL) manageError(ETYPE_REALLOC, errno);
        input[i]=c;
        i++;
    }
    input[i]=0;
    return input;
}