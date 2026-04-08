#pragma once

#include "KH_Common.h"
#include "Pipeline/KH_Buffer.h"
#include "Pipeline/KH_Shader.h"

class KH_ScanCPU
{
public:
	KH_ScanCPU(std::vector<int>& Data);
	~KH_ScanCPU() = default;

	std::vector<int> Data;
	int Depth;

	void ProcessScanCPU();

private:
	enum class KH_SCAN_STAGE
	{
		KH_SCAN_UPSWEEP_STAGE = 0,
		KH_SCAN_DOWNSWEEP_STAGE = 1,
		KH_SCAN_UNKNOWN = 2
	};

	void UpsweepCPU();
	void DownsweepCPU();
	void PrintArray(int Depth, KH_SCAN_STAGE ScanStage);
};

class KH_ScanGPU
{
public:
	KH_ScanGPU();

	void RunScanStep(glm::ivec4* dataPtr, int count);

private:
	KH_SSBO<glm::ivec4> DataSSBO;
	KH_SSBO<glm::ivec4> ScanSSBO;
	KH_SSBO<glm::ivec4> BlockSumSSBO;

	KH_Shader Scan_Pass1;
	KH_Shader Scan_Pass2;
	KH_Shader Scan_Pass3;
};

class KH_RadixSort
{
public:
	KH_RadixSort();
	void RunRadixSort(int* dataPtr, int count);
	void RunRadixSortStep(int* dataPtr, int count, int bitShift);
private:
	KH_SSBO<int> RadixSort_InputSSBO;
	KH_SSBO<int> RadixSort_LocalShuffleSSBO;
	KH_SSBO<glm::ivec4> RadixSort_BlockSumSSBO;
	KH_SSBO<glm::ivec4> Scan_ScanSSBO;
	KH_SSBO<glm::ivec4> Scan_BlockSumSSBO;
	KH_SSBO<int> RadixSort_FinalOrderSSBO;
	
	KH_Shader RadixSort_Pass1;
	KH_Shader RadixSort_Pass2;
	KH_Shader RadixSort_Scan_Pass1;
	KH_Shader RadixSort_Scan_Pass2;
	KH_Shader RadixSort_Scan_Pass3;
};

class KH_MortonCode
{
public:
	static uint32_t Morton3D(uint32_t x, uint32_t y, uint32_t z);
		  
	static uint32_t Morton3D_MagicBits(uint32_t x, uint32_t y, uint32_t z);
		  
	static uint32_t Morton3DFloat_MagicBits(float x, float y, float z, uint32_t MORTON_RESOLUTION = 1024u);

	static uint32_t Morton3DFloat_MagicBits(glm::vec3 p, uint32_t MORTON_RESOLUTION = 1024u);

	static uint64_t Morton3DFloat_IndexAugmentation(glm::vec3 p, uint32_t index, uint32_t MORTON_RESOLUTION = 1024u);
private: 
	static uint32_t ExpandBits(uint32_t v);
};

class KH_LowDiscrepancySequence
{
public:
	static float IntegerRadicalInverse(int Base, int i);

	static float RadicalInverse(int Base, int i);

	static float Halton(int Dimension, int Index);

	static float Hammersley(int Dimension, int Index, int NumSamples);

private:
	static int NthPrimeNumber(int Dimension);
};

class KH_Sobol
{
public:
	static uint32_t GenerateM(uint32_t k, uint32_t S, std::vector<uint32_t>& A_component, std::vector<uint32_t>& M);

	static uint32_t GrayCode(uint32_t i);

	static float Sobol(std::vector<uint32_t>& V, uint32_t i);

	static std::vector<float> GenerateSobols(uint32_t D, uint32_t S, uint32_t A, std::vector<uint32_t>& M);

	static std::vector<uint32_t> GenerateDirectionNumbers(uint32_t D, uint32_t S, uint32_t A, std::vector<uint32_t>& M);

};
