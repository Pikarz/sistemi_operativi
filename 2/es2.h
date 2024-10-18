#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define EXITMSG "EXIT\n"    // stringa che inidica la terminazione delle comunicazioni da parte del client e del server
#define INVALID_ARGUMENTS "Usage: %s piperd pipewr%s\n"
#define ERRMSG_NOT_FIFO "Named pipe %s is not a named pipe\n"
#define ERRMSG_SYSTEMCALL "System call %s failed because %s"
#define ETYPE_READ "read"
#define ETYPE_REALLOC "realloc"
#define ETYPE_MALLOC "malloc"

#define NEWLINE '\n'
#define BYTES_SPECIAL_CHARS 2
#define ARG_SEPARATOR "¿"
#define PROTOCOL_REGEX ".*¿"
#define STDOUT_END "¤"
#define END_OUT "¡"

/**
 * La funzione, definita sia da client che da server, è atta a controllare se il numero di argomenti con cui sono stati chiamati i processi client/server è sufficiente
 * */
void checkArgs(int argc, char *prog_name);

/**
 * La funzione, definita sia da client che da server, apre la fifo con il nome 'name'.
 * La funzione del server è atta alla creazione delle fifo.
 * La funzione del client controlla se le fifo siano fifo e non altri tipi di file.
 * */
int openFifo(char *name, int flag);

/**
 * Funzione condivisa da client e dal server che valuta se una pipe è tale oppure no.
 * */
void isPipe(char *name, struct stat *fstat) {
    if (!S_ISFIFO(fstat->st_mode)) {
        fprintf(stderr, ERRMSG_NOT_FIFO, name);
        exit(30);
    }
}

/**
 * Funzione atta alla gestione di eventuali errori creati da system call come read, malloc, etc.
 * */
void manageError(char *triggered_by, int n_error) {
    fprintf(stderr, ERRMSG_SYSTEMCALL, triggered_by, strerror(n_error));
    exit(100);
}