#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define IsAlloc(value) ((value) & (1))
#define GetSize(value) ((value) & (~1))

extern void debug(const char *fmt, ...);
extern void *sbrk(intptr_t increment);
uint32_t  PtrToIdx(void* ptr);
void* Coalesce(void* ptr);
char* ReduceNode(char* ptr, uint32_t size);
char* FindFreeBlock(uint32_t size);
void *myrealloc(void *ptr, size_t size);
void myfree(void *ptr);

typedef struct {
    uint32_t header;
    char* next;
} __attribute__((__packed__)) link_node;

void* heap_begin = NULL;
void* heap_end = NULL;

int initialization = 1;
link_node freeList[32];

uint32_t inline PtrToIdx(void* ptr){
    //주요 최적화 포인트
    for(int i = 2; i >= 0 ; i--){
        char c = *((char*)ptr + i);
        if(c != 0){
            for(int j = 7; j >= 0; j--){
                if(c & 0x80){
                    return j + (i*8);
                }
                c = c << 1;
            }
        }
    }
    printf("nope\n");
}

void pop(void* ptr){
    //ptr을 가진 노드를 free list에서 제거
    uint32_t idx = PtrToIdx(ptr);
    char* now = freeList[idx].next;
    char* before = &freeList[idx];
    link_node* tmp;
    while(now != NULL){
        tmp = (link_node*) now;
        if(now == ptr)
            break;
        before = now;
        now = tmp->next;
    }
    memcpy(before + 4, &tmp->next, 8);
}


void push(void* ptr){
    uint32_t idx = PtrToIdx(ptr);

    if(freeList[idx].next){
        char* before = &freeList[idx];
        memcpy(ptr + 4, (before+4), 8);
        memcpy(before + 4, &ptr, 8);
        return;
    }
    else{
        memset(ptr + 4, 0, 8);
        freeList[idx].next = ptr;
        return;
    }
}

void* Coalesce(void* ptr){
    //ptr 앞, 뒤 블럭을 체크하고 병합함
    uint32_t node_size = GetSize(*(int*)ptr);
    uint32_t tmp;
    char* front = ptr;

    //ptr 앞의 노드가 free 상태라면
    if(heap_begin != front){
        tmp = *(uint32_t*)(front-4);
        if(!IsAlloc(tmp)){
            front -= tmp;
            node_size += tmp;
            pop(front);
        }
    }

    //ptr 뒤의 노드가 free 상태라면
    char* end = front + node_size;
    if(end != heap_end){
        tmp = *(uint32_t*)end;
        if(!IsAlloc(tmp)){
            pop(end);
            end += tmp;
            node_size += tmp;
        }
    }

    //새로운 크기 저장
    memcpy(front, &node_size, 4);
    memcpy(end - 4, &node_size, 4);

    return front;
}
char* ReduceNode(char* ptr, uint32_t size){
    uint32_t newSize = GetSize(*(int*)ptr);
    uint32_t subsize = newSize - size;

    //할당 받은 크기가 원하는 크기보다 크면 분할하여, 여분은 다시 저장
    if(subsize > 16){
        char* newPtr = ptr + size;
        newSize -= subsize;
        memcpy(newPtr, &subsize, 4);
        memcpy(newPtr + subsize - 4, &subsize, 4);
        push(newPtr);
    }

    uint32_t header = newSize | 1;
    memcpy(ptr, &header, 4);
    memcpy(ptr + newSize - 4, &header, 4);

    return ptr;
}

char* FindFreeBlock(uint32_t size){ 
    //리스트 순회 후 메모리 할당
    uint32_t idx = PtrToIdx(&size);
    char* now;
    link_node* tmp = NULL;
    for(; idx < 32; idx++){
        now = freeList[idx].next;
        while(now != NULL){
            tmp = (link_node*) now;
            if(GetSize(tmp->header) >= size){
                pop(now);
                return ReduceNode(now, size);
            }
            now = tmp->next;
        }
    }
    return NULL;
}

void* GetMemory(uint32_t volSize){
    char* resultPtr = FindFreeBlock(volSize);
    if(!resultPtr){
        resultPtr = sbrk(volSize);
        heap_end = sbrk(0);
        uint32_t header = volSize | 1;
        memcpy(resultPtr, &header, 4);
        memcpy(heap_end - 4, &header, 4);
        return resultPtr + 4;
    }
    return resultPtr + 4;

}

uint32_t CalcAlignSize(uint32_t size){
    if(size & 7){
        return (size + 16) & 0xFFFFFFF8;
    }
    return size + 8;
}

void* myalloc(size_t size){
    if(initialization){
        for(int i = 31; i >= 0; i--){
            //freeList[i].header = 0;
            freeList[i].next = NULL;
        }
        heap_begin = sbrk(0);
        heap_end = heap_begin;
        initialization = 0;
    }

    uint32_t volume_size = CalcAlignSize(size);
    return GetMemory(volume_size);
}

void *myrealloc(void *ptr, size_t size){
    //ptr이 NULL이면 할당을 해주세요
    //size = 0이면 free 해주세요
    if(size == 0){
        myfree(ptr);
        return NULL;
    }

    if(!ptr){
        return myalloc(size);
    }

    uint32_t nodeSize = GetSize(*(int*)(ptr - 4));
    uint32_t newSize = CalcAlignSize(size);
    if(nodeSize == newSize){
        return ptr;
    }

    if(nodeSize > newSize){
        return ReduceNode(ptr-4, newSize) + 4;
    }
    else{
        myfree(ptr);
        return GetMemory(newSize);
    }
}

void myfree(void *ptr){
    //NULL ptr을 free할 경우
    if(ptr == NULL){
        return;
    } 
    push(Coalesce(ptr-4));
}

// char* nextNode = (char*)ptr + nodeSize - 4;
// if(nextNode != heap_end){
//     link_node* next = (link_node*) nextNode;
//     uint32_t nextSize = GetSize(next->header);
//     if(nodeSize + nextSize == newSize){
//         nodeSize = newSize | 1;
//         pop(nextNode);
//         memcpy(ptr-4, &nodeSize, 4);
//         memcpy(ptr + newSize - 8, &nodeSize, 4);
//         return ptr;
//     }
// }