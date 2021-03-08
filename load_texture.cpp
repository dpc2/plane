/*
 * load_texture.cpp
 *
 *  Created on: 2018年11月22日
 *      Author: x
 */

#include "h.h"

#define	BMP_Header_Length	54		  	//图像数据在内存块中的偏移量

int power_of_two(int n)        // 函数power_of_two用于判断一个整数是不是2的整数次幂
{
	if(n <= 0)
		return 0;
	return (n & (n - 1)) == 0;
}
unsigned char * load_imagine(const char * file_name, int * width, int * height, GLint * total_bytes)
{
//	LOG("file_name: %s", file_name);
	// 打开文件，如果失败，返回
	FILE* pFile = fopen(file_name, "rb");
	if(NULL == pFile)
	{
		LOG();
		exit(1);
	}

	// 读取文件中图象的宽度和高度
	fseek(pFile, 0x0012, SEEK_SET);
	fread(width, 4, 1, pFile);
	fread(height, 4, 1, pFile);
	fseek(pFile, BMP_Header_Length, SEEK_SET);

	// 计算每行像素所占字节数，并根据此数据计算总像素字节数
	{
		GLint line_bytes = *width * 3;
		while(line_bytes % 4 != 0)
			++line_bytes;
		*total_bytes = line_bytes * *height;
	}

	// 根据总像素字节数分配内存
	unsigned char *pixels = (GLubyte*) calloc(*total_bytes, 1);
	if(NULL == pixels)
	{
		LOG();
		exit(1);
	}

	// 读取像素数据
	int r;
	if((r = fread(pixels, 1, *total_bytes, pFile)) <= 0)
	{
		LOG("r: %d", r);
		perror("err:");
		exit(1);
	}
//	LOG("*total_bytes: %d, r: %d", *total_bytes, r);
	fclose(pFile);

	// 对就旧版本的兼容，如果图象的宽度和高度不是的整数次方，则需要进行缩放
	// 若图像宽高超过了OpenGL规定的最大值，也缩放
	{
		GLint max;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);
		if(!power_of_two(*width)
				|| !power_of_two(*height)
				|| *width > max
				|| *height > max)
		{
			LOG("file_name: %s, *width: %d, *height: %d, *total_bytes: %d, max: %d", file_name, *width, *height,
					*total_bytes, max);

			const GLint new_width = 256;
			const GLint new_height = 256;        // 规定缩放后新的大小为边长的正方形
			GLint new_line_bytes, new_total_bytes;
			GLubyte* new_pixels = 0;

			// 计算每行需要的字节数和总字节数
			new_line_bytes = new_width * 3;
			while(new_line_bytes % 4 != 0)
				++new_line_bytes;
			new_total_bytes = new_line_bytes * new_height;

			new_pixels = (GLubyte*) calloc(new_total_bytes, 1);
			if(NULL == new_pixels)
			{
				LOG();
				exit(1);
			}

			gluScaleImage(GL_RGBA,
					*width, *height, GL_UNSIGNED_BYTE, pixels,
					new_width, new_height, GL_UNSIGNED_BYTE, new_pixels);

			// 释放原来的像素数据，把pixels指向新的像素数据，并重新设置width和height
			*width = new_width;
			*height = new_height;
			*total_bytes = new_total_bytes;

			free(pixels);
//			pixels = new_pixels;
			return new_pixels;
		}
	}

	return pixels;
}
GLuint load_texture(const char* file_name)
{
	if(!file_name || !strlen(file_name))
	{
		LOG("file_name is empty");
		return 0;
	}
	LOG("file_name: %s", file_name);

	GLint width, height, total;
	GLubyte * pixels;
	GLenum format;
	pixels = load_imagine(file_name, &width, &height, &total);
//	pixels = LoadRGBImage(file_name, &width, &height, &format);
	if(NULL == pixels)
	{
		LOG();
		free(pixels);
		exit(1);
	}

	GLuint texture_id = 0;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);

#if 0
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);        // GL_REPEAT GL_CLAMP
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);        // GL_CLAMP
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
//	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);		// GL_MODULATE  GL_REPLACE

//	glTexImage2D(GL_TEXTURE_2D, 2, GL_RGB,
//			width, height, 0,
//			GL_RGB, GL_UNSIGNED_BYTE, pixels);

	gluBuild2DMipmaps(GL_TEXTURE_2D, 3,
			width, height, GL_RGB,
			GL_UNSIGNED_BYTE, pixels);
#else
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);        //GL_NEAREST
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);		// GL_DECAL、GL_REPLACE、GL_MODULATE、GL_BLEND、GL_ADD、GL_COMBINE
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
#endif

	free(pixels);
	LOG("texture_id: %d", texture_id);
	return texture_id;
}

