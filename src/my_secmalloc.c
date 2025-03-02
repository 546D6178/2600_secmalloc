/*******************************************************
Nom ......... : my_secmalloc.c
Role ........ : Implémentation personnalisée de la gestion
                dynamique d'allocation de la mémoire
Auteur ...... : 0xTmax (Maxence Brondelle)
Version ..... : V1.1 du 13/04/2023
Usage :
'make clean dynamic' pour générer ~/my_secmalloc/lib/libmy_secmalloc.so puis
Pour exécuter un binaire quelconque avec votre librairie, tapez : LD_PRELOAD=~/my_secmalloc/lib/libmy_secmalloc.so LS (par exemple)
Pour rediriger les logs : LD_PRELOAD=lib/libmy_secmalloc.so MSM_OUTPUT=/path/to/file ps aux
********************************************************/
#define _GNU_SOURCE
#include <sys/mman.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <alloca.h>
#include <fcntl.h>
#include <stdlib.h>

#include "my_secmalloc.h"
#include "my_secmalloc_private.h"

/*static*/ void *pool_data = 0; 
/*static*/ struct dmem *pool_meta = 0;
/*static*/ size_t size_pool_data = 1024 * 1024;
/*static*/ size_t size_pool_metainf = 1024 * 1024 * 100;

size_t  get_pool_metainf_size()
{
    return size_pool_metainf;
}

