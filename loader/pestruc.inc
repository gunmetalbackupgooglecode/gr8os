					 struct IMAGE_FILE_HEADER
  Machine		dw ?
  NumberOfSections	dw ?
  TimeDateStamp 	dd ?
  PointerToSymbolTable	dd ?
  NumberOfSymbols	dd ?
  SizeOfOptionalHeader	dw ?
  Characteristics	dw ?
ends

struct IMAGE_OPTIONAL_HEADER
  Magic 		dw ?
  MajorLinkerVersion	db ?
  MinorLinkerVersion	db ?
  SizeOfCode		dd ?
  SizeOfInitializedData dd ?
  SizeOfUninitializedData dd ?
  AddressOfEntryPoint	dd ?
  BaseOfCode		dd ?
  BaseOfData		dd ?
  ImageBase		dd ?
  SectionAlignment	dd ?
  FileAlignment 	dd ?
  MajorOSVersion	dw ?
  MinorOSVersion	dw ?
  MajorImageVersion	dw ?
  MinorImageVersion	dw ?
  MajorSubsystemVersion dw ?
  MinorSubsystemVersion dw ?
  Win32VersionValue	dd ?
  SizeOfImage		dd ?
  SizeOfHeaders 	dd ?
  CheckSum		dd ?
  Subsystem		dw ?
  DllCharacteristics	dw ?
  SizeOfStackReserve	dd ?
  SizeOfStackCommit	dd ?
  SizeOfHeapReserve	dd ?
  SizeOfHeapCommit	dd ?
  LoaderFlags		dd ?
  NumberOfRvaAndSizes	dd ?
ends

struct IMAGE_DOS_HEADER
  e_magic		dw ?
  e_cblp		dw ?
  e_cp			dw ?
  e_crlc		dw ?
  e_cparhdr		dw ?
  e_minalloc		dw ?
  e_maxalloc		dw ?
  e_ss			dw ?
  e_sp			dw ?
  e_csum		dw ?
  e_ip			dw ?
  e_cs			dw ?
  e_lfarlc		dw ?
  e_ovno		dw ?
  e_res 		rw 4
  e_oemid		dw ?
  e_oeminfo		dw ?
  e_res2		rw 10
  e_lfanew		dd ?
ends
