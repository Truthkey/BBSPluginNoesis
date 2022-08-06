//this is kind of a poor example for plugins, since the format's not totally known and the code is WIP.
//but it does showcase some interesting usages.

#include "stdafx.h"

#pragma pack(push, 1)

typedef  unsigned char    uint8;
typedef  unsigned short   uint16;
typedef  unsigned long    uint32;
typedef  unsigned __int64 uint64;
typedef  signed   char    int8;
typedef  signed   short   int16;
typedef  signed   long    int32;
typedef  signed   __int64 int64;

enum GsPSM
{
	// Pixel Storage Format (0 = 32bit RGBA)

	GS_PSMCT32		= 0,
	GS_PSMCT24		= 1,
	GS_PSMCT16		= 2,
	GS_PSMCT16S		= 10,
	GS_PSMT8		= 19,
	GS_PSMT4		= 20,
	GS_PSMT8H		= 27,
	GS_PSMT4HL		= 36,
	GS_PSMT4HH		= 44,
	GS_PSMZ32		= 48,
	GS_PSMZ24		= 49,
	GS_PSMZ16		= 50,
	GS_PSMZ16S		= 58,
};

enum IMG_TYPE 
{
	IT_RGBA			= 3,
	IT_CLUT4		= 4,
	IT_CLUT8		= 5,
};

enum CLT_TYPE
{
	CT_NONE			= 0,
	CT_A1BGR5		= 1,
	CT_XBGR8		= 2,
	CT_ABGR8		= 3,
};

typedef struct
{
	uint64 TBP0 :14; // Texture Buffer Base Pointer (Address/256)
	uint64 TBW  : 6; // Texture Buffer Width (Texels/64)
	uint64 PSM  : 6; // Pixel Storage Format (0 = 32bit RGBA)
	uint64 TW   : 4; // width = 2^TW
	uint64 TH   : 4; // height = 2^TH
	uint64 TCC  : 1; // 0 = RGB, 1 = RGBA
	uint64 TFX  : 2; // TFX  - Texture Function (0=modulate, 1=decal, 2=hilight, 3=hilight2)
	uint64 CBP  :14; // CLUT Buffer Base Pointer (Address/256)
	uint64 CPSM : 4; // CLUT Storage Format
	uint64 CSM  : 1; // CLUT Storage Mode
	uint64 CSA  : 5; // CLUT Offset
	uint64 CLD  : 3; // CLUT Load Control
} GsTex0;

// TM2 File Structure

struct TM2_HEADER
{
	char   fileID[4]; // usually 'TIM2' or 'CLT2'
	uint8  version;   // format version
	uint8  id;        // format id
	uint16 imageCount;
	uint32 padding[2];
};

struct TM2_PICTURE_HEADER
{
	// 0x00
	uint32 totalSize;
	uint32 clutSize;
	uint32 imageSize;
	uint16 headerSize;
	uint16 clutColors;
	// 0x10
	uint8  format;
	uint8  mipMapCount;
	uint8  clutType;
	uint8  imageType;
	uint16 width;
	uint16 height;

	GsTex0 GsTex0b; //[8];
	// 0x20
	GsTex0 GsTex1b; //[8];
	uint32 GsRegs;
	uint32 GsTexClut;
};

struct PMO_TEXTURE_HEADER
{
	// 0x00
	uint32 dataOffset;
	char   resourceName[0x0C];
	// 0x10
	uint32 unknown0x10[4];
};

struct PMO_JOINT
{
	uint16 index;
	uint16 padding0;

	uint16 parent;
	uint16 padding1;

	uint16 unknown0x08; // skinning index?
	uint16 padding2;

	uint32 unknown0x0C;

	// 0x10
	char   name[0x10];
	// 0x20
	float  transform[16];
	float  transformInverse[16];
};

struct PMO_SKEL_HEADER
{
	char   dataTag[4]; // 'BON'
	uint32 unknown0x04;
	uint32 jointCount;
	uint16 unknown0x0C; // number of skinned joints?
	uint16 unknown0x0E; // skinning start index
};

struct PMO_MESH_HEADER
{
	// 0x00
	uint16 vertexCount;
	uint8  textureID;
	uint8  vertexSize;

