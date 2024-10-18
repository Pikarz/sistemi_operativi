#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define INVALID_ARGUMENTS "Usage: p file_in file_out dd_args\n"
#define ERR_FINPUT "Unable to %s file %s because of %s"
#define ERRMSG_SYSTEMCALL "System call %s failed because %s"
#define ERRMSG_BADFORMAT "Wrong format for input binary file F at byte %d"
#define ETYPE_PIPE "pipe"
#define ETYPE_EXECVP "execvp"
#define ETYPE_READ "read"
#define ETYPE_MALLOC "malloc"
#define ETYPE_REALLOC "realloc"
#define READ_BINARY "rb"
#define WRITE_BINARY "wb"
#define READ_FROM "read from"
#define WRITE_TO "write to"
#define MIN_ARGS 4
#define BLUR_ 15  // valore 00001111
#define _RING 240 // valore 11110000
#define RE_BLUR_ "11110000\0"
#define RE__RING "00001111\0"
#define BUFF_SIZE 2
#define BITS 16
#define DEV_NULL "/dev/null"
#define CMD "/bin/dd"
#define NEWLINE '\n'

/**
 * La funzione controlla se gli argomenti passati in input sono sufficienti per l'esecuzione.
 * */
void checkArgs(int argc);
/**
 * La funzione apre il file binario con il nome passato in input. 
 * Flag identifica il tipo di operazione che si sta compiendo, utile eventualmente per la scrittura dell'errore.
 * mode identifica il modo in cui deve essere aperto il file (lettura o scrittura)
 * */
FILE *openBinFile(char *filename, char *flag, char *mode);
/**
 * deBlur si occupa del disoffuscamento del file. In questa funzione avviene il controllo per osservare se il file è correttamente formattato
 * La funzione restituisce la stringa deoffuscata che sarà poi elaborata da /bin/dd
 * */
char *deBlur(FILE *blurred);
/**
 * La funzione si occupa del rioffuscamento della stringa, output di /bin/dd, e della stampa sul file di output.
 * 
 * */
void blurAndWrite(char *tbb, FILE *fout);
/**
 * prepareCommand() prepara l'array di puntatori cmd con cui poi sarà eseguito execvp, tenendo conto della stringa s passata in input
 * */
char **prepareCommand(char **cmd, int argc, char *argv[]);
/**
 * execute() esegue /bin/dd con la stringa già convertita convfile, contenuto del file passato in input deoffuscato.
 * La funzione chiama anche la funzione blurAndWrite che si occuperà della stampa in output del risultato.
 * */
void execute(char **cmd, char *convfile, FILE *fout);
/**
 * Funzione atta alla gestione di errori causati da una cattiva formattazione del file in input.
 * */
void badFormat(int d);
/**
 * Funzione atta alla gestione di eventuali errori creati da system call come read, malloc, etc.
 * */
void manageError(char *triggered_by, int n_error);

int main(int argc, char *argv[]) {
    char **cmd=NULL, *convfile;
    FILE *fin, *fout;
    checkArgs(argc);    // vengono controllati se gli argomenti siano tutti
    fin = openBinFile(argv[1], READ_FROM, READ_BINARY);  // vengogno aperti i file binari in input e in output
    fout = openBinFile(argv[2], WRITE_TO, WRITE_BINARY);
    cmd=prepareCommand(cmd, argc, argv);
    convfile=deBlur(fin);
    execute(cmd, convfile, fout);
}

void manageError(char *triggered_by, int n_error) {
    fprintf(stderr, ERRMSG_SYSTEMCALL, triggered_by, strerror(n_error));
    exit(100);
}

char *deBlur(FILE *blurred) {
    long int c;
    int read, i=0;
    char *convfile, buffer[BUFF_SIZE], value[3];
    convfile=calloc(0, sizeof(char));
    while ( (read=fread(buffer, BUFF_SIZE, 1, blurred) )>0 )  { 
        if ( !( (buffer[0])>>4 == -1 )) badFormat(i*2);
        if ( !((buffer[1])>>8 == -1 || (buffer[1])>>8 == 0) ) badFormat(i*2+1);
        sprintf(value, "%x%x", buffer[0] & BLUR_, (buffer[1] &_RING)>>4);
        c=strtol(value, 0, BITS);
        convfile=realloc(convfile, (i+2)*sizeof(char));
        convfile[i++]=(char)c;
    }
    convfile[i]=0;
    fclose(blurred);
    return convfile;
}

