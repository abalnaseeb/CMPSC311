#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"
/*
#######   modified   ###############################################################################################################################################################
*/
int cli_sd = -1, lengthSize = 2, opSize = 4, retSize = 2;
/*
#######   modified   ###############################################################################################################################################################
*/

static bool nread(int fd, int len, uint8_t *buf)
{
  int loopResult;
  for (int bytesRead = 0; bytesRead < len; bytesRead += loopResult)
  {
    loopResult = read(fd, buf + bytesRead, len - bytesRead);
    if (loopResult < 0)
      return false;
  }
  return true;
}

/*
#######   modified   ###############################################################################################################################################################
*/
static bool nwrite(int fd, int len, uint8_t *buf)
{
  int bytesWritten = 0;
  while (bytesWritten < len && (bytesWritten += write(fd, buf + bytesWritten, len - bytesWritten)) > 0)
    ;
  return bytesWritten == len;
}

/*
#######   modified   ###############################################################################################################################################################
*/
static bool recv_packet(int sd, uint32_t *op, uint16_t *ret, uint8_t *block)
{
  uint8_t headbuff[HEADER_LEN];
  if (!nread(sd, HEADER_LEN, headbuff))
    return false;

  int headOffset = 0;
  uint16_t length = ntohs(*(uint16_t *)(headbuff + headOffset));
  headOffset += lengthSize;
  uint32_t nOp = ntohl(*(uint32_t *)(headbuff + headOffset));
  headOffset += opSize;
  uint16_t nReturn = ntohs(*(uint16_t *)(headbuff + headOffset));

  *op = nOp;
  *ret = nReturn;

  if (length > HEADER_LEN && !nread(sd, JBOD_BLOCK_SIZE, block))
    return false;
  return true;
}
/*
#######   modified   ###############################################################################################################################################################
*/
static bool send_packet(int sd, uint32_t op, uint8_t *block)
{
  uint8_t buff[HEADER_LEN + JBOD_BLOCK_SIZE];
  uint16_t length = HEADER_LEN + ((op >> 26) == JBOD_WRITE_BLOCK ? JBOD_BLOCK_SIZE : 0);
  uint16_t nLength = htons(length);
  uint32_t nOp = htonl(op);

  memcpy(buff, &nLength, lengthSize);
  memcpy(buff + lengthSize, &nOp, opSize);

  if ((op >> 26) == JBOD_WRITE_BLOCK)
  {
    memcpy(buff + HEADER_LEN, block, JBOD_BLOCK_SIZE);
  }

  return nwrite(sd, length, buff);
}

/*
#######   modified   ###############################################################################################################################################################
*/
bool jbod_connect(const char *ip, uint16_t port)
{
  struct sockaddr_in ipv4_addr = {.sin_family = AF_INET, .sin_port = htons(port)};
  return (cli_sd = socket(PF_INET, SOCK_STREAM, 0)) != -1 && connect(cli_sd, (struct sockaddr *)&ipv4_addr, sizeof ipv4_addr) != -1;

  /*
#######   modified   ###############################################################################################################################################################
  */
  if (inet_aton(ip, &ipv4_addr.sin_addr) == 0 || connect(cli_sd, (const struct sockaddr *)&ipv4_addr, sizeof(ipv4_addr)) == -1)
  {
    return false;
  }
  return true;
}
/*
##### the same  ###########################################################################################################################################################
*/
void jbod_disconnect(void)
{
  close(cli_sd);
  cli_sd = -1;
}
/*
#######   modified   ###############################################################################################################################################################
*/
int jbod_client_operation(uint32_t op, uint8_t *block)
{
  uint16_t ret;
  if (!send_packet(cli_sd, op, block) || !recv_packet(cli_sd, &op, &ret, block))
  {
    return -1;
  }
  return ret;
}
