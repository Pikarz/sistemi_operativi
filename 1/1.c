#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "sys/stat.h"
#include <string.h>
#include <dirent.h>
#include <sys/dir.h>
#include <errno.h>
#include <stdbool.h>
#include <fnmatch.h>

#define OPTIONS "apP:"
#define BRANCH "|"
#define BRANCH_SPAN "   "
#define BRANCH_SINGLE_SPAN " "
#define BRANCH_END "`"
#define PETIOLE "-- "
#define LINKS_TO " -> "
#define NEW_TREE ""
#define ERRMSG_SYSTEMCALL "System call %s failed because %s" // DA CONTROLLARE
#define ETYPE_SCANDIR "scandir"
#define ETYPE_MALLOC "malloc"
#define ETYPE_REALLOC "realloc"
#define TREE_END_DIR " directories, "
#define TREE_END_FILE " file"
#define NOT_A_DIR_MSG " [error opening dir because of being not a dir]"
#define BUFF_SIZE 256

/*
    Struttura che conterrà le varie informazioni riguardo al branch, cioè un insieme di BRANCH, BRANCH_SPAN, BRANCH_SINGLE_SPAN, 
    e al nodo, cioè PETIOLE, permessi se -p e filename e, se si tratta di un link, LINKS_TO con il nome del file linkato, di un file
*/
struct node_info {
    char *branch;
    char *node;
};

void manageError(char *triggered_by, int n_error);
char **getStartingDirs(int argc, char *argv[], int from, int *n_dirs);
char *newRoot(char *tree, char *root, bool *isdir);
char *growTree(char *tree, char **dirs, int *n_dirs, int *tot_dirs, int *tot_files, int *exit_status, int *aflag, int *pflag, char *ppattern);
struct node_info getChildInfo(char *fname, struct node_info *f, bool islast, char *fullpath, int *pflag);
char *checkDirAndAppend(char *tree, struct node_info *infodir, char *fullpath, int *tot_dirs, int *tot_files, bool islast, int *aflag, int *pflag, char *ppattern);
void manageError(char *triggered_by, int n_error);
char *appendNodeToTree(char *tree, struct node_info *ni);
char *completeTree(char *tree, int *tot_dirs, int *tot_files);
int posNumLen(int n);
char* getPermsMode(struct stat *st);

int main(int argc, char *argv[]) {
    int opt;            // mantiene il valore di ritorno di getopt
    char *ppattern=NULL;     // pattern desiderato con l'opzione -P
    int aflag=0;         // flag che determina l'opzione -a in cui devono essere printati i file nascosti
    int pflag=0;         // flag che indica l'opzione -p in cui devono essere printati i permsessi
    //char **dirs;        // lista di stringhe che conterrà le directory passate in input 
    int exit_status=0;
    while(( opt = getopt(argc, argv, OPTIONS)) != -1) {
        switch(opt) {
            case 'a':
                aflag=1; break;
            case 'p':
                pflag=1; break;
            case 'P':
                ppattern=strdup(optarg); break;
            case '?':
                fprintf (stderr, "Usage: 1 [-P pattern] [-ap] [dirs]");
                return 20;
        }
    }
   
    int input_dirs=0;       // n_totale di directory in input
    char **dirs=getStartingDirs(argc, argv, optind, &input_dirs);
    int tot_dirs=0, tot_files=0;      // inizializzo le variabili adibite al conteggio di files e directory totali che serviranno ai fini dell'output
    char *tree=malloc(sizeof(char));        // inizializzo il tree a '\0' tramite malloc cosicché possa allocare dinamicamente la memoria in seguito
    tree[0]='\0';
    tree=growTree(tree, dirs, &input_dirs, &tot_dirs, &tot_files, &exit_status, &aflag, &pflag, ppattern);
    for (int i=0; i<argc-optind; i++) free(dirs[i]);
    free(dirs);
    free(ppattern);
    tree=completeTree(tree, &tot_dirs, &tot_files);
    printf("%s", tree);
    free(tree);
    return exit_status;
}

 /*
        La funzione è atta al salvataggio delle varie directory passate in input
*/
char **getStartingDirs(int argc, char *argv[], int from, int *n_dirs) {
    char **dirs = malloc((argc-optind) * sizeof(char*));
    if(!dirs) manageError(ETYPE_MALLOC, errno);
    for (int i=0; i<argc-optind; i++) {
        int size=((strlen(argv[i+optind])+1)*sizeof(char));
        dirs[i]=malloc( size );
        memset(dirs[i],'\0',size);
        if(!dirs[i]) manageError(ETYPE_MALLOC, errno);
        strcat(dirs[i], argv[i+optind]);
        *n_dirs += 1;
    }
    return dirs;
}

