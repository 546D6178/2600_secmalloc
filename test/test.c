#include <criterion/criterion.h>
#include <criterion/redirect.h>
#define _GNU_SOURCE
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <alloca.h>
#include <fcntl.h>
#include <stdlib.h>
#include "my_secmalloc.h"
#include "my_secmalloc_private.h"
size_t False = -1;

Test(mmap, simple) {
    // Question: Est-ce que printf fait un malloc ?
    //printf("Ici on fait un test simple de mmap\n");
    char *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    cr_expect(ptr != NULL);
    for (int i = 0; i < 4096; i += 1)
        ptr[i] = 'X';
    cr_expect(ptr[199] == 'X');
    int res = munmap(ptr, 4096);
    cr_expect(res == 0);
}

Test(log, test_log, .init=cr_redirect_stderr)
{

    my_log("coucou %d\n", 12);
    cr_assert_stderr_eq_str("coucou 12\n");

}

Test(metainf, alloc)
{
    // alloue le pool de metainf
    pool_meta_init();
    //init a pool data
    pool_data_init(pool_meta); 
    //conf 1 descripteur
    int first = dmem_first_free(12);
    cr_assert(pool_meta[first].used == 1);
    cr_assert(pool_meta[first].busy == 0);
    cr_assert(pool_meta[first].data == pool_data);
    dmem_init(first, 12);
    size_t rest = dmem_first_free(6);
    dmem_init(rest, 6);
    size_t tata = dmem_first_free(1);
    if (first == -1)
        {   
            my_log("ERROR :  dmem_first_free return -1");    
        }
    cr_assert(pool_meta[first].sz == 12);
    cr_assert(pool_meta[first].full == (pool_meta[first].sz + sizeof (size_t)));
    cr_assert(pool_meta[first].data == pool_data);
    cr_assert(pool_meta[first].busy == 1);
    //cr_assert(pool_meta[first].data[pool_meta[first].sz] == 'X'); deprecated
    cr_assert(pool_meta[first].data[pool_meta[first].sz - 1 ] == 0);
    cr_assert(pool_meta[first].data[pool_meta[first].full] == 0);
    cr_assert(pool_meta[rest].data == pool_data + pool_meta[first].full);
    cr_assert(pool_meta[tata].data == pool_meta[tata - 1].data + pool_meta[tata - 1].full);
}

Test(ncalloc, oui)
{

    void *ptr = my_calloc(2, sizeof(size_t));
    int idx = find_ptr(ptr);
    for (size_t i=0; i < pool_meta[idx].sz; i++){
        cr_assert(pool_meta[idx].data[i] == 0 );
    }
}

Test(findptr, eq)
{

    my_malloc(1);
    my_malloc(2);
    my_malloc(3);
    void *ptr = my_malloc(4);
    size_t idx = find_ptr(ptr);
    my_malloc(5);
    cr_assert( ptr == pool_meta[idx].data);
    ptr = ptr + 1;
    idx = find_ptr(ptr);
    cr_assert(idx == False); // test d'erreur de find_ptr
}


Test(find_ptr, find_ptr_error)
{
    my_malloc(3);
    my_malloc(3);
    void *ptr = my_malloc(3);
    ptr = ptr + 1;
    size_t idx = find_ptr(ptr);
    if (idx == False){
        cr_assert(idx == False);
    } 
}

Test(free, bloc_already_free,.init=cr_redirect_stderr)
{
    my_malloc(3);
    my_malloc(3);
    void *ptr = my_malloc(3);
    my_free(ptr);
    my_free(ptr);
    cr_assert_stderr_eq_str("Erreur (my_free) : bloc avec idx 2 déjà libéré\n");
    
}

Test(free, check_canary,.init=cr_redirect_stderr)
{
    my_malloc(3);
    void *ptr = my_malloc(3);
    size_t idx = find_ptr(ptr);
    memset(ptr, 0, pool_meta[idx].data[pool_meta[idx].sz + 5]);
    my_free(ptr);
    cr_assert_stderr_eq_str("Erreur (my_free) : Heap over flow detected in pool_meta[1]\n");
    ptr = my_malloc(3);
    idx = find_ptr(ptr);
    for (size_t i = 0; i < sizeof(size_t); i++){
        cr_assert(pool_meta[idx].data[pool_meta[idx].sz + i] == pool_meta[idx].can[i]);
    }
    my_free(ptr);
}

