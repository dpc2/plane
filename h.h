/*
 * h.h
 *
 *  Created on: 2018年11月22日
 *      Author: x
 */

#ifndef H_H_
#define H_H_

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
#include <GL/glext.h>
#include <linux/input.h>
#include <libinput.h>
#include <libudev.h>
#include <libevdev-1.0/libevdev/libevdev.h>
#include <fstream>

using namespace std;

#define	MAX						500     	// LENGTH AND W_idTH OF MATRIX(2D array)
#define	MAX_PARTICLES		250			// Number Of Particles To Create

#ifndef LOG
#define LOG(fmt, args...)			do{	printf("--%-40s--%5d--%-10s--" fmt "\n", __FUNCTION__, __LINE__, __FILE__, ##args);	}while(0)
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

#endif /* H_H_ */