/*
    Funzione che inizializza l'albero concatenando le varie root, cioè files passati in input
*/
char *growTree(char *tree, char **dirs, int *n_dirs, int *tot_dirs, int *tot_files, int *exit_status, int *aflag, int *pflag, char *ppattern) {
    for (int i=0; i< *n_dirs; i++) {
        bool isdir=true;
        tree=newRoot(tree, dirs[i], &isdir);
        struct node_info root = { "", dirs[i] };
        if (isdir) tree=checkDirAndAppend(tree, &root, dirs[i], tot_dirs, tot_files, false, aflag, pflag, ppattern);
        else *exit_status=10;
    }
    return tree;
}

/*
    Funzione che si occupa dell'eventuale opzione -p ricavando i vari permessi che ha un dato file, ottenendo un output molto simile a ls -l
*/
char *getPermsMode(struct stat *st) {
    char *perms_mode = malloc(15*sizeof(char));
    mode_t perms = st->st_mode;
    perms_mode[0] = '[';
    perms_mode[1] = (S_ISDIR(perms)) ? 'd' : (S_ISLNK(perms)) ? 'l' : '-';
    perms_mode[2] = (perms & S_IRUSR) ? 'r' : '-';
    perms_mode[3] = (perms & S_IWUSR) ? 'w' : '-';
    perms_mode[4] = (perms & S_IXUSR) ? 'x' : '-';
    perms_mode[5] = (perms & S_IRGRP) ? 'r' : '-';
    perms_mode[6] = (perms & S_IWGRP) ? 'w' : '-';
    perms_mode[7] = (perms & S_IXGRP) ? 'x' : '-';
    perms_mode[8] = (perms & S_IROTH) ? 'r' : '-';
    perms_mode[9] = (perms & S_IWOTH) ? 'w' : '-';
    perms_mode[10] = (perms & S_IXOTH) ? 'x' : '-';
    perms_mode[11] = ']';
    perms_mode[12] = ' ';
    perms_mode[13] = ' ';
    perms_mode[14] = '\0';
    return perms_mode;     
}

/*
    Funzione atta alla restituzione del path relativo al file
*/
char *getFullPath(char *ffullpath, char *cfname) {
    int size_fullpath=(strlen(ffullpath) + strlen(cfname) +2)*sizeof(char);
    char *child_fullpath=malloc( size_fullpath );
    memset(child_fullpath, '\0', size_fullpath);
    strcat(child_fullpath, ffullpath); strcat(child_fullpath, "/");
    strcat(child_fullpath, cfname);
    return child_fullpath;
}

/*
    Funzione che appende una nuova root (directory passata in input) all'albero generato
*/
char *newRoot(char *tree, char *root, bool *isdir) {
    struct stat s;
    lstat(root, &s);
    int size_newtree=( strlen(tree) + strlen(root)+ 2 )*sizeof(char);
    if (!S_ISDIR(s.st_mode)) {
        *isdir=false;
        size_newtree += ( strlen(NOT_A_DIR_MSG)  )*sizeof(char);
        }
    void *new=realloc(tree, size_newtree);
    if (new == NULL) manageError(ETYPE_REALLOC, errno);
    tree=new;
    strcat(tree,root); 
    if (!S_ISDIR(s.st_mode)) strcat(tree, NOT_A_DIR_MSG);
    strcat(tree,"\n");
    return tree;
}

