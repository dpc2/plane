/*
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <signal.h>
#include <poll.h>
#include <fcntl.h>
#include <inttypes.h>
#include <fnmatch.h>
#include <getopt.h>

#include <sys/wait.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <GL/glut.h>
////#include <GL/glext.h>
#include <linux/input.h>
//#include <libinput.h>
//#include <libudev.h>
//#include <libevdev-1.0/libevdev/libevdev.h>
#include <fstream>

using namespace std;

#define	BMP_Header_Length	54		  	//图像数据在内存块中的偏移量
#define	MAX					500     	// LENGTH AND W_idTH OF MATRIX(2D array)
#define	MAX_PARTICLES		250			// Number Of Particles To Create

#ifndef LOG
#	define LOG(fmt, args...)			do{	\
		printf("--%-40s--%5d--%-10s--" fmt "\n", __FUNCTION__, __LINE__, __FILE__, ##args);	\
		}while(0)
#endif

typedef unsigned char byte;
typedef unsigned short word;
typedef struct						// Create A Structure For Particle
{
	bool active;					// Active (Yes/No)
	GLfloat life;					// Particle Life
	GLfloat fade;					// Fade Speed
	GLfloat r;						// Red Value
	GLfloat g;						// Green Value
	GLfloat b;						// Blue Value
	GLfloat x;						// X Position
	GLfloat y;						// Y Position
	GLfloat z;						// Z Position
	GLfloat xi;						// X Direction
	GLfloat yi;						// Y Direction
	GLfloat zi;						// Z Direction
	GLfloat xg;						// X Gravity
	GLfloat yg;						// Y Gravity
	GLfloat zg;						// Z Gravity
} particles;							// Particles Structure

class Model
{
private:
	struct Mesh
	{
		int m_materialIndex;
		int m_numTriangles;
		int *m_pTriangleIndices;
	};
	struct Material
	{
		GLfloat m_ambient[4], m_diffuse[4], m_specular[4], m_emissive[4];
		GLfloat m_shininess;
		GLuint m_texture_id;
		char *m_pTextureFilename;
	};
	struct Triangle
	{
		GLfloat m_vertexNormals[3][3];
		GLfloat m_s[3], m_t[3];
		int m_vertexIndices[3];
	};
	struct Vertex
	{
		char m_bone_id;        // for skeletal animation
		GLfloat m_location[3];
	};

	int m_graw_list;

protected:
	int m_numMeshes;
	Mesh *m_pMeshes;

	int m_numMaterials;
	Material *m_pMaterials;

	int m_numTriangles;
	Triangle *m_pTriangles;

	int m_numVertices;
	Vertex *m_pVertices;

public:
	Model(const char *filename);
	virtual ~Model();
	bool load_model_data(const char *filename);
	int makeDisplayList();
	void draw(void);
};

int power_of_two(int n);
unsigned char * load_imagine(const char * file_name, int * width, int * height, GLint * total_bytes);
GLuint load_texture(const char* file_name);
GLuint mod_load_texture(const char* file_name);
void load_all_textures();

GLboolean LoadRGBMipmaps(const char *imageFile, GLint intFormat);
GLboolean LoadRGBMipmaps2(const char *imageFile, GLenum target, GLint intFormat, GLint *width, GLint *height);
GLubyte * LoadRGBImage(const char *imageFile, GLint *width, GLint *height, GLenum *format);
GLushort * LoadYUVImage(const char *imageFile, GLint *width, GLint *height);

/*************************************************************************************************************************/
/*************************************************************************************************************************/

/*
 * load_texture.cpp
 *
 *  Created on: 2018年11月22日
 *      Author: x
 */

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
		exit(1);
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

/*************************************************************************************************************************/

/*
 * Model.cpp
 *
 *  Created on: 2018年11月22日
 *      Author: x
 */

Model::Model(const char *filename)
{
	m_numMeshes = 0;
	m_pMeshes = NULL;
	m_numMaterials = 0;
	m_pMaterials = NULL;
	m_numTriangles = 0;
	m_pTriangles = NULL;
	m_numVertices = 0;
	m_pVertices = NULL;
	m_graw_list = 0;

	load_model_data(filename);
	m_graw_list = makeDisplayList();
}
Model::~Model()
{
	int i;
	for(i = 0; i < m_numMeshes; i++)
		delete[] m_pMeshes[i].m_pTriangleIndices;

	m_numMeshes = 0;
	if(m_pMeshes != NULL)
	{
		delete[] m_pMeshes;
		m_pMeshes = NULL;
	}

	m_numMaterials = 0;
	if(m_pMaterials != NULL)
	{
		delete[] m_pMaterials;
		m_pMaterials = NULL;
	}

	m_numTriangles = 0;
	if(m_pTriangles != NULL)
	{
		delete[] m_pTriangles;
		m_pTriangles = NULL;
	}

	m_numVertices = 0;
	if(m_pVertices != NULL)
	{
		delete[] m_pVertices;
		m_pVertices = NULL;
	}
}
/*
 MS3D STRUCTURES
 */
// byte-align structures
#ifdef _MSC_VER
#	pragma pack( push, packing )
#	pragma pack( 1 )
#	define PACK_STRUCT
#elif defined( __GNUC__ )
#	define PACK_STRUCT	__attribute__((packed))
#else
#	error you must byte-align these structures with the appropriate compiler directives
#endif

struct ms3d_file_header_t
{
	char m__id[10];
	int m_version;
}PACK_STRUCT;
struct MS3DVertex
{
	byte m_flags;
	GLfloat m_vertex[3];
	char m_bone_id;
	byte m_refCount;
}PACK_STRUCT;
struct MS3DTriangle
{
	word m_flags;
	word m_vertexIndices[3];
	GLfloat m_vertexNormals[3][3];
	GLfloat m_s[3], m_t[3];
	byte m_smoothingGroup;
	byte m_groupIndex;
}PACK_STRUCT;
struct MS3DMaterial
{
	char m_name[32];
	GLfloat m_ambient[4];
	GLfloat m_diffuse[4];
	GLfloat m_specular[4];
	GLfloat m_emissive[4];
	GLfloat m_shininess;        // 0.0f - 128.0f
	GLfloat m_transparency;        // 0.0f - 1.0f
	byte m_mode;        // 0, 1, 2 is unused now
	char m_texture[128];
	char m_alphamap[128];
}PACK_STRUCT;
struct MS3DJoint
{
	byte m_flags;
	char m_name[32];
	char m_parentName[32];
	GLfloat m_rotation[3];
	GLfloat m_translation[3];
	word m_numRotationKeyframes;
	word m_numTranslationKeyframes;
}PACK_STRUCT;
struct MS3DKeyframe
{
	GLfloat m_time;
	GLfloat m_parameter[3];
}PACK_STRUCT;
// Default alignment
#ifdef _MSC_VER
#	pragma pack( pop, packing )
#endif
#undef PACK_STRUCT

