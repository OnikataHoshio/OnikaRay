#include "KH_Algorithms.h"
#include "KH_DebugUtils.h"

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
	auto& ShaderManager = KH_ShaderManager::Instance();

	Scan_Pass1 = ShaderManager.LoadComputeShader("Assert/Shaders/ComputeShaders/RadixSortV1/RadixSort_Scan_Pass1.comp");
	Scan_Pass2 = ShaderManager.LoadComputeShader("Assert/Shaders/ComputeShaders/RadixSortV1/RadixSort_Scan_Pass2.comp");
	Scan_Pass3 = ShaderManager.LoadComputeShader("Assert/Shaders/ComputeShaders/RadixSortV1/RadixSort_Scan_Pass3.comp");
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
	auto& ShaderManager = KH_ShaderManager::Instance();

	RadixSort_Pass1 = ShaderManager.LoadComputeShader("Assert/Shaders/ComputeShaders/RadixSortV1/RadixSort_Pass1.comp");
	RadixSort_Pass2 = ShaderManager.LoadComputeShader("Assert/Shaders/ComputeShaders/RadixSortV1/RadixSort_Pass2.comp");
	RadixSort_Scan_Pass1 = ShaderManager.LoadComputeShader("Assert/Shaders/ComputeShaders/RadixSortV1/RadixSort_Scan_Pass1.comp");
	RadixSort_Scan_Pass2 = ShaderManager.LoadComputeShader("Assert/Shaders/ComputeShaders/RadixSortV1/RadixSort_Scan_Pass2.comp");
	RadixSort_Scan_Pass3 = ShaderManager.LoadComputeShader("Assert/Shaders/ComputeShaders/RadixSortV1/RadixSort_Scan_Pass3.comp");
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

uint32_t KH_MortonCode::Morton3D(uint32_t x, uint32_t y, uint32_t z)
{
	constexpr uint32_t MaxCoord = 1023u;

	if (x > MaxCoord || y > MaxCoord || z > MaxCoord)
	{
		LOG_W(std::format(
			"KH_MortonCode::Morton3D: Input ({}, {}, {}) out of 10-bit range [0, 1023]. Clamped.",
			x, y, z));
		x = std::min(x, MaxCoord);
		y = std::min(y, MaxCoord);
		z = std::min(z, MaxCoord);
	}

	uint32_t Morton = 0u;

	for (uint32_t i = 0; i < 10u; ++i)
	{
		Morton |= ((x >> i) & 1u) << (3u * i + 2u);
		Morton |= ((y >> i) & 1u) << (3u * i + 1u);
		Morton |= ((z >> i) & 1u) << (3u * i + 0u);
	}

	return Morton;
}

uint32_t KH_MortonCode::Morton3D_MagicBits(uint32_t x, uint32_t y, uint32_t z)
{
	constexpr uint32_t MaxCoord = 1023u;

	if (x > MaxCoord || y > MaxCoord || z > MaxCoord)
	{
		LOG_W(std::format(
			"KH_MortonCode::Morton3D_MagicBits: Input ({}, {}, {}) out of 10-bit range [0, 1023]. Clamped.",
			x, y, z));
		x = std::min(x, MaxCoord);
		y = std::min(y, MaxCoord);
		z = std::min(z, MaxCoord);
	}

	const uint32_t bit_x = ExpandBits(x);
	const uint32_t bit_y = ExpandBits(y);
	const uint32_t bit_z = ExpandBits(z);

	return (bit_x << 2u) | (bit_y << 1u) | bit_z;
}

uint32_t KH_MortonCode::Morton3DFloat_MagicBits(float x, float y, float z, uint32_t MORTON_RESOLUTION)
{
	return Morton3DFloat_MagicBits(glm::vec3(x, y, z), MORTON_RESOLUTION);
}

