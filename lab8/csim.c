// 520021911275 李忱泽

#include "cachelab.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>

long unsigned hits=0;
long unsigned misses=0;
long unsigned evictions=0;
// long unsigned verbose=0;

typedef struct line {
    int valid; // 0/1 : invalid/valid
    int tag;
    // int* block;
    int LRU_tag; // larger --> earlier
} Line_t;

typedef struct  cache {
    int S; // the number of Set (2^s)
    int E; // the number of Line in each Set
    int B; // Block size (2^b)
    Line_t*** content;
} Cache_t;
Cache_t myCache;

void initCache(const int s, const int E, const int b) 
{
    myCache.S = (1 << s);
    myCache.E = E;
    myCache.B = (1 << b);

    // malloc
    myCache.content = (Line_t***)malloc(myCache.S* sizeof(Line_t **));

    for(int i = 0; i < myCache.S; ++i)
        myCache.content[i] = (Line_t **)malloc(myCache.E * sizeof(Line_t *));
    
    for(int i = 0; i < myCache.S; ++i) {
        for(int j = 0; j < myCache.E; ++j)
            myCache.content[i][j] = (Line_t *)malloc(sizeof(Line_t));
    }

    for(int i = 0; i < myCache.S; ++i) {
        for(int j = 0; j < myCache.E; ++j) {
            myCache.content[i][j]->valid = 0;
            myCache.content[i][j]->tag = 0;
            myCache.content[i][j]->LRU_tag = 0;
            // myCache.content[i][j]->block = (int *)malloc(myCache.B * sizeof(int));
        }
    }
}

void freeCache()
{
    // for(int i = 0; i < myCache.S; ++i) {
    //     for(int j = 0; j < myCache.E; ++j)
    //         free(myCache.content[i][j]->block);
    // }

    for(int i = 0; i < myCache.S; ++i) {
        for(int j = 0; j < myCache.E; ++j)
            free(myCache.content[i][j]);
    }

    for(int i = 0; i < myCache.S; ++i)
        free(myCache.content[i]);

    free(myCache.content);
}

int isHit(int s, int tag)
{
    for (int i = 0; i < myCache.E; i++) {
        if (myCache.content[s][i]->valid == 1 && myCache.content[s][i]->tag == tag)
            return i;
    }
    return -1;
}

long unsigned getMaxLRU(long unsigned s)
{
    long unsigned lru = 0, pos = 0;
    for (long unsigned i = 0; i < myCache.E; i++) {
        if (myCache.content[s][i]->valid == 0) return i;
        if (lru < myCache.content[s][i]->LRU_tag) {
            lru = myCache.content[s][i]->LRU_tag;
            pos = i;
        }
    }
    return pos;
}

void replace(long unsigned s, long unsigned e, long unsigned tag)
{
    if (myCache.content[s][e]->valid == 1 && 
        myCache.content[s][e]->tag != tag) {
            evictions += 1;
    }
    myCache.content[s][e]->valid = 1;
    myCache.content[s][e]->tag = tag;
    myCache.content[s][e]->LRU_tag = 0;

    // update LRU
    for (int i = 0; i < myCache.E; i++) {
        if (i != e) {
            myCache.content[s][i]->LRU_tag += 1;
        }
    }
}

void accessCache(long unsigned s, long unsigned tag)
{
    int flag = isHit(s, tag);
    if (flag == -1) {
        misses += 1;
        replace(s, getMaxLRU(s), tag);
    }
    else {
        hits += 1;
        replace(s, flag, tag);
    }
}

int main(int argc,char *const argv[])
{
	FILE *filePath = NULL;
	char parameter;
	int parS = -1, parE = -1, parB = -1;
	while ((parameter = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
		switch(parameter){
            case 'h':
                break;
			case 'v':
				break;
			case 's':
				parS = atol(optarg);
                break;
			case 'E':
				parE = atol(optarg);
                break;
			case 'b':
				parB = atol(optarg);
                break;
			case 't':
				filePath = fopen(optarg,"r");
                break;
			default:
				// shouldn't reach here
                return 0;
		}
	}
    initCache(parS, parE, parB);


    long unsigned addr, number, tag, s;
    char cmd[2], ch;
    while(fscanf(filePath, "%s%lx%c%lu", cmd, &addr, &ch, &number) != EOF){
		if (cmd[0]=='I') continue;

		tag = addr >> (parS + parB);
		s = (addr >> parB) & ((1 << parS) - 1);
		
        accessCache(s, tag);
		if (cmd[0] != 'L' && cmd[0] != 'S')
			accessCache(s, tag);
		
	}


    printSummary(hits, misses, evictions);
    freeCache();
    return 0;
}