void    pool_meta_init()
{
    if (!pool_meta){
        pool_meta = mmap(NULL, get_pool_metainf_size(), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    }
    memset(pool_meta, 0, sizeof (*pool_meta));
} 

long dmem_first_free(size_t sz)
{
    for (unsigned int i = 0; i < (size_pool_metainf / sizeof (struct dmem)); i += 1){
        if (pool_meta[i].busy == 0 && pool_meta[i].used == 1 && (pool_meta[i].full >= (sz + sizeof (size_t))) ){
            return i;
        }
    }
    return -1;
}


long dmem_first_notused()  
{
    for (unsigned int i = 0; i < (size_pool_metainf / sizeof (struct dmem)); i += 1){
        if (pool_meta[i].used == 0 ){
            return i;
        }
    }
    return -1;
}

void    pool_data_init(struct dmem *dm)
{
    if (!pool_data){
        pool_data = mmap(dm + size_pool_metainf, size_pool_data, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    }
    dm[0].data = pool_data;
    dm[0].full = size_pool_data;
    dm[0].busy = 0;
    dm[0].used = 1;
}

void    dmem_init(size_t idx, size_t sz)
{
    struct dmem *dm = &pool_meta[idx];
    if (dm->used){
        // trouver un descripteur non use 
        size_t rest = dmem_first_notused();
        pool_meta[rest].used = 1; //bloc mémoire utilisé ou non 
        pool_meta[rest].busy = 0; //busy : en cours d'utilisation (cad appel par my malloc sur ce bloc pour allouer de la mémoire à partir de ce bloc)
        pool_meta[rest].full = dm->full - (sz + sizeof (size_t));  //  taille totale de la zone mémoire, qui est la taille demandée sz plus la taille d'un entier (sizeof(size_t)) qui sera utilisé comme canary     
        pool_meta[rest].data = dm->data + (sz + sizeof (size_t));  // un pointeur vers la première adresse mémoire du bloc alloué
    }
    else
    {
        dm->used = 1;
        dm->busy = 0;
        dm->data = pool_meta[idx-1].data + pool_meta[idx-1].full;
    }
    dm->sz = sz;
    dm->full = sz + sizeof (size_t);
    dm->busy = 1; 
    // nettoie la mémoire
    memset(dm->data, 0, dm->sz);  
    random_canary(idx);

}

// recherche de ptr dans pool_meta
size_t  find_ptr(void* ptr){
    for(size_t i=0; i < (size_pool_metainf / sizeof (struct dmem)) ;i++){
        if (ptr == pool_meta[i].data){
            return i;
        }
    }
    return -1;
}


void merge_next(size_t idx){
    
    if (idx == (size_pool_metainf / sizeof (struct dmem)) - 1){
        return;
    } 
    if (!idx && idx != 0){
        my_log("ERROR (merge_next) : Initialized with NULL idx \n");
        return;
    }
    // Fusions bloc data adjacents suivants libres
    size_t next_idx = (idx + 1);       
    while (next_idx < (size_pool_metainf / sizeof (struct dmem)) && !pool_meta[next_idx].busy && (pool_meta[idx].data + pool_meta[idx].full) == pool_meta[next_idx].data ) {
        pool_meta[idx].full += pool_meta[next_idx].full;
        pool_meta[next_idx].sz = 0;
        pool_meta[next_idx].full = 0;
        next_idx++;
    }
    if (next_idx != (idx + 1)){
        memset(pool_meta[idx].data, 0, pool_meta[idx].full);
        pool_meta[idx].sz = pool_meta[idx].full - sizeof(size_t);
    }
    return;
}

void merge_prev(size_t idx){    // Fusions bloc data adjacents precedants libres 
    
    if (idx == 0){ 
        return;
    }
    size_t False = -1; 
    if (idx == False){
        my_log("ERROR (merge_prev) : Initialized with NULL idx \n");
        return;
    }

    size_t prev_idx = idx - 1;
    size_t full_temp = 0;
     
    while (prev_idx > 0 && !pool_meta[prev_idx].busy && (pool_meta[prev_idx].data + (pool_meta[prev_idx].full + full_temp) == pool_meta[idx].data)) { 
        full_temp += pool_meta[prev_idx].full;
        pool_meta[prev_idx].full = 0;
        pool_meta[prev_idx].sz = 0;
        prev_idx--;
    }
    if (prev_idx != idx - 1){
        full_temp += pool_meta[idx].full;
        pool_meta[idx].full = 0;
        pool_meta[idx].sz = 0;
        memset(pool_meta[prev_idx + 1].data, 0, full_temp);
        pool_meta[prev_idx + 1].full = full_temp;
        pool_meta[prev_idx + 1].sz = full_temp - sizeof(size_t);
    }
}

void random_canary(size_t idx)
{
    size_t canary_count = sizeof(size_t);
    size_t can = 0;    
    int fd = open("/dev/random", O_RDONLY);
    size_t False = -1;
    if (fd == -1) {
        my_log("Erreur (random_canary) : Unabe to open file descriptor for /dev/random\n");
        return;
    }

    size_t n = 0;
    while (n < canary_count){
        size_t res = read(fd, &can + n, canary_count - n);
        if (res == False) {
            my_log("Erreur (random_canary) : Unabe to read file descriptor for /dev/random\n");
            return;
        }
        n += res;
    }
    close(fd);
    char canary[8];
    snprintf(canary, sizeof(canary), "%ld", can);
    for (size_t i = 0; i < canary_count; i++) {
        pool_meta[idx].data[pool_meta[idx].sz + i] = canary[i];
        pool_meta[idx].can[i] = canary[i];
    }

}

void    my_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    size_t sz = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *buf = alloca(sz + 2);
    va_start(ap, fmt);
    vsnprintf(buf, sz + 1, fmt, ap);
    va_end(ap);
    char * output_file = getenv("MSM_OUTPUT");
    if (output_file){
        int fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (fd == -1){
            my_log("Erreur (my_log) : Unabe to open file descriptor for output file %s\n", output_file);
            return;
        }
        write(fd, buf, strlen(buf));
        close(fd);
    }else{
        //affiche le msg sur la sortie d'erreur standard 
        write(2, buf, sz);
    }
}

void    *my_malloc(size_t size)
{
    long unsigned test = -1;

    if (size <= 0) {
        my_log("Erreur (my_malloc) : Unable to call my_malloc(%ld) with negative and 0 value\n", size);
        return NULL;
    }
    // alloue le pool de metainf
    if (!pool_meta) {
        pool_meta_init();
    }
    //init a pool data
    if (!pool_data) {
        pool_data_init(pool_meta);
    }
    // chercher le premier descripteur non utilise de la taille suffisante
    size_t first = dmem_first_free(size);
    size_t pool_data_size_used = 0;
    size_t idx = 0;

    if (first == test) {
        for (unsigned int i = 0; i < (size_pool_metainf / sizeof (struct dmem)); i += 1){
            if (pool_meta[i].used == 1 ){
                pool_data_size_used += pool_meta[i].full;
                idx = idx + 1;
            }
        }

        if ((pool_data_size_used + (size + sizeof(size_t))) > size_pool_data){

            // Allouer un nouveau bloc de mémoire pour pool_data
            void *new_pool_data = mremap(pool_data, size_pool_data, size_pool_data + (size + sizeof(size_t)), MREMAP_MAYMOVE, pool_data);

            if (new_pool_data == MAP_FAILED) {
                my_log("Erreur (my_malloc) : unable to call mremap with %ld size at ptr (pool_data) %p\n", (size_pool_data + (size + sizeof(size_t))), pool_data);
                return NULL;
            }
            size_pool_data = size_pool_data + (size + sizeof(size_t));
            pool_data = new_pool_data;
        }
        dmem_init(idx-1, size);
        pool_meta[idx].full = 0;
        return pool_meta[idx-1].data;
    }
    // initialiser le descripteur de memoire et le marquer comme occupe
    dmem_init(first, size);

    // renvoyer l'adresse de la memoire allouee
    return pool_meta[first].data;
}

void    my_free(void *ptr)
{
    size_t False = -1; 
    if(ptr == NULL){
        my_log("EROOR (my_free) : initialized with pointeur NULL\n");
        return;
    }
    size_t idx = find_ptr(ptr);
    if (idx == False ){
        my_log("Erreur (my_free) : pointeur %p introuvable dans pool_meta par find_ptr\n", ptr);
        return ;
    } 

    if(pool_meta[idx].busy == 0){
        my_log("Erreur (my_free) : bloc avec idx %ld déjà libéré\n", idx);
        return;
    }

    for (size_t i = 0; i < pool_meta[idx].full - pool_meta[idx].sz; i++){
        if(pool_meta[idx].data[pool_meta[idx].sz + i] != pool_meta[idx].can[i]){//Verif canary
            my_log("Erreur (my_free) : Heap over flow detected in pool_meta[%ld]\n", idx);
            return;
        }
    }
    // Libération du bloc
    pool_meta[idx].busy = 0;

    // Nettoie le contenu du bloc de mémoire
    memset(ptr, 0, pool_meta[idx].full);   // memset(ptr, int, size);

    merge_prev(idx);
    merge_next(idx);

}

void    *my_calloc(size_t nmemb, size_t size) // nmemb :  nb elem à allouer && size : taille en octet de chaq element  
{   
    if (size == 0) {
        my_log("Error (my_calloc) : initialized with size 0\n");
        return NULL;
    }

    if (nmemb == 0) {
        my_log("Error (my_calloc) : initialized with nmemb 0\n");
        return NULL;
    }

    size_t total_size = nmemb * size;
    void *ptr = my_malloc(total_size);
    if (ptr != NULL) {
        memset(ptr, 0, total_size);
        return ptr;
    }
    my_log("Error (my_calloc) : Unable to return ptr with my_malloc(%ld * %ld)\n", nmemb, size);
    return NULL;
}

void    *my_realloc(void *ptr, size_t size)
{    
    // SI taille null libère le ptr 
    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    // si pas de ptr 
    if (ptr == NULL) {
        return my_malloc(size);
    }
    size_t False = -1; 
    size_t idx = find_ptr(ptr);
    if (idx == False){
        my_log("Error (my_realloc) : Pointeur %p introuvable dans my_realloc\n",ptr);
        return NULL;
    }
    size_t old_size = pool_meta[idx].sz;

    //Nouvelle zone mémoire
    void *new_ptr = my_malloc(size);
    if (new_ptr == NULL) {
        my_log("Error (my_realloc) : Error allowing new ptr in realloc with new size %ld before\n", size);
        return NULL;
    }

    memcpy(new_ptr, ptr, old_size);
    //memcpy(dest, src, size);
    my_free(ptr);
    return new_ptr;

}



#ifdef DYNAMIC
/*
 * Lorsque la bibliothèque sera compilé en .so les symboles malloc/free/calloc/realloc seront visible
 * */

void    *malloc(size_t size)
{
    return my_malloc(size);
}
void    free(void *ptr)
{
    my_free(ptr);
}
void    *calloc(size_t nmemb, size_t size)
{
    return my_calloc(nmemb, size);
}

void    *realloc(void *ptr, size_t size)
{
    return my_realloc(ptr, size);
}

#endif
