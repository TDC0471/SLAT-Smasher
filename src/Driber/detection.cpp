#include "detection.h"
#include "defs/cr.h"
#include "defs/pages.h"

constexpr uint64_t TAG = 0x1234568901234567;

cr3_t old_cr3;
cr3_t our_cr3;
struct alignas(0x1000) host_pt_t {
	pml4e_t pml4[512];
	pdpe_t pdpt[1];
} our_pml4;
constexpr uint64_t mapped_pml4e = 255;
constexpr uint64_t host_pa_base = mapped_pml4e << (12 + 9 + 9 + 9);

uint8_t* our_page = nullptr;

uint64_t rva_to_huge(uint64_t address) { return address % pdpes_address_range; }

#define absdiff(a, b) ((a) > (b) ? (a) - (b) : (b) - (a))
RESULT check_memory(uint64_t physical_address) {
	//physical_address should be (preferably) memory that will not be changed
	//otherwise the TLB check will fail

	auto& pdpte = our_pml4.pdpt[0];

	//set the huge page to include our physical_address
	pdpte.huge.page_pa = physical_address >> 30;

	static const uint64_t pool_pa = MmGetPhysicalAddress(our_page).QuadPart;
	static const uint64_t pool_rva = rva_to_huge(pool_pa);

	//calculate the rva (from the base of the huge page) of the physical_address
	const uint64_t physical_address_rva = rva_to_huge(physical_address);


	//if the rva diff is within the pdes address range then we will be addressing
	//the same page. So on a hv and a normal system the TLB will have the same behavior (unless the hv is using 4kb pages)
	//so it will mess up our check
	if (absdiff(physical_address_rva, pool_rva) <= pdes_address_range) {
		print("Rva diff is too small %llx %llx\n", physical_address_rva, pool_rva);
		return FAILURE;
	}

	//If pool_pa and physical_address are in the same huge page address space
	//this check wont work (for obv reasons)
	if (absdiff(pool_pa, physical_address) <= pdpes_address_range) {
		print("Pa diff is too small (same huge page) %llx %llx\n", pool_pa, physical_address);
		return FAILURE;
	}

	_disable(); //to prevent context switching
	__writecr3(our_cr3.value);


	//read at physical_address to cache it in the TLB
	uint64_t host_read = *reinterpret_cast<uint64_t*>(host_pa_base + physical_address_rva);

	for (uint8_t i = 0; i < 15; i++)
		print("[%d]: %llx\n", i, *reinterpret_cast<uint64_t*>(host_pa_base + physical_address_rva));

	//set PFN to our page
	pdpte.huge.page_pa = pool_pa >> 30;

	//read to check for TAGs presence 
	uint64_t read = *reinterpret_cast<uint64_t*>(host_pa_base + pool_rva);
	print("READ: %llx\n", read);

	//read the same address read before to check if its still paged in
	uint64_t nhost_read = *reinterpret_cast<uint64_t*>(host_pa_base + physical_address_rva);

	__writecr3(old_cr3.value);
	_enable();

	//if the huge page (or atleast this portion of it) is paged in
	//both on a hv on a regular system this check should have TAG present (which invaildates our test)
	print("new host read %llx, host read %llx\n", nhost_read, host_read);
	if (nhost_read != host_read)
	{
		print("TLB presence check failed... test results could be invaild, or values changed\n");
		return TLB_FLUSH;
		//TLB being flushed is also a good detection vector, on a real system, unless the page wasnt cached to begin with,
		//this will never (VERY UNLIKELY) happen, so if it fails consistently a hv is likely present (from my experience testing on vmware)
	}

	//if TAG is present then the huge page is not cached, most likley due to HV
	if (read == TAG) {
		print("HV detected\n");
		return HV;
	}

	print("NOHV found\n");
	return NOHV;
}


bool setup_cr3() {
	print("Setting up CR3\n");

	memset(&our_pml4, 0, sizeof(our_pml4));

	old_cr3 = { __readcr3() };

	our_page = static_cast<uint8_t*>(MmAllocateContiguousMemory(0x1000, { .QuadPart = -1 }));
	if (!our_page) {
		print("Failed to allocate memory\n");
		return false;
	}
	memcpy(our_page, &TAG, sizeof(uint64_t));

	auto system_process = reinterpret_cast<uintptr_t>(PsInitialSystemProcess);
	auto system_cr3 = *reinterpret_cast<cr3_t*>(system_process + 0x28);
	auto system_pml4 = reinterpret_cast<pml4e_t*>(MmGetVirtualForPhysical({ .QuadPart = system_cr3.get_phys_pml4() }));

	memcpy(&our_pml4.pml4[256], &system_pml4[256], sizeof(pml4e_t) * 256); //copy kernel address space

	auto& pml4e = our_pml4.pml4[mapped_pml4e];
	pml4e.present = 1;
	//pml4e.usermode = 1;
	pml4e.write = 1;
	pml4e.page_pa = MmGetPhysicalAddress(&our_pml4.pdpt).QuadPart >> 12;

	//should test / check for huge page support
	auto& pdpte = our_pml4.pdpt[0];
	pdpte.present = 1;
	pdpte.write = 1;
	pdpte.huge_page = 1;

	our_cr3.value = 0;
	our_cr3.pml4 = MmGetPhysicalAddress(&our_pml4.pml4).QuadPart >> 12;

	return true;
}

void unload() {
	if (our_page)
		MmFreeContiguousMemory(our_page);
}

void check_all_memory() {
	//just to make sure the check_memory succeeds

	const uint64_t pool_pa = MmGetPhysicalAddress(our_page).QuadPart;

	print("^%llx\n", pool_pa);
	check_memory(pool_pa + pdpes_address_range + pdes_address_range + 1);
}