/*
    Funzione che printa l'errore e termina l'esecuzione del programma
    in caso di fallita system call
*/
void manageError(char *triggered_by, int n_error) {
    fprintf(stderr, ERRMSG_SYSTEMCALL, triggered_by, strerror(n_error));
    exit(100);
}

/*
    La funzione analizza ogni directory e itera il suo contenuto. Se uno dei file iterati è una directory, la funzione verrà chiamata ricorsivamente.
    checkDirAndAppend si occupa anche di chiamare la funzione appendNodeToTree() che concatenerà le varie informazioni ricavate.
    Viene gestita l'eventuale l'opzione -P
*/
char *checkDirAndAppend(char *tree, struct node_info *infodir, char *fullpath, int *tot_dirs, int *tot_files, bool islast, int *aflag, int *pflag, char *ppattern) {
    struct dirent **list;
    int n=scandir(fullpath, &list, NULL, alphasort);
    if (n < 0) manageError(ETYPE_SCANDIR, errno);
    int j=0;
    char *last_file=NULL;
    int size_l;
    // Tengo traccia dell'ultimo file che soddisfa i requisiti e sarà iterato, per permettermi di, in caso di opzione -P,
    // sapere quale sia effettivamente l'ultimo file, a cui quindi corrisponderà il branch '`'
    for (int i=0; i<n; i++) {
        if( ppattern!=NULL && list[i]->d_type != DT_DIR && fnmatch(ppattern, list[i]->d_name, 0) != 0 ) continue; 
        size_l=(strlen(list[i]->d_name) +1)*sizeof(char);
        free(last_file); last_file=malloc(size_l);
        if(!last_file) manageError(ETYPE_MALLOC, errno);
        memset(last_file, '\0', size_l);
        strcat(last_file, list[i]->d_name);
    }
    bool ischildlast=false;
    while (j<n) {
        char *child_name=list[j]->d_name;     // nome della directory su cui sta attualmente avvenendo l'iterazione
        if (strcmp(child_name, last_file)==0) ischildlast=true;
        if (child_name[strlen(child_name)-1] == '.') // salto le directory speciali
            { ++j; continue; }
        if (*aflag==0){
            if (child_name[0] == '.') { ++j; continue; }
        }
        if (ppattern && list[j]->d_type != DT_DIR && fnmatch(ppattern, child_name, 0) != 0) { j++; continue; }
        char *child_fullpath=getFullPath(fullpath, child_name);
        struct node_info childinfo=getChildInfo(child_name, infodir, ischildlast, child_fullpath, pflag);
        tree=appendNodeToTree(tree, &childinfo);
        if ( list[j]->d_type == DT_DIR ) { 
            *tot_dirs+=1;
            tree = checkDirAndAppend(tree, &childinfo, child_fullpath, tot_dirs, tot_files, ischildlast, aflag, pflag, ppattern);
        }
        else *tot_files+=1; 
        free(child_fullpath);
        free (childinfo.branch);
        free (childinfo.node);
        ++j;
        if (ischildlast==true) { break; }
    }
    free(last_file);
    for (int i=0; i<n; i++) free(list[i]);
    free (list);
    return tree;
}

/*
    La funzione completeTree permette di finalizzare la fase finale del tree, concatenando le informazioni richieste,
    come numero file trovati, numero directories, etc.
*/
char *completeTree(char *tree, int *tot_dirs, int *tot_files) {
    int size_msg= ( strlen(TREE_END_DIR) + strlen(TREE_END_FILE) + posNumLen(*tot_dirs) + posNumLen(*tot_files) +2 );
    int size_p= *tot_files!=1 ? 3 : 2;
    char *plural=malloc(size_p);
    memset(plural, '\0', size_p);
    plural[0]='\n';
    if(*tot_files!=1) { plural[0]='s'; plural[1]='\n'; size_msg+=1; }
    char msg[size_msg];
    sprintf(msg, "\n%d%s%d%s%s", *tot_dirs, TREE_END_DIR, *tot_files, TREE_END_FILE, plural);
    free(plural);
    int newsize=( strlen(tree) + size_msg + 1 ) * sizeof(char);
    void *new=realloc(tree, newsize);
    if (new == NULL) manageError(ETYPE_REALLOC, errno);
    tree=new;
    strcat(tree, msg);
    return tree;
}

