#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

int maxInt(int a, int b){
	return a > b ? a : b;
}

int minInt(int a, int b){
	return a < b ? a : b;
}

unsigned char normalize(double val){
	if(val > 255){
		return 255;
	}
	if(val < 0){
		return 0;
	}
	return val;
}

struct pixelMap{
	size_t w, h;
	unsigned char *ptr;
};

struct pixelMap* constructPixelMap(struct pixelMap* src, size_t w, size_t h){
	src -> w = w;
	src -> h = h;
	src -> ptr = malloc(sizeof(char[w][h]) * 3);
	return src;
}

struct pixelMap* destructPixelMap(struct pixelMap* src){
	free(src -> ptr); // free(nullptr) does nothing
	return src;
}

struct BMPdata{
	struct pixelMap base;
};

struct BMPdata* inFile(struct BMPdata* data, char* BMPFileName){
	FILE* BMPFile = fopen(BMPFileName, "rb");
	unsigned char buf[28];
	unsigned int imgW, imgH;
	fread(buf, sizeof(char), 14, BMPFile);
	if(buf[0] * 0x100 + buf[1] != 0x424D){
		// not "BM"
		fclose(BMPFile);
		fprintf(stderr, "ERROR: not a valid BMP file.\nPress enter to exit.\n");
		getchar();
		exit(-1);
		return data;
	}
	
	fread(buf, sizeof(char), 4, BMPFile);
	fread(&imgW, sizeof(int), 1, BMPFile);
	fread(&imgH, sizeof(int), 1, BMPFile);
	fread(buf, sizeof(char), 28, BMPFile);
	constructPixelMap(&(data -> base), imgW, imgH);
	size_t lineSize = (imgW * 3 + 3) / 4 * 4, zeroSize = lineSize - imgW * 3;
	for(size_t i = 0; i < imgH; i ++){
		fread(data -> base.ptr + i * imgW * 3, sizeof(char), imgW * 3, BMPFile);
		fread(buf, sizeof(char), zeroSize, BMPFile);
	}
	fclose(BMPFile);
	return data;
}

struct BMPdata* inNew(struct BMPdata* data, size_t imgW, size_t imgH){
	constructPixelMap(&(data -> base), imgW, imgH);
	return data;
}

struct BMPdata* inCopy(struct BMPdata* data, struct BMPdata* src){
	constructPixelMap(&(data -> base), src -> base.w, src -> base.h);
	memcpy(data -> base.ptr, src -> base.ptr, sizeof(char[src -> base.w][src -> base.h]) * 3);
	return data;
}

struct BMPdata* outFile(struct BMPdata* data, char* BMPFileName){
	FILE* BMPFile = fopen(BMPFileName, "wb");
	unsigned char buf[28] = {0x42, 0x4d};
	fwrite(buf, sizeof(char), 2, BMPFile);
	unsigned int lineSize = (data -> base.w * 3 + 3) / 4 * 4, zeroSize = lineSize - data -> base.w * 3;
	unsigned int dataSize = lineSize * data -> base.h, headerSize = 54, totSize = headerSize + dataSize;
	fwrite(&totSize, sizeof(int), 1, BMPFile);
	buf[0] = buf[1] = 0x00;
	fwrite(buf, sizeof(char), 4, BMPFile);
	fwrite(&headerSize, sizeof(int), 1, BMPFile);
	unsigned int DIBSize = 0x28;
	unsigned int imgW = data -> base.w, imgH = data -> base.h;
	fwrite(&DIBSize, sizeof(int), 1, BMPFile);
	fwrite(&imgW, sizeof(int), 1, BMPFile);
	fwrite(&imgH, sizeof(int), 1, BMPFile);
	unsigned short planeCount = 1, bitCountPerPixel = 24;
	fwrite(&planeCount, sizeof(short), 1, BMPFile);
	fwrite(&bitCountPerPixel, sizeof(short), 1, BMPFile);
	fwrite(buf, sizeof(char), 24, BMPFile);
	for(size_t i = 0; i < imgH; i ++){
	//	fprintf(stderr, "i = %llu\n", i);
		fwrite(data -> base.ptr + i * imgW * 3, sizeof(char), imgW * 3, BMPFile);
		fwrite(buf, sizeof(char), zeroSize, BMPFile);
	}
	fclose(BMPFile);
	return data;
}

