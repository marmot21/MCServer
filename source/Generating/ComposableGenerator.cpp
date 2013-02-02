
// ComposableGenerator.cpp

// Implements the cComposableGenerator class representing the chunk generator that takes the composition approach to generating chunks

#include "Globals.h"

#include "ComposableGenerator.h"
#include "../World.h"
#include "../../iniFile/iniFile.h"
#include "../Root.h"
#include "ChunkDesc.h"

// Individual composed algorithms:
#include "BioGen.h"
#include "HeiGen.h"
#include "CompoGen.h"
#include "StructGen.h"
#include "FinishGen.h"
#include "Ravines.h"
#include "Caves.h"





cComposableGenerator::cComposableGenerator(cChunkGenerator & a_ChunkGenerator) :
	super(a_ChunkGenerator),
	m_BiomeGen(NULL),
	m_HeightGen(NULL),
	m_CompositionGen(NULL)
{
}





cComposableGenerator::~cComposableGenerator()
{
	// Delete the generating composition:
	for (cFinishGenList::const_iterator itr = m_FinishGens.begin(); itr != m_FinishGens.end(); ++itr)
	{
		delete *itr;
	}
	m_FinishGens.clear();
	for (cStructureGenList::const_iterator itr = m_StructureGens.begin(); itr != m_StructureGens.end(); ++itr)
	{
		delete *itr;
	}
	m_StructureGens.clear();
	delete m_CompositionGen;
	m_CompositionGen = NULL;
	delete m_HeightGen;
	m_HeightGen = NULL;
	delete m_BiomeGen;
	m_BiomeGen = NULL;
}





void cComposableGenerator::Initialize(cWorld * a_World, cIniFile & a_IniFile)
{
	super::Initialize(a_World, a_IniFile);
	
	InitBiomeGen(a_IniFile);
	InitHeightGen(a_IniFile);
	InitCompositionGen(a_IniFile);
	InitStructureGens(a_IniFile);
	InitFinishGens(a_IniFile);
}





void cComposableGenerator::GenerateBiomes(int a_ChunkX, int a_ChunkZ, cChunkDef::BiomeMap & a_BiomeMap)
{
	if (m_BiomeGen != NULL)  // Quick fix for generator deinitializing before the world storage finishes loading
	{
		m_BiomeGen->GenBiomes(a_ChunkX, a_ChunkZ, a_BiomeMap);
	}
}





void cComposableGenerator::DoGenerate(int a_ChunkX, int a_ChunkZ, cChunkDesc & a_ChunkDesc, cEntityList & a_Entities, cBlockEntityList & a_BlockEntities)
{
	cChunkDef::BiomeMap &     BiomeMap      = a_ChunkDesc.GetBiomeMap();
	cChunkDef::BlockTypes &   BlockTypes    = a_ChunkDesc.GetBlockTypes();
	cChunkDef::BlockNibbles & BlockMeta     = a_ChunkDesc.GetBlockMetas();
	cChunkDef::HeightMap &    HeightMap     = a_ChunkDesc.GetHeightMap();
	cEntityList &             Entities      = a_Entities;
	cBlockEntityList &        BlockEntities = a_BlockEntities;

	if (a_ChunkDesc.IsUsingDefaultBiomes())
	{
		m_BiomeGen->GenBiomes(a_ChunkX, a_ChunkZ, BiomeMap);
	}
	
	if (a_ChunkDesc.IsUsingDefaultHeight())
	{
		m_HeightGen->GenHeightMap(a_ChunkX, a_ChunkZ, HeightMap);
	}
	
	if (a_ChunkDesc.IsUsingDefaultComposition())
	{
		m_CompositionGen->ComposeTerrain(a_ChunkX, a_ChunkZ, BlockTypes, BlockMeta, HeightMap, BiomeMap, a_Entities, a_BlockEntities);
	}

	if (a_ChunkDesc.IsUsingDefaultStructures())
	{	
		for (cStructureGenList::iterator itr = m_StructureGens.begin(); itr != m_StructureGens.end(); ++itr)
		{
			(*itr)->GenStructures(a_ChunkX, a_ChunkZ, BlockTypes, BlockMeta, HeightMap, Entities, BlockEntities);
		}   // for itr - m_StructureGens[]
	}
	
	if (a_ChunkDesc.IsUsingDefaultFinish())
	{
		for (cFinishGenList::iterator itr = m_FinishGens.begin(); itr != m_FinishGens.end(); ++itr)
		{
			(*itr)->GenFinish(a_ChunkX, a_ChunkZ, BlockTypes, BlockMeta, HeightMap, BiomeMap, Entities, BlockEntities);
		}  // for itr - m_FinishGens[]
	}
}