	struct
	{
		uint32 texCoord : 2; // texture coordinate format: 0 - none, 1 - uint8, 2 - uint16, 3 - float
		uint32 unknown9 : 2; //
		uint32 unknown0 : 3; // unsure of bit size, but when this is not zero, diffuse color is present in vertex data
		uint32 position : 2; // position format: 0 - none, 2 - int16, 3 - float
		uint32 skinning : 1; // only seems to be set when there are joint weights present?
		uint32 unknown1 : 4;
		uint32 jointCnt : 4; // maximum joint index used? (index count = joint count + 1 - or just use (jointCnt + skinning)
		uint32 unknown2 : 6;
		uint32 diffuse  : 1; // seems to only be set when a diffuse color is present in header
		uint32 unknown3 : 3;
		uint32 dataType : 4; // 0x30 - tri-list, 0x40 - tri-strip - ?
	} dataFlags;

	uint8  unknown0x08;
	uint8  triStripCount;
	uint8  unknown0x0A[2];
};

struct PMO_HEADER
{
	// 0x00
	char   fileTag[4]; // 'PMO'
	uint8  unknown0x04[4];
	uint16 textureCount; //
	uint16 unknown0x0A;
	uint32 skeletonOffset;
	// 0x10
	uint32 meshOffset0;
	uint16 triangleCount;
	uint16 vertexCount;
	float  modelScale;
	uint32 meshOffset1;
	// 0x20
	float  boundingBox[8][4];
};

#pragma pack(pop)

//see if something is valid KH:BBS .pmo data
bool Model_KHBBS_Check(BYTE *fileBuffer, int bufferLen, noeRAPI_t *rapi)
{
	if (bufferLen < sizeof(PMO_HEADER))
	{
		return false;
	}
	PMO_HEADER *header = ((PMO_HEADER *)fileBuffer);
	if (memcmp(header->fileTag, "PMO\0", 4))
	{
		return false;
	}

	if ((header->meshOffset0 == 0 && header->meshOffset1 == 0) ||
		header->meshOffset0 >= bufferLen || header->meshOffset1 >= bufferLen)
	{
		return false;
	}

	return true;
}