struct BMPdata* destructBMP(struct BMPdata* data){
	destructPixelMap(&(data -> base));
	return data;
}

#define pixelate(_src, _totw, _dh, _dw, _doffset) ((unsigned char*)((_src) + 3 * ((_totw) * (_dh) + (_dw)) + (_doffset)))
struct BMPdata* linearFilter(struct BMPdata* data, double var){
	struct pixelMap temp;
	size_t imgH = data -> base.h, imgW = data -> base.w;
	constructPixelMap(&temp, imgW, imgH);
	for(size_t indexH = 0; indexH < imgH; indexH ++){
		for(size_t indexW = 0; indexW < imgW; indexW ++){
			for(size_t offset = 0; offset < 3; offset ++){
				int count = 0, sum = 0;
				if(indexH > 0)		 {count ++; sum += *pixelate(data -> base.ptr, imgW, indexH - 1, indexW, offset);}
				if(indexH + 1 < imgH){count ++; sum += *pixelate(data -> base.ptr, imgW, indexH + 1, indexW, offset);}
				if(indexW > 0)		 {count ++; sum += *pixelate(data -> base.ptr, imgW, indexH, indexW - 1, offset);}
				if(indexW + 1 < imgW){count ++; sum += *pixelate(data -> base.ptr, imgW, indexH, indexW + 1, offset);}
				*pixelate(temp.ptr, imgW, indexH, indexW, offset) = (unsigned char)normalize(var * *pixelate(data -> base.ptr, imgW, indexH, indexW, offset)
																+ (1 - var) * sum / count + 0.5);
			}
		}
	}
	for(size_t indexH = 0; indexH < imgH; indexH ++){
		for(size_t indexW = 0; indexW < imgW; indexW ++){
			for(int offset = 0; offset < 3; offset ++){
				*pixelate(data -> base.ptr, imgW, indexH, indexW, offset) = *pixelate(temp.ptr, imgW, indexH, indexW, offset);
			}
		}
	}
	destructPixelMap(&temp);
	return data;
}

unsigned char greyScale(unsigned char R, unsigned char G, unsigned char B){
	return (unsigned char)(0.299 * R + 0.587 * G + 0.114 * B);
	// https://www.zhihu.com/question/268352346
}

void greyScaleFilter(struct BMPdata* data){
	size_t imgH = data -> base.h, imgW = data -> base.w;
	for(size_t indexH = 0; indexH < imgH; indexH ++){
		for(size_t indexW = 0; indexW < imgW; indexW ++){
			unsigned char grey = greyScale(	*pixelate(data -> base.ptr, imgW, indexH, indexW, 0),
											*pixelate(data -> base.ptr, imgW, indexH, indexW, 1),
											*pixelate(data -> base.ptr, imgW, indexH, indexW, 2));
			for(int offset = 0; offset < 3; offset ++){
				*pixelate(data -> base.ptr, imgW, indexH, indexW, offset) = grey;
			}
		}
	}
}