void cComposableGenerator::InitBiomeGen(cIniFile & a_IniFile)
{
	AString BiomeGenName = a_IniFile.GetValueSet("Generator", "BiomeGen", "");
	if (BiomeGenName.empty())
	{
		LOGWARN("[Generator]::BiomeGen value not found in world.ini, using \"DistortedVoronoi\".");
		BiomeGenName = "DistortedVoronoi";
	}
	
	int Seed = m_ChunkGenerator.GetSeed();
	bool CacheOffByDefault = false;
	if (NoCaseCompare(BiomeGenName, "constant") == 0)
	{
		m_BiomeGen = new cBioGenConstant;
		CacheOffByDefault = true;  // we're generating faster than a cache would retrieve data :)
	}
	else if (NoCaseCompare(BiomeGenName, "checkerboard") == 0)
	{
		m_BiomeGen = new cBioGenCheckerboard;
		CacheOffByDefault = true;  // we're (probably) generating faster than a cache would retrieve data
	}
	else if (NoCaseCompare(BiomeGenName, "voronoi") == 0)
	{
		m_BiomeGen = new cBioGenVoronoi(Seed);
	}
	else if (NoCaseCompare(BiomeGenName, "multistepmap") == 0)
	{
		m_BiomeGen = new cBioGenMultiStepMap(Seed);
	}
	else
	{
		if (NoCaseCompare(BiomeGenName, "distortedvoronoi") != 0)
		{
			LOGWARNING("Unknown BiomeGen \"%s\", using \"DistortedVoronoi\" instead.", BiomeGenName.c_str());
		}
		m_BiomeGen = new cBioGenDistortedVoronoi(Seed);
	}
	
	// Add a cache, if requested:
	int CacheSize = a_IniFile.GetValueSetI("Generator", "BiomeGenCacheSize", CacheOffByDefault ? 0 : 64);
	if (CacheSize > 0)
	{
		if (CacheSize < 4)
		{
			LOGWARNING("Biomegen cache size set too low, would hurt performance instead of helping. Increasing from %d to %d", 
				CacheSize, 4
			);
			CacheSize = 4;
		}
		LOGINFO("Using a cache for biomegen of size %d.", CacheSize);
		m_BiomeGen = new cBioGenCache(m_BiomeGen, CacheSize);
	}
	m_BiomeGen->Initialize(a_IniFile);
}