void blurAndWrite(char *tbb, FILE *fout) {
    char lbyte[9]=RE_BLUR_, rbyte[9]=RE__RING;
    char bin_char[8], c;
    unsigned char value;
    int tbbsize=strlen(tbb);
    int z=0;
    while (c=tbb[z]) {
        int j=0;
        for (int i=7; i>=0; i--) { 
            bin_char[j++]=( (c & (1 << i)) ? '1' : '0' );
        }
        for (int i=0; i<4; i++) {
            lbyte[i+4]=bin_char[i];
            rbyte[i]=bin_char[i+4];
        }
        value = strtol(lbyte, NULL, 2); // ritrasformo il valore in binario
        fwrite(&value, 1, 1, fout);
        value = strtol(rbyte, NULL, 2);
        fwrite(&value, 1, 1, fout);
        z++;
    }
}

void checkArgs(int argc) {
    if (argc<MIN_ARGS) {
        fprintf(stderr, "%d\n"INVALID_ARGUMENTS, argc);
        exit(30);
    }
}

FILE *openBinFile(char *filename, char *flag, char *mode) {
    FILE *f;
    f=fopen(filename, mode); 
    if (f==NULL) {
        fprintf(stderr, ERR_FINPUT, flag, filename, strerror(errno));
        exit(20);
    }
    return f;
}

char **prepareCommand(char **cmd, int argc, char *argv[]) {
    cmd=malloc(2*sizeof(char*));
    if(cmd==NULL) manageError(ETYPE_MALLOC, errno);
    cmd[0]=CMD;
    for (int i=1; i<=argc-3; i++) {
        cmd=realloc(cmd, (i+2)*sizeof(char*));
        if(cmd==NULL) manageError(ETYPE_REALLOC, errno);
        cmd[i]=argv[i+2];
    }
    cmd[argc-2]=0;
    return cmd;
}

void execute(char **cmd, char *convfile, FILE *fout) {
    int pid, to_ch[2], to_fa[2], devnull;
    // apro le due pipe che mi permetteranno la comunicazione bidirezionale
    if (pipe(to_ch) < 0) manageError(ETYPE_PIPE, errno);   
    if (pipe(to_fa) < 0) manageError(ETYPE_PIPE, errno);
    if ( (pid=fork()) == 0 ) {  // funzione figlio
        close(to_ch[1]); close(to_fa[0]);   // chiudo write e read che non interessano al figlio
        dup2(to_ch[0], 0); close(to_ch[0]); // reindirizzo stdin alla read della pipe su cui scriverà il padre
        dup2(to_fa[1], 1); close(to_fa[1]); // reindirizzo stdout alla write della pipe che verrà letta dal padre
        devnull=open(DEV_NULL, O_WRONLY);
        dup2(devnull, 2);
        execvp(cmd[0], cmd);
        manageError(ETYPE_EXECVP, errno);
    }
    else {
        int outsize=1;
        char *out_child, *re_blurred, c;
        ssize_t rc;
        out_child=malloc(outsize*sizeof(char));
        memset(out_child, 0, outsize*sizeof(char));
        close(to_ch[0]); close(to_fa[1]);   // chiudo write e read che non interessano al padre
        write(to_ch[1], convfile, strlen(convfile)+1);  // scrivo nella pipe
        close(to_ch[1]);
        free(convfile);
        waitpid(pid, NULL, 0);
        free(cmd);
        dup2(to_fa[0], 0); close(to_fa[0]);
        while ( (c=getchar()) != EOF) {  
            out_child=realloc(out_child, (outsize)*sizeof(char));
            out_child[outsize-1]=c;
            outsize++;
        }
        int n=strlen(out_child)-1;
        if (out_child[n] != NEWLINE) {
        	out_child=realloc(out_child, n+3);
        	out_child[n+1]=NEWLINE;
        	out_child[n+2]=0;
        }
        blurAndWrite(out_child, fout);
        free(out_child);
        fclose(fout);
    }
}

void badFormat(int d) {
    fprintf(stderr, ERRMSG_BADFORMAT, d);
    exit(30);
}