bool Model::load_model_data(const char *filename)
{
	ifstream inputFile(filename, ios::in | ios::binary);
	if(inputFile.fail())
	{
		LOG();
		exit(1);        // "Couldn't open the model file."
	}

	inputFile.seekg(0, ios::end);
	long fileSize = inputFile.tellg();
	inputFile.seekg(0, ios::beg);

	byte *buf = new byte[fileSize];
	inputFile.read((char*) buf, fileSize);
	inputFile.close();

	/***************************************************************************/

	const byte *p = buf;
	ms3d_file_header_t *pHeader = (ms3d_file_header_t*) p;
	if(strncmp(pHeader->m__id, "MS3D000000", 10) != 0)
	{
		LOG();
		exit(1);        // "Not a valid Milkshape3D model file."
	}
	if(pHeader->m_version < 3 || pHeader->m_version > 4)
	{
		LOG("pHeader->m_version: %d", pHeader->m_version);
		exit(1);        // "Unhandled file version. Only Milkshape3D Version 1.3 and 1.4 is supported." );
	}

	p += sizeof(ms3d_file_header_t);
	int nVertices = *(word*) p;
	m_numVertices = nVertices;
	m_pVertices = new Vertex[nVertices];
	p += sizeof(word);

	for(int i = 0; i < nVertices; i++)
	{
		MS3DVertex *pVertex = (MS3DVertex*) p;
		m_pVertices[i].m_bone_id = pVertex->m_bone_id;
		memcpy(m_pVertices[i].m_location, pVertex->m_vertex, sizeof(GLfloat) * 3);
		p += sizeof(MS3DVertex);
	}

	int nTriangles = *(word*) p;
	m_numTriangles = nTriangles;
	m_pTriangles = new Triangle[nTriangles];
	p += sizeof(word);

	for(int i = 0; i < nTriangles; i++)
	{
		MS3DTriangle *pTriangle = (MS3DTriangle*) p;
		int vertexIndices[3] = {pTriangle->m_vertexIndices[0], pTriangle->m_vertexIndices[1],
				pTriangle->m_vertexIndices[2]};
		GLfloat t[3] = {1.0f - pTriangle->m_t[0], 1.0f - pTriangle->m_t[1], 1.0f - pTriangle->m_t[2]};
		memcpy(m_pTriangles[i].m_vertexNormals, pTriangle->m_vertexNormals, sizeof(GLfloat) * 3 * 3);
		memcpy(m_pTriangles[i].m_s, pTriangle->m_s, sizeof(GLfloat) * 3);
		memcpy(m_pTriangles[i].m_t, t, sizeof(GLfloat) * 3);
		memcpy(m_pTriangles[i].m_vertexIndices, vertexIndices, sizeof(int) * 3);
		p += sizeof(MS3DTriangle);
	}

	int nGroups = *(word*) p;
	m_numMeshes = nGroups;
	m_pMeshes = new Mesh[nGroups];
	p += sizeof(word);
	for(int i = 0; i < nGroups; i++)
	{
		p += sizeof(byte);        // flags
		p += 32;				// name

		word nTriangles = *(word*) p;
		p += sizeof(word);
		int *pTriangleIndices = new int[nTriangles];
		for(int j = 0; j < nTriangles; j++)
		{
			pTriangleIndices[j] = *(word*) p;
			p += sizeof(word);
		}

		char materialIndex = *(char*) p;
		p += sizeof(char);

		m_pMeshes[i].m_materialIndex = materialIndex;
		m_pMeshes[i].m_numTriangles = nTriangles;
		m_pMeshes[i].m_pTriangleIndices = pTriangleIndices;
	}

//	const char * bmp[7] = {"asphalt.bmp", "marsh2.bmp", "nebula.bmp", "sand.bmp", "sky.bmp", "sky.bmp", "sky.bmp", "water.bmp"};
	const char * bmp[7] = {
			"",
			/*机翼*/"sand.bmp",
			"",
			/*机身、机头*/"",
			/*尾翼*/"sand.bmp",
			/*机仓*/"",
			""};

	int nMaterials = *(word*) p;
	m_numMaterials = nMaterials;
	m_pMaterials = new Material[nMaterials];
	p += sizeof(word);
	for(int i = 0; i < nMaterials; i++)
	{
		MS3DMaterial *pMaterial = (MS3DMaterial*) p;
		memcpy(m_pMaterials[i].m_ambient, pMaterial->m_ambient, sizeof(GLfloat) * 4);
		memcpy(m_pMaterials[i].m_diffuse, pMaterial->m_diffuse, sizeof(GLfloat) * 4);
		memcpy(m_pMaterials[i].m_specular, pMaterial->m_specular, sizeof(GLfloat) * 4);
		memcpy(m_pMaterials[i].m_emissive, pMaterial->m_emissive, sizeof(GLfloat) * 4);
		m_pMaterials[i].m_shininess = pMaterial->m_shininess;

		if(0)
		{
			m_pMaterials[i].m_pTextureFilename = (char *) calloc(1, strlen(pMaterial->m_texture) + 1);
			strcpy(m_pMaterials[i].m_pTextureFilename, pMaterial->m_texture);

			if(strlen(m_pMaterials[i].m_pTextureFilename) >= 2)
			{
				m_pMaterials[i].m_pTextureFilename[1] = '/';
			}
			else
			{
				m_pMaterials[i].m_pTextureFilename[0] = '\0';
			}
			//			LOG("i: %d, strlen(m_texture): %d, m_texture: %s", i, strlen(pMaterial->m_texture), pMaterial->m_texture);
		}
		else
		{
			m_pMaterials[i].m_pTextureFilename = (char *) calloc(1, 50);
			strcpy(m_pMaterials[i].m_pTextureFilename, bmp[i]);        // (sizeof(bmp) / sizeof(bmp[0]))
		}
		LOG("i: %d, strlen(): %d, m_pTextureFilename: %s", i, strlen(m_pMaterials[i].m_pTextureFilename),
				m_pMaterials[i].m_pTextureFilename);

		m_pMaterials[i].m_texture_id = mod_load_texture(m_pMaterials[i].m_pTextureFilename);
		p += sizeof(MS3DMaterial);
	}

	delete[] buf;
	return true;
}
int Model::makeDisplayList()
{
	int list = glGenLists(1);
	glNewList(list, GL_COMPILE);							// Start With The Box List

//	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);			// 机翼缺口
	glEnable(GL_LIGHTING);        // must
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);        // must
	bool a = glIsEnabled(GL_TEXTURE_2D);

	LOG("m_numMeshes: %d", m_numMeshes);
	for(int i = 0; i < m_numMeshes; i++)
	{
		glDisable(GL_TEXTURE_2D);

		int materialIndex = m_pMeshes[i].m_materialIndex;
		if(materialIndex >= 0)
		{
			glMaterialfv(GL_FRONT, GL_AMBIENT, m_pMaterials[materialIndex].m_ambient);
			glMaterialfv(GL_FRONT, GL_DIFFUSE, m_pMaterials[materialIndex].m_diffuse);
			glMaterialfv(GL_FRONT, GL_SPECULAR, m_pMaterials[materialIndex].m_specular);
			glMaterialfv(GL_FRONT, GL_EMISSION, m_pMaterials[materialIndex].m_emissive);
			glMaterialf(GL_FRONT, GL_SHININESS, m_pMaterials[materialIndex].m_shininess);

			if(m_pMaterials[materialIndex].m_texture_id > 0)
			{
				glEnable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, m_pMaterials[materialIndex].m_texture_id);
			}
		}

		glBegin(GL_TRIANGLES);
		{
			for(int j = 0; j < m_pMeshes[i].m_numTriangles; j++)
			{
				int triangleIndex = m_pMeshes[i].m_pTriangleIndices[j];
				const Triangle* pTri = &m_pTriangles[triangleIndex];

				for(int k = 0; k < 3; k++)
				{
					int index = pTri->m_vertexIndices[k];

					glNormal3fv(pTri->m_vertexNormals[k]);
					glTexCoord2f(pTri->m_s[k], pTri->m_t[k]);
					glVertex3fv(m_pVertices[index].m_location);
				}
			}
		}
		glEnd();
	}

	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	if(a)
	{
		glEnable(GL_TEXTURE_2D);
	}
	else
	{
		glDisable(GL_TEXTURE_2D);
	}

	glEndList();