void cComposableGenerator::InitHeightGen(cIniFile & a_IniFile)
{
	AString HeightGenName = a_IniFile.GetValueSet("Generator", "HeightGen", "");
	if (HeightGenName.empty())
	{
		LOGWARN("[Generator]::HeightGen value not found in world.ini, using \"Biomal\".");
		HeightGenName = "Biomal";
	}
	
	int Seed = m_ChunkGenerator.GetSeed();
	bool CacheOffByDefault = false;
	if (NoCaseCompare(HeightGenName, "flat") == 0)
	{
		int Height = a_IniFile.GetValueSetI("Generator", "FlatHeight", 5);
		m_HeightGen = new cHeiGenFlat(Height);
		CacheOffByDefault = true;  // We're generating faster than a cache would retrieve data
	}
	else if (NoCaseCompare(HeightGenName, "classic") == 0)
	{
		// These used to be in terrain.ini, but now they are in world.ini (so that multiple worlds can have different values):
		float HeightFreq1 = (float)a_IniFile.GetValueSetF("Generator", "ClassicHeightFreq1", 0.1);
		float HeightFreq2 = (float)a_IniFile.GetValueSetF("Generator", "ClassicHeightFreq2", 1.0);
		float HeightFreq3 = (float)a_IniFile.GetValueSetF("Generator", "ClassicHeightFreq3", 2.0);
		float HeightAmp1  = (float)a_IniFile.GetValueSetF("Generator", "ClassicHeightAmp1",  1.0);
		float HeightAmp2  = (float)a_IniFile.GetValueSetF("Generator", "ClassicHeightAmp2",  0.5);
		float HeightAmp3  = (float)a_IniFile.GetValueSetF("Generator", "ClassicHeightAmp3",  0.5);
		m_HeightGen = new cHeiGenClassic(Seed, HeightFreq1, HeightAmp1, HeightFreq2, HeightAmp2, HeightFreq3, HeightAmp3);
	}
	else  // "biomal" or <not found>
	{
		if (NoCaseCompare(HeightGenName, "biomal") != 0)
		{
			LOGWARN("Unknown HeightGen \"%s\", using \"Biomal\" instead.", HeightGenName.c_str());
		}
		m_HeightGen = new cHeiGenBiomal(Seed, *m_BiomeGen);
	}
	
	// Add a cache, if requested:
	int CacheSize = a_IniFile.GetValueSetI("Generator", "HeightGenCacheSize", CacheOffByDefault ? 0 : 64);
	if (CacheSize > 0)
	{
		if (CacheSize < 4)
		{
			LOGWARNING("Heightgen cache size set too low, would hurt performance instead of helping. Increasing from %d to %d", 
				CacheSize, 4
			);
			CacheSize = 4;
		}
		LOGINFO("Using a cache for Heightgen of size %d.", CacheSize);
		m_HeightGen = new cHeiGenCache(m_HeightGen, CacheSize);
	}
}





void cComposableGenerator::InitCompositionGen(cIniFile & a_IniFile)
{
	AString CompoGenName = a_IniFile.GetValueSet("Generator", "CompositionGen", "");
	if (CompoGenName.empty())
	{
		LOGWARN("[Generator]::CompositionGen value not found in world.ini, using \"Biomal\".");
		CompoGenName = "Biomal";
	}
	if (NoCaseCompare(CompoGenName, "sameblock") == 0)
	{
		int Block = m_ChunkGenerator.GetIniBlock(a_IniFile, "Generator", "SameBlockType", "stone");
		bool Bedrocked = (a_IniFile.GetValueSetI("Generator", "SameBlockBedrocked", 1) != 0);
		m_CompositionGen = new cCompoGenSameBlock((BLOCKTYPE)Block, Bedrocked);
	}
	else if (NoCaseCompare(CompoGenName, "debugbiomes") == 0)
	{
		m_CompositionGen = new cCompoGenDebugBiomes;
	}
	else if (NoCaseCompare(CompoGenName, "classic") == 0)
	{
		int SeaLevel    = a_IniFile.GetValueSetI("Generator", "ClassicSeaLevel", 60);
		int BeachHeight = a_IniFile.GetValueSetI("Generator", "ClassicBeachHeight", 2);
		int BeachDepth  = a_IniFile.GetValueSetI("Generator", "ClassicBeachDepth", 4);
		BLOCKTYPE BlockTop         = m_ChunkGenerator.GetIniBlock(a_IniFile, "Generator", "ClassicBlockTop",         "grass");
		BLOCKTYPE BlockMiddle      = m_ChunkGenerator.GetIniBlock(a_IniFile, "Generator", "ClassicBlockMiddle",      "dirt");
		BLOCKTYPE BlockBottom      = m_ChunkGenerator.GetIniBlock(a_IniFile, "Generator", "ClassicBlockBottom",      "stone");
		BLOCKTYPE BlockBeach       = m_ChunkGenerator.GetIniBlock(a_IniFile, "Generator", "ClassicBlockBeach",       "sand");
		BLOCKTYPE BlockBeachBottom = m_ChunkGenerator.GetIniBlock(a_IniFile, "Generator", "ClassicBlockBeachBottom", "sandstone");
		BLOCKTYPE BlockSea         = m_ChunkGenerator.GetIniBlock(a_IniFile, "Generator", "ClassicBlockSea",         "stationarywater");
		m_CompositionGen = new cCompoGenClassic(
			SeaLevel, BeachHeight, BeachDepth, BlockTop, BlockMiddle, BlockBottom, BlockBeach,
			BlockBeachBottom, BlockSea
		);
	}
	else
	{
		if (NoCaseCompare(CompoGenName, "biomal") != 0)
		{
			LOGWARN("Unknown CompositionGen \"%s\", using \"biomal\" instead.", CompoGenName.c_str());
		}
		int SeaLevel = a_IniFile.GetValueSetI("Generator", "BiomalSeaLevel", 62);
		int Seed = m_ChunkGenerator.GetSeed();
		m_CompositionGen = new cCompoGenBiomal(Seed, SeaLevel);
	}
}





