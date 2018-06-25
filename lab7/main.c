#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int is_file_name_set = 0;
char file_name[100];
int unit_size = -1;

typedef int (*ActionHandler)();// Return whether to continue

struct {
	const char* desc;
	ActionHandler handler;
} typedef Action;

int handleFileName() {
	printf("Enter file name: ");
	scanf("%s", file_name);
	is_file_name_set = 1;

	return 1;
}

int handleUnitSize() {
	printf("Enter unit size: ");
	scanf("%d", &unit_size);

	return 1;
}

void printHex(char* bytes, int unit_size, int len) {
	const int total = unit_size * len;

	for (int i = 0; i < total; i += unit_size) {
		if (i != 0)
			printf(" ");

		for (int j = 0; j < unit_size; ++j) {
			char res[32];
			const unsigned int current =
				((1 << 8) - 1/*masking because of potentialy negative sign bit expansion*/) &
				(unsigned int)bytes[i + j];
			
			sprintf(res, "%02X", current);
			printf("%s", res);
		}
	}
}

void printDec(int* nums, int len) {

	for (int i = 0; i < len; ++i) {
		if (i != 0)
			printf(" ");

		printf("%d", nums[i]);
	}
}

int handleFileDisplay() {
	if (!is_file_name_set || unit_size < 0) {
		printf("You need to set first a proper file name and unit size.\n");
		return 1;
	}
	// otherwise

	FILE* file = fopen(file_name, "rb");
	if (file == 0) {
		printf("Can't open the file {%s}.\n", file_name);
		return 1;
	}
	// otherwise

	unsigned int loc;
	int len;
	printf("Enter { location length }: ");
	scanf("%X %d", &loc, &len);

	const int total = unit_size * len;

	char* bytes = malloc(total);

	fseek(file, loc, SEEK_SET);

	if (fread(bytes, sizeof(char), total, file) == total) {
		printf("Hex:\n");
		printHex(bytes, unit_size, len);
		printf("\n");

		printf("=====================\n");

		printf("Dec:\n");
		printDec((int*)bytes, total / sizeof(int));
		printf("\n");
	} else
		printf("Error while reading a file.\n");

	free(bytes);

	fclose(file);

	return 1;
}

int handleModify() {
	if (!is_file_name_set || unit_size < 0) {
		printf("You need to set first a proper file name and unit size.\n");
		return 1;
	}
	// otherwise

	FILE* file = fopen(file_name, "rb+");
	if (file == 0) {
		printf("Can't open the file {%s}.\n", file_name);
		return 1;
	}
	// otherwise

	unsigned int loc;
	char new_val_str[1024];
	printf("Enter { location val }: ");
	scanf("%X %s", &loc, new_val_str);

	char new_val[unit_size];
	for (int i = 0; i < unit_size; ++i) {
		char current_str[] = { new_val_str[2*i], new_val_str[2*i+1], '\0' };
		unsigned int val;
		sscanf(current_str, "%X", &val);
		new_val[i] = (char)val;
	}

	fseek(file, loc, SEEK_SET);

	fwrite(new_val, sizeof(char), unit_size, file);

	fclose(file);

	return 1;
}

int handleCopyFromFile() {
	if (!is_file_name_set) {
		printf("You need to set first a proper file name.\n");
		return 1;
	}
	// otherwise

	FILE* dst_file = fopen(file_name, "rb+");
	if (dst_file == 0) {
		printf("Can't open destination file {%s}.\n", file_name);
		return 1;
	}
	// otherwise

	char src_filename[100];
	unsigned int src_offset;
	unsigned int dst_offset;
	unsigned int length;
	printf("Enter { <src_file> <src_offset> <dst_offset> <length> }: ");
	scanf("%s %X %X %d", src_filename, &src_offset, &dst_offset, &length);

	FILE* src_file = fopen(src_filename, "rb+");
	if (src_file == 0) {
		printf("Can't open source file {%s}.\n", src_filename);

		fclose(dst_file);
		return 1;
	}
	// otherwise

	fseek(src_file, src_offset, SEEK_SET);
	fseek(dst_file, dst_offset, SEEK_SET);

	char* bytes = malloc(length);

	if (fread(bytes, sizeof(char), length, src_file) == length) {
		if (fwrite(bytes, sizeof(char), length, dst_file) == length)
			printf("Copied %d bytes into FROM %s at %X TO %s at %X\n", length, src_filename, src_offset, file_name, dst_offset);
		else
			printf("Error while writing destination file.\n");
	} else
		printf("Error while reading source file.\n");
	
	free(bytes);

	fclose(src_file);
	fclose(dst_file);


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
	Action actions[] = {
		{ "Set File Name", handleFileName },
		{ "Set Unit Size", handleUnitSize },
		{ "File Display", handleFileDisplay },
		{ "File Modify", handleModify },
		{ "Copy From File", handleCopyFromFile },
		{ "Quit", handleQuit },
		{ NULL, NULL }
	};

	// I didn't use fgets() like Meir said, wasn't sure how to use. Can anyone upload a fix with it?

	while (1) {
		ActionHandler handler = askOptions(actions);
		if (!handler)
			printf("Bad input were given!\n");
		else if (!handler())
			break;
	}

	return 0;
}