//	m_graw_list = list;
	return list;
}
void Model::draw(void)
{
#if(0)
	glCallList(m_graw_list);
#else

	//	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);			// 机翼缺口
	glEnable(GL_LIGHTING);        // must
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);        // must
	bool a = glIsEnabled(GL_TEXTURE_2D);

//	LOG("m_numMeshes: %d", m_numMeshes);
	for(int i = 0; i < m_numMeshes; i++)
	{
		glDisable(GL_TEXTURE_2D);

		int materialIndex = m_pMeshes[i].m_materialIndex;
		if(materialIndex >= 0)
		{
			glMaterialfv(GL_FRONT, GL_AMBIENT, m_pMaterials[materialIndex].m_ambient);
			glMaterialfv(GL_FRONT, GL_DIFFUSE, m_pMaterials[materialIndex].m_diffuse);
			glMaterialfv(GL_FRONT, GL_SPECULAR, m_pMaterials[materialIndex].m_specular);
			glMaterialfv(GL_FRONT, GL_EMISSION, m_pMaterials[materialIndex].m_emissive);
			glMaterialf(GL_FRONT, GL_SHININESS, m_pMaterials[materialIndex].m_shininess);

			if(m_pMaterials[materialIndex].m_texture_id > 0)
			{
				glEnable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, m_pMaterials[materialIndex].m_texture_id);
			}
		}

		glBegin(GL_TRIANGLES);
		{
			for(int j = 0; j < m_pMeshes[i].m_numTriangles; j++)
			{
				int triangleIndex = m_pMeshes[i].m_pTriangleIndices[j];
				const Triangle* pTri = &m_pTriangles[triangleIndex];

				for(int k = 0; k < 3; k++)
				{
					int index = pTri->m_vertexIndices[k];

					glNormal3fv(pTri->m_vertexNormals[k]);
					glTexCoord2f(pTri->m_s[k], pTri->m_t[k]);
					glVertex3fv(m_pVertices[index].m_location);
				}
			}
		}
		glEnd();
	}

	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	if(a)
	{
		glEnable(GL_TEXTURE_2D);
	}
	else
	{
		glDisable(GL_TEXTURE_2D);
	}
#endif
}

GLuint mod_load_texture(const char* file_name)
{
	if(!file_name || !strlen(file_name))
	{
		LOG("file_name is empty");
		return 0;
	}
	LOG("file_name: %s", file_name);

	GLint width, height, total;
	GLubyte * pixels = load_imagine(file_name, &width, &height, &total);
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
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, 3, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
//	gluBuild2DMipmaps(GL_TEXTURE_2D, 3,
//			width, height, GL_RGB,
//			GL_UNSIGNED_BYTE, pixels);
#else
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	// must ONE OF GL_REPLACE、GL_MODULATE
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);		// GL_REPLACE、GL_MODULATE、GL_BLEND、GL_ADD、GL_COMBINE, GL_DECAL

//	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
//			width, height, 0, GL_RGB,
//			GL_UNSIGNED_BYTE, pixels);

	gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB,
			width, height, GL_RGB,
			GL_UNSIGNED_BYTE, pixels);
#endif

	free(pixels);
	LOG("texture_id: %d", texture_id);
	return texture_id;
}

/*************************************************************************************************************************/
/*************************************************************************************************************************/
/*************************************************************************************************************************/

int w = 640;
int h = 480;

GLfloat visual_distance = 150;
int quality = 8;
float high_low_of_land = 0.15;        // 0.02 ~ 0.2 is not bad
float plane_default_hight = -15;

