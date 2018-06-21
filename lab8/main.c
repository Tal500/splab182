#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <elf.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define FILE_SIZE 2<<20

#define PRINT_HEADER_PADDING -34

typedef int (*ActionHandler)();// Return whether to continue

struct {
	const char* desc;
	ActionHandler handler;
} typedef Action;

char *map_start; /* will point to the start of the memory mapped file */
const Elf64_Ehdr *header; /* this will point to the header structure */
int num_of_section_headers;

const char* getDataEncoding(char ei_data) {
	switch (ei_data) {
		case ELFDATA2LSB: return "2's complement, little endian"; break;
		case ELFDATA2MSB: return "2's complement, big endian"; break;
		default: return "Invalid data encoding"; break;
	}
}

#define CASE_SHT(name) case SHT_##name: return #name; break;

const char* getSectionType(Elf32_Word type) {
	static char out[128];
	switch (type) {
		CASE_SHT(NULL)
		CASE_SHT(PROGBITS)
		CASE_SHT(SYMTAB)
		CASE_SHT(STRTAB)
		CASE_SHT(RELA)
		CASE_SHT(HASH)
		CASE_SHT(DYNAMIC)
		CASE_SHT(NOTE)
		CASE_SHT(NOBITS)
		CASE_SHT(REL)
		CASE_SHT(SHLIB)
		CASE_SHT(DYNSYM)
		CASE_SHT(NUM)
		CASE_SHT(LOPROC)
		CASE_SHT(HIPROC)
		CASE_SHT(LOUSER)
		CASE_SHT(HIUSER)
		default: sprintf(out, "%x", type); return out; break;
	}
}

int handleExamine() {
	if (header->e_ident[0] != 0x7f || header->e_ident[1] != 0x45 || header->e_ident[2] != 0x4c) {
		fprintf(stderr, "This file is not ELF! (by looking at the first 3 bytes)\n");
		return 1;
	}
	// otherwise

	printf("ELF Header:\n");

	printf("  %*s %02x %02x %02x\n", PRINT_HEADER_PADDING, "Magic:", header->e_ident[0], header->e_ident[1], header->e_ident[2]);

	printf("  %*s %s\n", PRINT_HEADER_PADDING, "Data:", getDataEncoding(header->e_ident[EI_DATA]));

	printf("  %*s %d\n", PRINT_HEADER_PADDING, "Version:", header->e_version);

	printf("  %*s 0x%lx\n", PRINT_HEADER_PADDING, "Entry point address:", header->e_entry);

	printf("  %*s %ld (bytes into file)\n", PRINT_HEADER_PADDING, "Start of program headers:", header->e_phoff);
	printf("  %*s %ld (bytes into file)\n", PRINT_HEADER_PADDING, "Start of section headers:", header->e_shoff);

	printf("  %*s 0x%x\n", PRINT_HEADER_PADDING, "Flags:", header->e_flags);

	printf("  %*s %d (bytes)\n", PRINT_HEADER_PADDING, "Size of program headers:", header->e_phentsize);
	printf("  %*s %d\n", PRINT_HEADER_PADDING, "Number of program headers:", header->e_phnum);

	printf("  %*s %d (bytes)\n", PRINT_HEADER_PADDING, "Size of section headers:", header->e_shentsize);
	printf("  %*s %d\n", PRINT_HEADER_PADDING, "Number of section headers:", header->e_shnum);
	printf("  %*s %d\n", PRINT_HEADER_PADDING, "Section header string table index:", header->e_shstrndx);

	return 1;
}

const void* getSectionDataStart(const Elf64_Shdr *section) {
	return map_start + section->sh_offset;
}

const char* getSectionName(const Elf64_Shdr *section) {
	const Elf64_Shdr *section_header = (Elf64_Shdr *)(map_start + header->e_shoff);
	const Elf64_Shdr *sh_strtab = &section_header[header->e_shstrndx];

	return (char*)(getSectionDataStart(sh_strtab) + section->sh_name);
}

