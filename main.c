#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>

#define bytes_to_u16(MSB,LSB) ((((unsigned int) ((unsigned char) (MSB))) & 255)<<8 | (((unsigned char) (LSB))&255))
#define array2_to_u16(NUM) bytes_to_u16((NUM)[1], ((NUM)[0]))
#define bufferToInt(ARRAY) (uint32_t)((ARRAY)[3] << 24 | (ARRAY)[2] << 16 | (ARRAY)[1] << 8 | (ARRAY)[0])

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
            uint8_t BPB_RootClus[4]; // sector of root cluster, usually 2 but not always
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

#pragma pack(push, 1)
typedef struct
{
    uint8_t DIR_Name[11];
    uint8_t DIR_Attr;
    uint8_t DIR_NTRes;
    uint8_t DIR_CrtTimeTenth;
    uint8_t DIR_CrtTime[2];
    uint8_t DIR_CrtDate[2];
    uint8_t DIR_LstAccDate[2];
    uint8_t DIR_FstClusHI[2];
    uint8_t DIR_WrtTime[2];
    uint8_t DIR_WrtDate[2];
    uint8_t DIR_FstClusLO[2];
    uint8_t DIR_FileSize[4];
} DIR;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    uint8_t LDIR_Ord;
    uint8_t LDIR_Name1[10];
    uint8_t LDIR_Attr;
    uint8_t LDIR_Type;
    uint8_t LDIR_Chksum;
    uint8_t LDIR_Name2[12];
    uint8_t LDIR_FstClusLO[2];
    uint8_t LDIR_Name3[4];
} LONG_NAME;
#pragma pack(pop)
FILE *raw;
uint32_t fourBytesToInt(uint8_t *array)
{
    uint8_t reversedArray[4];
    reversedArray[0] = array[3];
    reversedArray[1] = array[2];
    reversedArray[2] = array[1];
    reversedArray[3] = array[0];
    return (uint32_t)array;
}

uint32_t findFirstSectorOfCluster(BS_BPB *fat, uint32_t firstDataSector, uint32_t N)
{
    return (uint32_t)((N - 2) * fat->BPB_SecPerClus + firstDataSector);
}

uint8_t isEntryExists(DIR *dir)
{
    uint16_t size = sizeof(*dir);
    uint8_t flag = 0;
    for(int i = 0; i < size; i++)
    {
        if(((uint8_t*)dir)[i] != 0)
            return 1;
    }
    return 0;
}
void extractFile(uint32_t offset, BS_BPB *bs_bpb)
{
    fseek(raw, offset, SEEK_SET);
    DIR *rootDir = malloc(sizeof(DIR));
    fread(rootDir, 1, sizeof(DIR), raw);
    int i = 0;
    FILE *f = fopen("file.out", "ab+");
    while(isEntryExists(rootDir))
    {
        fprintf(f, (rootDir) + i);
        fseek(raw, offset + 32 * i, SEEK_SET);
        fread(rootDir, 1, sizeof(DIR), raw);
        i++;
    }
    fclose(f);
}


int constructLongName(char *name, LONG_NAME *longName)
{
    int k = 0;
    for(int i = 0; i < 26; i++)
    {
        name[i] = '\0';
    }
    for(int i = 0; i < sizeof(longName->LDIR_Name1); i++)
    {
        if(i%2 == 0) {
            if(longName->LDIR_Name1[i] == '\0') {
                return k;
            }
            name[k] = longName->LDIR_Name1[i];
            k++;
        }
    }
    for(int i = 0; i < sizeof(longName->LDIR_Name2); i++)
    {
        if(i%2 == 0) {
            if(longName->LDIR_Name2[i] == '\0') {
                return k;
            }
            name[k] = longName->LDIR_Name2[i];
            k++;
        }
    }
    for(int i = 0; i < sizeof(longName->LDIR_Name3); i++)
    {
        if(i%2 == 0) {
            if(longName->LDIR_Name3[i] == '\0') {
                return k;
            }
            name[k] = longName->LDIR_Name3[i];
            k++;
        }
    }
    return k;
}

uint8_t isDirFree(DIR *dir)
{
    if(dir->DIR_Name[0] == 0xE5)
        return 1;
    return 0;
}

