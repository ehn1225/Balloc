#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern void debug(const char *fmt, ...);
extern void *sbrk(intptr_t increment);
uint32_t SizeToIdx(uint32_t volSize);
void myfree(void *ptr);
char* FindFreeBlock(uint32_t size);

void* heap_begin = NULL;

typedef struct {
    uint32_t header;
    char* next;
} __attribute__((__packed__)) link_node;

int initialization = 1;
unsigned int max_size;
link_node freeList[32];
//int size_index[] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072};
//                                 | 시작 0 
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
        if(now == ptr)
            break;
        before = now;
        tmp = (link_node*)now;
        now = tmp->next;
    }
    //now가 찾는 포인터임
    if(now == NULL){
        //printf("pop3 : 찾을 수 없음(%p)\n", ptr);
        return;
    }

    //printf("pop 4 arr : %p, before : %p, now %p, ptr : %p\n", freeList[idx].next, before, now, ptr);
    if(before == NULL){
        freeList[idx].next = NULL;
    }
    else{
        //tmp->next는 null이거나 다음 노드의 주소임
        if(tmp->next == NULL){
            memset(before + 4, 0, 8);
        }
        else{
            memcpy(before + 4, tmp->next, 8);
        }
    }
    return;
}

void push(void* ptr){
    if(ptr == NULL)
        return;
    link_node* node = (link_node*) ptr;
    uint32_t nodeSize = GetSize(node->header);
    uint32_t idx = SizeToIdx(nodeSize);
    printf("push %p %d %d\n", ptr, nodeSize, idx);
    memset(ptr + 4, 0, 8);
    char* before = freeList[idx].next;
    char* now = freeList[idx].next;

    if(now == NULL){
        freeList[idx].next = ptr;
        //printf("Push complte first (%p)\n", freeList[idx].next);
        return;
    }

    link_node* tmp = (link_node*) before;
    //printf("tmp : %p, tmp->next : %p\n", tmp, tmp->next);

    if(tmp->next == NULL){
        //하나 있는 노드가 나보다 크다면, 시작과 노드 중간으로 들어감.
        if(GetSize(tmp->header) >= nodeSize){
            memcpy(ptr + 4, &before, 8);
            freeList[idx].next = ptr;
        }
        else{
            memcpy(before + 4, &ptr, 8);
        }
        //for(int i = 0; i < 16; i++)
            //printf("%02x ", (unsigned char)*((char*)ptr + i));
        //printf("Push Complete a\n");
        return;
    }
    else{
        while(now != NULL){
            link_node* tmp = (link_node*) now;
            //printf("Node %p, size %d\n", now, GetSize(tmp->header));
            if(GetSize(tmp->header) >= nodeSize){
                break;
            }
            before = now;
            now = tmp->next;
        }
        //before과 now 사이에 넣어야 함.
        //before->next = 나
        //나의 next -> now
        //printf("arr %p, before %p, now %p\n", freeList[idx].next, before, now);
        memcpy(ptr + 4, &now, 8);
        memcpy(before + 4, &ptr, 8);
        //printf("Push Complete b\n");
        return;
    }
}

void* Coalesce(void* ptr){
    //ptr 앞, 뒤 블럭을 체크하고 병합함
    //병합되는 블럭은 이 함수에서 처리해줌.
    link_node* node = (link_node*)ptr;
    uint32_t node_size = GetSize(node->header);
    printf("Coalesce (%p, %d)\n", ptr, node_size);
    uint32_t tmp = 0;
    uint32_t tmp2 = 0;
    char* newPtr = ptr;

    //ptr 앞의 노드가 free 상태라면
    if(heap_begin != ptr){
        memcpy(&tmp, ptr-4, 4);
        tmp2 = GetSize(tmp);
        if(tmp2 && !IsAlloc(tmp)){
            newPtr = (char*)newPtr - tmp2;
            node_size += tmp2;
            pop(newPtr);
        }
    }

    //ptr 뒤의 노드가 free 상태라면
    memcpy(&tmp, newPtr + node_size, 4);
    tmp2 = GetSize(tmp);
    if(tmp2 && !IsAlloc(tmp)){
        pop(newPtr + node_size);
        node_size += tmp2;
    }
    //크기 저장
    memcpy(newPtr, &node_size, 4);
    memcpy(newPtr + node_size - 4, &node_size, 4);

    return newPtr;
}