//load texture bundle
static void Model_KHBBS_LoadTextures(CArrayList<noesisTex_t *> &textures, CArrayList<noesisMaterial_t *> &materials, BYTE *data, int dataSize, int textureCount, noeRAPI_t *rapi)
{
	PMO_TEXTURE_HEADER *textureHeader = (PMO_TEXTURE_HEADER*)(data + sizeof(PMO_HEADER));

	char matName[128] = {0};

	// create default texture for un-textured models
	noesisTex_t *nt = rapi->Noesis_AllocPlaceholderTex("defaultTexture", 32, 32, false);
	textures.Append(nt);

	sprintf_s(matName, 128, "defaultTexture");
	noesisMaterial_t *nmat = rapi->Noesis_GetMaterialList(1, true);
	nmat->name = rapi->Noesis_PooledString(matName);
	nmat->noDefaultBlend = true;
	nmat->texIdx = 0;
	materials.Append(nmat);

	// read texture information
	int texNum = 0;
	for (texNum = 0; texNum < textureCount; ++texNum, ++textureHeader)
	{
		if (textureHeader->dataOffset >= dataSize)
		{
			// create placeholder texture
			nt = rapi->Noesis_AllocPlaceholderTex(textureHeader->resourceName, 32, 32, false);
			textures.Append(nt);

			sprintf_s(matName, 128, "material%02i", texNum);
			nmat = rapi->Noesis_GetMaterialList(1, true);
			nmat->name = rapi->Noesis_PooledString(matName);
			nmat->noDefaultBlend = true;
			nmat->texIdx = texNum + 1;
			materials.Append(nmat);

			continue;
		}

		uint8 *textureData = (data + textureHeader->dataOffset);

		TM2_HEADER *imageHeader = (TM2_HEADER*)textureData;
		TM2_PICTURE_HEADER *pictureHeader = (TM2_PICTURE_HEADER*)(imageHeader + 1);

		uint32 imageSize = (pictureHeader->imageSize);

		uint8 *imageData = (uint8*)(pictureHeader + 1);
		uint8 *clutData  = (imageData + imageSize);

		uint32 psWidth  = (1 << pictureHeader->GsTex0b.TW);
		uint32 psHeight = (1 << pictureHeader->GsTex0b.TH);
		uint32 width  = pictureHeader->width;
		uint32 height = pictureHeader->height;
		uint32 rem    = (psWidth - width) << 2;

		uint8 *tempImage = (uint8 *)rapi->Noesis_UnpooledAlloc(psWidth * psHeight * 4);
		uint8 *dstPixel = tempImage;

		uint32 _x = 0;
		uint32 _y = 0;
		for (_y = 0; _y < height; ++_y)
		{
			for (_x = 0; _x < width; ++_x, dstPixel += 4, ++imageData)
			{
				uint32 pixelIndex0 = *imageData;
				uint32 pixelIndex1 = pixelIndex0;

				switch (pictureHeader->imageType)
				{
				case IT_RGBA:
					break;
				case IT_CLUT4:
					pixelIndex0 &= 0x0F;
					pixelIndex1 = (pixelIndex1 >> 4) & 0x0F;
					break;
				case IT_CLUT8:
					{
						if ((pixelIndex0 & 31) >= 8)
						{
							if ((pixelIndex0 & 31) < 16)
							{
								pixelIndex0 += 8;				// +8 - 15 to +16 - 23
							}
							else if ((pixelIndex0 & 31) < 24)
							{
								pixelIndex0 -= 8;				// +16 - 23 to +8 - 15
							}
						}
					}
					break;
				default:
					return;
					break;
				}

				uint16 pixel = 0;
				switch (pictureHeader->clutType)
				{
				case CT_NONE:
					dstPixel[0] = imageData[0];
					dstPixel[1] = imageData[1];
					dstPixel[2] = imageData[2];
					dstPixel[3] = imageData[3];
					imageData += 3;
					break;
				case CT_A1BGR5:
					pixelIndex0 <<= 1;
					pixel = *((uint16*)(clutData + pixelIndex0));
					dstPixel[0] = ( pixel        & 0x1F) * (1.0 / 31.0 * 255.0);
					dstPixel[1] = ((pixel >>  5) & 0x1F) * (1.0 / 31.0 * 255.0);
					dstPixel[2] = ((pixel >> 10) & 0x1F) * (1.0 / 31.0 * 255.0);
					dstPixel[3] = (pixel & 0x8000) ? (0xFF) : (0);
					break;
				case CT_XBGR8:
					pixelIndex0 *= 3;
					dstPixel[0] = clutData[pixelIndex0 + 0];
					dstPixel[1] = clutData[pixelIndex0 + 1];
					dstPixel[2] = clutData[pixelIndex0 + 2];
					dstPixel[3] = 0xFF;
					break;
				case CT_ABGR8:
					pixelIndex0 <<= 2;
					dstPixel[0] = clutData[pixelIndex0 + 0];
					dstPixel[1] = clutData[pixelIndex0 + 1];
					dstPixel[2] = clutData[pixelIndex0 + 2];
					dstPixel[3] = clutData[pixelIndex0 + 3];
					break;
				default:
					return;
					break;
				}

				if (pictureHeader->imageType == IT_CLUT4)
				{
					dstPixel += 4;
					++_x;
					switch (pictureHeader->clutType)
					{
					case CT_A1BGR5:
						pixelIndex1 <<= 1;
						pixel = *((uint16*)(clutData + pixelIndex1));
						dstPixel[0] = ( pixel        & 0x1F) * (1.0 / 31.0 * 255.0);
						dstPixel[1] = ((pixel >>  5) & 0x1F) * (1.0 / 31.0 * 255.0);
						dstPixel[2] = ((pixel >> 10) & 0x1F) * (1.0 / 31.0 * 255.0);
						dstPixel[3] = (pixel & 0x8000) ? (0xFF) : (0);
						break;
					case CT_XBGR8:
						pixelIndex1 *= 3;
						dstPixel[0] = clutData[pixelIndex1 + 0];
						dstPixel[1] = clutData[pixelIndex1 + 1];
						dstPixel[2] = clutData[pixelIndex1 + 2];
						dstPixel[3] = 0xFF;
						break;
					case CT_ABGR8:
						pixelIndex1 <<= 2;
						dstPixel[0] = clutData[pixelIndex1 + 0];
						dstPixel[1] = clutData[pixelIndex1 + 1];
						dstPixel[2] = clutData[pixelIndex1 + 2];
						dstPixel[3] = clutData[pixelIndex1 + 3];
						break;
					default:
						break;
					}
				}
			}
			dstPixel += rem;
		}

		// create texture
		nt = rapi->Noesis_TextureAlloc(textureHeader->resourceName, psWidth, psHeight, tempImage, NOESISTEX_RGBA32);
		nt->shouldFreeData = true;
		textures.Append(nt);

		sprintf_s(matName, 128, "material%02i", texNum);
		nmat = rapi->Noesis_GetMaterialList(1, true);
		nmat->name = rapi->Noesis_PooledString(matName);
		nmat->noDefaultBlend = false;
		nmat->noLighting = true;
		nmat->texIdx = texNum + 1;
		materials.Append(nmat);
	}
}

