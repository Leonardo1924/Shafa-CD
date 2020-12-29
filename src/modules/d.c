/***************************************************
 *
 *  Author(s): Alexandre Martins, Beatriz Rodrigues
 *  Created Date: 3 Dec 2020
 *  Updated Date: 29 Dec 2020
 *
 **************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "d.h"
#include "utils/file.h"
#include "utils/extensions.h"

#define n_simb 256

/**
\brief Load's the rle file and saves it
 @param f_rle Pointer to the original/RLE file's path
 @param size_of_block Block size
 @param error Error status
 @returns Saved file content
*/
char* load_rle (FILE* f_rle, unsigned long size_of_block, _modules_error* error) 
{
    // Memory allocation for the buffer that will contain the contents of one block of symbols
    char* buffer = malloc(size_of_block + 1);

    if (buffer) {
        // The function fread loads the said contents into the buffer
        // For the correct execution, the amount read by fread has to match the amount that was supposed to be read: size_of_block
        if (fread(buffer,1,size_of_block, f_rle) == size_of_block) {

            buffer[size_of_block] = '\0'; // Ending the string

        } // Error caused by said amount not matching
        else {
            *error = _FILE_STREAM_FAILED;
            buffer = NULL;
        }

    } 
    else {
        *error = _LACK_OF_MEMORY;
    }

    return buffer;
}


/**
\brief Allocates space to the descompressed file
 @param buffer Saved file content
 @param size_of_block Block size
 @param size_string String size
 @param error Error status
 @returns String with space allocated to save the descompressed file
*/
char* rle_block_decompressor (char* buffer, unsigned long block_size, long long* size_string, _modules_error* error) 
{
    // Assumption of the smallest size possible for the decompressed file
    unsigned long orig_size; 
    if (block_size <= _64KiB) 
        orig_size = _64KiB + _1KiB;
    else if (block_size <= _640KiB) 
        orig_size = _640KiB + _1KiB;
    else if (block_size <= _8MiB)
        orig_size = _8MiB + _1KiB;
    else 
        orig_size = _64MiB + _1KiB;

    // Allocation of the corresponding memory 
    char* sequence = malloc(orig_size + 1);
    if (sequence) {

        // Loop to decompress block by block
        unsigned long l = 0; // Variable to be used to go through the sequence string 
        for (unsigned long i = 0; i < block_size; ++i) {
            
            char simb = buffer[i];
            int n_reps = 0;
            // Case of RLE pattern {0}char{n_rep} 
            if (!simb) {
                simb = buffer[++i];
                n_reps = buffer[++i];
            } 
            // Re-allocation of memory in the string
            if (l + n_reps > orig_size) {
                switch (orig_size) {
                    case _64KiB + _1KiB:
                        orig_size = _640KiB + _1KiB;
                        break;
                    case _640KiB + _1KiB:
                        orig_size = _8MiB + _1KiB;
                        break;
                    case _8MiB + _1KiB:
                        orig_size = _64MiB + _1KiB;
                        break;
                    default: // Invalid size
                        *error = _FILE_UNRECOGNIZABLE;
                        free(sequence);
                        return NULL;
                }
                sequence = realloc(sequence, orig_size);
                // In case of realloc failure, we free the previous memory allocation
                if (!sequence) {
                    *error = _LACK_OF_MEMORY;
                    free(sequence);
                    sequence = NULL;
                    break;
                }

            }
            if (n_reps) {
                memset(sequence + l, simb, n_reps);
                l += n_reps;
            }
            else {
                sequence[l++] = simb;
            }

        }
        *size_string = l;
        sequence[++l] = '\0';

    }
    else {
        *error = _LACK_OF_MEMORY;
    }

    return sequence;
}

