#include "KH_GPUAlgorithms.h"

KH_ScanCPU::KH_ScanCPU(std::vector<int>& Data)
	:Data(Data)
{
	size_t n = Data.size();
	Depth = static_cast<int>(glm::ceil(glm::log2(static_cast<float>(n))));
	size_t targetSize = static_cast<size_t>(1) << Depth;
	if (n < targetSize) {
		this->Data.resize(targetSize, 0);
	}
}

void KH_ScanCPU::ProcessScanCPU()
{
	UpsweepCPU();
	DownsweepCPU();
}

void KH_ScanCPU::UpsweepCPU()
{
	const int elementCount = Data.size();

	for (int level = 0; level < Depth; ++level)
	{
		const int stride = 1 << (level + 1);       
		const int halfStride = 1 << level;         

		for (int baseIndex = 0; baseIndex < elementCount; baseIndex += stride)
		{
			const int leftIndex = baseIndex + halfStride - 1;
			const int rightIndex = baseIndex + stride - 1;

			Data[rightIndex] += Data[leftIndex];
		}

		PrintArray(level, KH_SCAN_STAGE::KH_SCAN_UPSWEEP_STAGE);
	}
}

void KH_ScanCPU::DownsweepCPU()
{
	const int elementCount = Data.size();

	Data.back() = 0;

	for (int level = Depth - 1; level >= 0; --level)
	{
		const int stride = 1 << (level + 1);
		const int halfStride = 1 << level;

		for (int baseIndex = 0; baseIndex < elementCount; baseIndex += stride)
		{
			const int leftIndex = baseIndex + halfStride - 1;
			const int rightIndex = baseIndex + stride - 1;

			const int leftValue = Data[leftIndex];

			Data[leftIndex] = Data[rightIndex];
			Data[rightIndex] += leftValue;
		}

		PrintArray(level, KH_SCAN_STAGE::KH_SCAN_DOWNSWEEP_STAGE);
	}
}

void KH_ScanCPU::PrintArray(int level, KH_SCAN_STAGE stage)
{
	std::cout << std::format(
		"\nArray State (size = {})\nStage: {} | Level: {}\n",
		Data.size(),
		magic_enum::enum_name(stage),
		level
	);

	constexpr int columnCount = 8;

	for (size_t i = 0; i < Data.size(); ++i)
	{
		std::cout << std::setw(4) << Data[i] << " ";

		if ((i + 1) % columnCount == 0)
			std::cout << '\n';
	}

	std::cout << "\n\n";
}

KH_ScanGPU::KH_ScanGPU()
{
	Scan_Pass1.Create("Assert/Shaders/ComputeShaders/RadixSortV1/RadixSort_Scan_Pass1.comp");
	Scan_Pass2.Create("Assert/Shaders/ComputeShaders/RadixSortV1/RadixSort_Scan_Pass2.comp");
	Scan_Pass3.Create("Assert/Shaders/ComputeShaders/RadixSortV1/RadixSort_Scan_Pass3.comp");
}

