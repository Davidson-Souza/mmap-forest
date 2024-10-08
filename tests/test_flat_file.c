/* Tests the flat files implementation */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "flat_file.h"
#include "flat_file_impl.h"
#include "forest_node.h"
#include "parent_hash.h"
#include "test_utils.h"

static const char expected_hash[][32] = {{0x00, 0x01, 0x02, 0x03},
                                         {0x00, 0x01, 0x02, 0x04},
                                         {0x00, 0x01, 0x02, 0x05}};
// can we add stuff to the file?
utreexo_forest_node *test_create_nodes(struct utreexo_forest_file *file);

// can we get nodes back?
void test_retrieve_nodes(const utreexo_forest_node *parent);
// Can we delete stuff from the fale?
void test_delete_nodes(struct utreexo_forest_file *file,
                       const utreexo_forest_node *parent_pos);
// Add a bunch of stuff to see if page allocation works
void test_add_many(int n_adds);
// Add and remove a bunch of stuff to see if page reallocation works (TODO)

// Test if a page gets reused after being dealocated
void test_free_page_list();

int main() {
  struct utreexo_forest_file *file;
  void *heap = NULL;
  utreexo_forest_file_init(&file, &heap, "flat_file_test.bin");

  const utreexo_forest_node *parent = test_create_nodes(file);
  test_retrieve_nodes(parent);
  test_delete_nodes(file, parent);
  utreexo_forest_file_close(file);
  test_add_many(NODES_PER_PAGE + 3);
  test_free_page_list();
  return 0;
}

void print_hash(utreexo_node_hash hash) {
  for (int i = 0; i < 32; i++) {
    printf("%02x", hash.hash[i]);
  }
  printf("\n");
}

utreexo_forest_node *test_create_nodes(struct utreexo_forest_file *file) {
  TEST_BEGIN("create nodes");

  utreexo_forest_node *right_child_pos;
  utreexo_forest_node *left_child_pos;
  utreexo_forest_node *parent_pos;

  right_child_pos = utreexo_forest_file_node_alloc(file);
  *right_child_pos = (utreexo_forest_node){
      .hash = {{0x00, 0x01, 0x02, 0x05}},
      .left_child = 0,
      .right_child = 0,
      .parent = 0,
  };

  left_child_pos = utreexo_forest_file_node_alloc(file);
  *left_child_pos = (utreexo_forest_node){
      .hash = {{0x00, 0x01, 0x02, 0x04}},
      .left_child = 0,
      .right_child = 0,
      .parent = 0,
  };
  parent_pos = utreexo_forest_file_node_alloc(file);
  *parent_pos = (utreexo_forest_node){
      .hash = {{0x00, 0x01, 0x02, 0x03}},
      .parent = 0,
      .left_child = left_child_pos,
      .right_child = right_child_pos,
  };

  TEST_END;

  return parent_pos;
}

void test_delete_nodes(struct utreexo_forest_file *file,
                       const utreexo_forest_node *parent) {
  TEST_BEGIN("delete nodes");

  utreexo_forest_file_node_del(file, parent->left_child);

  utreexo_forest_file_node_del(file, parent->right_child);

  utreexo_forest_file_node_del(file, parent);
  TEST_END;
}

void test_retrieve_nodes(const utreexo_forest_node *parent) {
  TEST_BEGIN("retrieve nodes");

  // check the parent node
  ASSERT_ARRAY_EQ(parent->hash.hash, expected_hash[0], 32);
  // check the left child
  ASSERT_ARRAY_EQ(parent->left_child->hash.hash, expected_hash[1], 32);
  // check the right child
  ASSERT_ARRAY_EQ(parent->right_child->hash.hash, expected_hash[2], 32);

  TEST_END;
}

void test_add_many(int n_adds) {
  TEST_BEGIN("add many");

  void *heap = NULL;
  struct utreexo_forest_file *file;
  utreexo_forest_file_init(&file, &heap, "flat_fileadd_many.bin");

  for (int i = 0; i < n_adds; i++) {
    utreexo_forest_file_node_alloc(file);
  }

  utreexo_forest_file_close(file);
  TEST_END;
}

void test_free_page_list() {
  TEST_BEGIN("test free page reallocation");
  struct utreexo_forest_file *file;
  void *heap = NULL;
  utreexo_forest_file_init(&file, &heap, "flat_file_reallocation.bin");
  utreexo_forest_node *nodes[NODES_PER_PAGE] = {0};
  const utreexo_forest_node node = {
      .hash = {{0}},
      .parent = NULL,
      .left_child = NULL,
      .right_child = NULL,
  };

  // Fills up a page
  for (int i = 0; i < NODES_PER_PAGE; ++i) {
    nodes[i] = utreexo_forest_file_node_alloc(file);
    memcpy(nodes[i], &node, sizeof(utreexo_forest_node));
  }
  utreexo_forest_node *pnode = NULL;

  // triggers the allocation of a new page
  for (int i = 0; i < NODES_PER_PAGE; ++i) {
    pnode = utreexo_forest_file_node_alloc(file);
    memcpy(pnode, &node, sizeof(utreexo_forest_node));
  }

  // remove all nodes from the first page
  for (int i = 0; i < NODES_PER_PAGE; ++i) {
    utreexo_forest_file_node_del(file, nodes[i]);
  }

  // Add one node
  pnode = utreexo_forest_file_node_alloc(file);

  // It should be where nodes[0] was (the beggining of the first page)
  ASSERT_EQ(pnode, nodes[0]);
  TEST_END;
}
