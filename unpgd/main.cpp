#include <stdio.h>
#include <stdlib.h>

extern "C" {
#include "libkirk/kirk_engine.h"
#include "libkirk/amctrl.h"
}

typedef long i32;

void error(const char* msg) {
	printf("Fatal: %s\n", msg);
#ifdef _DEBUG
	getchar();
#endif
	exit(1);
}

void readKey(u8* key, const char* keyFilePath, const i32 keyOffset) {
	FILE* f = NULL;
	fopen_s(&f, keyFilePath, "rb");
	if (!f) error("Can't open key file for reading");
	if (fseek(f, keyOffset, SEEK_SET)) error("Key file seek failed");
	if (fread(key, sizeof(u8), 16, f) != 16) error("Reading key failed");
	fclose(f);
}

int main(int argc, char* argv[]) {
	kirk_init();
	if (argc != 6) error("Invalid arguments, usage: [inputFile] [outputFile] [keyFile] [keyOffset] [pgdOpenFlag]");
	const char* inputFilePath = argv[1];
	const char* outputFilePath = argv[2];
	const char* keyFilePath = argv[3];
	const i32 keyOffset = strtol(argv[4], NULL, 10);
	if (errno) error("Args parse error");
	const i32 pgdFlag = strtol(argv[5], NULL, 10);
	if (errno) error("Args parse error");

	u8 key[16];
	readKey(key, keyFilePath, keyOffset);

	FILE* in = NULL;
	FILE* out = NULL;
	fopen_s(&in, inputFilePath, "rb");
	if (!in) error("Can't open input file for reading");
	fopen_s(&out, outputFilePath, "wb");
	if (!out) error("Can't open output file for writing");

	const u8 headerSize = 0x90;
	u8 pgdHeader[headerSize];
	if (fread(&pgdHeader, sizeof(u8), headerSize, in) != headerSize) error("Error reading PGD header");
	PGD_DESC* pgd = pgd_open(pgdHeader, pgdFlag, key);
	if (!pgd) error("PGD open failed");

	const u32 blockSize = pgd->block_size;
	const u32 blockNr = pgd->block_nr;
	const u32 dataSize = pgd->data_size;
	u8* blockBuf = (u8*)malloc(blockSize);
	if (!blockBuf) error("Failed to allocate memory for PGD block");
	pgd->block_buf = blockBuf;
	i32 lastProgress = -1;
	for (u32 i = 0; i < blockNr; i++) {
		if (fread(blockBuf, sizeof(u8), blockSize, in) != blockSize) error("\nError reading PGD block");
		const i32 progress = i * 100 / (blockNr - 1);
		if (progress != lastProgress) {
			lastProgress = progress;
			const i32 progressCli = (i32)(progress * 0.5f);
			printf("\r[");
			for (i32 i = 0; i < progressCli; i++) printf("#");
			for (i32 i = 0; i < 50 - progressCli; i++) printf(" ");
			printf("] %i%% ", progress);
			if (progress != 100) {
				printf("Decrypting...");
			} else {
				printf("Done         \n");
			}
		}
		pgd_decrypt_block(pgd, i);
		i32 outBlockSize = blockSize;
		if (ftell(out) + blockSize > dataSize) outBlockSize = dataSize - ftell(out);
		if (fwrite(blockBuf, sizeof(u8), outBlockSize, out) != outBlockSize) error("\nError writing PGD block");
	}
	pgd->block_buf = NULL;
	free(blockBuf);
	pgd_close(pgd);
	fclose(in);
	fclose(out);

#ifdef _DEBUG
	getchar();
#endif
	return 0;
}
