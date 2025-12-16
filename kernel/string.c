#include "types.h"

void* memset(void *dst, int c, uint n) {
  char *cdst = (char *) dst;
  for(int i = 0; i < n; i++){
    cdst[i] = c;
  }
  return dst;
}

// 顺手把 memmove 和 memcpy 也加上，后面肯定会用
void* memmove(void *dst, const void *src, uint n) {
  const char *s;
  char *d;

  s = src;
  d = dst;
  if(s < d && s + n > d){
    s += n;
    d += n;
    while(n-- > 0)
      *--d = *--s;
  } else {
    while(n-- > 0)
      *d++ = *s++;
  }
  return dst;
}
// memcpy 可以直接调用 memmove，因为 memmove 已经处理了重叠区域的问题
void* memcpy(void *dst, const void *src, uint n) {
  return memmove(dst, src, n);
}