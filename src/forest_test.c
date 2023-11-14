#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "flat_file.h"
#include "forest_node.h"
#include "map_forest_impl.h"
#include "parent_hash.h"
#include "test_utils.h"

typedef struct {
  uint8_t expected_roots[10][32];
  size_t expected_roots_len;
  uint64_t *leaf_preimages;
  size_t preimage_count;
} add_test_data;

#define True 1
#define False 0

static const add_test_data insertion_tests[] = {
    {
        .expected_roots = {{0xb1, 0x51, 0xa9, 0x56, 0x13, 0x9b, 0xb8, 0x21,
                            0xd4, 0xef, 0xfa, 0x34, 0xea, 0x95, 0xc1, 0x75,
                            0x60, 0xe0, 0x13, 0x5d, 0x1e, 0x46, 0x61, 0xfc,
                            0x23, 0xce, 0xdc, 0x3a, 0xf4, 0x9d, 0xac, 0x42}},
        .leaf_preimages = (uint64_t[]){0, 1, 2, 3, 4, 5, 6, 7},
        .preimage_count = 8,
        .expected_roots_len = 1,
    },
    {
        .expected_roots =
            {{0xdf, 0x46, 0xb1, 0x7b, 0xe5, 0xf6, 0x6f, 0x07, 0x50, 0xa4, 0xb3,
              0xef, 0xa2, 0x6d, 0x46, 0x79, 0xdb, 0x17, 0x0a, 0x72, 0xd4, 0x1e,
              0xb5, 0x6c, 0x3e, 0x4f, 0xf7, 0x5a, 0x58, 0xc6, 0x53, 0x86},
             {0x9e, 0xec, 0x58, 0x8c, 0x41, 0xd8, 0x7b, 0x16, 0xb0, 0xee, 0x22,
              0x6c, 0xb3, 0x8d, 0xa3, 0x86, 0x4f, 0x95, 0x37, 0x63, 0x23, 0x21,
              0xd8, 0xbe, 0x85, 0x5a, 0x73, 0xd5, 0x61, 0x6d, 0xcc, 0x73},
             {0x67, 0x58, 0x6e, 0x98, 0xfa, 0xd2, 0x7d, 0xa0, 0xb9, 0x96, 0x8b,
              0xc0, 0x39, 0xa1, 0xef, 0x34, 0xc9, 0x39, 0xb9, 0xb8, 0xe5, 0x23,
              0xa8, 0xbe, 0xf8, 0x9d, 0x47, 0x86, 0x08, 0xc5, 0xec, 0xf6}},
        .leaf_preimages = (uint64_t[]){0, 1, 2, 3, 4, 5, 6},
        .preimage_count = 7,
        .expected_roots_len = 3,
    },
    {
        .expected_roots =
            {{0xb1, 0x51, 0xa9, 0x56, 0x13, 0x9b, 0xb8, 0x21, 0xd4, 0xef, 0xfa,
              0x34, 0xea, 0x95, 0xc1, 0x75, 0x60, 0xe0, 0x13, 0x5d, 0x1e, 0x46,
              0x61, 0xfc, 0x23, 0xce, 0xdc, 0x3a, 0xf4, 0x9d, 0xac, 0x42},
             {0x9c, 0x05, 0x3d, 0xb4, 0x06, 0xc1, 0xa0, 0x77, 0x11, 0x21, 0x89,
              0x46, 0x9a, 0x3a, 0xca, 0x05, 0x73, 0xd3, 0x48, 0x1b, 0xef, 0x09,
              0xfa, 0x3d, 0x2e, 0xda, 0x33, 0x04, 0xd7, 0xd4, 0x4b, 0xe8},
             {0x55, 0xd0, 0xa0, 0xef, 0x8f, 0x5c, 0x25, 0xa9, 0xda, 0x26, 0x6b,
              0x36, 0xc0, 0xc5, 0xf4, 0xb3, 0x10, 0x08, 0xec, 0xe8, 0x2d, 0xf2,
              0x51, 0x2c, 0x89, 0x66, 0xbd, 0xdc, 0xc2, 0x7a, 0x66, 0xa0},
             {0x4d, 0x7b, 0x3e, 0xf7, 0x30, 0x0a, 0xcf, 0x70, 0xc8, 0x92, 0xd8,
              0x32, 0x7d, 0xb8, 0x27, 0x2f, 0x54, 0x43, 0x4a, 0xdb, 0xc6, 0x1a,
              0x4e, 0x13, 0x0a, 0x56, 0x3c, 0xb5, 0x9a, 0x0d, 0x0f, 0x47}},
        .leaf_preimages =
            (uint64_t[]){0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
        .preimage_count = 15,
        .expected_roots_len = 4,
    },
    {
        .expected_roots =
            {{0x95, 0x0c, 0x36, 0x52, 0x1c, 0x4a, 0x3f, 0xa4, 0x58, 0x62, 0xf3,
              0x16, 0x82, 0xf6, 0x8b, 0x26, 0xaf, 0x1e, 0x4a, 0x48, 0x6f, 0xbf,
              0x4d, 0x7f, 0x77, 0x9a, 0x4b, 0xfc, 0xb8, 0xc9, 0xbb, 0xe9},
             {0xc9, 0xe5, 0x72, 0x5d, 0xcf, 0x41, 0x5c, 0x6e, 0xb0, 0xa2, 0xd3,
              0x81, 0xaa, 0x8b, 0x76, 0x78, 0xbc, 0x7a, 0x81, 0x0e, 0xc4, 0x6d,
              0x8f, 0x33, 0x0d, 0x4e, 0xf3, 0xce, 0x02, 0x38, 0x58, 0xbf},
             {0xa2, 0x40, 0x2e, 0xda, 0xc7, 0x6a, 0xcb, 0xf7, 0x7c, 0x01, 0xdc,
              0xe0, 0xcd, 0xf0, 0xfb, 0xcf, 0x5e, 0x6e, 0x1a, 0xcd, 0xf9, 0xeb,
              0x97, 0xb8, 0x91, 0xc2, 0xa6, 0xdc, 0x85, 0x82, 0x08, 0x6a},
             {0xbb, 0x22, 0x02, 0xe2, 0x45, 0x08, 0x1a, 0xcc, 0xf9, 0xdf, 0xeb,
              0xba, 0x22, 0x6a, 0xcd, 0xef, 0x30, 0xba, 0x22, 0x1c, 0x83, 0x50,
              0xef, 0x5c, 0x70, 0x7b, 0x0f, 0x9d, 0x29, 0x4a, 0xfe, 0x08},
             {0x25, 0x2f, 0x10, 0xc8, 0x36, 0x10, 0xeb, 0xca, 0x1a, 0x05, 0x9c,
              0x0b, 0xae, 0x82, 0x55, 0xeb, 0xa2, 0xf9, 0x5b, 0xe4, 0xd1, 0xd7,
              0xbc, 0xfa, 0x89, 0xd7, 0x24, 0x8a, 0x82, 0xd9, 0xf1, 0x11}},
        .leaf_preimages =
            (uint64_t[]){
                70,  13,  55,  152, 74,  33,  39,  122, 252, 53,  224, 211, 11,
                25,  122, 14,  191, 152, 115, 205, 160, 163, 90,  191, 199, 242,
                216, 32,  141, 6,   200, 109, 211, 53,  72,  250, 108, 163, 224,
                90,  17,  25,  92,  254, 172, 211, 26,  231, 254, 159, 183, 180,
                135, 131, 194, 83,  207, 158, 226, 49,  138, 136, 73,  143, 105,
                164, 50,  58,  94,  168, 90,  128, 132, 238, 168, 47,  153, 20,
                90,  106, 113, 168, 27,  136, 206, 3,   117, 87,  213, 48,  104,
                7,   59,  167, 164, 161, 151, 11,  63,  145, 61,  24,  40,  231,
                49,  78,  86,  52,  208, 35,  97,  15,  215, 238, 255, 227, 180,
                226, 18,  223, 126, 157, 123, 81,  149, 46,  133, 132, 173, 190,
                87,  227, 139, 199, 209, 17,  210, 112, 204, 177, 71,  195, 56,
                23,  67,  15,  226, 97,  62,  7,   235, 63,  200, 140, 104, 4,
                130, 47,  168, 33,  122, 118, 169, 129, 20,  186, 121, 114, 107,
                79,  215, 226, 45,  0,   108, 43,  53,  218, 252, 71,  176, 54,
                93,  0,   168, 238, 209, 41,  198, 111, 235, 215, 216, 60,  135,
                230, 205, 177, 102},
        .preimage_count = 199,
        .expected_roots_len = 5,
    },
};

void test_parent_hash() {
  TEST_BEGIN("parent_hash");
  unsigned char left[32] = {0}, right[32] = {0}, _parent_hash[32] = {0};
  unsigned char expected[32] = {0x02, 0x24, 0x2b, 0x37, 0xd8, 0xe8, 0x51, 0xf1,
                                0xe8, 0x6f, 0x46, 0x79, 0x02, 0x98, 0xc7, 0x09,
                                0x7d, 0xf0, 0x68, 0x93, 0xd6, 0x22, 0x6b, 0x7c,
                                0x14, 0x53, 0xc2, 0x13, 0xe9, 0x17, 0x17, 0xde};
  hash_from_u8(left, 0);
  hash_from_u8(right, 1);

  parent_hash(_parent_hash, left, right);
  ASSERT_ARRAY_EQ(_parent_hash, expected, 32);
  TEST_END;
}

void test_add_single() {
  TEST_BEGIN("add_single");
  utreexo_node_hash leaf = {.hash = {0}};
  hash_from_u8(leaf.hash, 0);

  struct utreexo_forest_file *file = NULL;
  utreexo_forest_file_init(&file, "forest.bin");

  struct utreexo_forest p = {
      .data = file,
      .roots = {0},
      .nLeaf = 0,
  };
  utreexo_forest_add(&p, leaf);

  utreexo_forest_node *root = p.roots[0];
  ASSERT_ARRAY_EQ(root->hash.hash, leaf.hash, 32);
  TEST_END;
}
void test_add_two() {
  TEST_BEGIN("add_two");
  utreexo_node_hash leaf = {.hash = {0}};
  hash_from_u8(leaf.hash, 0);

  struct utreexo_forest_file *file = NULL;
  utreexo_forest_file_init(&file, "forest.bin");

  struct utreexo_forest p = {
      .data = file,
      .roots = {0},
      .nLeaf = 0,
  };
  utreexo_forest_add(&p, leaf);
  utreexo_forest_add(&p, leaf);
  utreexo_forest_node *root = p.roots[1];

  unsigned char expected[32] = {0};
  parent_hash(expected, leaf.hash, leaf.hash);

  ASSERT_ARRAY_EQ(root->hash.hash, expected, 32);
  TEST_END;
}

void test_add_many() {
  TEST_BEGIN("add_many");
  uint8_t values[] = {0, 1, 2, 3, 4, 5, 6, 7};
  uint8_t expected[32] = {0xb1, 0x51, 0xa9, 0x56, 0x13, 0x9b, 0xb8, 0x21,
                          0xd4, 0xef, 0xfa, 0x34, 0xea, 0x95, 0xc1, 0x75,
                          0x60, 0xe0, 0x13, 0x5d, 0x1e, 0x46, 0x61, 0xfc,
                          0x23, 0xce, 0xdc, 0x3a, 0xf4, 0x9d, 0xac, 0x42};

  struct utreexo_forest_file *file = NULL;
  utreexo_forest_file_init(&file, "forest.bin");

  struct utreexo_forest p = {
      .data = file,
      .roots = {0},
      .nLeaf = 0,
  };
  for (int i = 0; i < 8; i++) {
    utreexo_node_hash leaf = {.hash = {0}};
    hash_from_u8(leaf.hash, values[i]);
    utreexo_forest_add(&p, leaf);
  }
  utreexo_forest_node *root = p.roots[3];

  ASSERT_ARRAY_EQ(root->hash.hash, expected, 32);
  TEST_END;
}

/* Tests from https://github.com/mit-dci/rustreexo */
void test_from_test_cases(void) {
  TEST_BEGIN("rustreexo test suite");
  for (int i = 0; i < 4; i++) {
    const add_test_data *tc = &insertion_tests[i];
    struct utreexo_forest_file *file = NULL;
    char filename[100] = {0};

    sprintf(filename, "forest_%d.bin", i);
    utreexo_forest_file_init(&file, filename);
    struct utreexo_forest p = {
        .data = file,
        .roots = {0},
        .nLeaf = 0,
    };

    for (int j = 0; j < tc->preimage_count; j++) {
      utreexo_node_hash leaf = {.hash = {0}};
      hash_from_u8(leaf.hash, tc->leaf_preimages[j]);
      utreexo_forest_add(&p, leaf);
    }

    assert(p.nLeaf == tc->preimage_count);

    size_t root = 63;

    for (size_t j = 0; j < tc->expected_roots_len; ++j) {
      while (p.roots[root] == NULL && root >= 0)
        --root;
      if (root < 0) {
        printf("missing roots\n");
        abort();
      }
      ASSERT_ARRAY_EQ(p.roots[root]->hash.hash, tc->expected_roots[j], 32);
      --root;
    }
  }

  TEST_END;
}

void test_grab_node() {
  struct utreexo_forest_file *file = NULL;
  utreexo_forest_file_init(&file, "test_grab_node.bin");

  struct utreexo_forest p = {
      .data = file,
      .roots = {0},
      .nLeaf = 0,
  };

  for (size_t i = 0; i < 15; ++i) {
    utreexo_node_hash leaf = {.hash = {0}};
    hash_from_u8(leaf.hash, i);
    utreexo_forest_add(&p, leaf);
  }

  utreexo_forest_node *node = NULL, *sibling = NULL, *parent = NULL;
  grab_node(&p, &node, &sibling, &parent, 4);
  utreexo_forest_print(p.roots[1]);
  for (size_t i = 0; i < 32; ++i)
    printf("%02x", node->hash.hash[i]);

  printf("\n");
}

int main() {
  // test_parent_hash();
  // test_add_single();
  // test_add_two();
  // test_add_many();
  // test_from_test_cases();
  test_grab_node();
  return 0;
}