_modules_error _rle_decompress (char ** const path, BlocksSize blocks_size) 
{
    _modules_error error = _SUCCESS;
    // Opening RLE file
    FILE* f_rle = fopen(*path, "rb");
    if (f_rle) {
        // Creating path to FREQ file
        char* path_freq = add_ext(*path, FREQ_EXT);
        if (path_freq) {
            // Opening FREQ file
            FILE* f_freq = fopen(path_freq, "rb");
            if (f_freq) {
                // Creating path to TXT file
                char* path_txt = rm_ext(*path);
                if (path_txt) {
                    // Opening TXT file
                    FILE* f_txt = fopen(path_txt, "wb");
                    if (f_txt) {
                        unsigned long* sizes;
                        long long length;
                        if (blocks_size.length) { // There is data saved from the COD file during shafa decompression
                            sizes = blocks_size.sizes;
                            length = blocks_size.length;
                        }
                        else { // There isn't data available, therefore it's necessary to use the FREQ file to obtain information
                            char mode;
                            // Reads the header of the FREQ file
                            if (fscanf(f_freq, "@%c@%lld", &mode, &length) == 2) {
                                // Checks if the format is correct
                                if ((mode == 'N') || (mode == 'R')) {
                                    // Allocates memory for an array to contain the sizes of all the blocks of the file
                                    sizes = malloc(length);

                                    if (sizes) {
                                        
                                        for (long long i = 0; i < length && !error; ++i) {

                                            if (fscanf(f_freq, "@%lu@%*[^@]", sizes + i) != 1) {

                                                error = _FILE_STREAM_FAILED;
                                            }

                                        }

                                    }
                                    else {
                                        error = _LACK_OF_MEMORY;
                                    }

                                }
                                else {
                                    error = _FILE_UNRECOGNIZABLE;
                                }

                            } 
                            else {
                                error = _FILE_STREAM_FAILED;
                            }

                        }
                        
                        // Loop to execute block by block
                        for (long long i = 0; i < length && !error; ++i) {
                            // Loading rle block
                            char* buffer = load_rle(f_rle, sizes[i], &error);
                            if (!error) {
                                // Decompressing the RLE block
                                long long size_sequence;
                                char* sequence = rle_block_decompressor(buffer, sizes[i], &size_sequence, &error);
                                if (!error) {
                                    // Writing the decompressed block in TXT file
                                    if (fwrite(sequence, 1, size_sequence, f_txt) != size_sequence) {

                                        error = _FILE_STREAM_FAILED;
    
                                    }
                                    free(sequence);

                                }
                                free(buffer);

                            }
                    
                        }
                        fclose(f_txt);
                    }
                    else {
                        error = _FILE_INACCESSIBLE;
                    }
                    free(path_txt);

                }
                else {
                    error = _LACK_OF_MEMORY;
                }
                fclose(f_freq);

            }
            else {
                error = _FILE_INACCESSIBLE;
            }
            free(path_freq);

        }
        else {
            error = _LACK_OF_MEMORY;
        }

        fclose(f_rle);

    }
    else {
        error = _FILE_INACCESSIBLE;
    }

    return error;
}

/**
\brief Binary tree that will contain all of the symbols codes needed to the descompressed file
*/
typedef struct btree{
    char symbol;
    struct btree *left,*right;
} *BTree;

/**
\brief Free's all the memory used by a BTree
 @param tree Binary tree
*/
void free_tree(BTree tree) {

    if (tree) {
        free_tree(tree->right);
        free_tree(tree->left);
        free(tree);
    }

}

/**
\brief Add's a given code to the BTree
 @param decoder Tree with saved symbols to help in decoding
 @param *code Path to save the symbol in the correct leaf
 @param symbol character to be saved in the tree 
 @returns Error status
*/
_modules_error add_tree(BTree* decoder, char *code, char symbol) 
{
    int i;
    // Creation of the path to the symbol we are placing
    for (i = 0; code[i]; ++i) {
        if (*decoder && code[i] == '0') decoder = &(*decoder)->left;
        else if (*decoder && code[i] == '1') decoder = &(*decoder)->right;
        else {
            *decoder = malloc(sizeof(struct btree));
            if (!(*decoder)) return _LACK_OF_MEMORY;
            (*decoder)->left = (*decoder)->right = NULL;
            if (code[i] == '0') decoder = &(*decoder)->left;
            else decoder = &(*decoder)->right;
        } 
    }
    // Adding the symbol to the corresponding leaf of the tree
    *decoder = malloc(sizeof(struct btree));
    (*decoder)->symbol = symbol;
    (*decoder)->left = (*decoder)->right = NULL;
    return _SUCCESS;
}

