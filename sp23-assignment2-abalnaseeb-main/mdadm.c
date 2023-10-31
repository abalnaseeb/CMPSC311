/*
Ali AlNaseeb
CMPSC 311
997506551
*/

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mdadm.h"
#include "jbod.h"

typedef struct JBOD
{
  uint8_t current_block_id;
  int8_t current_disk_id;
  uint32_t block_pointer;
} JBOD;

int mount_status = 1;
JBOD jbod = {0, 0, 0}; // Explicitly initialize all fields to zero

uint32_t construct_block_operation(uint8_t block_id, uint16_t reserved_bits, uint8_t disk_id, uint8_t command)
{
  uint32_t op = ((uint32_t)block_id) | (((uint32_t)reserved_bits) << 8) | (((uint32_t)disk_id) << 22) | (((uint32_t)command) << 26);
  return op;
}

int mdadm_mount(void)
{
  // Check if the system is currently unmounted
  if (mount_status == 1)
  {
    // Mount the system by calling jbod_operation with a constructed block operation
    jbod_operation(construct_block_operation(jbod.current_block_id, 0, jbod.current_disk_id, JBOD_MOUNT), NULL);
    // Update the mount status and JBOD struct to reflect the mounted state
    mount_status = 2;
    jbod.current_block_id = 0;
    jbod.current_disk_id = 0;
    jbod.block_pointer = 0;
    // Return 1 to indicate success
    return 1;
  }
  // Return -1 to indicate failure if the system is already mounted
  return -1;
}

int mdadm_unmount(void)
{
  // Check if the system is currently mounted
  if (mount_status == 2)
  {
    // Unmount the system by calling jbod_operation with a constructed block operation
    jbod_operation(construct_block_operation(jbod.current_block_id, 0, jbod.current_disk_id, JBOD_UNMOUNT), NULL);
    // Update the mount status to reflect the unmounted state
    mount_status = 1;
    // Return 1 to indicate success
    return 1;
  }
  // Return -1 to indicate failure if the system is already unmounted
  return -1;
}

// Move the disk head to the specified block and disk.
int seek(uint8_t newBlockID, uint8_t newDiskID);

// Read data from the disk into a buffer.
int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf)
{
  // Check if the file system is mounted.
  if (mount_status == 1)
  {
    return -1;
  }

  // Check that the parameters are valid.
  if ((len > 1024) || ((len != 0) && (buf == NULL)) || (addr + len > (JBOD_DISK_SIZE * JBOD_NUM_DISKS)))
  {
    return -1;
  }

  // If the length is 0 and the buffer is NULL, return success.
  if (len == 0 && buf == NULL)
  {
    return 0;
  }

  // Create a local buffer to hold the data read from the disk.
  uint8_t local_buff[JBOD_BLOCK_SIZE];
  uint32_t remaining_len = len;
  uint32_t block_offset = addr % JBOD_BLOCK_SIZE;

  // Read the data from the disk one block at a time.
  while (remaining_len > 0)
  {
    uint32_t current_block_id = (addr / JBOD_BLOCK_SIZE) % JBOD_NUM_BLOCKS_PER_DISK;
    uint32_t current_disk_id = addr / JBOD_DISK_SIZE;

    // Move the disk head to the current block and disk.
    seek(current_block_id, current_disk_id);

    // Read the current block from the disk into the local buffer.
    uint32_t read_op = construct_block_operation(current_block_id, 0, current_disk_id, JBOD_READ_BLOCK);
    jbod_operation(read_op, local_buff);

    // Copy the data from the local buffer to the output buffer.
    uint32_t bytes_to_read = (remaining_len > JBOD_BLOCK_SIZE - block_offset) ? JBOD_BLOCK_SIZE - block_offset : remaining_len;
    memcpy(buf + (len - remaining_len), local_buff + block_offset, bytes_to_read);

    // Update the remaining length and the block offset.
    remaining_len -= bytes_to_read;
    block_offset = 0;
    addr += bytes_to_read;
  }

  // Update the current block and disk in the file system.
  jbod.current_block_id = (addr / JBOD_BLOCK_SIZE) % JBOD_NUM_BLOCKS_PER_DISK;
  jbod.current_disk_id = addr / JBOD_DISK_SIZE;

  // Return the length of the data read.
  return len;
}

// Move the disk head to the specified block and disk.
int seek(uint8_t new_block_id, uint8_t new_disk_id)
{
  // Check if the file system is mounted.
  if (mount_status == 1)
  {
    return -1;
  }

  // Create block and disk operation codes to move the disk head.
  uint32_t new_block_op = construct_block_operation(new_block_id, 0, 0, JBOD_SEEK_TO_BLOCK);
  uint32_t new_disk_op = construct_block_operation(0, 0, new_disk_id, JBOD_SEEK_TO_DISK);

  // Perform the block and disk operations.
  if ((jbod_operation(new_disk_op, NULL) != 0) || (jbod_operation(new_block_op, NULL) != 0))
  {
    return -1;
  }

  // Update the current block and disk in the file system.
  jbod_operation(new_disk_op, NULL);
  jbod_operation(new_block_op, NULL);
  jbod.current_block_id = new_block_id;
  jbod.current_disk_id = new_disk_id;

  return 1;
}
