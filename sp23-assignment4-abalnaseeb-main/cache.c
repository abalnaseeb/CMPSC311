#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries)
{
  // Check if cache has already been created
  if (cache != NULL)
  {
    return -1; // Cache already exists
  }

  // Check parameter value
  if (num_entries < 2 || num_entries > 4096)
  {
    return -1; // Invalid number of entries
  }

  // Allocate memory for cache and initialize cache entries to zero
  cache = calloc(num_entries, sizeof(cache_entry_t));
  if (cache == NULL)
  {
    return -1; // Failed to allocate memory
  }
  memset(cache, 0, num_entries * sizeof(cache_entry_t));

  // Set cache size and return success
  cache_size = num_entries;
  return 0; // Success
}

int cache_destroy(void)
{
  // Check if cache exists
  if (cache == NULL)
  {
    return -1; // Cache does not exist, return error
  }

  // Free memory used by cache
  free(cache);

  // Reset global variables
  cache = NULL;
  cache_size = 0;
  clock = 0;

  return 0; // Success
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf)
{
  // Check for null cache or buffer
  if (cache == NULL || buf == NULL)
  {
    return -1;
  }

  // Bounds check
  if (block_num < 0 || block_num >= 256 || disk_num < 0 || disk_num >= 16)
  {
    return -1;
  }

  // Increment query counter
  num_queries++;

  // Look for block in cache
  for (int i = 0; i < cache_size; i++)
  {
    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num && cache[i].valid)
    {
      // Block found in cache
      memcpy(buf, cache[i].block, 256);
      cache[i].access_time = clock++;
      num_hits++;
      return 1;
    }
  }

  // Block not found in cache
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf)
{
  const int MAX_DISK_NUM = 16;
  const int MAX_BLOCK_NUM = 256;
  const int MAX_CACHE_SIZE = 1024;

  // Error handling
  if (buf == NULL)
  {
    printf("Error: NULL buffer\n");
    return;
  }
  if (disk_num < 0 || disk_num >= MAX_DISK_NUM)
  {
    printf("Error: Invalid disk number %d\n", disk_num);
    return;
  }
  if (block_num < 0 || block_num >= MAX_BLOCK_NUM)
  {
    printf("Error: Invalid block number %d\n", block_num);
    return;
  }

  // Lookup the block in the cache
  int i;
  for (i = 0; i < cache_size; i++)
  {
    if (cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num)
    {
      // Block found in cache, update it
      memcpy(cache[i].block, buf, MAX_BLOCK_NUM);
      cache[i].access_time = clock++;
      return;
    }
  }

  // Block not found in cache, add it if there is space
  if (cache_size < MAX_CACHE_SIZE)
  {
    cache[cache_size].disk_num = disk_num;
    cache[cache_size].block_num = block_num;
    memcpy(cache[cache_size].block, buf, MAX_BLOCK_NUM);
    cache[cache_size].valid = true;
    cache[cache_size].access_time = clock++;
    cache_size++;
  }
  else
  {
    // Cache is full, replace the least recently used block
    int oldest_index = 0;
    int oldest_time = cache[0].access_time;
    for (i = 1; i < cache_size; i++)
    {
      if (cache[i].access_time < oldest_time)
      {
        oldest_index = i;
        oldest_time = cache[i].access_time;
      }
    }
    cache[oldest_index].disk_num = disk_num;
    cache[oldest_index].block_num = block_num;
    memcpy(cache[oldest_index].block, buf, MAX_BLOCK_NUM);
    cache[oldest_index].valid = true;
    cache[oldest_index].access_time = clock++;
  }
}
// Done
// This function inserts data into the cache
// disk_num: integer representing the disk number
// block_num: integer representing the block number
// buf: pointer to the buffer containing the data
// Returns 1 on success, -1 on failure
int cache_insert(int disk_num, int block_num, const uint8_t *buf)
{
  // Check if cache, buf, disk_num or block_num are invalid
  if (!cache || !buf || block_num < 0 || block_num >= 256 || disk_num < 0 || disk_num >= 16)
  {
    return -1;
  }

  // Search the cache for the given disk and block number
  for (int index = 0; index < cache_size; index++)
  {
    // If block entry for block and disk num exists, return -1
    if (cache[index].disk_num == disk_num && cache[index].block_num == block_num && cache[index].valid)
    {
      return -1;
    }
  }

  // Find the least recently used cache entry
  int lru_index = 0;
  for (int index = 0; index < cache_size; index++)
  {
    if (cache[index].access_time < cache[lru_index].access_time)
    {
      lru_index = index;
    }
  }

  // Copy contents of buffer into the cache and update the cache entry properties
  cache[lru_index].block_num = block_num;
  cache[lru_index].disk_num = disk_num;
  cache[lru_index].valid = true;
  cache[lru_index].access_time = ++clock;
  memcpy(cache[lru_index].block, buf, 256);

  return 1;
}

// This function returns true if the cache is enabled and false otherwise.
bool is_cache_enabled()
{
  return (cache != NULL);
}

// This function prints the cache hit rate to the standard error stream.
// It assumes that the variables num_hits and num_queries have been correctly updated.
void cache_print_hit_rate()
{
  if (num_queries == 0)
  {
    fprintf(stderr, "No cache queries made.\n");
    return;
  }

  float hit_rate = 100 * (float)num_hits / num_queries;
  fprintf(stderr, "Hit rate: %5.1f%%\n", hit_rate);
}