/**
\brief Allocates memory to a binary tree that contains symbols codes
 @param f_cod File .cod
 @param block_sizes Block sizes
 @param index Index to the block that we are currently on
 @param decoder Pointer to the binary tree
 @returns Error status
*/
_modules_error create_tree (FILE* f_cod, unsigned long* block_sizes, long long index, BTree* decoder)
{
    _modules_error error = _SUCCESS;
    // Initialize root without meaning 
    *decoder = malloc(sizeof(struct btree));
    
    if (*decoder) {
        
        (*decoder)->left = (*decoder)->right = NULL;
       
        unsigned long crrb_size; 
        // Reads the current block size
        if (fscanf(f_cod, "@%lu", &crrb_size) == 1) {
           
            // Saves it to the array
            block_sizes[index] = crrb_size; 
            // Allocates memory to a block of code from .cod file
            char* code = malloc(33152); //sum 1 to 256 (worst case shannon fano) + 255 semicolons + 1 byte NULL + 1 extra byte for algorithm efficiency avoiding 256 compares           
            if (code) {
                
                if (fscanf(f_cod,"@%33151[^@]", code) == 1) {   
                         
                    for (int k = 0, l = 0; code[l];) {
                        // When it finds a ';' it is no longer on the same symbol. This updates it.
                        while (code[l] == ';') {
                            k++;
                            l++;
                        }
                        // Allocates memory for the sf code of a symbol
                        char* sf = malloc(257); // 256 maximum + 1 NULL
                        if (sf) {           
                            int j;
                            for (j = 0; code[l] && (code[l] != ';'); ++j, ++l) 
                                sf[j] = code[l];
                            sf[j] = '\0';           
                            if (j != 0) {
                                // Adds the code to the tree
                                error = add_tree(decoder, sf, k);
                                if (error) break;
                              
                            }   
                            free(sf);        
                        }
                        else {    
                            error = _LACK_OF_MEMORY;
                        }
                      
                    }
                }
                else {
                    error = _FILE_STREAM_FAILED; 
                }
                free(code);
            }
            else {    
                error = _LACK_OF_MEMORY;
            }          
              
        }
        else  {  
            error = _FILE_STREAM_FAILED;
        }
        
    }
    else {
        error = _LACK_OF_MEMORY;
    }

    
    return error;
}

/**
\brief Allocates memory to the descompressed file
 @param shafa Content of the file to be descompressed
 @param block_size Block size
 @param decoder Binary tree with the symbols codes
 @returns Error status
*/
char* shafa_block_decompressor (char* shafa, unsigned long block_size, BTree decoder) {
    
    // String for the decompressed contents 
    char* decomp = malloc(block_size+1);
    if (!decomp) return NULL;
    BTree root = decoder;
    uint8_t mask = 128; // 1000 0000 
    unsigned long i = 0, l = 0;
    int bit;

    // Loop to check every byte, bit by bit
    while (l < block_size) {
        
        bit = mask & shafa[i];
        if (!bit) decoder = decoder->left; // bit = 0
        else decoder = decoder->right;
        // Finds a leaf of the tree
        if (decoder && !(decoder->left) && !(decoder->right)) {
                decomp[l++] = decoder->symbol;
                decoder = root;
        }
        mask >>= 1; // 1000 0000 >> 0100 0000 >> ... >> 0000 0001 >> 0000 0000
        
        // When mask = 0 it's time to move to the next byte
        if (!mask) {
            ++i;
            mask = 128;
        }
        
    }
   
    decomp[l] = '\0';
    return decomp;
}


