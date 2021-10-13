// rtspSend.cpp : Defines the entry point for the console application.
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include "h264.h"
static char* dumpRoot = ".\\dump\\";
static char file2open[1024];
#define  UDP_MAX_SIZE 1400



FILE* bits = NULL;                //!< the bit stream file
static int FindStartCode2(unsigned char* Buf);//���ҿ�ʼ�ַ�0x000001
static int FindStartCode3(unsigned char* Buf);//���ҿ�ʼ�ַ�0x00000001

// 
static int info2 = 0, info3 = 0;
// RTP_FIXED_HEADER *rtp_hdr;

// NALU_HEADER      *nalu_hdr;
// FU_INDICATOR *fu_ind;
// FU_HEADER        *fu_hdr;

FILE* getFile()
{
    return bits;
}

//ΪNALU_t�ṹ������ڴ�ռ�
NALU_t* AllocNALU(int buffersize)
{
    NALU_t* nal = NULL;

    if ((nal = (NALU_t*)calloc(1, sizeof(NALU_t))) == NULL)
    {
        printf("AllocNALU: n");
        exit(0);
    }

    nal->max_size = buffersize;

    if ((nal->buf = (char*)calloc(buffersize, sizeof(char))) == NULL)
    {
        free(nal);
        printf("AllocNALU: nal->buf");
        exit(0);
    }

    return nal;
}

//�ͷ�
void FreeNALU(NALU_t* n)
{
    if (n)
    {
        if (n->buf)
        {
            free(n->buf);
            n->buf = NULL;
        }
        free(n);
    }
}

void OpenBitstreamFile(char* fn)
{
    if (NULL == (bits = fopen(fn, "rb")))
    {
        printf("open file error\n");
        exit(0);
    }
}

//�����������Ϊһ��NAL�ṹ�壬��Ҫ����Ϊ�õ�һ��������NALU��������NALU_t��buf�У�
//��ȡ���ĳ��ȣ����F,IDC,TYPEλ��
//���ҷ���������ʼ�ַ�֮�������ֽ�������������ǰ׺��NALU�ĳ���
int GetAnnexbNALU(NALU_t* nalu)
{
    int pos = 0;
    int StartCodeFound, rewind;
    unsigned char* Buf;

    if ((Buf = (unsigned char*)calloc(nalu->max_size, sizeof(char))) == NULL)
        printf("GetAnnexbNALU: Could not allocate Buf memory\n");

    nalu->startcodeprefix_len = 3;//��ʼ���������еĿ�ʼ�ַ�Ϊ3���ֽ�

    if (3 != fread(Buf, 1, 3, bits))//�������ж�3���ֽ�
    {
        free(Buf);
        return 0;
    }
    info2 = FindStartCode2(Buf);//�ж��Ƿ�Ϊ0x000001 
    if (info2 != 1)
    {
        //������ǣ��ٶ�һ���ֽ�
        if (1 != fread(Buf + 3, 1, 1, bits))//��һ���ֽ�
        {
            free(Buf);
            return 0;
        }
        info3 = FindStartCode3(Buf);//�ж��Ƿ�Ϊ0x00000001
        if (info3 != 1)//������ǣ�����-1
        {
            free(Buf);
            return -1;
        }
        else
        {
            //�����0x00000001,�õ���ʼǰ׺Ϊ4���ֽ�
            pos = 4;
            nalu->startcodeprefix_len = 4;
        }
    }
    else
    {
        //�����0x000001,�õ���ʼǰ׺Ϊ3���ֽ�
        nalu->startcodeprefix_len = 3;
        pos = 3;
    }
    //������һ����ʼ�ַ��ı�־λ
    StartCodeFound = 0;
    info2 = 0;
    info3 = 0;

    while (!StartCodeFound)
    {
        if (feof(bits))//�ж��Ƿ����ļ�β
        {
            nalu->len = (pos - 1) - nalu->startcodeprefix_len;
            memcpy(nalu->buf, &Buf[nalu->startcodeprefix_len], nalu->len);
            nalu->forbidden_bit = nalu->buf[0] & 0x80; //1 bit
            nalu->nal_reference_idc = nalu->buf[0] & 0x60; // 2 bit
            nalu->nal_unit_type = (nalu->buf[0]) & 0x1f;// 5 bit
            free(Buf);
            return pos - 1;
        }
        Buf[pos++] = fgetc(bits);//��һ���ֽڵ�BUF��
        info3 = FindStartCode3(&Buf[pos - 4]);//�ж��Ƿ�Ϊ0x00000001
        if (info3 != 1)
            info2 = FindStartCode2(&Buf[pos - 3]);//�ж��Ƿ�Ϊ0x000001
        StartCodeFound = (info2 == 1 || info3 == 1);
    }

    // Here, we have found another start code (and read length of startcode bytes more than we should
    // have.  Hence, go back in the file
    rewind = (info3 == 1) ? -4 : -3;

    if (0 != fseek(bits, rewind, SEEK_CUR))//���ļ�ָ��ָ��ǰһ��NALU��ĩβ
    {
        free(Buf);
        printf("GetAnnexbNALU: Cannot fseek in the bit stream file");
    }

    // Here the Start code, the complete NALU, and the next start code is in the Buf.  
    // The size of Buf is pos, pos+rewind are the number of bytes excluding the next
    // start code, and (pos+rewind)-startcodeprefix_len is the size of the NALU excluding the start code

    nalu->len = (pos + rewind) - nalu->startcodeprefix_len;
    memcpy(nalu->buf, &Buf[nalu->startcodeprefix_len], nalu->len);//����һ������NALU����������ʼǰ׺0x000001��0x00000001
    nalu->forbidden_bit = nalu->buf[0] & 0x80;      //1 bit
    nalu->nal_reference_idc = nalu->buf[0] & 0x60;  //2 bit
    nalu->nal_unit_type = (nalu->buf[0]) & 0x1f;    //5 bit
    free(Buf);

    return (pos + rewind);//����������ʼ�ַ�֮�������ֽ�������������ǰ׺��NALU�ĳ���
}

