#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern void debug(const char *fmt, ...);
extern void *sbrk(intptr_t increment);
uint32_t  PtrToIdx(void* ptr);
void* Coalesce(void* ptr);
char* ReduceNode(char* ptr, uint32_t size);
void *myrealloc(void *ptr, size_t size);
void myfree(void *ptr);
char* FindFreeBlock(uint32_t size);

typedef struct {
    uint32_t header;
    char* next;
} __attribute__((__packed__)) link_node;

void* heap_begin = NULL;
void* heap_end = NULL;

int initialization = 1;
link_node freeList[32];

uint32_t inline IsAlloc(uint32_t value){
    return value & 1;
}

uint32_t inline GetSize(uint32_t value){
    return value & ~1;
}

uint32_t inline PtrToIdx(void* ptr){
    for(int i = 3; i >= 0 ; i--){
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

}

void pop(void* ptr){
    //ptr을 가진 노드를 free list에서 제거
    uint32_t idx = PtrToIdx(ptr);

    char* before = &freeList[idx];
    link_node* tmp = (link_node*) before;
    char* now = tmp->next;

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
    link_node* node = (link_node*) ptr;
    uint32_t nodeSize = GetSize(node->header);
    uint32_t idx = PtrToIdx(ptr);
    memset(ptr + 4, 0, 8);
    char* before = &freeList[idx];
    link_node* tmp = (link_node*) before;

    if(freeList[idx].next == NULL){
        freeList[idx].next = ptr;
        return;
    }
    
    char* now = tmp->next;
    while(now != NULL){
        tmp = (link_node*) now;
        if(nodeSize < GetSize(tmp->header))
            break;
        before = now;
        now = tmp->next;
    }

    if(now == NULL)
        memcpy(before + 4, &ptr, 8);
    else{
        memcpy(ptr + 4, &now, 8);
        memcpy(before + 4, &ptr, 8);
    }
}

void* Coalesce(void* ptr){
    //ptr 앞, 뒤 블럭을 체크하고 병합함
    link_node* node = (link_node*)ptr;
    uint32_t node_size = GetSize(node->header);
    uint32_t tmp = 0;
    char* front = ptr;

    //ptr 앞의 노드가 free 상태라면
    if(heap_begin != front){
        memcpy(&tmp, front-4, 4);
        if(!IsAlloc(tmp)){
            front -= tmp;
            node_size += tmp;
            pop(front);
        }
    }

    //ptr 뒤의 노드가 free 상태라면
    char* end = front + node_size;
    if(end != heap_end){
        memcpy(&tmp, end, 4);
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
    link_node* tmp = (link_node*) ptr;
    uint32_t newSize = GetSize(tmp->header);
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
    char* now = NULL;
    //리스트 순회 후 메모리 할당
    
    uint32_t idx = PtrToIdx(&size);
    for(idx; idx < 32; idx++){
        now = freeList[idx].next;
        if(now == NULL)
            continue;
        while(now != NULL){
            link_node* tmp = (link_node*) now;
            if(GetSize(tmp->header) >= size){
                    pop(now);
                    return ReduceNode(now, size);
            }
            now = tmp->next;
        }
    }
    if(now == NULL){
        return NULL;
    }
}

void* GetMemory(uint32_t volSize){
    char* resultPtr = FindFreeBlock(volSize);

    if(resultPtr == NULL){
        resultPtr = sbrk(volSize);
        heap_end = sbrk(0);
        uint32_t header = volSize | 1;
        memcpy(resultPtr, &header, 4);
        memcpy(resultPtr + volSize - 4, &header, 4);
    }

    return resultPtr + 4;
}

uint32_t CalcAlignSize(uint32_t size){
    uint32_t quotient = size >> 3;
    uint32_t residue = size % 8;
    return (quotient + (residue ? 1 : 0)) * 8;
}

void* myalloc(size_t size){
    if(initialization){
        for(int i = 0; i < 32; i++){
            freeList[i].header = 0;
            freeList[i].next = NULL;
        }
        heap_begin = sbrk(0);
        heap_end = heap_begin;
        initialization = 0;
    }
    uint32_t volume_size = CalcAlignSize(size + 8);
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

    link_node* ptr_node = (link_node*) ((char*)ptr - 4);
    uint32_t nodeSize = GetSize(ptr_node->header);
    uint32_t newSize = CalcAlignSize(size + 8);
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
    void* newPtr = Coalesce(ptr-4);
    push(newPtr);
}