_modules_error shafa_decompress (char ** const path, bool rle_decompression) 
{
    _modules_error error = _SUCCESS;
    int flag = 1; // Flag to know if the file to write on was closed or not
    // Opening the files
    FILE* f_shafa = fopen(*path, "rb");
    if (f_shafa) {
        
        char* path_cod = rm_ext(*path);
        if (path_cod) {

            path_cod = add_ext(path_cod, CODES_EXT);
            if (path_cod) {

                FILE* f_cod = fopen(path_cod, "rb");
                if (f_cod) {

                    char* path_wrt = rm_ext(*path);
                    if (path_wrt) {

                        FILE* f_wrt = fopen(path_wrt, "wb");
                        if (f_wrt) {
                            // Reading header of shafa file
                            long long length;
                            if (fscanf(f_shafa, "@%lld", &length) == 1) {

                                char mode;
                                // Reading header of cod file
                                if (fscanf(f_cod, "@%c@%lld", &mode, &length)) {
                                    
                                    if ((mode == 'N') || (mode == 'R')) { 
                                        // Allocates memory to an array with the purpose of saving the sizes of each block
                                        unsigned long* sizes = malloc(sizeof(int)*length);
                                        if (sizes) {
                                       
                                            for (long long i = 0; i < length && !error; ++i) {
                                                // Creates the binary tree with the paths to decode the symbols
                                                BTree decoder;
                                                error = create_tree(f_cod, sizes, i, &decoder);

                                                if (!error) {

                                                    unsigned long sf_bsize;
                                                    if (fscanf(f_shafa, "@%lu@", &sf_bsize) == 1) {
                                                        // Allocates memory to a buffer in which will be loaded one block of shafa code
                                                        char* shafa_code = malloc(sf_bsize);
                                                        if (shafa_code) {

                                                            if (fread(shafa_code, 1, sf_bsize, f_shafa) == sf_bsize) { 
                                                                // Generates the block decoded to RLE/TXT
                                                                char* decomp = shafa_block_decompressor(shafa_code, sizes[i], decoder);

                                                                if (decomp) {
                                                                   
                                                                    if (fwrite(decomp, 1, sizes[i], f_wrt) != sizes[i]) {

                                                                        error = _FILE_STREAM_FAILED;
                                                                     
                                                                    }
                                                                    
                                                                    free(decomp);
                                                                }
                                                                else {
                                                                    error = _LACK_OF_MEMORY;
                                                                }
                                                            }
                                                            else {
                                                                error = _FILE_STREAM_FAILED;
                                                            }

                                                            free(shafa_code);
                                                        }
                                                        else {
                                                            error = _LACK_OF_MEMORY;
                                                        }

                                                    }
                                                    else {
                                                        error = _FILE_STREAM_FAILED;
                                                    }

                                                free_tree(decoder);
                                                }
                                                
                                            }
                                            // Executes the rle decompression, using the data read in the COD file (avoids having to read the FREQ file to get the same data)
                                            if (!error && rle_decompression) {
                                                BlocksSize blocks_size; 
                                                blocks_size.sizes = sizes; 
                                                blocks_size.length = length;
                                                fclose(f_wrt); // Neccessary to close to be able to read the RLE file generated correctly in the rle decompression
                                                flag = 0; // Says that the RLE file doesn't need to be closed further ahead (avoids a double free)
                                                error = _rle_decompress(&path_wrt, blocks_size);
                                                
                                            }
                                            free(sizes);
                                        }
                                        else {
                                            error = _LACK_OF_MEMORY;
                                        }

                                    }
                                    else {
                                        error = _FILE_UNRECOGNIZABLE;
                                    }
                                    
                                }
                                else {
                                    error = _FILE_STREAM_FAILED;
                                }

                            }
                            else {
                                error = _FILE_STREAM_FAILED;
                            }

                            if (flag) {
                                fclose(f_wrt);
                            }
                            
                        }
                        else {
                            error = _FILE_UNRECOGNIZABLE;
                            
                        }
                        free(path_wrt);
                    }
                    else {
                        error = _LACK_OF_MEMORY;
                    }
                    fclose(f_cod);

                }
                else {
                    error = _FILE_UNRECOGNIZABLE;
                }

            }
            else {
                error = _LACK_OF_MEMORY;
            }
            free(path_cod);

        }
        else {
            error = _LACK_OF_MEMORY;
        }

        fclose(f_shafa);

    }
    else {
        error = _FILE_UNRECOGNIZABLE;
    }

    return error;
}