/*
    La funzione permette di creare una struct node_info con le corrette informazioni del nodo, pronte per essere concatenate al tree.
    La funzione controlla vari aspetti per creare un nodo corretto:
        - se il nodo è l'ultimo
        - se è stata inserita l'opzione -p
        - se il nodo è un link, verrà mostrato anche il file linkato
        - 
*/
struct node_info getChildInfo(char *fname, struct node_info *f, bool islast, char *fullpath, int *pflag) {
    struct node_info ni;
    int branch_size;
    int node_size;
    char *perms = NULL;
    char *to_file;
    char *node;
    char *child_branch;
    int flen = strlen(f->branch);
    // costruzione proprio branch
    if (flen == 0) {
        branch_size = ( islast ? strlen(BRANCH)+1 : strlen(BRANCH_END)+1 ) * sizeof(char);
        child_branch=malloc(branch_size);
        memset(child_branch, '\0', branch_size);
        if(islast) strcat(child_branch, BRANCH_END);
        else strcat(child_branch, BRANCH);
    }
    else {
        bool isfatherlast = (f->branch[flen-1] == BRANCH[0]) ? false : true;
        branch_size = (strlen(f->branch) + strlen(BRANCH_SPAN) + 1);
        branch_size += (isfatherlast ? ( strlen(BRANCH_SINGLE_SPAN)) : strlen(BRANCH) );
        branch_size += (islast ? strlen(BRANCH_END) : strlen(BRANCH)); branch_size *= sizeof(char);
        child_branch=malloc(branch_size);
        memset(child_branch, '\0', branch_size);
        strcat(child_branch, f->branch);
        if(isfatherlast) child_branch[flen-1]=BRANCH_SINGLE_SPAN[0];
        strcat(child_branch, BRANCH_SPAN);
        if (islast) strcat(child_branch, BRANCH_END); 
        else strcat(child_branch, BRANCH);
    }
    // costruzione proprio node
    struct stat sf = {0};
    node_size = ( strlen(fname) + strlen(PETIOLE) +1);
    lstat(fullpath, &sf);
    if (*pflag==1) {
        perms=getPermsMode(&sf);
        node_size+=( strlen(perms) );
    }
    if (S_ISLNK(sf.st_mode)) {
        to_file = malloc(BUFF_SIZE*sizeof(char));
        memset(to_file, '\0', BUFF_SIZE);
        int rl=readlink(fullpath, to_file, BUFF_SIZE);
        node_size += rl+(strlen(LINKS_TO));
    }
    node=malloc(node_size*sizeof(char));
    memset(node, '\0', node_size);
    strcat(node, PETIOLE);
    if (*pflag==1) strcat(node, perms);
    free(perms);
    strcat(node, fname);
    if (S_ISLNK(sf.st_mode)) {
        strcat(node, LINKS_TO); strcat(node, to_file);
        free(to_file);
    }
    ni.branch=strdup(child_branch);
    ni.node=strdup(node);
    free(node); free(child_branch);
    return ni;
}

/*
    La funzione, dato un nodo con le sue informazioni, appende al tree
    ramo e nome del nodo più un carattere newline
*/
char *appendNodeToTree(char *tree, struct node_info *ni) {
    int newsize=( strlen(tree) + strlen(ni->branch) + strlen(ni->node) + 2 ) * sizeof(char);
    void *new=realloc(tree, newsize);
    if (new == NULL) manageError(ETYPE_REALLOC, errno);
    tree=new;
    strcat(tree,ni->branch); strcat(tree, ni->node); 
    strcat(tree,"\n");
    return tree;
}

/*
    Funzione che mi calcola la lunghezza in caratteri di un intero >=0
*/
int posNumLen(int n) {
    int len=1;
    while (n>10) {
        n /= 10;
        len++;
    }
    return len;
}