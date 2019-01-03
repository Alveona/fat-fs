#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#pragma pack(push, 1)
typedef struct
{
    uint8_t BS_jmpBoot[3];
    uint8_t BS_OEMName[8];
    uint8_t BPB_BytsPerSec[2];
    uint8_t BPB_SecPerClus;
    uint8_t BPB_RsvdSecCnt[2];
    uint8_t BPB_NumFATs;
    uint8_t BPB_RootEntCnt[2];
    uint8_t BPB_TotSec16[2];
    uint8_t BPB_Media;
    uint8_t BPB_FATSz16[2]; // must be 0 in fat32
    uint8_t BPB_SecPerTrk[2];
    uint8_t BPB_NumHeads[2];
    uint8_t BPB_HiddSec[4];
    uint8_t BPB_TotSec32[4];
    union OFFSET36
    {
        struct FAT12_16_OFFSET36
        {
            uint8_t PLACEHOLDER[28]; // TODO complete header
        } fat12_16_offset36;
        struct FAT32_OFFSET36
        {
            uint8_t BPB_FATSz32[4];
            uint8_t BPB_ExtFlags[2];
            uint8_t BPB_FSVer[2];
            uint8_t BPB_RootClus[4];
            uint8_t BPB_FSInfo[2];
            uint8_t BPB_BkBootSec[2];
            uint8_t BPB_Reserved[12];
            uint8_t BS_DrvNum;
            uint8_t BS_Reserved1;
            uint8_t BS_BootSig;
            uint8_t BS_VolID[4];
            uint8_t BS_VolLab[11];
            uint8_t BS_FilSysType[8];
        } fat32_offset36;
    } offset36;
} BS_BPB;
#pragma pack(pop)

int main() {
    printf("%d\n", sizeof(BS_BPB));

    BS_BPB *bs_bpb = (BS_BPB*)malloc(sizeof(BS_BPB));

    FILE *raw = fopen("rdisk2s1", "rb+");

    fread(bs_bpb, 1, sizeof(BS_BPB), raw);

    for(int i = 0; i < sizeof(BS_BPB); i++)
    {
        //printf("%d ", i);
        printf("%02hhX%s", ((uint8_t*)bs_bpb)[i], ((i+1)%0x10 == 0 && i != 0) ? "\n" : " ");
    }


    return 0;
}