long long getMatchValue(int* cnt, int* lis, size_t colorCnt, int* fr){ // fr == NULL or &xxx
	int *dis = calloc(256, sizeof(int)), *tmp1 = calloc(512, sizeof(int)), *tmp2 = calloc(512, sizeof(int));
	for(size_t i = 0; i < 256; i ++){
		dis[i] = 256;
	}
	size_t cnt1 = 0, cnt2 = 0;
	for(size_t i = 0; i < colorCnt; i ++){
		tmp1[cnt1 ++] = lis[i];
		dis[lis[i]] = 0;
		if(fr){
			fr[lis[i]] = i;
		}
	}
	while(cnt1 > 0){
		for(size_t i = 0; i < cnt1; i ++){
			tmp2[i] = tmp1[i];
			tmp1[i] = 0;
		}
		cnt2 = cnt1;
		cnt1 = 0;
		for(size_t i = 0; i < cnt2; i ++){
			if(tmp2[i] > 0){
				int upd = tmp2[i] - 1;
				if(dis[upd] > dis[tmp2[i]] + 1){
					dis[upd] = dis[tmp2[i]] + 1;
					if(fr){
						fr[upd] = fr[tmp2[i]];
					}
					tmp1[cnt1 ++] = upd;
				}
			}
			if(tmp2[i] < 256 - 1){
				int upd = tmp2[i] + 1;
				if(dis[upd] > dis[tmp2[i]] + 1){
					dis[upd] = dis[tmp2[i]] + 1;
					if(fr){
						fr[upd] = fr[tmp2[i]];
					}
					tmp1[cnt1 ++] = upd;
				}
			}
		}
	}
	long long ans = 0;
	for(size_t i = 0; i < 256; i ++){
		ans += dis[i] * (long long)cnt[i];
	}
	free(dis);
	free(tmp1);
	free(tmp2);
	return ans;
}

long long randomMonoNearestFilter(struct BMPdata* data, size_t offset, size_t colorCnt, size_t maxGenerateCnt, size_t maxRetryCnt){ // offset: 0R 1G 2B
	if(offset > 3 || colorCnt > 255 || colorCnt == 0){
		fprintf(stderr, "args invalid in randomMonoNearestFilter().\n");
		return 0;
	}
	size_t imgH = data -> base.h, imgW = data -> base.w;
	int *cnt = calloc(256, sizeof(int));
	int *buc = calloc(256, sizeof(int));
	int *lis = malloc(sizeof(int) * colorCnt);

	for(size_t indexH = 0; indexH < imgH; indexH ++){
		for(size_t indexW = 0; indexW < imgW; indexW ++){
		//	unsigned char* px = pixelate(data -> base.ptr, imgW, indexH, indexW, offset);
		//	fprintf(stderr, "%p\n", px);
		//	fprintf(stderr, "%p\n", data -> base.ptr);
		//	fprintf(stderr, "nh = %llu, nw = %llu\n", indexH, indexW);
			cnt[*pixelate(data -> base.ptr, imgW, indexH, indexW, offset)] ++;
		}
	}

	int *bucTemp = calloc(256, sizeof(int));
	long long valTemp = 10000000000000000ll;
	int bestIdx = 0;
	for(size_t genCnt = 0; genCnt < maxGenerateCnt; genCnt ++){
		for(size_t ncnt = 0; ncnt < colorCnt; ncnt ++){
			size_t idx;
			for(idx = rand() & 0xff; bucTemp[idx] == (int)genCnt + 1; idx = rand() & 0xff){;}
			bucTemp[idx] = genCnt + 1;
		}
		for(size_t i = 0, ccnt = 0; i < 256; i ++){
			if((size_t)bucTemp[i] == genCnt + 1){
				lis[ccnt ++] = i;
			}
		}
		long long nowTemp = getMatchValue(cnt, lis, colorCnt, NULL);
		if(nowTemp < valTemp){
			valTemp = nowTemp;
			memcpy(buc, bucTemp, 256 * sizeof(int));
			bestIdx = genCnt;
		}
	}
	for(size_t i = 0, ccnt = 0; i < 256; i ++){
		if(buc[i] == bestIdx + 1){
			lis[ccnt ++] = i;
		}
	}
	

	int DEBUG_success_count = 0;
	long long nowMatchValue = getMatchValue(cnt, lis, colorCnt, NULL);
	for(size_t tms = 0; tms < maxRetryCnt; tms ++){
		unsigned idx = rand() % colorCnt;
		int canl = maxInt(0, idx > 0 ? lis[idx - 1] + 1 : 0), canr = minInt(255, idx + 1 < colorCnt ? lis[idx + 1] - 1 : 255);
		if(canl == canr){
			continue;
		}
		int newpos = rand() % (canr - canl) + canl, oldpos = lis[idx];
		if(newpos >= oldpos){
			newpos ++;
		}
		if(newpos > oldpos){
			newpos = (newpos - oldpos + 1) / 2 + oldpos;
		}else{
			newpos = (newpos - oldpos - 1) / 2 + oldpos;
		}
		lis[idx] = newpos;
		long long newMatchValue = getMatchValue(cnt, lis, colorCnt, NULL);
		if(newMatchValue > nowMatchValue){
			lis[idx] = oldpos;
		}else{
			nowMatchValue = newMatchValue;
			DEBUG_success_count ++;
		//	fprintf(stderr, "[%llu] success\n", tms);
		}
	}
	fprintf(stderr, "%d success / %d\n", DEBUG_success_count, (int)maxRetryCnt);
	int *fr = calloc(256, sizeof(int));
	getMatchValue(cnt, lis, colorCnt, fr);
//	for(int i = 0; i < 256; i ++){
//		fprintf(stderr, "%d%c", fr[i], " \n"[i == 255]);
//	}
	for(size_t indexH = 0; indexH < imgH; indexH ++){
		for(size_t indexW = 0; indexW < imgW; indexW ++){
			*pixelate(data -> base.ptr, imgW, indexH, indexW, offset) = lis[fr[*pixelate(data -> base.ptr, imgW, indexH, indexW, offset)]];
		}
	}

	free(cnt);
	free(buc);
	free(bucTemp);
	free(lis);
	free(fr);
	return nowMatchValue;
}

