#ifndef UTREEXO_FLAT_FILE_IMPL_H
#define UTREEXO_FLAT_FILE_IMPL_H

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "flat_file.h"
int posix_fallocate(int fd, off_t offset, off_t len);
static inline void utreexo_forest_file_close(struct utreexo_forest_file *file) {
  munmap(file->map, file->filesize);
  close(file->fd);
  free(file);
}

static inline void utreexo_forest_file_init(struct utreexo_forest_file **file,
                                            const char *filename) {

  int fd = 0;

  *file =
      (struct utreexo_forest_file *)malloc(sizeof(struct utreexo_forest_file));

  fd = open(filename, O_RDWR | O_CREAT, 0644);
  if (fd < 0) {
    perror("open");
    exit(1);
  }

  const int fsize = lseek(fd, 0, SEEK_END);

  char *data = (char *)mmap(NULL, 1024 * 1024 * 1024,
                            PROT_READ | PROT_WRITE | PROT_GROWSUP,
                            MAP_FILE | MAP_SHARED, fd, 0);

  if (data == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  (*file)->wrt_page = (struct utreexo_forest_page_header *)(data + sizeof(int));

  (*file)->fd = fd;
  (*file)->filename = filename;
  (*file)->filesize = fsize;
  (*file)->n_pages = 0;
  (*file)->map = data + sizeof(int);
  /* This is a new file, we need to initialize at least the first page */
  if (fsize < 4 || *(int *)data != FILE_MAGIC) {
    DEBUG_PRINT("No pages found, creating new file\n");
    posix_fallocate(fd, 0, 4);
    utreexo_forest_page_alloc(*file);
    *(int *)data = FILE_MAGIC;
    return;
  }
  (*file)->n_pages = 1;
  (*file)->wrt_page = (struct utreexo_forest_page_header *)(data + 4);
  DEBUG_PRINT("Found %d pages\n", (*file)->n_pages);
}
static inline int utreexo_forest_page_alloc(struct utreexo_forest_file *file) {
  DEBUG_PRINT("Allocating page\n");
  const int page_offset = file->n_pages;
  file->n_pages++;
  file->filesize += PAGE_SIZE;
  posix_fallocate(file->fd, file->filesize, (PAGE_SIZE)*file->n_pages);

  char *pg = (((char *)file->map) + PAGE_SIZE * page_offset);
  file->wrt_page = (struct utreexo_forest_page_header *)pg;

  utreexo_forest_mkpg(file, file->wrt_page);
  DEBUG_PRINT("Allocated page %d\n", page_offset);
  ASSERT(file->wrt_page->n_nodes == 0)
  ASSERT(file->wrt_page->pg_magic == MAGIC)
  ASSERT(file->n_pages == page_offset + 1)
  return EXIT_SUCCESS;
}

static inline void utreexo_forest_mkpg(struct utreexo_forest_file *file,
                                       struct utreexo_forest_page_header *pg) {
  pg->pg_magic = MAGIC;
  pg->n_nodes = 0;

  DEBUG_PRINT("Initialized page %d\n", file->n_pages - 1);
  ASSERT(pg->n_nodes == 0)
  ASSERT(pg->pg_magic == MAGIC)
}

static inline void
utreexo_forest_file_node_get(struct utreexo_forest_file *file,
                             utreexo_forest_node **node,
                             utreexo_forest_node_ptr ptr) {
  DEBUG_PRINT("Reading node %d from page %d offset=%d\n", ptr.offset, ptr.page,
              ptr.offset);
  char *page_data = PAGE_DATA(file->map, ptr.page);
  utreexo_forest_node *nodes = (utreexo_forest_node *)page_data;
  *node = nodes + ptr.offset;
}

static inline void
utreexo_forest_file_node_put(struct utreexo_forest_file *file,
                             utreexo_forest_node_ptr *offset,
                             utreexo_forest_node node) {
  const uint64_t page_nodes = file->wrt_page->n_nodes;
  if (page_nodes == NODES_PER_ARENA) {
    DEBUG_PRINT("Page is full, allocating new page\n");
    if (utreexo_forest_page_alloc(file)) {
      fprintf(stderr, "Failed to allocate page\n");
      exit(1);
    }
  }
  const uint64_t page_offset = page_nodes;
  DEBUG_PRINT("Writing node %d to page %d offset=%d\n", page_nodes,
              file->n_pages - 1, page_offset);
  memcpy((utreexo_forest_node *)((char *)file->wrt_page + 16) + page_nodes,
         &node, sizeof(node));
  ++(file->wrt_page->n_nodes);
  *offset = (utreexo_forest_node_ptr){.offset = page_offset,
                                      .page = file->n_pages - 1};
}

static inline void
utreexo_forest_file_node_del(struct utreexo_forest_file *file,
                             utreexo_forest_node_ptr ptr) {
  struct utreexo_forest_page_header *pg =
      (struct utreexo_forest_page_header *)((char *)file->map +
                                            ptr.page * PAGE_SIZE);
  if (pg->n_nodes == 0) {
    fprintf(stderr, "Trying to delete from empty page\n");
    exit(1);
  }
  if (pg->n_nodes == 1) {
    DEBUG_PRINT("Deallocating page %d\n", ptr.page);
    struct utreexo_forest_free_page *f =
        (struct utreexo_forest_free_page *)malloc(
            sizeof(struct utreexo_forest_free_page));
    *f = (struct utreexo_forest_free_page){
        .pos = ptr.page * PAGE_SIZE,
        .next = file->free_list.tail,
    };
    if (file->free_list.head == NULL) {
      file->free_list.head = f;
    } else {
      file->free_list.tail->next = f;
      file->free_list.tail = f;
    }
  }
  DEBUG_PRINT("Deleting node %d from page %d\n", ptr.offset, ptr.page);
  --pg->n_nodes;
}
#endif