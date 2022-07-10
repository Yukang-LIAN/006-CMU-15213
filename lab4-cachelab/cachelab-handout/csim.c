#include "cachelab.h"
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int h, v, s, E, b, S;
int hit_count, miss_count, eviction_count;
char t[1000];

typedef struct {
  int valid;
  int tag;
  int timestamp;
} cache_line, *cache_asso, **cache;

cache _cache_ = NULL;

void printUsage() {
  printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\n"
         "Options:\n"
         "  -h         Print this help message.\n"
         "  -v         Optional verbose flag.\n"
         "  -s <num>   Number of set index bits.\n"
         "  -E <num>   Number of lines per set.\n"
         "  -b <num>   Number of block offset bits.\n"
         "  -t <file>  Trace file.\n\n"
         "Examples:\n"
         "  linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n"
         "  linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

void init_cache() {
  _cache_ = (cache)malloc(sizeof(cache_asso) * S);
  for (int i = 0; i < S; i++) {
    _cache_[i] = (cache_asso)malloc(sizeof(cache_line) * E);
    for (int j = 0; j < E; j++) {
      _cache_[i][j].valid = 0;
      _cache_[i][j].tag = -1;
      _cache_[i][j].timestamp = 0;
    }
  }
}

void update(unsigned int address) {
  int Set = (address >> b) & ((-1U) >> (64 - s));
  int Tag = address >> (b + s);

  int max_stamp = INT_MIN;
  int max_stamp_index = -1;

  for (int i = 0; i < E; i++) {
    if (_cache_[Set][i].tag == Tag) {
      hit_count++;
      _cache_[Set][i].timestamp = 0;
      return;
    }
  }

  for (int i = 0; i < E; i++) {
    if (_cache_[Set][i].valid == 0) {
      miss_count++;
      _cache_[Set][i].valid= 1;
      _cache_[Set][i].timestamp = 0;
      _cache_[Set][i].tag = Tag;
      return;
    }
  }

  for (int i = 0; i < E; i++) {
    if (_cache_[Set][i].timestamp > max_stamp) {
      max_stamp_index = i;
      max_stamp = _cache_[Set][i].timestamp;
    }
  }
  miss_count++;
  eviction_count++;

  _cache_[Set][max_stamp_index].tag = Tag;
  _cache_[Set][max_stamp_index].timestamp = 0;
  return;
}

void update_stamp() {
  for (int i = 0; i < S; ++i)
    for (int j = 0; j < E; ++j)
      if (_cache_[i][j].valid == 1)
        ++_cache_[i][j].timestamp;
}

void parse_trace() {
  FILE *fp = fopen(t, "r"); // 读取文件名
  if (fp == NULL) {
    printf("open error");
    exit(-1);
  }

  char operation;       // 命令开头的 I L M S
  unsigned int address; // 地址参数
  int size;             // 大小
  while (fscanf(fp, " %c %xu,%d\n", &operation, &address, &size) > 0) {

    switch (operation) {
    // case 'I': continue;	   // 不用写关于 I 的判断也可以
    case 'L':
      update(address);
      break;
    case 'M':
      update(address); // miss的话还要进行一次storage
    case 'S':
      update(address);
    }
    update_stamp(); //更新时间戳
  }

  fclose(fp);
  for (int i = 0; i < S; ++i)
    free(_cache_[i]);
  free(_cache_); // malloc 完要记得 free 并且关文件
}

int main(int argc, char *argv[]) {
  h = 0;
  v = 0;
  hit_count = miss_count = eviction_count = 0;
  int opt; // 接收getopt的返回值

  // getopt 第三个参数中，不可省略的选项字符后要跟冒号，这里h和v可省略
  while (-1 != (opt = (getopt(argc, argv, "hvs:E:b:t:")))) {
    switch (opt) {
    case 'h':
      h = 1;
      printUsage();
      break;
    case 'v':
      v = 1;
      printUsage();
      break;
    case 's':
      s = atoi(optarg);
      break;
    case 'E':
      E = atoi(optarg);
      break;
    case 'b':
      b = atoi(optarg);
      break;
    case 't':
      strcpy(t, optarg);
      break;
    default:
      printUsage();
      break;
    }
  }

  if (s <= 0 || E <= 0 || b <= 0 || t == NULL) // 如果选项参数不合格就退出
    return -1;
  S = 1 << s; // S=2^s

  FILE *fp = fopen(t, "r");
  if (fp == NULL) {
    printf("open error");
    exit(-1);
  }

  init_cache();  // 初始化cache
  parse_trace(); // 更新最终的三个参数

  printSummary(hit_count, miss_count, eviction_count);

  return 0;
}
