/*
 * Model.cpp
 *
 *  Created on: 2018年11月22日
 *      Author: x
 */

#include "h.h"

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
	glCallList(m_graw_list);
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
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);		// GL_DECAL、GL_REPLACE、GL_MODULATE、GL_BLEND、GL_ADD、GL_COMBINE

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

