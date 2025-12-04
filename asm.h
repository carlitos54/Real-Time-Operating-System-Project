#ifndef ASM_H_
#define ASM_H_

#include <stdint.h>

void setASP(void);
void setPSP(uint32_t* p);
void setTMPL(int x);
uint32_t* getPSP(void);
uint32_t* getMSP(void);
void pushRegs(void);
void popRegs(void);

#endif /* PSP_STACK_H_ */
