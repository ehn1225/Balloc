#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern void debug(const char *fmt, ...);
extern void *sbrk(intptr_t increment);
uint32_t SizeToIdx(uint32_t volSize);
void myfree(void *ptr);
char* FindFreeBlock(uint32_t size);

typedef struct {
    uint32_t header;
    char* next;
} __attribute__((__packed__)) link_node;

void* heap_begin = NULL;
void* heap_end = NULL;

int initialization = 1;
unsigned int max_size;
link_node freeList[32];

uint32_t inline IsAlloc(uint32_t value){
    return value & 1;
}

uint32_t inline GetSize(uint32_t value){
    return value & ~1;
}

uint32_t inline SizeToIdx(uint32_t volSize){
    int i = 32;
    for(i = 32; i > 4; i--){
        if(volSize & 0x80000000)
            return i;
        volSize = volSize << 1;
    }
    return 0;
}

void TravelLinkList(char* arr){
    char* tmp_ptr = arr;
    printf("TravelLinkList Begin\n");
    if(arr == NULL){
        printf("TravelLinkList null\n");
    }
    else{
        while(tmp_ptr != NULL){
            link_node* tmp = (link_node*) tmp_ptr;
            printf("Node %p, size %d\n", tmp_ptr, GetSize(tmp->header));
            // for(int i = 0; i < 16; i++)
            //     printf("%02x ", (unsigned char)*((char*)tmp_ptr + i));
            // printf("\n");
            tmp_ptr = tmp->next;
        }
    }
    printf("TravelLinkList End\n");
}

void pop(void* ptr){
    //ptr을 가진 노드를 free list에서 제거
    if(ptr == NULL)
        return;
    
    link_node* node = (link_node*) ptr;
    uint32_t nodeSize = GetSize(node->header);
    uint32_t idx = SizeToIdx(nodeSize);
    printf("pop %p %d %d\n", ptr, nodeSize, idx);
    char* now = freeList[idx].next;
    char* before = NULL;

    if(now == NULL)
        return;
    
    //node 연결 끊기
    link_node* tmp;
    while(now != NULL){
        tmp = (link_node*)now;
        if(now == ptr)
            break;
        before = now;
        now = tmp->next;
    }
    //now가 찾는 포인터임
    if(now == NULL){
        printf("pop : 찾을 수 없음(%p)\n", ptr);
        return;
    }
    //맨 첫번째 노드일 경우
    if(before == NULL){
        freeList[idx].next = NULL;
    }
    else{
        //tmp->next는 삭제하려는 노드의 다음 노드 주소
        memcpy(before + 4, &tmp->next, 8);
    }
    //printf("Pop complete\n");
    return;
}

void push(void* ptr){
    if(ptr == NULL)
        return;

    link_node* node = (link_node*) ptr;
    uint32_t nodeSize = GetSize(node->header);
    uint32_t idx = SizeToIdx(nodeSize);
    //printf("push %p %d %d\n", ptr, nodeSize, idx);
    memset(ptr + 4, 0, 8);

    char* before = &freeList[idx];
    link_node* tmp = (link_node*) before;

    if(tmp->next == NULL){
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
    return;
}

void* Coalesce(void* ptr){
    //ptr 앞, 뒤 블럭을 체크하고 병합함
    link_node* node = (link_node*)ptr;
    uint32_t node_size = GetSize(node->header);
    uint32_t tmp = 0;
    uint32_t tmp33 = node_size;
    char* front = ptr;

    //printf("Coalesce (%p, %d)\n", front, node_size);
    //ptr 앞의 노드가 free 상태라면
    if(heap_begin != front){
        memcpy(&tmp, front-4, 4);
        if(tmp && !IsAlloc(tmp)){
            front -= tmp;
            node_size += tmp;
            pop(front);
        }
    }

    //ptr 뒤의 노드가 free 상태라면
    //printf("Coalesce 2(%p, %d, )\n", front, node_size);
    char* end = front + node_size;
    if(end != heap_end){
        memcpy(&tmp, end, 4);  //Error
        if(tmp && !IsAlloc(tmp)){
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

char* FindFreeBlock(uint32_t size){
    if(size == 0)
        return NULL;
    
    int loop = 0;
    char* now = NULL;
    link_node* tmp = NULL;
    //printf("FindFreeBlock size(%d), idx(%d)\n", size, idx);

    //리스트 순회 후 메모리 할당
    uint32_t idx = SizeToIdx(size);
    for(idx; idx < 32 && loop; idx++){
        now = freeList[idx].next;
        if(now == NULL)
            continue;

        while(now != NULL){
            tmp = (link_node*) now;
            if(GetSize(tmp->header) >= size){
                //printf("FindFreeBlock find\n");
                loop = 1;
                break;
            }
            now = tmp->next;
        }
    }
    if(now == NULL){
        //printf("FindFreeBlock No Free Block\n");
        return NULL;
    }

    //Find Block
    tmp = (link_node*) now;
    int32_t newSize = GetSize(tmp->header);
    uint32_t subsize = newSize - size;
    pop(now);

    //할당 받은 크기가 원하는 크기보다 크면 분할하여, 여분은 다시 저장
    if(subsize > 16){
        char* newPtr = now + size;
        newSize -= subsize;
        memcpy(newPtr, &subsize, 4);
        memcpy(newPtr + subsize - 4, &subsize, 4);
        push(newPtr);
    }

    uint32_t header = newSize | 1;
    memcpy(now, &header, 4);
    memcpy(now + newSize - 4, &header, 4);
    return now;
}

void* GetMemory(uint32_t volSize){
    char* resultPtr = NULL;
    uint32_t index = SizeToIdx(volSize);
    resultPtr = FindFreeBlock(volSize);

    if(resultPtr == NULL){
        resultPtr = sbrk(volSize);
        max_size += volSize;
        //heap_end = (char*)resultPtr + volSize;
        heap_end =  sbrk(0);
        uint32_t header = volSize | 1;
        memcpy(resultPtr, &header, 4);
        memcpy(resultPtr + volSize - 4, &header, 4);
    }

    return resultPtr + 4;
}

uint32_t CalcAlignSize(uint32_t size){
    uint32_t quotient = size / 8;
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
    void* ret = GetMemory(volume_size);
    //printf("myalloc allocation(%p, %d, %d)\n",ret, size, volume_size);
    return ret;
}

void *myrealloc(void *ptr, size_t size){
    void *p = NULL;
    if (size != 0){
        p = myalloc(size);
        //ptr 뒤에 size 공간이 비어있다면 연속해서 할당하면 좋을듯
        if (ptr)
            memcpy(p, ptr, size);
    }
    //printf("realloc(%p, %u): %p\n", ptr, (unsigned int)size, p);
    myfree(ptr);
    return p;
}

void myfree(void *ptr){
    //NULL ptr을 free할 경우
    if(ptr == NULL)
        return;
    
    //printf("myfree (%p)\n", ptr);
    ptr -= 4;

    //Coalesce & free list에 추가
    void* newPtr = Coalesce(ptr);
    push(newPtr);
}