static int FindStartCode2(unsigned char* Buf)
{
    if (Buf[0] != 0 || Buf[1] != 0 || Buf[2] != 1)
        return 0; //�ж��Ƿ�Ϊ0x000001,����Ƿ���1
    else
        return 1;
}

static int FindStartCode3(unsigned char* Buf)
{
    if (Buf[0] != 0 || Buf[1] != 0 || Buf[2] != 0 || Buf[3] != 1)
        return 0;//�ж��Ƿ�Ϊ0x00000001,����Ƿ���1
    else
        return 1;
}

int rtpnum = 0;

//���NALU���Ⱥ�TYPE
void dump(NALU_t* nal)
{
    if (!nal)
        return;
    printf("%3d, len: %6d  ", rtpnum++, nal->len);
    printf("nal_unit_type: %x\n", nal->nal_unit_type);
}

int DumpChar(char* filename, char* buf, int len)
{
    FILE* file;
    int w, h;
    unsigned char* temp = (unsigned char*)buf;
    sprintf(file2open, "%s%s", dumpRoot, filename);
    int mHeight = 0;
    int mWidth = 100;
    int mYu = 0;
    mHeight = len / 100;
    mYu = len % 100;
    file = fopen(file2open, "w+");
    for (h = 0; h < mHeight; h++)
    {
        for (w = 0; w < mWidth - 1; w++)
        {
            fprintf_s(file, "%3x,", temp[h * mWidth + w]);
        }
        fprintf_s(file, "%3x\n", temp[h * mWidth + w]);
    }

    for (w = 0; w < mYu - 1; w++)
    {
        fprintf_s(file, "%3x,", temp[h * mWidth + w]);
    }
    fprintf_s(file, "%3x\n", temp[h * mWidth + w]);
    fclose(file);
    return 0;
}