uint32_t KH_MortonCode::Morton3DFloat_MagicBits(glm::vec3 p, uint32_t MORTON_RESOLUTION)
{
	constexpr uint32_t MaxCoord = 1023u;
	constexpr uint32_t DefaultResolution = 1024u; // 2^10

	const uint32_t Resolution =
		(MORTON_RESOLUTION == 0u) ? DefaultResolution
		: std::min(MORTON_RESOLUTION, DefaultResolution);

	const glm::vec3 Original = p;

	p = glm::clamp(p, glm::vec3(0.0f), glm::vec3(1.0f));

	if (p != Original)
	{
		LOG_W(std::format(
			"KH_MortonCode::Morton3DFloat_MagicBits: Input ({}, {}, {}) out of range [0, 1]. Clamped.",
			Original.x, Original.y, Original.z));
	}

	const uint32_t x = std::min(
		MaxCoord,
		static_cast<uint32_t>(p.x * static_cast<float>(Resolution)));

	const uint32_t y = std::min(
		MaxCoord,
		static_cast<uint32_t>(p.y * static_cast<float>(Resolution)));

	const uint32_t z = std::min(
		MaxCoord,
		static_cast<uint32_t>(p.z * static_cast<float>(Resolution)));

	const uint32_t bit_x = ExpandBits(x);
	const uint32_t bit_y = ExpandBits(y);
	const uint32_t bit_z = ExpandBits(z);

	return (bit_x << 2u) | (bit_y << 1u) | bit_z;
}

uint64_t KH_MortonCode::Morton3DFloat_IndexAugmentation(glm::vec3 p, uint32_t index, uint32_t MORTON_RESOLUTION)
{
	const uint32_t morton = Morton3DFloat_MagicBits(p, MORTON_RESOLUTION);
	return (static_cast<uint64_t>(morton) << 32u) | static_cast<uint64_t>(index);
}


uint32_t KH_MortonCode::ExpandBits(uint32_t v)
{
	v = (v * 0x00010001u) & 0xFF0000FFu;
	v = (v * 0x00000101u) & 0x0F00F00Fu;
	v = (v * 0x00000011u) & 0xC30C30C3u;
	v = (v * 0x00000005u) & 0x49249249u;

	return v;
}

float KH_LowDiscrepancySequence::IntegerRadicalInverse(int Base, int i)
{
	int numPoints, inverse;
	numPoints = 1;

	for (inverse = 0; i > 0; i /= Base)
	{
		inverse = inverse * Base + (i % Base);
		numPoints = numPoints * Base;
	}

	return inverse / (double)numPoints;
}

float KH_LowDiscrepancySequence::RadicalInverse(int Base, int i)
{
	double Digit, Radical, Inverse;
	Digit = Radical = 1.0 / (double)Base;
	Inverse = 0.0;
	while (i)
	{
		Inverse += Digit * (double)(i % Base);
		Digit *= Radical;

		i /= Base;
	}
	return Inverse;
}

int KH_LowDiscrepancySequence::NthPrimeNumber(int Dimension)
{
	static const int primes256[] = {
		 2,    3,    5,    7,   11,   13,   17,   19,
		23,   29,   31,   37,   41,   43,   47,   53,
		59,   61,   67,   71,   73,   79,   83,   89,
		97,  101,  103,  107,  109,  113,  127,  131,
	   137,  139,  149,  151,  157,  163,  167,  173,
	   179,  181,  191,  193,  197,  199,  211,  223,
	   227,  229,  233,  239,  241,  251,  257,  263,
	   269,  271,  277,  281,  283,  293,  307,  311,

	   313,  317,  331,  337,  347,  349,  353,  359,
	   367,  373,  379,  383,  389,  397,  401,  409,
	   419,  421,  431,  433,  439,  443,  449,  457,
	   461,  463,  467,  479,  487,  491,  499,  503,
	   509,  521,  523,  541,  547,  557,  563,  569,
	   571,  577,  587,  593,  599,  601,  607,  613,
	   617,  619,  631,  641,  643,  647,  653,  659,
	   661,  673,  677,  683,  691,  701,  709,  719,

	   727,  733,  739,  743,  751,  757,  761,  769,
	   773,  787,  797,  809,  811,  821,  823,  827,
	   829,  839,  853,  857,  859,  863,  877,  881,
	   883,  887,  907,  911,  919,  929,  937,  941,
	   947,  953,  967,  971,  977,  983,  991,  997,
	  1009, 1013, 1019, 1021, 1031, 1033, 1039, 1049,
	  1051, 1061, 1063, 1069, 1087, 1091, 1093, 1097,
	  1103, 1109, 1117, 1123, 1129, 1151, 1153, 1163,

	  1171, 1181, 1187, 1193, 1201, 1213, 1217, 1223,
	  1229, 1231, 1237, 1249, 1259, 1277, 1279, 1283,
	  1289, 1291, 1297, 1301, 1303, 1307, 1319, 1321,
	  1327, 1361, 1367, 1373, 1381, 1399, 1409, 1423,
	  1427, 1429, 1433, 1439, 1447, 1451, 1453, 1459,
	  1471, 1481, 1483, 1487, 1489, 1493, 1499, 1511,
	  1523, 1531, 1543, 1549, 1553, 1559, 1567, 1571,
	  1579, 1583, 1597, 1601, 1607, 1609, 1613, 1619
	};

	Dimension = std::clamp(Dimension, 0, 255);

	return primes256[Dimension];
}

