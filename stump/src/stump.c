#include "stump.h"

stump add(stump s, struct utreexo_hash add) {
    for (uint8_t row = 0; row <= tree_rows(s.num_leaves); row++) {
        if (!root_present(s.num_leaves, row)) {
            break;
        }

        s.merkle_roots.size--;
        utreexo_sha512_256 root = s.merkle_roots.roots[s.merkle_roots.size];
        parent_hash(add.hash, root.hash, add.hash);
    }

    s.merkle_roots.roots[s.merkle_roots.size] = add;
    s.merkle_roots.size++;
    s.num_leaves++;

    return s;
}

stump del(stump s, proof hash_proof) {
    utreexo_sha512_256 roots[64];
    int count = calculate_roots(roots, s.num_leaves, 0, NULL, hash_proof);

    printf("count %d\n", count);

    size_t* idxs = (size_t*)malloc(hash_proof.target_count * sizeof(size_t));
    uint8_t root_count = root_idxs(s.num_leaves, idxs, hash_proof.target_count, hash_proof.targets);

    if (count != root_count) {
        printf("error!!! count and root_count are different");
        return s;
    }

    for (int i = 0; i < count; i++) {
        int idx = idxs[i];
        s.merkle_roots.roots[idx] = roots[i];
    }

    return s;
}

int verify(stump s, struct utreexo_hash* hashes, proof hash_proof) {
    return 1;
}

//#include <stdint.h>
//#include <stdbool.h>
//#include <stdlib.h>
//#include <string.h>
//#include <stdio.h>
//
//// Function prototypes (assumed to exist)
//int tree_rows(int numleaves);
//bool root_present(int numleaves, int row);
//void parent_hash(uint8_t* root, uint8_t* add, uint8_t* output, size_t hash_size);  // Combine root and add
//
//typedef struct {
//    uint8_t** roots;  // Array of pointers to byte arrays (roots)
//    size_t numleaves;  // Number of leaves (size of tree)
//    size_t hash_size;  // Size of each hash (in bytes)
//} MerkleTree;
//
//// Pop the last root from the roots array
//uint8_t* pop_root(MerkleTree* tree) {
//    if (tree->numleaves == 0) {
//        return NULL;
//    }
//    uint8_t* last_root = tree->roots[tree->numleaves - 1];
//    tree->roots[tree->numleaves - 1] = NULL;  // Remove reference to the root
//    tree->numleaves--;
//    return last_root;
//}
//
//// Push a new root to the roots array
//void push_root(MerkleTree* tree, uint8_t* add) {
//    tree->roots = realloc(tree->roots, (tree->numleaves + 1) * sizeof(uint8_t*));
//    tree->roots[tree->numleaves] = malloc(tree->hash_size);
//    memcpy(tree->roots[tree->numleaves], add, tree->hash_size);
//    tree->numleaves++;
//}
//
//// Equivalent of the Python 'add' method in C99
//void add(MerkleTree* tree, uint8_t* add) {
//    for (int row = 0; row <= tree_rows(tree->numleaves); row++) {
//        if (!root_present(tree->numleaves, row)) {
//            break;
//        }
//
//        // Pop the root and combine with 'add' using parent_hash
//        uint8_t* root = pop_root(tree);
//        if (root == NULL) {
//            break;
//        }
//
//        uint8_t new_add[tree->hash_size];
//        parent_hash(root, add, new_add, tree->hash_size);
//
//        // 'add' becomes the parent hash
//        memcpy(add, new_add, tree->hash_size);
//
//        free(root);  // Free the popped root
//    }
//
//    // Push the final 'add' to the roots
//    push_root(tree, add);
//}
//
//// Example implementations for required functions (to make this more complete):
//int tree_rows(int numleaves) {
//    // Dummy implementation: compute log2 of numleaves
//    int rows = 0;
//    while (numleaves > 1) {
//        numleaves >>= 1;
//        rows++;
//    }
//    return rows;
//}
//
//bool root_present(int numleaves, int row) {
//    // Dummy implementation: always return true for the first few rows
//    return row < 3;
//}
//
//// Dummy parent hash: just XOR the two input arrays
//void parent_hash(uint8_t* root, uint8_t* add, uint8_t* output, size_t hash_size) {
//    for (size_t i = 0; i < hash_size; i++) {
//        output[i] = root[i] ^ add[i];  // Simple XOR for demonstration
//    }
//}
//
//// Main function for testing
//int main() {
//    // Example use case
//    MerkleTree tree = {NULL, 0, 32};  // Hash size is 32 bytes (256 bits)
//
//    uint8_t new_hash[32] = {0x01, 0x02, 0x03};  // Dummy 32-byte hash for testing
//
//    add(&tree, new_hash);  // Add hash to the Merkle tree
//
//    // Free allocated memory for roots
//    for (size_t i = 0; i < tree.numleaves; i++) {
//        free(tree.roots[i]);
//    }
//    free(tree.roots);
//
//    return 0;
//}