Model * plane;

bool key_status_GLUT_KEY_LEFT;
bool key_status_GLUT_KEY_RIGHT;
bool key_status_GLUT_KEY_UP;
bool key_status_GLUT_KEY_DOWN;
bool key_status_GLUT_KEY_HOME;
bool key_status_GLUT_KEY_END;
bool key_status_GLUT_KEY_PAGE_DOWN;

// bool light = true;  				// Lighting ON/OFF
bool wireframe = false;
bool water = true;
bool after_burner = true;		// jet burner color
bool status_pause = false;

//GLUquadricObj *quadratic;        	// Storage For Our Quadratic Objects ( NEW )
GLuint texture[8];
GLuint MODEL;

GLfloat V;
GLfloat Angle;

particles particle[MAX_PARTICLES];        // Particle Array (Room For Particle Info)

static const GLfloat colors[12][3] =        // Rainbow Of Colors
		{
				{1.0f, 0.5f, 0.5f}, {1.0f, 0.75f, 0.5f}, {1.0f, 1.0f, 0.5f}, {0.75f, 1.0f, 0.5f},
				{0.5f, 1.0f, 0.5f}, {0.5f, 1.0f, 0.75f}, {0.5f, 1.0f, 1.0f}, {0.5f, 0.75f, 1.0f},
				{0.5f, 0.5f, 1.0f}, {0.75f, 0.5f, 1.0f}, {1.0f, 0.5f, 1.0f}, {1.0f, 0.5f, 0.75f}
		};

const GLfloat LightAmbient[] = {0.5f, 0.5f, 0.5f, 1.0f};
const GLfloat LightDiffuse[] = {1.0f, 1.0f, 1.0f, 1.0f};
const GLfloat LightSpecular[] = {0.5f, 0.5f, 0.5f, 1.0f};
const GLfloat LightPosition[] = {0.0f, 0.0f, 0.0f, 1.0f};

const float fog_r = 211.f / 255.f;
const float fog_g = 237.f / 255.f;
const float fog_b = 254.f / 255.f;

GLuint fogMode[] = {GL_EXP, GL_EXP2, GL_LINEAR};        // Storage For Three Types Of Fog
GLfloat fogColor[4] = {fog_r, fog_g, fog_b, 1};        // Fog Color

struct vertex
{
	GLfloat x, y, z, light;
};
vertex field[MAX + 9][MAX + 9];

GLfloat xtrans = MAX / 2;
GLfloat ytrans = 0;
GLfloat ztrans = MAX / 2;

const GLfloat sun_height = 1000;
const GLfloat sun_zdistance = -5000;

//#include "load_texture.cpp"
/*
 * load_texture.cpp
 *  Created on: 2018年11月21日
 *      Author: x
 */

void load_all_textures()
{
	LOG();
	texture[0] = load_texture("sand.bmp");        // 地面 green.bmp sand.bmp
	texture[1] = load_texture("sky.bmp");        // sky
	texture[5] = load_texture("nebula.bmp");		// plane jet, nebula.bmp
	texture[6] = load_texture("water.bmp");        // water

	texture[2] = load_texture("sand.bmp");        // ???
	texture[3] = load_texture("sand.bmp");        // DrawProgress ???
	texture[4] = load_texture("nebula.bmp");        // nothing
	texture[7] = load_texture("nebula.bmp");        // sun
}

GLfloat Hypot(GLfloat A, GLfloat B)
{
	return sqrt(A * A + B * B);
}
GLfloat ABS(GLfloat A)
{
	if(A < 0)
		A = -A;
	return A;
}
void reshape(int width, int height)        // Resize And Initialize The GL Window
{
	if(height == 0)										// Prevent A Divide By Zero By
	{
		height = 1;										// Making Height Equal One
	}

	glViewport(0, 0, width, height);						// Reset The Current Viewport
	w = width;
	h = height;

	glMatrixMode(GL_PROJECTION);						// Select The Projection Matrix
	glLoadIdentity();									// Reset The Projection Matrix

	gluPerspective(45.0f, (GLfloat) width / (GLfloat) height, 0.9f, 50000.0f);
	glMatrixMode(GL_MODELVIEW);							// Select The Modelview Matrix
	glLoadIdentity();									// Reset The Modelview Matrix
}
//static void RestoreMyDefaultSettings()
//{
//	glEnable(GL_DEPTH_TEST);
//	glEnable(GL_TEXTURE_2D);
//	glEnable(GL_ALPHA_TEST);
//	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
////	glEnable(GL_FOG);
//}
static int init(void)
{
	for(int i = 0; i < MAX_PARTICLES; i++)
	{
		particle[i].active = true;						// Make All The Particles Active
		particle[i].life = 1.0f;					// Give It New Life
		particle[i].fade = GLfloat(rand() % 100) / 7500 + 0.0075f;        // Random Fade Value
		if(i < MAX_PARTICLES / 2)
			particle[i].x = 0.75f;
		else
			particle[i].x = -.75f;
		particle[i].y = -.15;						// Center On Y Axis
		particle[i].z = 3;							// Center On Z Axis
		V = (GLfloat((rand() % 5)) + 2) / 5;
		Angle = GLfloat(rand() % 360);
		particle[i].zg = 0.15;
		particle[i].xi = sin(Angle) * V;
		particle[i].yi = cos(Angle) * V;
		particle[i].zi = ((rand() % 10) - 5) / 5;						//V*5;// +(oldzp/8);
	}

//	RestoreMyDefaultSettings();
	glClearColor(fog_r, fog_g, fog_b, 1);			// Black Background
	glClearDepth(1.0f);		   							// Depth Buffer Setup
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);        // Really Nice Perspective Calculations

	glLightfv(GL_LIGHT1, GL_AMBIENT, LightAmbient);        // Setup The Ambient Light
	glLightfv(GL_LIGHT1, GL_DIFFUSE, LightDiffuse);        // Setup The Diffuse Light
	glLightfv(GL_LIGHT1, GL_SPECULAR, LightSpecular);        // Setup The Specular Light
	glLightfv(GL_LIGHT1, GL_POSITION, LightPosition);        // Position The Light
	glEnable(GL_LIGHT1);								// Enable Light One