uint32_t KH_Sobol::GenerateM(uint32_t k, uint32_t S, std::vector<uint32_t>& A_component, std::vector<uint32_t>& M)
{
	int Radical = 1;
	uint32_t Result = 0;
	for (int i = 1; i < S; i++)
	{
		Radical *= 2;
		Result ^= Radical * A_component[i] * M[k - i];
	}

	Result ^= (Radical * 2 * M[k - S]) ^ (M[k - S]);

	M[k] = Result;

	return Result;
}

uint32_t KH_Sobol::GrayCode(uint32_t i)
{
	return i ^ (i >> 1);
}

float KH_Sobol::Sobol(std::vector<uint32_t>& V, uint32_t i)
{
	unsigned int result = 0;
	for (unsigned int j = 1; i; i >>= 1, j++)
		if (i & 1)
			result ^= V[j];
	return result * (1.0f / 4294967296.0f);
}

std::vector<float> KH_Sobol::GenerateSobols(uint32_t D, uint32_t S, uint32_t A, std::vector<uint32_t>& M)
{
	std::vector<uint32_t> V = GenerateDirectionNumbers(D, S, A, M);

	std::vector<float> Sobols(30);

	for (int i = 0; i < 30; i++)
	{
		Sobols[i] = Sobol(V, GrayCode(i));
	}

	std::cout << std::format("Sobols[0]-[30]:\n");
	for (int i = 0; i < 30; i++)
	{
		std::cout << Sobols[i] << std::endl;
	}
	std::cout << std::endl;

	return Sobols;
}

std::vector<uint32_t> KH_Sobol::GenerateDirectionNumbers(uint32_t D, uint32_t S, uint32_t A, std::vector<uint32_t>& M)
{
	std::vector<uint32_t> V(33);
	if (D == 0)
	{
		for (int i = 1; i<=32; i++)
		{
			V[i] = (1 << (32 - i));
		}

		std::cout << std::format("V[1]-[32]:\n");
		for (int i = 1; i <= 32; i++)
		{
			std::cout << V[i] << std::endl;
		}
		std::cout << std::endl;

		return V;
	}

	std::vector<uint32_t> A_component(S);

	M.resize(33);

	// decompose A
	for (int i = 1; i < S; i++)
	{
		A_component[S-i] = A & 1;
		A >>= 1;
	}

	std::cout << std::format("A_component[1]-[{}]:\n", S-1);
	for (int i = 1; i < S; i++)
	{
		std::cout << A_component[i] << " ";
	}
	std::cout << std::endl << std::endl;


	// Generate M
	for (int i = S + 1; i <= 32; i++)
	{
		GenerateM(i, S, A_component, M);
	}

	std::cout << std::format("M[1]-[32]:\n");
	for (int i = 1; i <= 32; i++)
	{
		std::cout << M[i] << std::endl;
	}
	std::cout << std::endl;

	// Generate Direction Numbers
	for (int i = 1; i <= 32; i++)
	{
		V[i] = M[i] * (1 << (32 - i));
	}

	std::cout << std::format("V[1]-[32]:\n");
	for (int i = 1; i <=32;i++)
	{
		std::cout << V[i] << std::endl;
	}
	std::cout << std::endl;
	return V;
}

float KH_LowDiscrepancySequence::Halton(int Dimension, int Index)
{
	return RadicalInverse(NthPrimeNumber(Dimension), Index);
}

float KH_LowDiscrepancySequence::Hammersley(int Dimension, int Index, int NumSamples)
{
	if (Dimension == 0)
		return Index / (float)NumSamples;
	else
		return RadicalInverse(NthPrimeNumber(Dimension - 1), Index);
}