void KH_ScanGPU::RunScanStep(glm::ivec4* dataPtr, int count)
{
	DataSSBO.SetData(dataPtr, count, GL_DYNAMIC_DRAW);

	if (ScanSSBO.GetCount() != count)
		ScanSSBO.SetData(nullptr, count, GL_DYNAMIC_DRAW);

	int numBlocks = (count + 255) / 256;
	if (BlockSumSSBO.GetCount() != numBlocks)
		BlockSumSSBO.SetData(nullptr, numBlocks, GL_DYNAMIC_DRAW);

	DataSSBO.Bind(0);      
	ScanSSBO.Bind(1);      
	BlockSumSSBO.Bind(2);  

	// --- Pass 1: Local Scan ---
	Scan_Pass1.Use();
	Scan_Pass1.SetInt("u_elementCount", count);

	glDispatchCompute(numBlocks, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	// --- Pass 2: Global Scan ---
	Scan_Pass2.Use();
	Scan_Pass2.SetInt("u_blockCount", numBlocks);
	glDispatchCompute(1, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	// --- Pass 3: Add Offset ---
	Scan_Pass3.Use();
	Scan_Pass3.SetInt("u_elementCount", count);
	glDispatchCompute(numBlocks, 1, 1);
	glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);


	std::vector<glm::ivec4> resultData;
	ScanSSBO.GetData(resultData);

	std::vector<glm::ivec4>  BlockSumData;
	BlockSumSSBO.GetData(BlockSumData);

	memcpy(dataPtr, resultData.data(), count * sizeof(glm::ivec4));
}

KH_RadixSort::KH_RadixSort()
{
	RadixSort_Pass1.Create("Assert/Shaders/ComputeShaders/RadixSortV1/RadixSort_Pass1.comp");
	RadixSort_Pass2.Create("Assert/Shaders/ComputeShaders/RadixSortV1/RadixSort_Pass2.comp");
	RadixSort_Scan_Pass1.Create("Assert/Shaders/ComputeShaders/RadixSortV1/RadixSort_Scan_Pass1.comp");
	RadixSort_Scan_Pass2.Create("Assert/Shaders/ComputeShaders/RadixSortV1/RadixSort_Scan_Pass2.comp");
	RadixSort_Scan_Pass3.Create("Assert/Shaders/ComputeShaders/RadixSortV1/RadixSort_Scan_Pass3.comp");
}

void KH_RadixSort::RunRadixSort(int* dataPtr, int count)
{
	for (int bitShift = 0; bitShift <= 30; bitShift += 2)
		RunRadixSortStep(dataPtr, count, bitShift);
}

void KH_RadixSort::RunRadixSortStep(int* dataPtr, int count, int bitShift)
{
	RadixSort_InputSSBO.SetData(dataPtr, count, GL_DYNAMIC_DRAW);

	if (RadixSort_LocalShuffleSSBO.GetCount() != count)
		RadixSort_LocalShuffleSSBO.SetData(nullptr, count, GL_DYNAMIC_DRAW);

	int RadixSort_NumBlocks = (count + 255) / 256;
	if (RadixSort_BlockSumSSBO.GetCount() != RadixSort_NumBlocks)
		RadixSort_BlockSumSSBO.SetData(nullptr, RadixSort_NumBlocks, GL_DYNAMIC_DRAW);

	if (Scan_ScanSSBO.GetCount() != RadixSort_NumBlocks)
		Scan_ScanSSBO.SetData(nullptr, RadixSort_NumBlocks, GL_DYNAMIC_DRAW);

	int Scan_NumBlocks = (RadixSort_NumBlocks + 255) / 256;

	if (Scan_BlockSumSSBO.GetCount() != Scan_NumBlocks)
		Scan_BlockSumSSBO.SetData(nullptr, Scan_NumBlocks, GL_DYNAMIC_DRAW);

	if (RadixSort_FinalOrderSSBO.GetCount() != count)
		RadixSort_FinalOrderSSBO.SetData(nullptr, count, GL_DYNAMIC_DRAW);

	RadixSort_InputSSBO.Bind(0);
	RadixSort_LocalShuffleSSBO.Bind(1);
	RadixSort_BlockSumSSBO.Bind(2);
	Scan_ScanSSBO.Bind(3);
	Scan_BlockSumSSBO.Bind(4);
	RadixSort_FinalOrderSSBO.Bind(5);

	// --- Pass 1: Radix Sort Local Pass ---
	RadixSort_Pass1.Use();
	RadixSort_Pass1.SetInt("u_elementCount", count);
	RadixSort_Pass1.SetInt("u_bitShift", bitShift);
	glDispatchCompute(RadixSort_NumBlocks, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	// --- Pass 2: Local Scan Pass---
	RadixSort_Scan_Pass1.Use();
	RadixSort_Scan_Pass1.SetInt("u_elementCount", RadixSort_NumBlocks);

	glDispatchCompute(Scan_NumBlocks, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	// --- Pass 3: Global Scan Pass---
	RadixSort_Scan_Pass2.Use();
	RadixSort_Scan_Pass2.SetInt("u_blockCount", Scan_NumBlocks);
	glDispatchCompute(1, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	// --- Pass 4: Add Scan Offset ---
	RadixSort_Scan_Pass3.Use();
	RadixSort_Scan_Pass3.SetInt("u_elementCount", RadixSort_NumBlocks);
	glDispatchCompute(Scan_NumBlocks, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	// --- Pass 5: Add Radix Sort Offset ---
	RadixSort_Pass2.Use();
	RadixSort_Pass2.SetInt("u_elementCount", count);
	RadixSort_Pass2.SetInt("u_bitShift", bitShift);
	glDispatchCompute(RadixSort_NumBlocks, 1, 1);
	glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

	//std::vector<int> RadixSort_LocalShuffleData;
	//RadixSort_LocalShuffleSSBO.GetData(RadixSort_LocalShuffleData);

	//std::vector<glm::ivec4> RadixSort_BlockSumData;
	//RadixSort_BlockSumSSBO.GetData(RadixSort_BlockSumData);

	//std::vector<glm::ivec4> Scan_ScanData;
	//Scan_ScanSSBO.GetData(Scan_ScanData);

	//std::vector<glm::ivec4> Scan_BlockSumData;
	//Scan_BlockSumSSBO.GetData(Scan_BlockSumData);

	std::vector<int> RadixSort_FinalOrderData;
	RadixSort_FinalOrderSSBO.GetData(RadixSort_FinalOrderData);

	memcpy(dataPtr, RadixSort_FinalOrderData.data(), count * sizeof(int));
}