// TODO: 把 randomMonoNearestFilter 里的移动改成区域内随机，也许会好一点

#undef pixelate

int main(){
	printf("Guide:\n  1. open the image with mspaint\n  2. save the image as a 24-bit BMP picture named \"image.bmp\"\n  3. run this program\n\n");
	printf("Due to various reasons, the result may be not optimal.\nRun it several times to get a better result.\n");


	struct BMPdata aa, bb, cc;
	inFile(&aa, "image.bmp");
	inCopy(&bb, &aa);
	inCopy(&cc, &aa);


	int colorCnt = 4;
	printf("Enter Color count (2 ~ 8):");
	scanf("%d", &colorCnt);

	/*
	int steps;
	printf("Enter Steps(1 ~ 500):");
	scanf("%d", &steps);

	double lossRate;
	printf("Enter lossRate(-1 ~ 1):");
	scanf("%lf", &lossRate);

	for(int i = 0; i < steps; i ++){
		linearFilter(&aa, 1 - lossRate);
	}
	*/
	
	srand(time(0));
	{
		long long edv = 0;
		edv += randomMonoNearestFilter(&aa, 0, colorCnt, 10, 7000);
		edv += randomMonoNearestFilter(&aa, 1, colorCnt, 10, 7000);
		edv += randomMonoNearestFilter(&aa, 2, colorCnt, 10, 7000);
		fprintf(stderr, "val() = %lld\n", edv);
	}
	{
		long long edv = 0;
		edv += randomMonoNearestFilter(&bb, 0, colorCnt, 10, 7000);
		edv += randomMonoNearestFilter(&bb, 1, colorCnt, 10, 7000);
		edv += randomMonoNearestFilter(&bb, 2, colorCnt, 10, 7000);
		fprintf(stderr, "val() = %lld\n", edv);
	}
	{
		long long edv = 0;
		edv += randomMonoNearestFilter(&cc, 0, colorCnt, 10, 7000);
		edv += randomMonoNearestFilter(&cc, 1, colorCnt, 10, 7000);
		edv += randomMonoNearestFilter(&cc, 2, colorCnt, 10, 7000);
		fprintf(stderr, "val() = %lld\n", edv);
	}
	
	outFile(&aa, "image2.bmp");
	outFile(&bb, "image3.bmp");
	outFile(&cc, "image4.bmp");
	destructBMP(&aa);
	destructBMP(&bb);
	destructBMP(&cc);
}