typedef struct
{
    char *longName;
    uint32_t offsetInDir;
} fileToList;

fileToList** listDirectory(uint32_t offset, BS_BPB *bs_bpb, uint8_t isRoot, uint32_t *countOfFiles)
{
    fseek(raw, offset, SEEK_SET);
    DIR *rootDir = malloc(sizeof(DIR));
    fread(rootDir, 1, sizeof(DIR), raw);
    uint32_t filesCount = 0;
    uint32_t capacityFiles = 3;
    fileToList **arrayToReturn = malloc(capacityFiles * sizeof(fileToList));
    uint32_t FATOffset = bufferToInt(bs_bpb->offset36.fat32_offset36.BPB_RootClus)*4;
    uint32_t ThisFATSecNum = array2_to_u16(bs_bpb->BPB_RsvdSecCnt) + (FATOffset / array2_to_u16(bs_bpb->BPB_BytsPerSec));
    uint32_t ThisFatEntOffset = FATOffset % array2_to_u16(bs_bpb->BPB_BytsPerSec);
    //printf("\n0x%X", ThisFATSecNum * array2_to_u16(bs_bpb->BPB_BytsPerSec));
    fseek(raw, ThisFATSecNum * array2_to_u16(bs_bpb->BPB_BytsPerSec) + ThisFatEntOffset, SEEK_SET);
    uint32_t nextClusterOfRoot;
    fread(&nextClusterOfRoot, sizeof(uint32_t), 1, raw);
    //printf("\nnextRootCluster is 0x%X\n", nextClusterOfRoot);

    DIR *currentDir = malloc(sizeof(DIR));
    LONG_NAME *currentLong = malloc((sizeof(LONG_NAME)));
    int i = 0;
    uint8_t lastWasLong = 0;
    char *currentLongName = malloc(26 * 1);
    int countOfParts = 1;
    while(1)
    {
        fseek(raw, offset + 32 * i, SEEK_SET);
        fread(currentDir, 1, sizeof(DIR), raw);
        if(!isEntryExists(currentDir)) {
            break;
        }
        if(currentDir->DIR_Attr & 0x0F)
        {
            if(lastWasLong == 0) {
                currentLongName = malloc(26 * 1);
                strcpy(currentLongName, "");
            }
            //countOfParts = 1;
            memcpy(currentLong, currentDir, 32);
            char longName[26];
            strcpy(longName, "");
            int length = constructLongName(longName, currentLong);
            countOfParts++;
            if(lastWasLong == 1)
                realloc(currentLongName, 26*countOfParts);

            char *temp = strdup(currentLongName);
            strcpy(currentLongName, longName);
            strcat(currentLongName, temp);
            free(temp);
            lastWasLong = 1;
        } else
        {
            if(!isDirFree(currentDir)) {
                fileToList *temp = malloc(sizeof(fileToList));
                temp->longName = malloc(sizeof(26*countOfParts));
                temp->longName = strdup(currentLongName);
                temp->offsetInDir = i;
                filesCount++;
                if (filesCount >= capacityFiles) {
                    capacityFiles *= 2;
                    realloc(arrayToReturn, capacityFiles * sizeof(fileToList));
                }
                arrayToReturn[filesCount - 1] = temp;
                /*printf(arrayToReturn[filesCount - 1]->longName);
                printf("%d", arrayToReturn[filesCount - 1]->offsetInDir);*/

            }
            lastWasLong = 0;
            strcpy(currentLongName, "");
            countOfParts = 1;
        }
        i++;

    }
    if(nextClusterOfRoot != 0xFFFFFF8) {
        uint32_t FATSz;
        if(array2_to_u16(bs_bpb->BPB_FATSz16) != 0)
        {
            FATSz = bs_bpb->BPB_FATSz16;
        }
        else
        {
            FATSz = bufferToInt(bs_bpb->offset36.fat32_offset36.BPB_FATSz32);
        }
        uint32_t rootDirSectors = (uint32_t)ceil((((array2_to_u16(bs_bpb->BPB_RootEntCnt)) * 32)
                                                  + ((array2_to_u16(bs_bpb->BPB_BytsPerSec) - 1))
                                                    / (array2_to_u16(bs_bpb->BPB_BytsPerSec))));
        uint32_t firstDataSector = (uint32_t)(array2_to_u16(bs_bpb->BPB_RsvdSecCnt) + (bs_bpb->BPB_NumFATs * FATSz) + rootDirSectors);
        uint32_t firstSectorOfCluster = findFirstSectorOfCluster(bs_bpb, firstDataSector, bufferToInt(bs_bpb->offset36.fat32_offset36.BPB_RootClus));
        uint32_t sectorOffset = firstSectorOfCluster * array2_to_u16(bs_bpb->BPB_BytsPerSec);
        int *countOfFiles2;
        uint32_t *fileToList2;
        fileToList2 = listDirectory(sectorOffset, bs_bpb, 0, countOfFiles2);
        realloc(arrayToReturn, *countOfFiles*(*countOfFiles2) * sizeof(int));
        *(arrayToReturn + *countOfFiles) = fileToList2;
    }

    *countOfFiles = filesCount;
    return arrayToReturn;
}
int main(int argc, char **argv) {
    BS_BPB *bs_bpb = (BS_BPB*)malloc(sizeof(BS_BPB));
    //printf("%s", argv[1]);
    if(argc == 1) {
        raw = fopen("hello-world 2.img", "rb");
    }
    else
    {
        raw = fopen(argv[1], "rb");
    }
    fread(bs_bpb, 1, sizeof(BS_BPB), raw);

    // compute number of sectors occupied by root dir
    uint32_t rootDirSectors = (uint32_t)ceil((((array2_to_u16(bs_bpb->BPB_RootEntCnt)) * 32)
                                              + ((array2_to_u16(bs_bpb->BPB_BytsPerSec) - 1))
                                            / (array2_to_u16(bs_bpb->BPB_BytsPerSec))));


    // find start of data region
    uint32_t FATSz;
    if(array2_to_u16(bs_bpb->BPB_FATSz16) != 0)
    {
        FATSz = bs_bpb->BPB_FATSz16;
    }
    else
    {
        FATSz = bufferToInt(bs_bpb->offset36.fat32_offset36.BPB_FATSz32);
    }

    uint32_t firstDataSector = (uint32_t)(array2_to_u16(bs_bpb->BPB_RsvdSecCnt) + (bs_bpb->BPB_NumFATs * FATSz) + rootDirSectors);
    //printf("\nfirst data sector 0x%X", firstDataSector);
    uint32_t firstSectorOfRootCluster = findFirstSectorOfCluster(bs_bpb, firstDataSector, bufferToInt(bs_bpb->offset36.fat32_offset36.BPB_RootClus));
    //printf("\nfirstSectorOfCluster: 0x%X", firstSectorOfRootCluster);
    uint32_t rootSectorOffset = firstSectorOfRootCluster * array2_to_u16(bs_bpb->BPB_BytsPerSec);
    //printf("\nrootOffset: 0x%X", rootSectorOffset);

    uint32_t currentOffset = rootSectorOffset;

    uint32_t *countOfFiles = malloc(sizeof(int));
    fileToList **arrayOfFiles = listDirectory(currentOffset, bs_bpb, 1, countOfFiles);

    char *cmd = malloc(10);
    char *prevPath = malloc(255);
    char *path = malloc(255);
    prevPath = "/";
    uint32_t prevOffset = rootSectorOffset;
    path = "/";
    printf("Commands:\nls - list directory\ncd - navigate to\nextract - extract to file.out\nexit - exit\n");
    while(1) {
        scanf("%s", cmd);
        printf("\n");
        if(strstr(cmd, "exit") != NULL)
        {
            exit(0);
        }
        if (strstr(cmd, "cd") != NULL) {
            char *file = malloc(255);
            printf("Enter directory name or '..' for level-up\n");
            scanf("%s", file);
            printf("\n");
            if (strcmp(file, "..") == 0) {
                currentOffset = prevOffset;
                path = prevPath;
                printf("%s\n", path);
            } else {
                for (int i = 0; i < *countOfFiles; i++) {
                    if (strstr(arrayOfFiles[i]->longName, file) != NULL) {
                        prevPath = path;
                        path = arrayOfFiles[i]->longName;
                        fseek(raw, currentOffset + 32 * arrayOfFiles[i]->offsetInDir, SEEK_SET);
                        DIR *currDir = malloc(sizeof(DIR));
                        fread(currDir, 1, sizeof(DIR), raw);
                        //printEntryInfo(currDir);
                        uint8_t firstDirClus[4];
                        firstDirClus[0] = currDir->DIR_FstClusLO[0];
                        firstDirClus[1] = currDir->DIR_FstClusLO[1];
                        firstDirClus[2] = currDir->DIR_FstClusHI[0];
                        firstDirClus[3] = currDir->DIR_FstClusHI[1];
                        uint32_t firstSectorOfCluster = findFirstSectorOfCluster(bs_bpb, firstDataSector,
                                                                                 bufferToInt(firstDirClus));
                        uint32_t sectorOffset = firstSectorOfCluster * array2_to_u16(bs_bpb->BPB_BytsPerSec);
                        prevOffset = currentOffset;
                        currentOffset = sectorOffset;
                        printf("%s\n", path);
                        break;
                    }
                }
            }
        }

        if (strstr(cmd, "ls") != NULL) {
            char *file = malloc(255);
            printf("Enter directory name or '.' for current\n");
            scanf("%s", file);
            printf("\n");
            if (strcmp(file, ".") == 0) {
                for (int i = 0; i < *countOfFiles; i++) {
                    printf(arrayOfFiles[i]->longName);
                    printf("\n");
                }
            } else {
                for (int i = 0; i < *countOfFiles; i++) {
                    if (strstr(arrayOfFiles[i]->longName, file) != NULL) {
                        fseek(raw, currentOffset + 32 * arrayOfFiles[i]->offsetInDir, SEEK_SET);
                        DIR *currDir = malloc(sizeof(DIR));
                        fread(currDir, 1, sizeof(DIR), raw);
                        //printEntryInfo(currDir);
                        uint8_t firstDirClus[4];
                        firstDirClus[0] = currDir->DIR_FstClusLO[0];
                        firstDirClus[1] = currDir->DIR_FstClusLO[1];
                        firstDirClus[2] = currDir->DIR_FstClusHI[0];
                        firstDirClus[3] = currDir->DIR_FstClusHI[1];
                        uint32_t firstSectorOfCluster = findFirstSectorOfCluster(bs_bpb, firstDataSector,
                                                                                 bufferToInt(firstDirClus));
                        uint32_t sectorOffset = firstSectorOfCluster * array2_to_u16(bs_bpb->BPB_BytsPerSec);
                        uint32_t *countOfFiles = malloc(sizeof(int));
                        //printf("SECTOR OFFSET: 0x%X", sectorOffset);
                        fileToList **arrayOfFiles = listDirectory(sectorOffset, bs_bpb, 1, countOfFiles);
                        for (int i = 0; i < *countOfFiles; i++) {
                            printf(arrayOfFiles[i]->longName);
                            printf("\n");
                        }
                        break;
                    }
                }
            }
        }
        if(strstr(cmd, "extract") != NULL)
        {
            char *file = malloc(255);
            printf("Enter directory name or '.' for cancel\n");
            scanf("%s", file);
            printf("\n");
            if (strcmp(file, ".") == 0) {
                continue;
            } else {
                for (int i = 0; i < *countOfFiles; i++) {
                    if (strstr(arrayOfFiles[i]->longName, file) != NULL) {
                        fseek(raw, currentOffset + 32 * arrayOfFiles[i]->offsetInDir, SEEK_SET);
                        DIR *currDir = malloc(sizeof(DIR));
                        fread(currDir, 1, sizeof(DIR), raw);
                        //printEntryInfo(currDir);
                        uint8_t firstDirClus[4];
                        firstDirClus[0] = currDir->DIR_FstClusLO[0];
                        firstDirClus[1] = currDir->DIR_FstClusLO[1];
                        firstDirClus[2] = currDir->DIR_FstClusHI[0];
                        firstDirClus[3] = currDir->DIR_FstClusHI[1];
                        uint32_t firstSectorOfCluster = findFirstSectorOfCluster(bs_bpb, firstDataSector,
                                                                                 bufferToInt(firstDirClus));
                        uint32_t sectorOffset = firstSectorOfCluster * array2_to_u16(bs_bpb->BPB_BytsPerSec);
                        extractFile(sectorOffset, bs_bpb);
                        break;
                    }
                }
            }
        }

    }

}