//convert the bones
modelBone_t *Model_KHBBS_CreateBones(BYTE *data, noeRAPI_t *rapi, int &numBones)
{
	PMO_SKEL_HEADER *skeletonHeader = (PMO_SKEL_HEADER*)data;

	numBones = 0;

	if (skeletonHeader->jointCount > 0)
	{
		PMO_JOINT *jointData = (PMO_JOINT*)(data + sizeof(PMO_SKEL_HEADER));
		numBones = skeletonHeader->jointCount;

		modelBone_t *bones = rapi->Noesis_AllocBones(numBones);
		modelBone_t *bone = bones;

		uint32 jointNum = 0;
		for (jointNum = 0; jointNum < numBones; ++jointNum, ++jointData, ++bone)
		{
			bone->index = jointNum;
			sprintf_s(bone->name, 30, jointData->name, jointNum);
			bone->mat = g_identityMatrix;

			g_mfn->Math_ModelMatFromGL(&bone->mat, jointData->transform);

			bone->eData.parent = (jointData->parent != 0xFFFF) ? (bones + jointData->parent) : NULL;
		}

		// transform bones
		rapi->rpgMultiplyBones(bones, numBones);
		return bones;
	}

	return NULL;
}

static void Model_MeshVertex(PMO_MESH_HEADER *meshHeader, uint8 *vertexStart, uint8 *jointIndices, float modelScale, noeRAPI_t *rapi)
{
	uint8 *vertexData = vertexStart;
	uint8 *jointWeights = NULL;
	if (meshHeader->dataFlags.skinning)
	{
		jointWeights = vertexData;

		rapi->rpgVertBoneIndexUB(jointIndices, meshHeader->dataFlags.jointCnt + 1);
		rapi->rpgVertBoneWeightUB(jointWeights, meshHeader->dataFlags.jointCnt + 1);

		vertexData += meshHeader->dataFlags.jointCnt + 1;
	}

	float tempData[4] = {0};

	switch (meshHeader->dataFlags.texCoord)
	{
	case 1:
		{
			uint8 *textureCoord = (uint8*)vertexData;
			tempData[0] = (float)textureCoord[0] * (1.0 / 127.0);
			tempData[1] = (float)textureCoord[1] * (1.0 / 127.0);
			rapi->rpgVertUV2f(tempData, 0);
			vertexData += 2;
		}
		break;
	case 2:
		{
			vertexData += ((0x2 - ((vertexData - vertexStart) & 0x1)) & 0x1);
			uint16 *textureCoord = (uint16*)vertexData;
			tempData[0] = (float)textureCoord[0] * (1.0 / 32767.0);
			tempData[1] = (float)textureCoord[1] * (1.0 / 32767.0);
			rapi->rpgVertUV2f(tempData, 0);
			vertexData += 4;
		}
		break;
	case 3:
		{
			vertexData += ((0x4 - ((vertexData - vertexStart) & 0x3)) & 0x3);
			float *textureCoord = (float*)vertexData;
			tempData[0] = (float)textureCoord[0];
			tempData[1] = (float)textureCoord[1];
			rapi->rpgVertUV2f(tempData, 0);
			vertexData += 8;
		}
		break;
	default:
		break;
	}

	switch (meshHeader->dataFlags.unknown0)
	{
	case 1:
		vertexData += ((0x4 - ((vertexData - vertexStart) & 0x3)) & 0x3);
		{
			float color[4] = {(float)vertexData[0]/128.0f, (float)vertexData[1]/128.0f, (float)vertexData[2]/128.0f, (float)vertexData[3]/255.0f};
			rapi->rpgVertColor4f(color);
		}
		vertexData += 4;
		break;
	case 2:
		rapi->rpgVertColor3ub(vertexData);
		vertexData += 3;
		break;
	default:
		{
			// set a default white diffuse color to avoid zeroing
			uint8 color[4] = {0xFF, 0xFF, 0xFF, 0xFF};
			rapi->rpgVertColor4ub(color);
		}
		break;
	}

	switch (meshHeader->dataFlags.position)
	{
	case 2:
		{
			vertexData += ((0x2 - ((vertexData - vertexStart) & 0x1)) & 0x1);
			int16 *position = (int16*)vertexData;
			tempData[0] = (float)position[0] * (1.0 / 32767.0) * modelScale;
			tempData[1] = (float)position[1] * (1.0 / 32767.0) * modelScale;
			tempData[2] = (float)position[2] * (1.0 / 32767.0) * modelScale;
			rapi->rpgVertex3f(tempData);
			vertexData += 6;
		}
		break;
	case 3:
		{
			vertexData += ((0x4 - ((vertexData - vertexStart) & 0x3)) & 0x3);
			float *position = (float*)vertexData;
			tempData[0] = (float)position[0] * modelScale;
			tempData[1] = (float)position[1] * modelScale;
			tempData[2] = (float)position[2] * modelScale;
			rapi->rpgVertex3f(tempData);
			vertexData += 12;
		}
		break;
	default:
		// should hopefully never happen...
		break;
	}
}

