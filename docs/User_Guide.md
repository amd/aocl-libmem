# User Guide for _AOCL-LibMem_
**_AOCL-LibMem_** library provides an option for the user to pass configuration via
tunable parameter to choose specific implementation of a string/memory function
as per the use case requirements. This document describe how to tune the parameters
 along with the valid configuration settings for each of the supported function.

## 1. Running an Application with Tunables
Following two states are possible with this library based on the tunable settings:

### 1.1. Default State
By default, LibMem will choose the best fit implementation based on the underlying microarchitecture.

_Micro Architecuture specific Implementations:_
 * `ZEN1` : *<mem_func>_zen1*
 * `ZEN2` : *<mem_func>_zen2*
 * `ZEN3` : *<mem_func>_zen3*
 * `Generic Implementation`: *<mem_func>_system*

`Note:` _If the library failed to identify the underlying microarchitecture, then a generic implementation will be chosen._

### 1.2. Tunable State
*LibMem library exposes two tunable parameters for the users to choose
the variant of implementations as per the application requirement.*
 * **`LIBMEM_OPERATION`** : instruction based on alignment and cacheability
 * **`LIBMEM_THRESHOLD`** : threshold for ERMS and Non-Temporal instructions

#### 1.2.1. LIBMEM_OPERATION
Setting tunable _LIBMEM_OPERATION_ will make the library choose
the best implementation based on the combination of move instructions and
alignment of the source and destination addresses.

 **LIBMEM_OPERATION** format:
    **`<operations>,<source_alignment>,<destination_alignmnet>`**

 ##### Valid options:
 * `<operations> = [avx2|avx512|erms]`
 * `<source_alignment> = [b|w|d|q|x|y|n]`
 * `<destination_alignmnet> = [b|w|d|q|x|y|n]`

Refer to the below table for choosing the right implementation of <func> for your application.

Application Requirement | LIBMEM_OPERATION | Implementation | Instructions |Side effects
------------------------|------------------|----------------|--------------|------------
**vector unaligned `source` & `destination`**|**[avx2\|avx512],b,b**|*__<func>_[avx2\|avx512]_unaligned*| Load:VMOVDQU; Store:VMOVDQU| None
**vector aligned `source` and `destination`**|**[avx2\|avx512],y,y**|*__<func>_[avx2\|avx512]_aligned*| Load:VMOVDQA; Store:VMOVDQA| _**unaligned source &/ destination** address will lead to_ **_`CRASH`_**.
**vector aligned `source` and unaligned `destination`**|**[avx2\|avx512],y,[b\|w\|d\|q\|x]**|*__<func>_[avx2\|avx512]_aligned_load*| Load:VMOVDQA; Store:VMOVDQU| None.
**vector unaligned `source` and aligned `destination`**|**[avx2\|avx512],[b\|w\|d\|q\|x], y**|*__<func>_[avx2\|avx512]_aligned_store*| Load:VMOVDQU; Store:VMOVDQA| None.
**vector `non temporal load and store`**|**[avx2\|avx512],n,n**|*__<func>_[avx2\|avx512]_nt_load*| Load:VMOVNTDQA; Store:VMOVNTDQ| _**unaligned source & destination** address will lead to_ **_`CRASH`_**.
**vector `non temporal load`**|**[avx2\|avx512],n,[b\|w\|d\|q\|x\|y]**|*__<func>_[avx2\|avx512]_nt_load*| Load:VMOVNTDQA; Store:VMOVDQU| None.
**vector `non temporal store`**|**[avx2\|avx512],[b\|w\|d\|q\|x\|y],n**|*__<func>_[avx2\|avx512]_nt_store*| Load:VMOVDQU; Store:VMOVNTDQ| None.
**rep movs `unaligned source or destination`**|**erms,b,b**|*__<func>_erms_b_aligned*| REP MOVSB | None
**rep movs `word aligned source & destination`**|**erms,w,w**|*__<func>_erms_w_aligned*| REP MOVSW | **`Data Coruption or Crash`** if length is not multiple of WORD.
**rep movs `double word aligned source & destination`**|**erms,d,d**|*__<func>_erms_d_aligned*| REP MOVSD | **`Data Coruption or Crash`** if length is not multiple of DOUBLE WORD.
**rep movs `quad word aligned source & destination`**|**erms,q,q**|*__<func>_erms_q_aligned*| REP MOVSQ | **`Data Coruption or Crash`** if length is not multiple of QUAD WORD.


**`Note:` If the tunable setting is invalid, then a best-fit solution for the underlying microarchitecture will be chosen.**(_refer section **1.1**_)

#### 1.2.2. LIBMEM_THRESHOLD
Setting tunable _LIBMEM_THRESHOLD_ will make the library choose implementation with tuned threshold settings for supported instruction sets:{`vector, rep mov, non-temporal`}

 **LIBMEM_THRESHOLD** format: **`<repmov_start_threshold>,<repmov_stop_threshold>,<nt_start_threshold>,<nt_stop_threshold>`**

 ##### Valid options:
 * `<repmov_start_threshold> = [0, +ve integers]`
 * `<repmov_stop_threshold> = [0, +ve integers, -1]`
 * `<nt_start_threshold> = [0, +ve integers]`
 * `<nt_stop_threshold> = [0, +ve integers, -1]`

 *Make sure that you provide valid start and stop range values.*
 *If the size has to be set to maximum length then pass "-1"*

Refer to the below table for sample threshold settings.

LIBMEM_THRESHOLD|Vector range|Rep Mov range|Non-Temporal range
----------------|------------|-------------|------------------
0,2048,1048576,-1|(2049, 1048576)|[0,2048]|[1048576, <max value of unsigned long long>)
0,0,1048576,-1|[0,1048576)|[0,0]|[1048576,<max value of unsigned long long>)

**`Note:` If the tunable setting is invalid, then <func>_system will be executed with system computed threshold settings based on CPU feature flags.**
