# Balloc - BEST OF THE BEST Memory Allocator
### sbrk() 함수를 이용하여 만든 메모리 할당 코드 입니다.
- Segregated Free List 방식을 이용하여 구현되었습니다.
- 메모리 할당 크기를 빠르게 계산하기 위한 PtrToIdx() 함수 속도를 빠르게 하기 위해 이진탐색 구조로 구현하였습니다.
  - 기존 2중 반복문 구조를 이진탐색 조건문으로 구현함으로써 시간복잡도 대폭 감소
- 개발 일자 : 2023.02
