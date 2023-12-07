#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

typedef char ALIGNMENT[16];

union header {
	struct {
		size_t size;
		unsigned is_free;
		union header *next;
	} h;
	ALIGNMENT stub;
};
typedef union header header_t;

static header_t *head = NULL;
static header_t *tail = NULL;
static pthread_mutex_t global_malloc_lock = PTHREAD_MUTEX_INITIALIZER;

static void *allocate_block(size_t size);
static header_t *get_free_block(size_t size);

static header_t *get_free_block(size_t size) {
  header_t *current_block = head;

  while(current_block) {
    if(current_block->h.is_free && current_block->h.size >= size) {
      return current_block;
    }

    current_block = current_block->h.next;
  }

  return NULL;
}

static void *allocate_block(size_t size) {
  size_t total_size = size + sizeof(header_t);

  void *new_block = sbrk(total_size);

  if(new_block == (void *) -1) {
    pthread_mutex_unlock(&global_malloc_lock);
    return NULL;
  }

  header_t *block = new_block;
  
  block->h.size = size;
  block->h.is_free = 0;
  block->h.next = NULL;

  return (void*)block;
}

void *malloc(size_t size) {
  if(!size) {
    return NULL;
  }

  pthread_mutex_lock(&global_malloc_lock);

  header_t *free_block = get_free_block(size);
  
  if(free_block) {
    free_block->h.is_free = 0;
    pthread_mutex_unlock(&global_malloc_lock);

    return (void *)free_block + 1;
  }

  header_t *new_block = allocate_block(size);
  pthread_mutex_unlock(&global_malloc_lock);

  if(!head) {
    head = new_block;
  }

  if(tail) {
    tail->h.next = new_block;
  }

  tail = new_block;

  return (void*)(new_block + 1);
}


void free(void *block) {
  if (!block) {
    return;
  }

  pthread_mutex_lock(&global_malloc_lock);
  header_t *header = (header_t *)block - 1;

  void *program_break = sbrk(0);

  header_t *tmp;
  if ((char *)block + header->h.size == program_break) {
    if (head == tail) {
      head = tail = NULL;
    } else {
      tmp = head;
      while (tmp) {
        if (tmp->h.next == tail) {
          tmp->h.next = NULL;
          tail = tmp;
        }
        tmp = tmp->h.next;
      }
    }

    sbrk(0 - sizeof(header_t) - header->h.size);
    pthread_mutex_unlock(&global_malloc_lock);
    return;
  }

  header->h.is_free = 1;
  pthread_mutex_unlock(&global_malloc_lock);
}

void *realloc(void *block, size_t size) {
  if (size == 0) {
    free(block);
    return NULL;
  }

  if (block == NULL) {
    return malloc(size);
  }

  header_t *header = (header_t *)block - 1;

  if (header->h.size >= size) {
    return block;
  }

  void *ret = malloc(size);

  if (ret) {
    memcpy(ret, block, header->h.size);
    free(block);
  }

  return ret;
}

void print_mem_list()
{
	header_t *curr = head;
	printf("head = %p, tail = %p \n", (void*)head, (void*)tail);
	while(curr) {
		printf("addr = %p, size = %zu, is_free=%u, next=%p\n",
			(void*)curr, curr->h.size, curr->h.is_free, (void*)curr->h.next);
		curr = curr->h.next;
	}
}