//	glFogi(GL_FOG_MODE, fogMode[2]);			        // Fog Mode
//	glFogfv(GL_FOG_COLOR, fogColor);					// Set Fog Color
//	glFogf(GL_FOG_DENSITY, 0.294f);						// How Dense Will The Fog Be
//	glHint(GL_FOG_HINT, GL_NICEST);					    // Fog Hint Value
//	glFogf(GL_FOG_START, visual_distance * 0.8);						// Fog Start Depth
//	glFogf(GL_FOG_END, visual_distance);							// Fog End Depth
//	glEnable(GL_FOG);									// Enables GL_FOG

//	quadratic = gluNewQuadric();				// Create A Pointer To The Quadric Object (Return 0 If No Memory) (NEW)
//	gluQuadricNormals(quadratic, GLU_SMOOTH);			// Create Smooth Normals (NEW)
//	gluQuadricTexture(quadratic, true);				// Create Texture Coords (NEW)

	glEnable(GL_TEXTURE_2D);
	load_all_textures();

	field[0][0].y = (GLfloat(rand() % 100) - 50) / 3;

//  GENERATE TERRAIN (IF YOU MODIFY THE SHAPE OF THE TERRAIN, YOU SHOULD RECALCULATE THE LIGHTMAP)
	int i, i2;
	for(i = 0; i < MAX; i++)
	{
		for(i2 = 0; i2 < MAX; i2++)
		{
			if(i < 10 || i2 < 10 || i > MAX - 10 || i2 > MAX - 10)
				field[i][i2].y = 0;
			else
				field[i][i2].y = (GLfloat(rand() % 151) - 75) * high_low_of_land
						+ (field[i - 1][i2 - 1].y + field[i - 1][i2].y + field[i - 1][i2 + 1].y + field[i - 1][i2 - 2].y
								+ field[i - 1][i2 + 2].y) / 5.05;        //Calculate the y coordinate on the same principle.
		}
	}
// SMOOTH TERRAIN
	for(int cnt = 0; cnt < 3; cnt++)
	{
		for(int t = 1; t < MAX - 1; t++)
		{
			for(int t2 = 1; t2 < MAX - 1; t2++)
			{
				field[t][t2].y = (field[t + 1][t2].y + field[t][t2 - 1].y + field[t - 1][t2].y + field[t][t2 + 1].y)
						/ 4;

				if(cnt == 0)
				{
					if(field[t][t2].y < -1 && field[t][t2].y > -1 - 0.5)
						field[t][t2].y -= 0.45, field[t][t2].y *= 2;
					else
						if(field[t][t2].y > -1 && field[t][t2].y < -1 + 0.5)
							field[t][t2].y += 0.5, field[t][t2].y /= 5;
				}
			}
		}
	}

	return true;
}

//#include "display.cpp"
/*
 * ttt.cpp
 *
 *  Created on: 2018年11月19日
 *      Author: x
 */

GLfloat xrot = 0;				// X Rotation
GLfloat yrot = 0;				// Y Rotation
GLfloat zrot = 0;				// Y Rotation
GLfloat Throttlei;
GLfloat Throttle = 5;
GLfloat throttle = 1;
GLfloat Speed = Throttle;
GLfloat Speedi;

const GLfloat piover180 = 0.0174532925f;
GLfloat XPOS = -MAX / 2;
GLfloat YPOS = plane_default_hight;
GLfloat ZPOS = -MAX / 2;
GLfloat XP = 0;
GLfloat YP = 0;
GLfloat ZP = 0;

GLfloat sceneroty;
GLfloat heading;
GLfloat pitch = 0;
GLfloat yaw = 0;
GLfloat walkbias = 0;
GLfloat walkbiasangle = 0;
GLfloat zprot;
//GLfloat yptrans2 = 0;

//GLfloat H = 0;
GLfloat angle;

GLfloat xdist;
GLfloat zdist;

GLfloat FPS = 0;
GLfloat multiplier = 360 / (3.14159 * 2);        // multiplier is necessary for conversion from 360 degrees.

GLfloat glow = 0.4;
GLfloat glowp = 0;

GLfloat xtexa;
GLfloat ytexa;
GLfloat xtexa2;
GLfloat ytexa2;

int xrange1;
int xrange2;
int zrange1;
int zrange2;

