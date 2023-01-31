#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern void debug(const char *fmt, ...);
extern void *sbrk(intptr_t increment);
uint32_t SizeToIdx(uint32_t volSize);
void myfree(void *ptr);
char* FindFreeBlock(uint32_t size);

void* heap_begin = NULL;

typedef struct _link_node{
    uint32_t header;
    char* next;
} link_node;

int count = 10;
int initialization = 1;
unsigned int max_size;
char* freeList[32];
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

void pop(void* ptr){
    //ptr 연결 리스트를 순회하면서
    //volume == 0일 땐, ptr을 가진 노드를 free list에서 제거
    //volume != 0일 땐, size >= volume인 노드 탐색 및 반환
    if(ptr == NULL)
        return;
    
    link_node* node = (link_node*) ptr;
    uint32_t nodeSize = GetSize(node->header);
    uint32_t idx = SizeToIdx(nodeSize);
    printf("pop %p %ld %ld\n", ptr, nodeSize, idx);
    char* now = freeList[idx];
    char* before = NULL;

    if(now == NULL) //연결 리스트 부재
        return;

    printf("pop 2\n");

    //node 연결 끊기
    printf("pop 3 now : %p\n", now);
    link_node* tmp = (link_node*)now;
    while(tmp->next != NULL){
        printf("pop 3 tmp->next : %p\n", tmp->next);
        before = now;
        now = tmp->next;
        if(now == ptr) //수정필요
            break; 
        tmp = (link_node*) now;
    }
    //now가 찾는 포인터임
    if(now == NULL){
        printf("pop3 : 찾을 수 없음\n");
        return;
    }

    printf("pop 4-1 arr : %p, before : %p, now %p, next : %p\n", freeList[idx], before, now, tmp->next);

    if(before == now){
        freeList[idx] = NULL;
    }
    else{
        //tmp->next는 null이거나 다음 노드의 주소임
        printf("pop - next : %p\n", tmp->next);
        if(before == NULL){
            if(tmp->next == NULL){
                freeList[idx] = NULL;
            }
            else{
                memcpy(freeList[idx], tmp->next, 8);
            }
        }
        else{
            if(tmp->next == NULL){
                memset(before + 4, 0, 8);
            }
            else{
                memcpy(before + 4, tmp->next, 8);
            }
        }
    }
    printf("pop 4-2 arr : %p\n", freeList[idx]);
    return;
}

void push(void* ptr){
    if(ptr == NULL)
        return;
    
    link_node* node = (link_node*) ptr;
    uint32_t nodeSize = GetSize(node->header);
    uint32_t idx = SizeToIdx(nodeSize);
    printf("push %p %d %d\n", ptr, nodeSize, idx);
    memset(ptr + 4, 0, nodeSize - 8);
    char* before = freeList[idx];
    char* now = freeList[idx];

    if(now == NULL){
        freeList[idx] = ptr;
        for(int i = 0; i < nodeSize; i++)
            printf("%02x ", *((char*)ptr + i));

        printf("Push complte(%p)\n", freeList[idx]);
        return;
    }

    link_node* tmp = (link_node*) now;
    printf("tmp : %p, tmp->next : %p\n", tmp, tmp->next);
    while(tmp->next != NULL && GetSize(tmp->header) < nodeSize){
        printf("tmp->next : %p\n", tmp->next);
        before = now;
        now = tmp->next;
        tmp = (link_node*) now;
    }
    //before->next를 나->next, before->next가 나, 
    // printf("Push before %p\n", before);
    // printf("Push now %p\n", now);
    // printf("Push ptr %p\n", ptr);
    memcpy(ptr+4, now, 8);
    memcpy(before + 4, ptr, 8);
    printf("Push complete between %p and %p, %p %d at %d\n", before, ptr, now, nodeSize, idx);
}