Test(free, merge_prev)
{
    //la taille totale du bloc est de 3 octets + la taille du canary.
    void *ptr1 = my_malloc(1);
    void *ptr2 = my_malloc(2);
    void *ptr3 = my_malloc(3);
    void *ptr4 = my_malloc(4);

    size_t idx1 = find_ptr(ptr1);
    size_t idx2 = find_ptr(ptr2);
    size_t idx4 = find_ptr(ptr4);


    my_free(ptr2);
    my_free(ptr3);

    size_t size_between_ptrs = pool_meta[idx4].data - (pool_meta[idx1].data + pool_meta[idx1].full );

    void *ptrnew = my_malloc(4);
    size_t idxnew = find_ptr(ptrnew);

    cr_assert(size_between_ptrs >= pool_meta[idxnew].full);
    cr_assert(pool_meta[idx2].data = pool_meta[idxnew].data);
}

Test(free, merge_next)
{

    void *ptr1 = my_malloc(1);
    void *ptr2 = my_malloc(2);
    void *ptr3 = my_malloc(3);
    void *ptr4 = my_malloc(4);

    size_t idx1 = find_ptr(ptr1);
    size_t idx2 = find_ptr(ptr2);
    size_t idx4 = find_ptr(ptr4);

    my_free(ptr3);
    my_free(ptr2);
    
    size_t size_between_ptrs = pool_meta[idx4].data - (pool_meta[idx1].data + pool_meta[idx1].full);

    void *ptrnew = my_malloc(4);
    size_t idxnew = find_ptr(ptrnew);

    cr_assert(size_between_ptrs >= pool_meta[idxnew].full);
    cr_assert(pool_meta[idx2].data = pool_meta[idxnew].data);
}


Test(realloc, oui)
{
    void *ptr1 = my_malloc(2);
    void *ptr2 = my_malloc(3);
    size_t idx1 = find_ptr(ptr1);
    void *oldptr = pool_meta[idx1].data;
    void *newptr1 = my_realloc(ptr1, 4);
    cr_assert((newptr1 != oldptr) && (newptr1 > ptr2));
    void *oldptr1 = my_malloc(1);
    cr_assert((oldptr1 == oldptr) && (oldptr1 < ptr2));
}

Test(remap_pdata, oui)
{
    void *ptr1 = my_malloc((1024 * 1024) + 4567);
    size_t idx1 = find_ptr(ptr1);

    void *ptr2 = my_malloc(1);
    size_t idx2 = find_ptr(ptr2);
    cr_assert(pool_meta[idx1].full > (1024*1024));
    cr_assert(ptr2 != NULL);
    cr_assert(pool_meta[idx2].data == (pool_meta[idx1].data + (pool_meta[idx1].full)));
    cr_assert(pool_meta[(idx2+1)].used == 1 && pool_meta[(idx2+1)].busy == 0 && pool_meta[idx2+1].data == (pool_meta[idx2].data + (pool_meta[idx2].full)) && pool_meta[idx2+1].full == 0 );

}

Test(zcanary, random)
{
    // on réserve une taille de 12
    size_t szdata = 12;
    // je dois réserver pour la szdata plsu le canary
    size_t size = szdata + sizeof (size_t);
    // J'alloue la mémoire
    char *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    // nettoie la mémoire
    memset(ptr, 0, szdata);

    // je génère le can random
    size_t canary_count = sizeof(size_t);
    size_t can = 0;    
    int fd = open("/dev/random", O_RDONLY);
    size_t False = -1;
    if (fd == -1) {
        // Gestion de l'erreur
    }

    size_t n = 0;
    while (n < canary_count){
        size_t res = read(fd, &can + n, canary_count - n);
        if (res == False) {
            // Gestion de l'erreur
        }
        n += res;
    }
    close(fd);
    char canary[8]; // contenu du struct dmem : dm->can[8] 
    snprintf(canary, sizeof(canary), "%ld", can);

    // je remplis mon canary 
    for (size_t i = 0; i < sizeof(size_t); i += 1)
        ptr[szdata + i] = canary[i];

    for (size_t i=0; i < sizeof(size_t); i += 1)
        cr_assert(ptr[szdata + i] == canary[i]);

}