//load a single model from a dat set
static void Model_KHBBS_LoadModel(BYTE* data, BYTE *groupStart, CArrayList<noesisTex_t *> &textures, CArrayList<noesisMaterial_t *> &materials, modelBone_t *bones, int numBones, noeRAPI_t *rapi)
{
	PMO_HEADER *header = (PMO_HEADER *)data;
	PMO_MESH_HEADER *meshHeader = (PMO_MESH_HEADER*)groupStart;

	int meshIndex = 0;
	while (meshHeader->vertexCount)
	{
		uint8 *vertexStart = (uint8*)(meshHeader + 1);

		uint8 *jointIndices = NULL;
		// if joints are present...
		if (numBones)
		{
			// store address of joint indices
			jointIndices = vertexStart;
			vertexStart += 8;
		}

		uint32 diffuseColor = 0xFFFFFFFF;
		if (meshHeader->dataFlags.diffuse)
		{
			// retrieve mesh diffuse color
			diffuseColor = *((uint32*)vertexStart);
			vertexStart += 4;
		}

		uint32 stripCount = 0;
		uint16 *stripLengths = NULL;
		if (meshHeader->triStripCount)
		{
			// setup multiple tri-strip lists...
			stripCount   = meshHeader->triStripCount;
			stripLengths = (uint16*)vertexStart;
			vertexStart += (stripCount << 1);
		}
		else if (meshHeader->dataFlags.dataType == 4)
		{
			// simplified access to a single tri-strip...
			stripCount = 1;
			stripLengths = &(meshHeader->vertexCount);
		}

		// determine mesh material index
		int texIndex = (meshHeader->textureID != 0xFF) ? (meshHeader->textureID + 1) : (0);
		rapi->rpgSetMaterialIndex(texIndex);

		// set mesh name
// 		char meshName[128];
// 		sprintf_s(meshName, 128, "mesh%04i", meshIndex);
// 		rapi->rpgSetName(meshName);

		++meshIndex;

		if (stripLengths)
		{
			// process tri-strip data
			uint32 stripNum = 0;
			for (stripNum = 0; stripNum < stripCount; ++stripNum)
			{
				uint32 vertexCount = stripLengths[stripNum];

				if (!vertexCount)
					continue;

				rapi->rpgBegin(RPGEO_TRIANGLE_STRIP);
				uint32 vertexNum = 0;
				for (vertexNum = 0; vertexNum < vertexCount; ++vertexNum, vertexStart += meshHeader->vertexSize)
				{
					Model_MeshVertex(meshHeader, vertexStart, jointIndices, header->modelScale, rapi);
				}
				rapi->rpgEnd();
			}
		}
		else
		{
			// process triangle data
			uint32 vertexCount = meshHeader->vertexCount;

			rapi->rpgBegin(RPGEO_TRIANGLE);
			uint32 vertexNum = 0;
			for (vertexNum = 0; vertexNum < vertexCount; ++vertexNum, vertexStart += meshHeader->vertexSize)
			{
				Model_MeshVertex(meshHeader, vertexStart, jointIndices, header->modelScale, rapi);
			}
			rapi->rpgEnd();
		}

		meshHeader = (PMO_MESH_HEADER*)(vertexStart + ((0x4 - ((vertexStart - data) & 0x3)) & 0x3));
	}
}

