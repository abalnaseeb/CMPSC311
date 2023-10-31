#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"
#include "net.h"

typedef struct JBOD
{
  uint8_t current_block_id;
  int8_t current_disk_id;
  uint32_t block_pointer;
} JBOD;

JBOD jbod;
int mount_status = 1;
uint32_t block_constructor(uint8_t BlockID, uint16_t Reserved, uint8_t Disk_ID, uint8_t Command)
{
  return (BlockID | ((uint32_t)Reserved << 8) | ((uint32_t)Disk_ID << 22) | ((uint32_t)Command << 26));
}

/*
#######   same   ###############################################################################################################################################################
*/
int mdadm_mount(void)
{
  if (mount_status != 1)
    return -1;
  jbod_client_operation(block_constructor(jbod.current_block_id, 0, jbod.current_disk_id, JBOD_MOUNT), NULL);
  mount_status = 2, jbod.current_block_id = jbod.current_disk_id = jbod.block_pointer = 0;
  return 1;
}

/*
#######   same   ###############################################################################################################################################################
*/

int mdadm_unmount(void)
{
  if (mount_status == 2)
  {
    jbod_client_operation(block_constructor(jbod.current_block_id, 0, jbod.current_disk_id, JBOD_UNMOUNT), NULL);
    return (mount_status = 1, 1);
  }
  return -1;
}

/*
############################################################################################################################################################
*/

int seek(uint8_t newBlockID, uint8_t newDiskID)
{
  if (mount_status == 1)
    return -1;
  if (jbod_client_operation(block_constructor(0, 0, newDiskID, JBOD_SEEK_TO_DISK), NULL) != 0 ||
      jbod_client_operation(block_constructor(newBlockID, 0, 0, JBOD_SEEK_TO_BLOCK), NULL) != 0)
    return -1;
  jbod.current_block_id = newBlockID;
  jbod.current_disk_id = newDiskID;
  jbod_client_operation(block_constructor(0, 0, newDiskID, JBOD_SEEK_TO_DISK), NULL);
  jbod_client_operation(block_constructor(newBlockID, 0, 0, JBOD_SEEK_TO_BLOCK), NULL);
  return 1;
}

/*
  #####  modified   ##################################################################################################################################################
*/

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf)
{
  if (mount_status == 1 || len > 1024 || len < 0 || (len != 0 && buf == NULL) || addr + len > (JBOD_DISK_SIZE * JBOD_NUM_DISKS))
  {
    return -1;
  }

  uint8_t localBuff[JBOD_BLOCK_SIZE];
  jbod.current_disk_id = addr / JBOD_DISK_SIZE;
  jbod.current_block_id = (addr / JBOD_BLOCK_SIZE) % JBOD_NUM_BLOCKS_PER_DISK;
  jbod.block_pointer = addr % JBOD_BLOCK_SIZE;
  uint32_t length = len;

  /*
  #####  modified   ##################################################################################################################################################
  */
  while (length > 0)
  {
    seek(jbod.current_block_id, jbod.current_disk_id);
    uint32_t read_op = block_constructor(jbod.current_block_id, 0, jbod.current_disk_id, JBOD_READ_BLOCK);

    if (cache_enabled() == true && cache_lookup(jbod.current_disk_id, jbod.current_block_id, localBuff) == -1)
    {
      jbod_client_operation(read_op, localBuff);
      cache_insert(jbod.current_disk_id, jbod.current_block_id, localBuff);
    }
    else if (cache_enabled() == false)
    {
      jbod_client_operation(read_op, localBuff);
    }

    int copy_len = (length > JBOD_BLOCK_SIZE - jbod.block_pointer) ? JBOD_BLOCK_SIZE - jbod.block_pointer : length;
    memcpy(buf + (len - length), localBuff + jbod.block_pointer, copy_len);

    jbod.block_pointer += copy_len;
    length -= copy_len;

    jbod.block_pointer = (jbod.block_pointer >= JBOD_BLOCK_SIZE) ? 0 : jbod.block_pointer;
    jbod.current_block_id += (jbod.block_pointer == 0 && jbod.current_block_id < JBOD_NUM_BLOCKS_PER_DISK - 1) ? 1 : 0;
    jbod.current_disk_id += (jbod.block_pointer == 0 && jbod.current_block_id == 0) ? 1 : 0;
  }

  /*
  #####  modified   ##################################################################################################################################################
  */
  if (++jbod.current_block_id >= JBOD_NUM_BLOCKS_PER_DISK)
  {
    jbod.current_block_id = 0;
    ++jbod.current_disk_id;
  }
  jbod.block_pointer = 0;
  return len;
}

/*
  #####  modified   ##################################################################################################################################################
*/

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf)
{
  if (mount_status == 1 || len > 1024 || len < 0 || (len != 0 && buf == NULL) || addr + len > JBOD_DISK_SIZE * JBOD_NUM_DISKS)
  {
    return -1;
  }

  /*
  ######################################################################################################################################################################
  */
  uint8_t localBuff[JBOD_BLOCK_SIZE];
  jbod.current_disk_id = (addr / JBOD_DISK_SIZE);
  jbod.current_block_id = ((addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE);
  uint32_t length = len;
  jbod.block_pointer = addr % JBOD_BLOCK_SIZE;

  /*
  ######################################################################################################################################################################
  */
  while (0 < length)
  {
    seek(jbod.current_block_id, jbod.current_disk_id);
    uint32_t read_op = block_constructor(jbod.current_block_id, 0, jbod.current_disk_id, JBOD_READ_BLOCK);

    jbod_client_operation(read_op, localBuff);
    seek(jbod.current_block_id, jbod.current_disk_id);
    /*
  #####  modified   ##################################################################################################################################################
    */
    if (jbod.block_pointer + length > JBOD_BLOCK_SIZE - 1)
    {
      memcpy(localBuff + jbod.block_pointer, buf + (len - length), JBOD_BLOCK_SIZE - jbod.block_pointer);
      length -= JBOD_BLOCK_SIZE - jbod.block_pointer;
      if (cache_enabled())
        cache_insert(jbod.current_disk_id, jbod.current_block_id, localBuff);
      jbod.current_block_id = (jbod.current_block_id + 1) % JBOD_NUM_BLOCKS_PER_DISK;
      jbod.block_pointer = 0;
      if (jbod.current_block_id == 0)
        jbod.current_disk_id++;
    }

    else
    {
      memcpy(localBuff + jbod.block_pointer, buf + (len - length), length);
      length = 0;
    }

    /*
    ######################################################################################################################################################################
    */
    jbod_client_operation(block_constructor(jbod.current_block_id, 0, jbod.current_disk_id, JBOD_WRITE_BLOCK), localBuff);
    if (cache_enabled())
      cache_update(jbod.current_disk_id, jbod.current_block_id, localBuff);
  }
  /*
  #####  modified   ##################################################################################################################################################
  */
  jbod.current_block_id = (jbod.current_block_id + 1) % JBOD_NUM_BLOCKS_PER_DISK;
  if (jbod.current_block_id == 0)
    jbod.current_disk_id++;
  jbod.block_pointer = 0;
  return len;
}