int handleSection() {
	if (header->e_ident[0] != 0x7f || header->e_ident[1] != 0x45 || header->e_ident[2] != 0x4c) {
		fprintf(stderr, "This file is not ELF! (by looking at the first 3 bytes)\n");
		return 1;
	}
	// otherwise

	//const Elf64_Shdr* section_header = (Elf64_Shdr*)((void*)(header) + header->e_shoff);

	printf("ELF Sections:\n");

	//const void* strtab_header_section = (void*)section_header + header->e_shstrndx*sizeof(Elf64_Shdr);

	const Elf64_Shdr *section_header = (Elf64_Shdr *)(map_start + header->e_shoff);
	const int shnum = header->e_shnum;

	for (int i = 0; i < shnum; ++i) {
		printf(" [%3d] %-20s %016lx %08lx %016lx %s\n", i, getSectionName(&section_header[i]), section_header[i].sh_addr,
			section_header[i].sh_offset, section_header[i].sh_size, getSectionType(section_header[i].sh_type));
	}

	return 1;
}

int handleSymbols() {
	if (header->e_ident[0] != 0x7f || header->e_ident[1] != 0x45 || header->e_ident[2] != 0x4c) {
		fprintf(stderr, "This file is not ELF! (by looking at the first 3 bytes)\n");
		return 1;
	}
	// otherwise

	const Elf64_Shdr *section_header = (Elf64_Shdr *)(map_start + header->e_shoff);
	const int shnum = header->e_shnum;

	int is_first = 1;

	for (int i = 0; i < shnum; ++i) {
		const Elf64_Shdr *current_section = section_header + i;

		const Elf64_Word type = current_section->sh_type;
		if (type != SHT_DYNSYM && type != SHT_SYMTAB)
			continue;
		// otherwise

		if (is_first)
			is_first = 0;
		else
			printf("\n");

		const Elf64_Sym* symbol_start = (const Elf64_Sym*)getSectionDataStart(current_section);

		const char* name = getSectionName(current_section);
		const int symbol_count = current_section->sh_size / sizeof(Elf64_Sym);
		
		printf("Symbol table '%s' contains %d entries:\n", name, symbol_count);

		for (int j = 0; j < symbol_count; ++j) {
			const Elf64_Sym *current_symbol = symbol_start + j;
			const Elf64_Shdr *current_symbol_section = section_header + current_symbol->st_shndx;
			//printf(" [%3d] %016lx %3d\n", j, current_symbol->st_value, current_symbol->st_shndx);
			const char* section_name = (current_symbol->st_shndx < shnum) ? getSectionName(current_symbol_section) : "No Section";
			const char* symbol_name = getSectionDataStart(current_section + 1) + current_symbol->st_name;// Can someone tell me how to do this with sh_link instead of +1?
			printf(" [%3d] %016lx %5d %-20s %s\n", j, current_symbol->st_value, current_symbol->st_shndx, section_name, symbol_name);
		}
	}

	return 1;
}

int handleQuit() {
	return 0;
}

ActionHandler askOptions(const Action* actions) {
	printf("Choose action:\n");

	int i = 0;
	for (; actions[i].desc != NULL; ++i)
		printf("%d-%s\n", (i + 1), actions[i].desc);

	int chosen_option;
	if (scanf("%d", &chosen_option) != 1 || chosen_option < 1 || chosen_option > i)
		return NULL;
	else
		return actions[chosen_option - 1].handler;
}

int main(int argc, char** argv) {
	if (argc != 2) {
		fprintf(stderr, "Bad arguments. Run with \"myELF <ELF File>\"\n");
		return -1;
	}

	int fd;
	if( (fd = open(argv[1], O_RDONLY)) < 0 ) {
		perror("error in open");
		return -1;
	}
	// otherwise

	struct stat fd_stat; /* this is needed to  the size of the file */
	if( fstat(fd, &fd_stat) != 0 ) {
		close(fd);
		perror("stat failed");
		return -1;
	}

	if ( (map_start = mmap(0, fd_stat.st_size, PROT_READ , MAP_SHARED, fd, 0)) == MAP_FAILED ) {
		close(fd);
		perror("mmap failed");
		return -4;
	}
	// otherwise

	header = (Elf64_Ehdr*)map_start;
	num_of_section_headers = header->e_shnum;

	Action actions[] = {
		{ "Examine ELF File", handleExamine },
		{ "Print Section Names", handleSection },
		{ "Print Symbols", handleSymbols },
		{ "Quit", handleQuit },
		{ NULL, NULL }
	};

	while (1) {
		ActionHandler handler = askOptions(actions);
		if (!handler)
			printf("Bad input were given!\n");
		else if (!handler())
			break;
	}

	munmap(map_start, fd_stat.st_size);
	close(fd);

	return 0;
}
