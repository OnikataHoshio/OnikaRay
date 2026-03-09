#pragma once

#include "KH_Common.h"
#include "Pipeline/KH_SSBO.h"
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