//load it
noesisModel_t *Model_KHBBS_Load(BYTE *fileBuffer, int bufferLen, int &numMdl, noeRAPI_t *rapi)
{
	PMO_HEADER *header = (PMO_HEADER *)fileBuffer;

	CArrayList<noesisTex_t *> textures;
	CArrayList<noesisMaterial_t *> materials;

	numMdl = 0;

	if (header->textureCount > 0)
	{
		// attempt to load textures
		Model_KHBBS_LoadTextures(textures, materials, fileBuffer, bufferLen, header->textureCount, rapi);
	}

	int numBones = 0;
	modelBone_t *bones = NULL;
	if (header->skeletonOffset)
	{
		// process joint data
		bones = Model_KHBBS_CreateBones(fileBuffer + header->skeletonOffset, rapi, numBones);
	}

	void *pgctx = rapi->rpgCreateContext();
	rapi->rpgSetEndian(false);

	noesisMatData_t *md = rapi->Noesis_GetMatDataFromLists(materials, textures);
	rapi->rpgSetExData_Materials(md);
	rapi->rpgSetExData_Bones(bones, numBones);

	rapi->rpgSetTriWinding(false);

	uint32 meshOffsets[2] = {header->meshOffset0, header->meshOffset1};

	CArrayList<noesisModel_t *> models;
	uint32 groupNum = 0;
	for (groupNum = 0; groupNum < 2; ++groupNum)
	{
		if (meshOffsets[groupNum])
		{
			if (meshOffsets[groupNum] >= bufferLen)
			{ //bad mesh offset
				continue;
			}
			uint8 *groupStart = (fileBuffer + meshOffsets[groupNum]);

			Model_KHBBS_LoadModel(fileBuffer, groupStart, textures, materials, bones, numBones, rapi);
		}
	}

#if 0 //create a procedural anim to move random bones around
	if (bones)
	{
		const int numMoveBones = 1 + rand()%(numBones-1);
		sharedPAnimParm_t *aparms = (sharedPAnimParm_t *)_alloca(sizeof(sharedPAnimParm_t)*numMoveBones);
		memset(aparms, 0, sizeof(aparms)); //it's a good idea to do this, in case future noesis versions add more meaningful fields.
		for (int i = 0; i < numMoveBones; i++)
		{
			aparms[i].angAmt = 25.0f;
			aparms[i].axis = 1; //rotate left and right
			aparms[i].boneIdx = rand()%numBones; //random bone
			aparms[i].timeScale = 0.1f; //acts as a framestep
		}
		noesisAnim_t *anim = rapi->rpgCreateProceduralAnim(bones, numBones, aparms, numMoveBones, 500);
		if (anim)
		{
			rapi->rpgSetExData_AnimsNum(anim, 1);
		}
	}
#endif

	noesisModel_t *mdl = rapi->rpgConstructModel();
	if (!mdl)
	{ //nothing could be created
		return NULL;
	}

	models.Append(mdl);
	rapi->rpgDestroyContext(pgctx);

	materials.Clear();

	if (models.Num() <= 0)
	{
		return NULL;
	}

	numMdl = models.Num();

	noesisModel_t *mdlList = rapi->Noesis_ModelsFromList(models, numMdl);
	models.Clear();

	return mdlList;
}

bool NPAPI_InitLocal()
{
	//g_mfn = mathfn;
	//g_nfn = noepfn;

	if (g_nfn->NPAPI_GetAPIVersion() < NOESIS_PLUGINAPI_VERSION)
	{ //bad version of noesis for this plugin
		return false;
	}

	int fh = g_nfn->NPAPI_Register("Kingdom Hearts: Birth by Sleep Model", ".pmo");
	if (fh < 0)
	{
		return false;
	}

	//set the data handlers for this format
	g_nfn->NPAPI_SetTypeHandler_TypeCheck(fh, Model_KHBBS_Check);
	g_nfn->NPAPI_SetTypeHandler_LoadModel(fh, Model_KHBBS_Load);

	return true;
}

void NPAPI_ShutdownLocal()
{

}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
    return TRUE;
}
