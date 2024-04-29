
# SLAT-Smasher
A redpill POC that utilizes how the TLB works on virtualized systems to detect hypervisor presence.

### Before you read: 
- Understand how the [TLB](https://en.wikipedia.org/wiki/Translation_lookaside_buffer) works
 - Understand how [SLAT](https://en.wikipedia.org/wiki/Second_Level_Address_Translation) works
 - NPTs/EPTs are going to be referred to as “SLAT page(s)”
 - “Huge page” refers to a page that addresses either 1GB or 2MB (depending on system support for 1GB pages)

# How it works:
### The key difference:
When a system is virtualized the TLB has to account for SLAT. It does this by creating the TLB entry to match the size of the smaller page. (AMD Programmer's Manual Vol 2 15.25.9)
### The check:
The check goes as follows (check_memory in detection.cpp):

 1. Mapping a huge page in the cr3
 2. Set huge page PFN to contain the address to be checked
 3. Get the address to be checked cached in the TLB
 4. Switch PFN to an allocated pool which contains a TAG
 5. Read at the address of the TAG
 
If the TAG is present it means the address to be checked was cached in the TLB not as a huge page but as a smaller variant therefore there is a hypervisor running.

Skimmed down version of the check_memory function
```c++
pdpte.huge.page_pa = physical_address >> 30;

uint64_t host_read = *reinterpret_cast<uint64_t*>(host_pa_base + scan_rva);

for (uint8_t i = 0; i < 15; i++)
	*reinterpret_cast<uint64_t*>(host_pa_base + scan_rva);

pdpte.huge.page_pa = pool_pa >> 30;

uint64_t read = *reinterpret_cast<uint64_t*>(host_pa_base + pool_rva);

if (read == TAG) {
	return HV;
}

return NOHV;
```

### Caveats:

The difference between the physical address of the “address to be checked” and our memory containing the TAG cannot be smaller than the range of memory a huge page addresses.

If the RVA (from the base of the huge page) of the “address to be checked” and the TAG is less than the smaller page size this check is ineffective as we are reading the same cached memory the whole time.

There is a very rare chance that the original huge page could be flushed from the TLB, which should be checked for.

This POC requires cr3 control

### Benefits:
Fast

Consistent (unlikely to fail)

No VMEXITs

# Implementation:
This redpill uses the fact that hypervisors set up their ept/npt tables with 2MB pages instead of 1GB pages due to convenience.

Example from [jonomango/hv](https://github.com/jonomango/hv/blob/783ad9d99396546313b809bada530aa91366eb9d/hv/ept.cpp#L58) showing use of 2MB pages to setup EPTs  
 ```c++
 //Irrelevant code has been deleted 
 for  (size_t  j  =  0;  j  <  512;  ++j)  {
	//  identity-map  every  GPA  to  the  corresponding  HPA
	auto&  pde  =  ept.pds_2mb[i][j]; <------------//

	pde.large_page  =  1;
	pde.page_frame_number  =  (i  <<  9)  +  j;

}
 ```
 This is a very easy detection, simply check any physical address. (check_all_memory in detection.cpp)
 
 This POC could also be used to check for split SLAT pages if set up correctly (scanning all of physical memory). For example, redpill software could set up self-referential page-tables to check for inconsistencies by “walking” physical memory.

 # Testing
 Confirmed to be working on AMD (Ryzen 5 3600X 6-Core Processor) by me (TDC0471)
 
 Confirmed to be working on Intel soon...