char* FindFreeBlock(uint32_t size){
    if(size == 0)
        return NULL;
    
    uint32_t idx = SizeToIdx(size);
    printf("FindFreeBlock size(%d), idx(%d)\n", size, idx);

    char* now = NULL;
    int loop = 0;
    link_node* tmp = NULL; 
    //리스트 순회 후 메모리 할당
    for(idx; idx < 32; idx++){
        now = freeList[idx].next;
        if(now == NULL)
            continue;
        printf("idx : %d, now : %p\n", idx, now);
        while(now != NULL){
            tmp = (link_node*) now;
            //오류 발생 파트
            for(int i = 0; i < 16; i++)
                printf("%02x ", (unsigned char)*(now + i));
            
            printf("\nFindFreeBlock loop: %p, size %d, idx(%d), %p\n", now, GetSize(tmp->header), idx, tmp->next);

            if(GetSize(tmp->header) >= size){
                printf("FindFreeBlock find\n");
                loop = 1;
                break;
            }
            now = tmp->next;
        }
        if(loop){
            break;
        }
    }
    if(now == NULL){
        printf("FindFreeBlock No Free Block\n");
        return NULL;
    }

    printf("FindFreeBlock Report %p\n", now);
    tmp = (link_node*) now;
    int32_t newSize = GetSize(tmp->header);
    printf("%p, %d, %d\n", now, newSize, idx);
    pop(now);
    uint32_t subsize = newSize - size;

    if(subsize > 16){
        //Execute Split
        char* newPtr = now + size;
        newSize -= subsize;
        printf("Split %d into %d, %d\n", newSize + subsize, newSize, subsize);
        memcpy(newPtr, &subsize, 4);
        memcpy(newPtr + subsize - 4, &subsize, 4);
        push(newPtr);
    }

    uint32_t header = newSize | 1;
    memcpy(now, &header, 4);
    memcpy(now + newSize - 4, &header, 4);
    printf("FindFreeBlock Complete %p\n", now);
    return now;
}

void* GetMemory(uint32_t volSize){
    char* resultPtr = NULL;
    uint32_t index = SizeToIdx(volSize);
    resultPtr = FindFreeBlock(volSize);

    if(resultPtr == NULL){
        resultPtr = sbrk(volSize);
        max_size += (volSize);
        uint32_t header = volSize | 1;
        memcpy(resultPtr, &header, 4);
        memcpy(resultPtr + volSize - 4, &header, 4);
        printf("SearchFreeList-sbrk(%p, %d)\n", resultPtr, volSize);
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
        initialization = 0;
    }
    printf("myalloc request(%d)\n",size);
    uint32_t volume_size = CalcAlignSize(size + 8);
    max_size += volume_size;
    void* ret = GetMemory(volume_size);
    printf("myalloc allocation(%p, %d, %d)\n",ret, size, volume_size);
    return ret;
}

void *myrealloc(void *ptr, size_t size){
    //printf("Call Realloc\n");
    void *p = NULL;
    if (size != 0){
        p = myalloc(size);
        if (ptr)
            memcpy(p, ptr, size);
        debug("max: %u\n", max_size);
    }
    debug("realloc(%p, %u): %p\n", ptr, (unsigned int)size, p);
    myfree(ptr);
    return p;
}

void myfree(void *ptr){
    //NULL ptr을 free할 경우
    if(ptr == NULL)
        return;
    
    //printf("myfree(%p)\n", ptr - 4);
    //블럭의 주소로 변경
    ptr -= 4;
    
    //내부 size 계산
    size_t* size = (size_t*)ptr;
    max_size -= GetSize(*size);

    //Coalesce & free list에 추가
    void* newPtr = Coalesce(ptr);
    push(newPtr);
    //printf("myfree end\n");
}