void* Coalesce(void* ptr){
    //ptr 앞, 뒤 블럭을 체크하고 병합함
    //병합되는 블럭은 이 함수에서 처리해줌.
    void* newPtr = ptr;
    link_node* node = (link_node*)ptr;
    uint32_t node_size = GetSize(node->header);
    //printf("Coalesce-0(%p, %d)\n", newPtr, node_size);
    uint32_t tmp;

    //ptr 앞의 노드가 free 상태라면
    if(heap_begin != ptr){
        memcpy(&tmp, newPtr-4, 4);
        //printf("Coalesce-11(%p, %d)\n", newPtr-4, GetSize(tmp));

        if(GetSize(tmp) && !IsAlloc(tmp)){
            //printf("fuck\n");
            newPtr = (char*)newPtr - tmp;
            node_size += tmp;
            pop(newPtr);
        }
        //printf("Coalesce-12(%p, %d)\n", newPtr, node_size);

    }

    //ptr 뒤의 노드가 free 상태라면
    memcpy(&tmp, (char*)newPtr + node_size, 4);
    //printf("Coalesce-21(%p, %ld)\n", (char*)newPtr + node_size, GetSize(tmp));

    if(GetSize(tmp) && !IsAlloc(tmp)){
        //printf("fuck22 %p\n", (char*)newPtr + node_size);
        pop((char*)newPtr + node_size);
        //printf("fuck22\n");        
        node_size += tmp;
    }
    //printf("Coalesce-22(%p, %d)\n", newPtr, node_size);

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
        now = freeList[idx];
        if(now == NULL)
            continue;
        
        while(now != NULL){
            tmp = (link_node*) now;
            printf("FindFreeBlock loop: %p, size %d, idx(%d), %p\n", now, GetSize(tmp->header), idx, tmp->next);
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

    printf("FindFreeBlock Report\n");
    tmp = (link_node*) now;
    int32_t newSize = GetSize(tmp->header);
    printf("%p, %d, %d\n", now, newSize, idx);
    pop(now);

    if(newSize > size + 16){
        //Execute Split
        char* newPtr = now + size;
        newSize -= size;
        printf("Split into %d, %d\n", size, newSize);
        uint32_t header = newSize;
        memcpy(newPtr, &header, 4);
        memcpy(newPtr + newSize - 4, &header, 4);
        push(newPtr);
    }

    uint32_t header = size;
    memcpy(now, &header, 4);
    memcpy(now + size - 4, &header, 4);
    printf("FindFreeBlock Complete %p\n", now);
    return now;
}

void* GetMemory(uint32_t volSize){
    char* resultPtr = NULL;
    //입력한 볼륨 크기만큼 리턴해주고, 남은 사이즈는 다시 넣어주기. > 삽입정렬 수행
    //찾은 볼륨의 사이즈가 16이상 차이가 난다면, 쪼개기
    uint32_t index = SizeToIdx(volSize);
    resultPtr = FindFreeBlock(volSize);

    if(resultPtr == NULL){
        resultPtr = sbrk(volSize);
        max_size += (volSize);
        printf("SearchFreeList-sbrk(%p, %d)\n", resultPtr, volSize);
    }
    uint32_t header = volSize | 1;
    memcpy(resultPtr, &header, 4);
    memcpy(resultPtr + volSize - 4, &header, 4);

    return resultPtr + 4;
}

uint32_t CalcAlignSize(uint32_t size){
    uint32_t quotient = size / 8;
    uint32_t residue = size % 8;
    return (quotient + (residue ? 1 : 0)) * 8;
}

void* myalloc(size_t size){
    //printf("Call alloc\n");
    if(initialization){
        for(int i = 0; i < 32; i++){
            freeList[i] = NULL;
        }
        heap_begin = sbrk(0);
        initialization = 0;
    }

    uint32_t volume_size = CalcAlignSize(size + 8);
    max_size += volume_size;
    void* ret = GetMemory(volume_size);
    //printf("myalloc(%p, %d, %d)\n",ret, size, volume_size);
    return ret;
}

void *myrealloc(void *ptr, size_t size){
    printf("Call Realloc\n");
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
    
    printf("myfree(%p)\n", ptr - 4);
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
