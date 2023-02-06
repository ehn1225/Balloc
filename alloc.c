#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define IsAlloc(value) ((value) & (1))
#define GetSize(value) ((value) & (~1))
#define CalcAlignSize(value) ((value & 7) ? ((size + 16) & ~7) : size + 8)

extern void debug(const char *fmt, ...);
extern void *sbrk(intptr_t increment);
uint32_t  PtrToIdx(void* ptr);
void* Coalesce(void* ptr);
char* ReduceNode(char* ptr, uint32_t size);
void *myrealloc(void *ptr, size_t size);
void myfree(void *ptr);

typedef struct {
    uint32_t header;
    char* next;
} __attribute__((__packed__)) link_node;

void* heap_begin = NULL;
void* heap_end = NULL;

link_node freeList[32] = {0, };

uint32_t inline PtrToIdx(void* ptr){
    //주요 최적화 포인트
    //이진조건
    int value = *(int*)ptr;
    if(value & 0xFFFF0000){
        //상위 2바이트에 값이 있을 경우
        if(value & 0xFF000000){
            //4번째 바이트에 값이 있을 경우
            if(value & 0xF0000000){
                //상위 4비트에 값이 있다면 (0xF0000000)
                if(value & 0x80000000)
                    return 31;
                if(value & 0x40000000)
                    return 30;
                if(value & 0x20000000)
                    return 29;
                if(value & 0x10000000)
                    return 28;
            }
            else{
                //하위 4비트에 값이 있다면 (0x0F000000)
                if(value & 0x08000000)
                    return 27;
                if(value & 0x04000000)
                    return 26;
                if(value & 0x02000000)
                    return 25;
                if(value & 0x01000000)
                    return 24;
            }
        }
        else{
            //3번째 바이트에 값이 있을 경우
            if(value & 0x00F00000){
                //상위 4비트에 값이 있다면 (0x00F00000)
                if(value & 0x00800000)
                    return 23;
                if(value & 0x00400000)
                    return 22;
                if(value & 0x00200000)
                    return 21;
                if(value & 0x00100000)
                    return 20;

            }
            else{
                //하위 4비트에 값이 있다면 (0x000F0000)
                if(value & 0x00080000)
                    return 19;
                if(value & 0x00040000)
                    return 18;
                if(value & 0x00020000)
                    return 17;
                if(value & 0x00010000)
                    return 16;
            }
        }
    }
    else{
        //하위 2바이트만 확인
        if(value & 0x0000FF00){
            //2번째 바이트에 값이 있을 경우
            if(value & 0xF000){
                //상위 4비트에 값이 있다면(0x0000F000)
                if(value & 0x00008000)
                    return 15;
                if(value & 0x00004000)
                    return 14;
                if(value & 0x00002000)
                    return 13;
                if(value & 0x00001000)
                    return 12;
            }
            else{
                //하위 4비트에 값이 있다면(0x00000F00)
                if(value & 0x00000800)
                    return 11;
                if(value & 0x00000400)
                    return 10;
                if(value & 0x00000200)
                    return 9;
                if(value & 0x00000100)
                    return 8;
            }
        }
        else{
            //1번째 바이트에 값이 있을 경우
            if(value & 0xF0){
                //상위 4비트에 값이 있다면(0x000000F0)
                if(value & 0x00000080)
                    return 7;
                if(value & 0x00000040)
                    return 6;
                if(value & 0x00000020)
                    return 5;
                if(value & 0x00000010)
                    return 4;
            }
            else{
                //하위 4비트에 값이 있다면(0x0000000F)
                if(value & 0x00000008)
                    return 3;
                if(value & 0x00000004)
                    return 2;
                if(value & 0x00000002)
                    return 1;
                if(value & 0x00000001)
                    return 0;
            }

        }

    }



    // for(int i = 2; i >= 0 ; i--){
    //     char c = *((char*)ptr + i);
    //     if(c != 0){
    //         for(int j = 7; j >= 0; j--){
    //             if(c & 0x80){
    //                 return j + (i*8);
    //             }
    //             c = c << 1;
    //         }
    //     }
    // }
}

void pop(void* ptr, uint32_t idx){
    //ptr을 가진 노드를 free list에서 제거
    if(idx == 0)
        idx = PtrToIdx(ptr);
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
            pop(front, 0);
        }
    }

    //ptr 뒤의 노드가 free 상태라면
    char* end = front + node_size;
    if(end != heap_end){
        tmp = *(uint32_t*)end;
        if(!IsAlloc(tmp)){
            pop(end, 0);
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

void* GetMemory(uint32_t volSize){
    char* resultPtr = NULL;
    uint32_t idx = PtrToIdx(&volSize);
    link_node* tmp = NULL;
    for(; idx < 32; idx++){
        resultPtr = freeList[idx].next;
        while(resultPtr != NULL){
            tmp = (link_node*) resultPtr;
            if(GetSize(tmp->header) >= volSize){
                pop(resultPtr, idx);
                resultPtr = ReduceNode(resultPtr, volSize);
                idx = 64;
                break;
            }
            resultPtr = tmp->next;
        }
    }

    if(!resultPtr){
        if(heap_begin == NULL){
            heap_begin = sbrk(0);
            heap_end = heap_begin;
        }
        resultPtr = sbrk(volSize);
        heap_end += volSize;
        volSize++;      //VolSize = Volsize | 1
        memcpy(resultPtr, &volSize, 4);
        memcpy(heap_end - 4, &volSize, 4);
        return resultPtr + 4;
    }

    return resultPtr + 4;
}

void* myalloc(size_t size){
    uint32_t volume_size = CalcAlignSize(size);
    return GetMemory(volume_size);
}

void *myrealloc(void *ptr, size_t size){
    //ptr이 NULL이면 할당을 해주세요
    //size = 0이면 free 해주세요
    if(!ptr){
        return myalloc(size);
    }

    if(size == 0){
        myfree(ptr);
        return NULL;
    }

    uint32_t nodeSize = GetSize(*(int*)(ptr - 4));
    uint32_t newSize = CalcAlignSize(size);
    if(nodeSize == newSize){
        return ptr;
    }

    if(nodeSize < newSize){
        myfree(ptr);
        return GetMemory(newSize);
    }
    else{
        return ReduceNode(ptr-4, newSize) + 4;
    }
}

void myfree(void *ptr){
    //NULL ptr을 free할 경우
    if(ptr == NULL){
        return;
    }
    
    push(Coalesce(ptr-4));    
}