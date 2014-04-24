#ifndef VM_CONTROL_H
#define VM_CONTROL_H

class AddrSpace;
class VmControl{
public:
	VmControl(){};
	AddrSpace* ThreadPageTable[32];
	int ThreadVirtualPage[32];
}
#endif
