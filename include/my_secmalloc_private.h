/*******************************************************
Nom ......... : my_secmalloc_private.h
Role ........ : Permet la définition des fonctions privées
                pour l'implémentation de my_malloc(),
                my_realloc(), my_calloc(), my_free()
Auteur ...... : 0xTmax (Maxence Brondelle)
Version ..... : V1.1 du 13/04/2023
********************************************************/

#ifndef _SECMALLOC_PRIVATE_H
#define _SECMALLOC_PRIVATE_H



/*!
* @brief struct dmem used to describe metadata of a data block
*/
struct dmem
{
    char    *data;  //!< ptr to data
    size_t  sz;     //!< size of data
    size_t  full;   //!< size avec canary  
    char    busy;   //!< memory bloc used or not
    char    used;   //!< memory bloc initialized or not by my_malloc
    char    can[8]; //!< 8 random char for canary 
};

/*!
* @brief Permet l'affichage des log sur la sortie d'erreur ou dans un fichier de log 
         si et seulement si le fichier est définit au travers de la variable d'environement 
         MSM_OUTPUT=/path/to/file
* @param [in] fmt pointeur vers une chaine de caractères constantes utilisé pou formater les
         messages de journalisation
         [in] ... qui indiquent que 
         la fonction peut prendre un nombre variable d'arguments supplémentaires.
* @return
* @pre
* @post
* @note
*/
void    my_log(const char *fmt, ...);
/*!
* @brief Initialise un descripteur de memoire (dmem) et le marquer comme occupe 
* @param [in] idx est l'index dans le pool de metadonnées vers lequel elles seront stockées.
* @param [in] sz est la taille necessaire à allouer pour le pool de data 
*/
void    dmem_init(size_t idx, size_t sz);
/*!
* @brief Permet de récupérer la taille du pool de metadata
* @return La taile du pool de données
*/
size_t  get_pool_metainf_size();
/*!
* @brief Alloue un pool de données
* @param [in] dm descripteur du bloc de data à initialiser 
*/
void    pool_data_init(struct dmem *);
/*!
* @brief Alloue un pool de metadata
*/
void    pool_meta_init();
/*!
* @brief Permet de rechercher le premier bloc libre
* Recherche le premier bloc de donnée libre (busy == 0 ) mais initialisée (used == 1)
* @param [in] size qui est la taille du bloc recherché
* @return #STATUS_SUCCESS retourne l'index dans le pool de meta du descripteur associé
                            au premier bloc libre trouvé
* @return #STATUS_FAILED retourne -1 si aucun bloc n'est trouvé  
*/
long    dmem_first_free(size_t sz);
/*!
* @brief Permet de rechercher le premier non initialisé
* Recherche le premier bloc de donnée non initialisée (used == 0)
* @return #STATUS_SUCCESS retourne l'index dans le pool de meta du descripteur associé
                            au pointeur
* @return #STATUS_FAILED retourne -1 si le pointeur est introuvable 
*/
long    dmem_first_free_notused();
/*!
* @brief Prends un pointeur en entrée et recherche son index dans le pool de metadata
* @return #STATUS_SUCCESS retourne l'index dans le pool de meta du descripteur associé
                            au premier bloc libre trouvé
* @return #STATUS_FAILED retourne -1 si aucun bloc n'est trouvé  
*/
size_t  find_ptr(void* ptr);
/*!
* @brief Permet la fusion des X précédents blocs adjacents entre eux  
* @return #STATUS_INVALID_PARAM si idx est null 
*/
void    merge_next(size_t idx);
/*!
* @brief Permet la fusion des X suivants blocs adjacents entre eux  
* @return #STATUS_INVALID_PARAM si idx est null 
*/
void    merge_prev(size_t idx);
/*!
* @brief Ajoute une implémentation randomisée des canaries
* Permet d'ajouter le canary au bloc de data, puis au bloc de metadata
* @param [in] idx est l'index associé au pool de data dans le pool de meta
* @return #STATUS_FAILED retourne une erreur si impossible d'ouvrir le file descriptor 
* @return #STATUS_FAILED retourne une erreur si impossible de lire le file descriptor 
*/
void    random_canary(size_t idx);

#if TEST
extern void *pool_data; 
extern struct dmem *pool_meta; 
extern size_t size_pool_data;
extern size_t size_pool_metainf;
#endif

#endif
