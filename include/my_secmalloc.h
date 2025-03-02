/*******************************************************
Nom ......... : my_secmalloc.h
Role ........ : Permet l'utilisation des fonctions privées
                au travers de l'appel de leurs fonctions 
                respectives équivalentes (malloc, calloc, 
                realloc et free)
Auteur ...... : 0xTmax (Maxence Brondelle)
Version ..... : V1.1 du 13/04/2023
********************************************************/

#ifndef _SECMALLOC_H
#define _SECMALLOC_H

#include <stdlib.h>

#if DYNAMIC
void    *malloc(size_t size);
void    free(void *ptr);
void    *calloc(size_t nmemb, size_t size);
void    *realloc(void *ptr, size_t size); 
// lorsqu'on compile le .so les fonctions my_ deviennent privées
#define MY static
#else
// pour les tests, les fonctions my_ sont publiques
#define MY
#endif

MY void    *my_malloc(size_t size);
MY void    my_free(void *ptr);
MY void    *my_calloc(size_t nmemb, size_t size);
MY void    *my_realloc(void *ptr, size_t size); 

#endif