void cComposableGenerator::InitStructureGens(cIniFile & a_IniFile)
{
	AString Structures = a_IniFile.GetValueSet("Generator", "Structures", "Ravines,WormNestCaves,OreNests,Trees");

	int Seed = m_ChunkGenerator.GetSeed();
	AStringVector Str = StringSplit(Structures, ",");
	for (AStringVector::const_iterator itr = Str.begin(); itr != Str.end(); ++itr)
	{
		if (NoCaseCompare(*itr, "trees") == 0)
		{
			m_StructureGens.push_back(new cStructGenTrees(Seed, m_BiomeGen, m_HeightGen, m_CompositionGen));
		}
		else if (NoCaseCompare(*itr, "marblecaves") == 0)
		{
			m_StructureGens.push_back(new cStructGenMarbleCaves(Seed));
		}
		else if (NoCaseCompare(*itr, "dualridgecaves") == 0)
		{
			float Threshold = (float)a_IniFile.GetValueSetF("Generator", "DualRidgeCavesThreshold", 0.3);
			m_StructureGens.push_back(new cStructGenDualRidgeCaves(Seed, Threshold));
		}
		else if (NoCaseCompare(*itr, "orenests") == 0)
		{
			m_StructureGens.push_back(new cStructGenOreNests(Seed));
		}
		else if (NoCaseCompare(*itr, "ravines") == 0)
		{
			m_StructureGens.push_back(new cStructGenRavines(Seed, 128));
		}
		//*
		// TODO: Not implemented yet; need a name
		else if (NoCaseCompare(*itr, "wormnestcaves") == 0)
		{
			m_StructureGens.push_back(new cStructGenWormNestCaves(Seed));
		}
		//*/
		else
		{
			LOGWARNING("Unknown structure generator: \"%s\". Ignoring.", itr->c_str());
		}
	}  // for itr - Str[]
}





void cComposableGenerator::InitFinishGens(cIniFile & a_IniFile)
{
	int Seed = m_ChunkGenerator.GetSeed();
	AString Structures = a_IniFile.GetValueSet("Generator", "Finishers", "SprinkleFoliage,Ice,Snow,Lilypads,BottomLava,DeadBushes,PreSimulator");

	AStringVector Str = StringSplit(Structures, ",");
	for (AStringVector::const_iterator itr = Str.begin(); itr != Str.end(); ++itr)
	{
		// Finishers, alpha-sorted:
		if (NoCaseCompare(*itr, "BottomLava") == 0)
		{
			int BottomLavaLevel = a_IniFile.GetValueSetI("Generator", "BottomLavaLevel", 10);
			m_FinishGens.push_back(new cFinishGenBottomLava(BottomLavaLevel));
		}
		else if (NoCaseCompare(*itr, "DeadBushes") == 0)
		{
			m_FinishGens.push_back(new cFinishGenDeadBushes(Seed));
		}
		else if (NoCaseCompare(*itr, "Ice") == 0)
		{
			m_FinishGens.push_back(new cFinishGenIce);
		}
		else if (NoCaseCompare(*itr, "Lilypads") == 0)
		{
			m_FinishGens.push_back(new cFinishGenLilypads(Seed));
		}
		else if (NoCaseCompare(*itr, "PreSimulator") == 0)
		{
			m_FinishGens.push_back(new cFinishGenPreSimulator);
		}
		else if (NoCaseCompare(*itr, "Snow") == 0)
		{
			m_FinishGens.push_back(new cFinishGenSnow);
		}
		else if (NoCaseCompare(*itr, "SprinkleFoliage") == 0)
		{
			m_FinishGens.push_back(new cFinishGenSprinkleFoliage(Seed));
		}
	}  // for itr - Str[]
}



