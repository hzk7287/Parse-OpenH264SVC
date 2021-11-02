#include <iostream>
#include <fstream>
using namespace std;


typedef struct
{
	uint32_t len;                     //! Length of the NAL unit (Excluding the start code, which does not belong to the NALU)
	int32_t forbidden_bit;            //! should be always FALSE
	int32_t nal_reference_idc;        //! NALU_PRIORITY_xxxx
	int32_t nal_unit_type;            //! NALU_TYPE_xxxx
	char* buf;                        //! contains the first byte followed by the EBSP
} NALU_t;


char* DetectStartCodePrefix(const char* kpBuf, char* pOffset, char iBufSize) {
	char* pBits = (char*)kpBuf;

	do {
		int iIdx = 0;
		while ((iIdx < iBufSize) && (!(*pBits))) {
			++pBits;
			++iIdx;
		}
		if (iIdx >= iBufSize)  break;

		++iIdx;
		++pBits;

		if ((iIdx >= 3) && ((*(pBits - 1)) == 0x1)) {
			*pOffset = (int)(((uintptr_t)pBits) - ((uintptr_t)kpBuf));
			return pBits;
		}

		iBufSize -= iIdx;
	} while (1);

	return NULL;
}

static int FindStartCode(const char* buf)
{
	if (buf[0] != 0x0 || buf[1] != 0x0) {
		return 0;
	}

	if (buf[2] == 0x1) {
		return 3;
	}
	else if (buf[2] == 0x0) {
		if (buf[3] == 0x1) {
			return 4;
		}
		else {
			return 0;
		}
	}
	else {
		return 0;
	}
	return 0;
}

static int32_t ReadOneNaluFromBuf(const char* buffer, uint32_t nBufferSize, uint32_t offSet, NALU_t* pNalu)
{
	uint32_t start = 0;
	if (offSet < nBufferSize) {
		start = FindStartCode(buffer + offSet);
		if (start != 0) {
			uint32_t pos = start;
			while (offSet + pos < nBufferSize) {
				if (buffer[offSet + pos++] == 0x00 &&
					buffer[offSet + pos++] == 0x00 &&
					buffer[offSet + pos++] == 0x00 &&
					buffer[offSet + pos++] == 0x01
					) {
					break;
				}
			}

			if (offSet + pos == nBufferSize) {
				pNalu->len = pos - start;
			}
			else {
				pNalu->len = (pos - 4) - start;
			}

			pNalu->buf = (char*)&buffer[offSet + start];
			pNalu->forbidden_bit = pNalu->buf[0] >> 7;
			pNalu->nal_reference_idc = pNalu->buf[0] >> 5; // 2 bit
			pNalu->nal_unit_type = (pNalu->buf[0]) & 0x1f;// 5 bit
			return (pNalu->len + start);
		}
	}

	return 0;
}

void parseH264(const char* pH264, uint32_t bufLen)
{
	NALU_t nalu;
	int32_t pos = 0, len = 0;
	len = ReadOneNaluFromBuf(pH264, bufLen, pos, &nalu);
	while (len != 0) {
		printf("## nalu type(%d)\n", nalu.nal_unit_type);
		pos += len;
		len = ReadOneNaluFromBuf(pH264, bufLen, pos, &nalu);
	}

}


int main() {
	filebuf* pbuf;
	ifstream filestr;
	ofstream fileout;
	long size;
	int i = 0;
	char* buffer;
	char* outbuffer;
	int32_t iBufPos = 0;
	int32_t iSrcConsumed = 0;
	char iOffset = 0;
	int32_t iSliceSize=0;
	int32_t iSrcLength = 0;

	NALU_t nalu;
	int32_t pos = 0, len = 0, posout = 0, posp = 0, pospout=0;
	char* pSrcNal = NULL;
	char* pDstNal = NULL;


	// 要读入整个文件，必须采用二进制打开 
	filestr.open("test.264", ios::binary);
	fileout.open("testvdlaye0.264", ios::binary);
	// 获取filestr对应buffer对象的指针 
	pbuf = filestr.rdbuf();

	// 调用buffer对象方法获取文件大小
	size = pbuf->pubseekoff(0, ios::end, ios::in);
	pbuf->pubseekpos(0, ios::in);

	// 分配内存空间 +1是关键，不然又些文件会乱码
	buffer = new char[size + 1];
	outbuffer = new char[size + 1];
	// 获取文件内容
	pbuf->sgetn(buffer, size);
	uint8_t uiDependencyId=255;

	len = ReadOneNaluFromBuf(buffer, size, pos, &nalu);
	while (len != 0) {
		printf("## nalu type(%d)\n", nalu.nal_unit_type);
		if (nalu.nal_unit_type == 14|| nalu.nal_unit_type == 20)
		{	
			uint8_t uiCurByte = *(++nalu.buf);
			uint8_t bIdrFlag = !!(uiCurByte & 0x40);
			uint8_t uiPriorityId = uiCurByte & 0x3F;

			uiCurByte = *(++nalu.buf);
			uint8_t NoInterLayerPredFlag = uiCurByte >> 7;
			uiDependencyId = (uiCurByte & 0x70) >> 4;
			uint8_t uiQualityId = uiCurByte & 0x0F;
			uiCurByte = *(++nalu.buf);
			uint8_t uiTemporalId = uiCurByte >> 5;
			uint8_t bUseRefBasePicFlag = !!(uiCurByte & 0x10);
			uint8_t bDiscardableFlag = !!(uiCurByte & 0x08);
			uint8_t bOutputFlag = !!(uiCurByte & 0x04);
			uint8_t uiReservedThree2Bits = uiCurByte & 0x03;
			uint8_t uiLayerDqId = (uiDependencyId << 4) | uiQualityId;
			cout << "uiDependencyId:"<<endl<<(int)uiDependencyId << endl;
			cout << "uiTemporalId:" << endl << (int)uiTemporalId << endl;
		}
		posp = pos;
		pospout = posout;
		pos += len;
		if (uiDependencyId != 1)
		{
			posout += len;
			memcpy(outbuffer + pospout, buffer + posp, len);
		}
		len = ReadOneNaluFromBuf(buffer, size, pos, &nalu);
		//uiDependencyId = 1;
	}
	//for (uint8_t i = 0; i < pos; i++)
	fileout.write(outbuffer, posout);
	fileout.close();

	delete[]buffer;
	return 0;
}