static void keyboard_cb(unsigned char key, int x, int y)
{
	switch(key)
	{
		case '0':
			throttle = 15;
			break;
		case '1':
			throttle = 1;
			break;
		case '2':
			throttle = 2;
			break;
		case '3':
			throttle = 3;
			break;
		case '4':
			throttle = 4;
			break;
		case '5':
			throttle = 5;
			break;
		case '6':
			throttle = 6;
			break;
		case '7':
			throttle = 7;
			break;
		case '8':
			throttle = 8;
			break;
		case '9':
			throttle = 9;
			break;

		case 'a':
			case 'A':
			break;

		case 'b':
			case 'B':
			break;

		case 'l':
			case 'L':
			water = !water;
			break;

		case 'w':
			case 'W':
			wireframe = !wireframe;
			if(wireframe)
			{
				glDisable(GL_TEXTURE_2D);
			}
			else
			{
				glEnable(GL_TEXTURE_2D);
			}
			break;

		case 'f':
			case 'F':
			break;

		case 'p':
			case 'P':
			status_pause = !status_pause;
			break;

		case '\e':        // esc
			exit(1);
			break;

		default:
			break;
	}
	return;
}
static void keyboard_cb_special(int key, int x, int y)
{
	switch(key)
	{
		case GLUT_KEY_LEFT:
			key_status_GLUT_KEY_LEFT = true;
			break;

		case GLUT_KEY_RIGHT:
			key_status_GLUT_KEY_RIGHT = true;
			break;

		case GLUT_KEY_UP:
			key_status_GLUT_KEY_UP = true;
			break;

		case GLUT_KEY_DOWN:
			key_status_GLUT_KEY_DOWN = true;
			break;

		case GLUT_KEY_HOME:
			key_status_GLUT_KEY_HOME = true;
			break;

		case GLUT_KEY_END:
			key_status_GLUT_KEY_END = true;
			break;

		case GLUT_KEY_PAGE_DOWN:
			key_status_GLUT_KEY_PAGE_DOWN = true;
			break;

		default:
			break;
	}
}
static void keyboard_cb_special_up(int key, int x, int y)
{
	switch(key)
	{
		case GLUT_KEY_LEFT:
			key_status_GLUT_KEY_LEFT = false;
			break;

		case GLUT_KEY_RIGHT:
			key_status_GLUT_KEY_RIGHT = false;
			break;

		case GLUT_KEY_UP:
			key_status_GLUT_KEY_UP = false;
			break;

		case GLUT_KEY_DOWN:
			key_status_GLUT_KEY_DOWN = false;
			break;

		case GLUT_KEY_HOME:
			key_status_GLUT_KEY_HOME = false;
			break;

		case GLUT_KEY_END:
			key_status_GLUT_KEY_END = false;
			break;

		case GLUT_KEY_PAGE_DOWN:
			key_status_GLUT_KEY_PAGE_DOWN = false;
			break;

		default:
			break;
	}
}
static void update_data(void)
{
	if(throttle == 15)
	{
		after_burner = true;
	}
	else
	{
		after_burner = false;
	}

	if(key_status_GLUT_KEY_LEFT)
	{
		zprot += 5 / (ABS(Speed) + 1);
		Throttle *= 0.99;
	}
	if(key_status_GLUT_KEY_RIGHT)
	{
		zprot -= 5 / (ABS(Speed) + 1);
		Throttle *= 0.99;
	}
	if(key_status_GLUT_KEY_UP)
	{
		pitch -= 15 / (ABS(Speed) + 1);
	}
	if(key_status_GLUT_KEY_DOWN)
	{
		pitch += 15 / (ABS(Speed) + 1);
	}
	if(key_status_GLUT_KEY_PAGE_DOWN)
	{
		pitch = 0;
		if(pitch > 0)
		{
			pitch -= 15 / (ABS(Speed) + 1);
			pitch = (int) pitch;
		}
		else
			if(pitch < 0)
			{
				pitch += 15 / (ABS(Speed) + 1);
				pitch = (int) pitch;
			}
	}

	if(key_status_GLUT_KEY_HOME)
	{
		if(quality <= 1)
			quality = 1;
		else
			quality--;
	}
	if(key_status_GLUT_KEY_END)
	{
		if(quality >= 8)
			quality = 8;
		else
			quality++;
	}

	zprot *= 0.935f;
	heading += zprot / 3;
	yaw += zprot / 5;
	yaw *= 0.95f;

	Throttlei += (throttle - Throttle) / 10;
	Throttlei *= 0.9f;
	Throttle += Throttlei / 10;

	GLfloat MAX_Speed = (sqrt(Throttle + 1)) * 10;
	Speedi += MAX_Speed - Speed;
	Speedi *= 0.9f;
	Speed += Speedi / 1000;
	XP = -(GLfloat) sin(heading * piover180) * Speed;
	YP = -(GLfloat) sin(pitch * piover180) * Speed;
	ZP = -(GLfloat) cos(heading * piover180) * Speed;
	GLfloat overallspeed = Hypot(Hypot(XP, YP), ZP) / (ABS(Speed) + 1);

	YP *= overallspeed;
	XP *= overallspeed;
	ZP *= overallspeed;

	XPOS += XP / 30;
	YPOS += YP / 30;
	ZPOS += ZP / 30;

	if(XPOS > 0)
		XPOS -= MAX;
	if(XPOS < -MAX)
		XPOS += MAX;
	if(ZPOS > 0)
		ZPOS -= MAX;
	if(ZPOS < -MAX)
		ZPOS += MAX;

	xtrans = -XPOS;
	ztrans = -ZPOS;
	ytrans = YPOS;

	yrot = heading;
	sceneroty = 360.0f - yrot;

	xrange1 = int(MAX - xtrans - visual_distance);
	xrange2 = int(MAX - xtrans + visual_distance);
	zrange1 = int(MAX - ztrans - visual_distance);
	zrange2 = int(MAX - ztrans + visual_distance);

	xrange1 /= quality;
	xrange1 *= quality;
	xrange2 /= quality;
	xrange2 *= quality;

	zrange1 /= quality;
	zrange1 *= quality;
	zrange2 /= quality;
	zrange2 *= quality;

	return;
}
static void display_with_wireframe(void)
{
//	LOG();
	int i, i2;
	int t, t2;

	int a = 1;

	glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
	glBegin(GL_LINE_LOOP);
	//glBegin(GL_LINES);
	for(t = xrange1; t < xrange2; t += quality * a)
	{
		for(t2 = zrange1; t2 < zrange2; t2 += quality * a)
		{
			i = t;
			i2 = t2;

			while(i < 0)
				i += MAX;
			while(i > MAX)
				i -= MAX;
			while(i2 < 0)
				i2 += MAX;
			while(i2 > MAX)
				i2 -= MAX;
			int coord = t - MAX;
			int coord2 = t2 - MAX;

			//glBegin(GL_LINE_LOOP);
#if 1
			glVertex3f(coord, field[i][i2].y, coord2);
//			printf("glVertex3f(%f,%f,%f);\n", coord, field[i][i2].y, coord2);
			glVertex3f(coord + quality, field[i + quality][i2].y, coord2);
//			printf("glVertex3f(%f,%f,%f);\n", coord + quality, field[i + quality][i2].y, coord2);
			glVertex3f(coord + quality, field[i + quality][i2 + quality].y, coord2 + quality);
//			printf("glVertex3f(%f,%f,%f);\n", coord + quality, field[i + quality][i2 + quality].y, coord2 + quality);
			glVertex3f(coord, field[i][i2 + quality].y, coord2 + quality);
//			printf("glVertex3f(%f,%f,%f);\n", coord, field[i][i2 + quality].y, coord2 + quality);
			glVertex3f(coord + quality, field[i + quality][i2].y, coord2);
//			printf("glVertex3f(%f,%f,%f);\n", coord + quality, field[i + quality][i2].y, coord2);
#else			
			glVertex3f(coord, field[i][i2].y, coord2);
			glVertex3f(coord + quality, field[i + quality][i2].y, coord2);

			glVertex3f(coord + quality, field[i + quality][i2].y, coord2);
			glVertex3f(coord + quality, field[i + quality][i2 + quality].y, coord2 + quality);
			glVertex3f(coord + quality, field[i + quality][i2 + quality].y, coord2 + quality);
			glVertex3f(coord, field[i][i2 + quality].y, coord2 + quality);
			glVertex3f(coord, field[i][i2 + quality].y, coord2 + quality);
			glVertex3f(coord + quality, field[i + quality][i2].y, coord2);
			glVertex3f(coord + quality, field[i + quality][i2].y, coord2);
			glVertex3f(coord, field[i][i2].y, coord2);
#endif

			//glEnd();

		}

//		return;
	}
	glEnd();
}
static void draw_sand(void)
{
	if(texture[0] <= 0)
	{
//		LOG();
//		exit(1);
		return;
	}

	glDisable(GL_BLEND);        // necessary
	glEnable(GL_DEPTH_TEST);
	glFogf(GL_FOG_START, visual_distance * 0.8);        // Fog Start Depth
	glFogf(GL_FOG_END, visual_distance);        // Fog End Depth

	glColor4f(0, 1, 0.8, 0.8);
	glBindTexture(GL_TEXTURE_2D, texture[0]);

	for(int x = xrange1; x < xrange2; x += quality)
	{
		for(int z = zrange1; z < zrange2; z += quality)
		{
			int xt = x;
			int zt = z;

			while(xt < 0)
				xt += MAX;
			while(xt > MAX)
				xt -= MAX;
			while(zt < 0)
				zt += MAX;
			while(zt > MAX)
				zt -= MAX;

			xtexa = (GLfloat(xt) / MAX) * 57;
			xtexa2 = (GLfloat(xt + quality) / MAX) * 57;
			ytexa = (GLfloat(zt) / MAX) * 57;
			ytexa2 = (GLfloat(zt + quality) / MAX) * 57;
			int coord = x - MAX;
			int coord2 = z - MAX;

			//			LOG("xtexa: %d, ytexa: %d, xtexa2: %d, ytexa2: %d", xtexa, ytexa, xtexa2, ytexa2);
			glBegin(GL_TRIANGLE_STRIP);
			glTexCoord2f(xtexa2, ytexa2);
			glVertex3f(coord + quality, field[xt + quality][zt + quality].y, coord2 + quality);
			glTexCoord2f(xtexa2, ytexa);
			glVertex3f(coord + quality, field[xt + quality][zt].y, coord2);
			glTexCoord2f(xtexa, ytexa2);
			glVertex3f(coord, field[xt][zt + quality].y, coord2 + quality);
			glTexCoord2f(xtexa, ytexa);
			glVertex3f(coord, field[xt][zt].y, coord2);
			glEnd();
		}
	}

	glDisable(GL_DEPTH_TEST);
}
static void draw_water(void)
{
	if(!water || texture[6] <= 0)
	{
		return;
	}
	glEnable(GL_DEPTH_TEST);		// necessary
	glEnable(GL_BLEND);

	glLoadIdentity();
	glTranslatef(0, 0, -10);
	glRotatef(sceneroty, 0, 1, 0);
	glTranslatef(xtrans, ytrans - 3.5 - ABS(Speed) / 5, ztrans);

	glColor4f(0, 0, 0.5, 0.7);
	glBindTexture(GL_TEXTURE_2D, texture[0]);

	int C = 200;
	glBegin(GL_TRIANGLE_STRIP);
	glTexCoord2f(0, 0);
	glVertex3f(xrange2 - MAX, -1, zrange2 - MAX);
	glTexCoord2f(C, C);
	glVertex3f(xrange2 - MAX, -1, zrange1 - MAX);
	glTexCoord2f(0, C);
	glVertex3f(xrange1 - MAX, -1, zrange2 - MAX);
	glTexCoord2f(C, 0);
	glVertex3f(xrange1 - MAX, -1, zrange1 - MAX);
	glEnd();

	glDisable(GL_DEPTH_TEST);
}
static void draw_plane_jet_burner(void)
{
	if(texture[5] <= 0)
	{
		return;
	}
	glEnable(GL_ALPHA_TEST);

//	glDisable (GL_DEPTH_TEST);		// necessary
	glEnable(GL_BLEND);        // necessary
	// GL_ONE, GL_ZERO
	// GL_ZERO, GL_ONE
	// GL_ONE, GL_ONE
	// GL_SRC_ALPHA  GL_DST_ALPHA
	// GL_ONE_MINUS_SRC_ALPHA
	glBlendFunc(GL_ONE, GL_ONE);
	glBindTexture(GL_TEXTURE_2D, texture[5]);

	GLfloat exhaust_r, exhaust_g, exhaust_b;
	if(after_burner)
	{
		exhaust_r = 1;
		exhaust_g = 0.5;
		exhaust_b = 0;
	}
	else
	{
		exhaust_r = 0.05;
		exhaust_g = 0.05;
		exhaust_b = 0.8;
	}

	glowp += 0.5f - glow;
	glow += glowp * (ABS(Throttle) / 500);
	if(glow > 1)
		glow = 1;
	else
		if(glow < 0.25f)
			glow = 0.25f;

	glColor4f(exhaust_r, exhaust_g, exhaust_b, glow);

	int t = 5;
	if(t > MAX_PARTICLES / 2)
	{
		t = MAX_PARTICLES / 2;
	}
	bool draw_bunner;

	for(int i = 0; i < MAX_PARTICLES; i++)        // jet bunner
	{
		draw_bunner = i < t || (i >= MAX_PARTICLES / 2 && i < MAX_PARTICLES / 2 + t);
		if(!draw_bunner)
		{
			continue;
		}

		GLfloat x = particle[i].x;						// Grab Our Particle X Position
		GLfloat y = particle[i].y;						// Grab Our Particle Y Position
		GLfloat z = particle[i].z;						// Particle Z Pos + Zoom

		glColor4f(particle[i].r, particle[i].g, particle[i].b, particle[i].life / 2);
		glBegin(GL_TRIANGLE_STRIP);						// Build Quad From A Triangle Strip
		glTexCoord2f(0.3, 0.3);
		glVertex3f(x + 0.2f, y + 0.2f, z);						// Top Right
		glTexCoord2f(0, 0.3);
		glVertex3f(x - 0.2f, y + 0.2f, z);						// Top Left
		glTexCoord2f(0.3, 0.05);
		glVertex3f(x + 0.2f, y - 0.2f, z);						// Bottom Right
		glTexCoord2f(0, 0.05);
		glVertex3f(x - 0.2f, y - 0.2f, z);						// Bottom Left
		glEnd();

		particle[i].x += particle[i].xi / 250;						// Move On The X Axis By X Speed
		particle[i].y += particle[i].yi / 250;						// Move On The Y Axis By Y Speed
		particle[i].z += particle[i].zi / 250;						// Move On The Z Axis By Z Speed
		particle[i].xi *= 0.975f;
		particle[i].yi *= 0.975f;
		particle[i].zi *= 0.975f;
		particle[i].zi += particle[i].zg;						// Take Pull On Z Axis Into Account
		particle[i].life -= particle[i].fade * 3;						// Reduce Particles Life By 'Fade'
		if(particle[i].life < 0.5f)
			particle[i].life *= 0.975;

		if(particle[i].life < 0.05f)						// If Particle Is Burned Out
		{
			particle[i].r = exhaust_r;
			particle[i].g = exhaust_g;
			particle[i].b = exhaust_b;

			particle[i].life = 1.0f;					// Give It New Life
			particle[i].fade = GLfloat(rand() % 100) / 2500 + 0.02f;					// Random Fade Value
			if(i < MAX_PARTICLES / 2)
				particle[i].x = 0.52f;
			else
				particle[i].x = -0.52f;

			particle[i].y = -0.8f;
			particle[i].z = 3.f;
			V = (GLfloat((rand() % 5)) + 2) / 5;
			Angle = GLfloat(rand() % 360);

			particle[i].xi = sin(Angle) * V;
			particle[i].yi = cos(Angle) * V;
			particle[i].zi = ((rand() % 10) - 5) / 5 + Throttle * 4;
		}
	}

	glDisable(GL_BLEND);
	return;
}
static void draw_sky(void)
{
	//	glFrontFace (GL_CW);
	// SKYDOME GENERATED WITH A PRECISELY POSITIONED SPHERE(a shortcut to the real thing)
//	glEnable(GL_FOG);
//	glDisable(GL_FOG);
//	glFogf(GL_FOG_START, visual_distance * 0.8);        // Fog Start Depth
//	glFogf(GL_FOG_END, visual_distance);        // Fog End Depth

//	glColor4f(0, 0.1, 0.8, 1);
//	glBindTexture(GL_TEXTURE_2D, texture[1]);
//	glTranslatef(-xtrans, -ytrans - MAX * 5, -ztrans);
//	glLoadIdentity();
//	glTranslatef(0, 0, 0);
//	glRotatef(90, 1, 0, 1);
//	gluSphere(quadratic, visual_distance * AAA, 20, 20);

//	glDisable(GL_FOG);
}
static void draw_sun(void)
{
//	glLoadIdentity();        // 重置当前指定的矩阵为单位矩阵.在语义上，其等同于用单位矩阵调用glLoadMatrix
//	glRotatef(sceneroty, 0, 1, 0);        // 使用一个旋转矩阵乘以当前矩阵
//
//	glEnable(GL_BLEND);
////	glEnable (GL_ALPHA_TEST);
////	glEnable(GL_DEPTH_TEST);
//	glBindTexture(GL_TEXTURE_2D, texture[7]);
//	glColor4f(0.8, 0.5, 0.1, 0.7);
//
//	float sun_flare_size;
//	sun_flare_size = 500;
//
//	glBegin(GL_TRIANGLE_STRIP);						// Build Quad From A Triangle Strip
//	glTexCoord2f(1, 1);
//	glVertex3f(MAX / 2 + sun_flare_size, sun_height + sun_flare_size, sun_zdistance);						// Top Right
//	glTexCoord2f(0, 1);
//	glVertex3f(MAX / 2 - sun_flare_size, sun_height + sun_flare_size, sun_zdistance);						// Top Left
//	glTexCoord2f(1, 0);
//	glVertex3f(MAX / 2 + sun_flare_size, sun_height - sun_flare_size, sun_zdistance);					// Bottom Right
//	glTexCoord2f(0, 0);
//	glVertex3f(MAX / 2 - sun_flare_size, sun_height - sun_flare_size, sun_zdistance);					// Bottom Left
//	glEnd();
//
//	glDisable(GL_BLEND);
//	return;
}
static void display_with_texture(void)
{
////	glEnable(GL_LIGHT1);
////	glDisable (GL_FOG);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	draw_sand();
	draw_water();
	glDisable(GL_CULL_FACE);

//	draw_sky();
//	draw_sun();
//	glDisable (GL_FOG);
}
static void display(void)
{
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);        // Clear The Screen And The Depth Buffer
	glLoadIdentity();
	glTranslatef(0, 0, -10);
	glRotatef(sceneroty, 0, 1, 0);
	glTranslatef(xtrans, ytrans - 3.5 - ABS(Speed) / 5, ztrans);

	if(wireframe)
	{
//		display_with_wireframe();
	}
	else
	{
		display_with_texture();
	}

	glLoadIdentity();
	glTranslatef(0, -0.5f, -10);
	glRotatef(yaw, 0, 1, 0);
	glRotatef(zprot * 15, 0, 0, 1);
	glRotatef(pitch, 1, 0, 0);
	plane->draw();
	draw_plane_jet_burner();
	glutSwapBuffers();

	static int s = 0;
	printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b%6d, FPS: %3.0f", s, FPS);
	s++;
	return;
}
static void idle(void)
{
	if(status_pause)
	{
//		usleep(100 * 1000);
//		Speed = 0;
	}

	update_data();
	glutPostRedisplay();
}
int main(int argc, char ** argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(w, h);
	glutInitWindowPosition(0, 0);
	glutCreateWindow(NULL);

	init();
	plane = new Model("model.ms3d");

	glutKeyboardFunc(keyboard_cb);
	glutSpecialFunc(keyboard_cb_special);
	glutSpecialUpFunc(keyboard_cb_special_up);
	glutWarpPointer(w / 2, h / 2);
	glutSetCursor(GLUT_CURSOR_NONE);
	glutReshapeFunc(reshape);

	glutDisplayFunc(display);
	glutIdleFunc(idle);
	glutMainLoop();

	return 0;
}
